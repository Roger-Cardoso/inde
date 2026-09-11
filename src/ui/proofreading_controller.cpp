#include "inde/ui/proofreading_controller.hpp"

#include <algorithm>
#include <cstdint>
#include <glib-object.h>
#include <sigc++/adaptors/track_obj.h>

namespace inde::ui {
namespace {

constexpr const char *state_key = "inde-proofreading-state";
constexpr const char *watched_key = "inde-proofreading-watched";
constexpr std::size_t live_issue_limit = 300;
ProofreadingController *global_controller{};

struct WidgetState {
  std::vector<project::TextIssue> issues;
  std::uint64_t generation{};
  Glib::ustring original_tooltip;
  bool tooltip_remembered{};
};

WidgetState &state(Gtk::Widget &widget) {
  auto *existing = static_cast<WidgetState *>(
      g_object_get_data(G_OBJECT(widget.gobj()), state_key));
  if (existing)
    return *existing;
  auto *created = new WidgetState;
  g_object_set_data_full(
      G_OBJECT(widget.gobj()), state_key, created,
      [](gpointer data) { delete static_cast<WidgetState *>(data); });
  return *created;
}

const project::TextIssue *issue_at(Gtk::Widget &widget, std::size_t offset,
                                   bool fallback_to_first) {
  auto *value = static_cast<WidgetState *>(
      g_object_get_data(G_OBJECT(widget.gobj()), state_key));
  if (!value || value->issues.empty())
    return nullptr;
  const auto found =
      std::ranges::find_if(value->issues, [&](const auto &issue) {
        return offset >= issue.start && offset < issue.start + issue.length;
      });
  if (found != value->issues.end())
    return &*found;
  return fallback_to_first ? &value->issues.front() : nullptr;
}

bool textual_purpose(Gtk::InputPurpose purpose) {
  return purpose == Gtk::InputPurpose::FREE_FORM ||
         purpose == Gtk::InputPurpose::ALPHA ||
         purpose == Gtk::InputPurpose::NAME;
}

void configure_issue_tag(const Glib::RefPtr<Gtk::TextBuffer> &buffer,
                         const Glib::ustring &name, Pango::Underline underline,
                         const char *color) {
  auto tag = buffer->get_tag_table()->lookup(name);
  if (!tag)
    tag = buffer->create_tag(name);
  tag->property_underline() = underline;
  Gdk::RGBA rgba;
  rgba.set(color);
  tag->property_underline_rgba() = rgba;
}

} // namespace

ProofreadingController::ProofreadingController(
    application::ProofreadingService &service)
    : service_(service) {}

ProofreadingController::~ProofreadingController() {
  close_popover();
  if (focused_)
    g_object_remove_weak_pointer(G_OBJECT(focused_->gobj()),
                                 reinterpret_cast<gpointer *>(&focused_));
  if (global_controller == this)
    global_controller = nullptr;
}

void ProofreadingController::set_global(ProofreadingController *controller) {
  global_controller = controller;
}

void ProofreadingController::watch_with_global(Gtk::Widget &root) {
  if (global_controller)
    global_controller->watch(root);
}

void ProofreadingController::watch(Gtk::Widget &root) {
  if (!root_)
    root_ = &root;
  if (auto *entry = dynamic_cast<Gtk::Entry *>(&root))
    watch_entry(*entry);
  else if (auto *view = dynamic_cast<Gtk::TextView *>(&root))
    watch_text_view(*view);
  for (auto *child = root.get_first_child(); child;
       child = child->get_next_sibling())
    watch(*child);
}

void ProofreadingController::watch_entry(Gtk::Entry &entry) {
  const bool search_entry = dynamic_cast<Gtk::SearchEntry *>(&entry) != nullptr;
  // gtkmm mantém SearchEntry na hierarquia C++ de Entry, mas GtkSearchEntry
  // deixou de ser um GtkEntry no GTK 4. Propriedades específicas de GtkEntry
  // não podem ser consultadas nesse wrapper; a interface Editable é segura.
  if (dynamic_cast<Gtk::SpinButton *>(&entry) || !entry.get_editable() ||
      (!search_entry && (!entry.get_visibility() ||
                         !textual_purpose(entry.get_input_purpose()))) ||
      g_object_get_data(G_OBJECT(entry.gobj()), watched_key))
    return;
  g_object_set_data(G_OBJECT(entry.gobj()), watched_key,
                    reinterpret_cast<gpointer>(1));
  entry.signal_changed().connect([this, &entry] { refresh_entry(entry); });

  auto focus = Gtk::EventControllerFocus::create();
  focus->signal_enter().connect([this, &entry] { remember_focus(&entry); });
  focus->signal_leave().connect([this, &entry] {
    if (focused_ == &entry)
      remember_focus(nullptr);
  });
  entry.add_controller(focus);

  auto click = Gtk::GestureClick::create();
  click->set_button(3);
  click->signal_pressed().connect(
      [this, &entry, click](int, double x, double y) {
        const auto offset =
            static_cast<std::size_t>(std::max(0, entry.get_position()));
        const auto *issue = issue_at(entry, offset, false);
        if (!issue)
          return;
        click->set_state(Gtk::EventSequenceState::CLAIMED);
        show_issue_popover(entry, issue, x, y);
      });
  entry.add_controller(click);
  refresh_entry(entry);
}

void ProofreadingController::watch_text_view(Gtk::TextView &view) {
  if (!view.get_editable() || !textual_purpose(view.get_input_purpose()) ||
      g_object_get_data(G_OBJECT(view.gobj()), watched_key))
    return;
  g_object_set_data(G_OBJECT(view.gobj()), watched_key,
                    reinterpret_cast<gpointer>(1));
  const auto buffer = view.get_buffer();
  configure_issue_tag(buffer, "inde-spelling-error",
                      Pango::Underline::ERROR_LINE, "#c01c28");
  configure_issue_tag(buffer, "inde-grammar-error",
                      Pango::Underline::DOUBLE_LINE, "#b36b00");
  buffer->signal_changed().connect([this, &view] {
    auto &value = state(view);
    const auto generation = ++value.generation;
    Glib::signal_timeout().connect_once(sigc::track_object(
                                            [this, &view, generation] {
                                              if (state(view).generation ==
                                                  generation)
                                                refresh_text_view(view);
                                            },
                                            view),
                                        550);
  });

  auto focus = Gtk::EventControllerFocus::create();
  focus->signal_enter().connect([this, &view] { remember_focus(&view); });
  focus->signal_leave().connect([this, &view] {
    if (focused_ == &view)
      remember_focus(nullptr);
  });
  view.add_controller(focus);

  auto click = Gtk::GestureClick::create();
  click->set_button(3);
  click->signal_pressed().connect(
      [this, &view, click](int, double x, double y) {
        Gtk::TextBuffer::iterator iter;
        int trailing{};
        int buffer_x{};
        int buffer_y{};
        view.window_to_buffer_coords(Gtk::TextWindowType::WIDGET,
                                     static_cast<int>(x), static_cast<int>(y),
                                     buffer_x, buffer_y);
        const auto located =
            view.get_iter_at_position(iter, trailing, buffer_x, buffer_y);
        const auto offset =
            located ? static_cast<std::size_t>(iter.get_offset()) : 0;
        const auto *issue = issue_at(view, offset, false);
        if (!issue)
          return;
        click->set_state(Gtk::EventSequenceState::CLAIMED);
        show_issue_popover(view, issue, x, y);
      });
  view.add_controller(click);
  refresh_text_view(view);
}

void ProofreadingController::refresh_entry(Gtk::Entry &entry) {
  auto &value = state(entry);
  if (!value.tooltip_remembered) {
    value.original_tooltip = entry.get_tooltip_text();
    value.tooltip_remembered = true;
  }
  value.issues = service_.analyze(entry.get_text(), live_issue_limit);
  entry.remove_css_class("proofreading-spelling-error");
  entry.remove_css_class("proofreading-grammar-error");
  if (std::ranges::any_of(value.issues, [](const auto &issue) {
        return issue.kind == project::TextIssueKind::Spelling;
      }))
    entry.add_css_class("proofreading-spelling-error");
  if (std::ranges::any_of(value.issues, [](const auto &issue) {
        return issue.kind == project::TextIssueKind::Grammar;
      }))
    entry.add_css_class("proofreading-grammar-error");
  if (value.issues.empty()) {
    entry.set_tooltip_text(value.original_tooltip);
  } else {
    Glib::ustring details;
    const auto count = std::min<std::size_t>(value.issues.size(), 3);
    for (std::size_t index = 0; index < count; ++index) {
      if (index)
        details += "\n";
      details += "• " + value.issues[index].message;
    }
    if (value.issues.size() > count)
      details += "\n• e mais " + std::to_string(value.issues.size() - count);
    details += "\nClique secundário ou pressione F7 para revisar.";
    entry.set_tooltip_text(details);
  }
}

void ProofreadingController::refresh_text_view(Gtk::TextView &view) {
  const auto buffer = view.get_buffer();
  auto &value = state(view);
  value.issues = service_.analyze(buffer->get_text(), live_issue_limit);
  buffer->remove_tag_by_name("inde-spelling-error", buffer->begin(),
                             buffer->end());
  buffer->remove_tag_by_name("inde-grammar-error", buffer->begin(),
                             buffer->end());
  for (const auto &issue : value.issues) {
    const auto begin =
        buffer->get_iter_at_offset(static_cast<int>(issue.start));
    const auto end = buffer->get_iter_at_offset(
        static_cast<int>(issue.start + issue.length));
    buffer->apply_tag_by_name(issue.kind == project::TextIssueKind::Spelling
                                  ? "inde-spelling-error"
                                  : "inde-grammar-error",
                              begin, end);
  }
}

void ProofreadingController::refresh_all() {
  if (!root_)
    return;
  const auto refresh = [this](auto &&self, Gtk::Widget &widget) -> void {
    if (auto *entry = dynamic_cast<Gtk::Entry *>(&widget)) {
      if (g_object_get_data(G_OBJECT(entry->gobj()), watched_key))
        refresh_entry(*entry);
    } else if (auto *view = dynamic_cast<Gtk::TextView *>(&widget)) {
      if (g_object_get_data(G_OBJECT(view->gobj()), watched_key))
        refresh_text_view(*view);
    }
    for (auto *child = widget.get_first_child(); child;
         child = child->get_next_sibling())
      self(self, *child);
  };
  refresh(refresh, *root_);
}

void ProofreadingController::show_current_issue() {
  if (!focused_)
    return;
  std::size_t offset{};
  if (auto *entry = dynamic_cast<Gtk::Entry *>(focused_))
    offset = static_cast<std::size_t>(std::max(0, entry->get_position()));
  else if (auto *view = dynamic_cast<Gtk::TextView *>(focused_))
    offset = static_cast<std::size_t>(
        view->get_buffer()
            ->get_iter_at_mark(view->get_buffer()->get_insert())
            .get_offset());
  show_issue_popover(*focused_, issue_at(*focused_, offset, true));
}

void ProofreadingController::remember_focus(Gtk::Widget *widget) {
  if (focused_)
    g_object_remove_weak_pointer(G_OBJECT(focused_->gobj()),
                                 reinterpret_cast<gpointer *>(&focused_));
  focused_ = widget;
  if (focused_)
    g_object_add_weak_pointer(G_OBJECT(focused_->gobj()),
                              reinterpret_cast<gpointer *>(&focused_));
}

void ProofreadingController::show_issue_popover(Gtk::Widget &widget,
                                                const project::TextIssue *issue,
                                                double x, double y) {
  close_popover();
  popover_ = std::make_unique<Gtk::Popover>();
  popover_->set_parent(widget);
  Gdk::Rectangle rectangle(static_cast<int>(x), static_cast<int>(y), 1, 1);
  popover_->set_pointing_to(rectangle);
  auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
  box->set_margin(10);
  box->set_size_request(280, -1);
  if (!issue) {
    auto *label = Gtk::make_managed<Gtk::Label>(
        "Nenhum problema encontrado neste campo.");
    label->set_wrap(true);
    label->set_xalign(0.0f);
    box->append(*label);
  } else {
    auto *heading = Gtk::make_managed<Gtk::Label>(
        issue->kind == project::TextIssueKind::Spelling
            ? "Possível erro ortográfico"
            : "Possível problema gramatical");
    heading->add_css_class("heading");
    heading->set_xalign(0.0f);
    box->append(*heading);
    auto *message = Gtk::make_managed<Gtk::Label>(issue->message);
    message->set_wrap(true);
    message->set_xalign(0.0f);
    box->append(*message);

    std::vector<std::string> replacements = issue->suggestions;
    if (issue->replacement)
      replacements.insert(replacements.begin(), *issue->replacement);
    if (!replacements.empty()) {
      auto *suggestions = Gtk::make_managed<Gtk::Label>("Substituir por:");
      suggestions->add_css_class("dim-label");
      suggestions->set_xalign(0.0f);
      box->append(*suggestions);
      for (const auto &replacement : replacements) {
        auto *button = Gtk::make_managed<Gtk::Button>(
            replacement.empty() ? "Remover trecho" : replacement);
        button->set_halign(Gtk::Align::FILL);
        button->signal_clicked().connect(
            [this, &widget, selected = *issue, replacement] {
              replace_issue(widget, selected, replacement);
              if (popover_)
                popover_->popdown();
            });
        box->append(*button);
      }
    }
    if (issue->kind == project::TextIssueKind::Spelling) {
      auto *add = Gtk::make_managed<Gtk::Button>(
          "Adicionar “" + issue->excerpt + "” ao vocabulário pessoal");
      add->signal_clicked().connect([this, &widget, word = issue->excerpt] {
        try {
          service_.add_to_personal_dictionary(word);
          refresh_all();
          if (popover_)
            popover_->popdown();
        } catch (const std::exception &problem) {
          const auto message = std::string(problem.what());
          Glib::signal_idle().connect_once(sigc::track_object(
              [this, &widget, message] {
                const project::TextIssue failure{
                    project::TextIssueKind::Grammar,
                    0,
                    0,
                    {},
                    message,
                    {},
                    std::nullopt};
                show_issue_popover(widget, &failure);
              },
              widget));
        }
      });
      box->append(*add);
    }
  }
  popover_->set_child(*box);
  popover_->popup();
}

void ProofreadingController::close_popover() {
  if (!popover_)
    return;
  popover_->popdown();
  if (popover_->get_parent())
    popover_->unparent();
  popover_.reset();
}

void ProofreadingController::replace_issue(Gtk::Widget &widget,
                                           const project::TextIssue &issue,
                                           const std::string &replacement) {
  if (auto *entry = dynamic_cast<Gtk::Entry *>(&widget)) {
    auto text = entry->get_text();
    text.replace(issue.start, issue.length, replacement);
    entry->set_text(text);
    entry->set_position(
        static_cast<int>(issue.start + Glib::ustring(replacement).size()));
  } else if (auto *view = dynamic_cast<Gtk::TextView *>(&widget)) {
    const auto buffer = view->get_buffer();
    auto begin = buffer->get_iter_at_offset(static_cast<int>(issue.start));
    auto end = buffer->get_iter_at_offset(
        static_cast<int>(issue.start + issue.length));
    const auto insertion = buffer->erase(begin, end);
    buffer->insert(insertion, replacement);
  }
}

} // namespace inde::ui

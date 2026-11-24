#include "inde/application/document_alternatives.hpp"
#include "inde/application/text_clipboard.hpp"
#include "inde/project/document_paragraphs.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "object_native_ui.hpp"
#include "object_text_projection.hpp"
#include <stdexcept>
namespace inde::ui {
namespace {
bool object_specialty(const std::string &id) {
  return id == "composition" || id == "tables" || id == "formulas" ||
         id == "math";
}
} // namespace
void WritingWorkspace::build_object_specializations() {
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"composition", "Composição"},
           {"tables", "Tabelas"},
           {"formulas", "Fórmulas"},
           {"math", "Matemática"}})
    specialization_selector_.append(id, name);
  specialty_strip_.set_name("writing-specialty-strip");
  back_button_.set_name("writing-library");
  title_entry_.set_name("writing-title");
  undo_button_.set_name("writing-undo");
  redo_button_.set_name("writing-redo");
  for (const auto &[id, name] :
       std::vector<std::pair<std::string, std::string>>{
           {"text", "Texto"},
           {"semantic", "Semântica"},
           {"linguistic", "Linguística"},
           {"structure", "Estrutura"},
           {"bindings", "Vínculos"},
           {"composition", "Composição"},
           {"tables", "Tabelas"},
           {"formulas", "Fórmulas"},
           {"math", "Matemática"}}) {
    auto *b = Gtk::make_managed<Gtk::ToggleButton>(name);
    b->set_name("specialty-" + id);
    b->set_tooltip_text("Abrir a especialidade " + name + " deste Documento");
    b->signal_clicked().connect([this, id] {
      if (changing_specialization_)
        return;
      set_editor_specialization(id);
      if (object_specialty(id) && editor_specialization_ == id &&
          object_editor_)
        object_editor_->show_tools(true);
    });
    specialty_strip_.append(*b);
  }
  // Keep the old selector for saved view compatibility, with the strip as
  // access.
  specialization_selector_.set_visible(false);
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::NEVER);
  scroll->set_child(specialty_strip_);
  editor_page_.insert_child_after(*scroll, editor_toolbar_);
}
bool WritingWorkspace::objects_can_leave() const {
  return !object_editor_ || object_editor_->can_leave();
}
bool WritingWorkspace::request_objects_leave(const std::string &action,
                                            std::function<void()> resume) {
  return !object_editor_ || object_editor_->request_leave(action, std::move(resume));
}
void WritingWorkspace::show_document_objects() {
  set_editor_specialization("composition");
}
void WritingWorkspace::show_object_specialization(const std::string &id) {
  // Every specialty uses the same mixed sheet. Before a document is opened
  // there is deliberately no composition session to create or persist.
  if (!current_document_)
    return;
  if (!project::canonical_paragraphs(current_document_->paragraphs)) {
    document_surfaces_.set_visible_child("text");
    signal_status_message_.emit(
        "Documento legado preservado. Identifique seus parágrafos em Estrutura "
        "para usar a folha com objetos.");
    return;
  }
  if (!current_document_ || !owner_window())
    throw std::runtime_error("Abra um Documento na Escrita.");
  // Reconcile pending input without saving or replacing a stable sheet.
  if (!object_editor_) {
    object_editor_ = std::make_unique<ObjectDocumentEditor>(
        service_, *owner_window(), current_document_->id);
    mount_specialty_tools(true);
    object_editor_->set_specialty_panel(&specialty_tools_);
    object_editor_->paragraph_actions = [this] { return editor_actions_; };
    object_editor_->tools_changed = [this](bool open) {
      tools_visible_ = open;
      if(open) refresh_text_tools();
    };
    object_editor_->save_document = [this] { flush_changes(); };
    object_editor_->text_composing = [this] { return composing_input(); };
    object_editor_->paragraph_view = [this](const std::string &paragraph) {
      return make_paragraph_view(paragraph);
    };
    object_editor_->visible_paragraphs =
        [this](const project::Document &document)
        -> std::optional<std::map<std::string, std::string>> {
      if (active_visibility_id_.empty())
        return std::nullopt;
      std::map<std::string, std::string> visible;
      const Glib::ustring body(document.content);
      for (const auto &row : document.paragraphs.rows) {
        if (row.start == row.end) {
          try {
            static_cast<void>(require_visible_interval(row.start, row.end));
            visible[row.id] = "";
          } catch (const std::exception &) {
          }
          continue;
        }
        const auto ranges = visible_ranges(row.start, row.end);
        if (ranges.empty())
          continue;
        std::string out;
        std::size_t previous = row.start;
        for (const auto &range : ranges) {
          if (previous < range.start)
            out += "…";
          out += body.substr(range.start, range.end - range.start).raw();
          previous = range.end;
        }
        if (previous < row.end)
          out += "…";
        visible[row.id] = std::move(out);
      }
      return visible;
    };
    object_editor_->editing_changed = [this](bool editing) {
      editor_toolbar_.set_sensitive(!editing);
      // A specialty change can offer to keep or discard an unapplied draft.
      // Keeping these controls reachable avoids a silent, apparently frozen UI.
      specialty_strip_.set_sensitive(true);
      specialization_bar_.set_sensitive(true);
    };
    object_editor_->edit_paragraph = [this](const std::string &paragraph) {
      const auto row =
          std::find_if(current_document_->paragraphs.rows.begin(),
                       current_document_->paragraphs.rows.end(),
                       [&](const auto &p) { return p.id == paragraph; });
      if (row == current_document_->paragraphs.rows.end())
        return;
      const auto offset = row->start;
      object_editor_->focus_text(static_cast<int>(offset));
    };
    object_editor_->commit_document = [this](const project::Document &after) {
      committing_object_document_ = true;
      try {
        flush_changes();
        const auto result =
            apply_editorial_change(*service_.writing().document(after.id),
                                   after, "Mover na composição");
        committing_object_document_ = false;
        return result;
      } catch (...) {
        committing_object_document_ = false;
        throw;
      }
    };
    document_surfaces_.add(*object_editor_, "objects");
  }
  object_editor_->synchronize_document(capture_editor_state());
  object_editor_->set_specialty(id);
  document_surfaces_.set_visible_child("objects");
}
void WritingWorkspace::flush_document_objects() {
  if (!object_editor_ || !current_document_ || !service_.current())
    return;
  object_editor_->synchronize_document(capture_editor_state());
  object_editor_->save_for_project(service_.current()->path());
}
void WritingWorkspace::reset_document_objects() {
  common_sheet_idle_.disconnect();
  if (!object_editor_)
    return;
  document_surfaces_.set_visible_child("text");
  object_editor_->set_specialty_panel(nullptr);
  mount_specialty_tools(false);
  document_surfaces_.remove(*object_editor_);
  object_editor_.reset();
}

void WritingWorkspace::focus_common_sheet() {
  if (object_editor_ &&
      document_surfaces_.get_visible_child_name() == "objects") {
    const auto buffer = editor_.get_buffer();
    object_editor_->focus_text(
        buffer->get_iter_at_mark(buffer->get_insert()).get_offset());
  } else
    editor_.grab_focus();
}

void WritingWorkspace::schedule_common_sheet() {
  if (!object_editor_ || common_sheet_idle_.connected())
    return;
  // Adopt accepted edits and visibility before GTK lays out the next frame.
  common_sheet_idle_ = Glib::signal_idle().connect([this] {
    common_sheet_idle_.disconnect();
    if (!object_editor_ || !current_document_)
      return false;
    if (composing_input() || !objects_can_leave()) {
      // Input-method ownership stays with GTK; no viewport is replaced while
      // it contains provisional text. The next mutation/explicit save retries.
      return false;
    }
    try {
      object_editor_->synchronize_document(capture_editor_state());
    } catch (const std::exception &e) {
      signal_status_message_.emit(e.what());
    }
    return false;
  }, Glib::PRIORITY_HIGH_IDLE);
}

Gtk::Widget *WritingWorkspace::make_paragraph_view(const std::string &id) {
  const auto row = std::find_if(current_document_->paragraphs.rows.begin(),
                                current_document_->paragraphs.rows.end(),
                                [&](const auto &p) { return p.id == id; });
  if (row == current_document_->paragraphs.rows.end())
    return nullptr;
  auto *view = Gtk::make_managed<ObjectTextProjection>(
      editor_.get_buffer(), static_cast<int>(row->start),
      static_cast<int>(row->end));
  view->set_name("writing-paragraph-" + id);
  view->editable = [this] { return objects_can_leave() || composing_input(); };
  view->preedit = [this, document = current_document_->id,
                   lifetime = std::weak_ptr<int>(clipboard_lifetime_)](const char *s) {
    if (lifetime.expired() || !current_document_ || current_document_->id != document)
      return;
    on_preedit_changed(s);
    if (!s || !*s)
      schedule_common_sheet();
  };
  view->replace_selection = [this](const std::string &text) {
    try {
      const auto before = capture_edit_state();
      const auto a = std::min(before.selection.cursor, before.selection.bound);
      const auto b = std::max(before.selection.cursor, before.selection.bound);
      auto after =
          active_visibility_id_.empty()
              ? application::paste_leaf_fragment(before, a, b, {text, {}},
                                                 false, focused_resma_id_)
              : application::paste_visibility_fragment(
                    before, editor_projection_, a, b, {text, {}}, false);
      const bool changed = apply_edit_transaction(
          std::move(after), "Substituir seleção na folha");
      schedule_common_sheet();
      return changed;
    } catch (const std::exception &e) {
      signal_status_message_.emit(e.what());
      return false;
    }
  };
  view->navigate = [this](int offset, bool extend) {
    auto buffer = editor_.get_buffer();
    const auto at = buffer->get_iter_at_offset(
        std::clamp(offset, 0, buffer->get_char_count()));
    if (extend)
      buffer->select_range(
          at, buffer->get_iter_at_mark(buffer->get_selection_bound()));
    else
      buffer->place_cursor(at);
    schedule_common_sheet();
    Glib::signal_idle().connect_once(sigc::track_obj(
        [this, offset] {
          if (object_editor_)
            object_editor_->focus_text(offset);
        },
        *this));
  };
  view->shortcut = [this](guint key, Gdk::ModifierType mods) {
    if (ime_active_)
      return false;
    if ((mods & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType{} ||
        (mods & (Gdk::ModifierType::ALT_MASK |
                 Gdk::ModifierType::SUPER_MASK)) != Gdk::ModifierType{})
      return false;
    const bool shift =
        (mods & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType{};
    if (key == GDK_KEY_z || key == GDK_KEY_Z) {
      if (shift)
        redo();
      else
        undo();
    } else if (key == GDK_KEY_y || key == GDK_KEY_Y)
      redo();
    else if (key == GDK_KEY_c || key == GDK_KEY_C)
      copy_selection(shift);
    else if (key == GDK_KEY_x || key == GDK_KEY_X)
      copy_selection(false, true);
    else if (key == GDK_KEY_v || key == GDK_KEY_V)
      paste_clipboard(shift);
    else if (key == GDK_KEY_a || key == GDK_KEY_A) {
      auto buffer = editor_.get_buffer();
      buffer->select_range(buffer->end(), buffer->begin());
    } else if (key == GDK_KEY_b || key == GDK_KEY_B)
      apply_text_style(project::DocumentTextStyle::Bold);
    else if (key == GDK_KEY_i || key == GDK_KEY_I)
      apply_text_style(project::DocumentTextStyle::Italic);
    else if (key == GDK_KEY_u || key == GDK_KEY_U)
      apply_text_style(project::DocumentTextStyle::Underline);
    else if (key == GDK_KEY_f || key == GDK_KEY_F)
      open_find(false);
    else if (key == GDK_KEY_h || key == GDK_KEY_H)
      open_find(true);
    else
      return false;
    schedule_common_sheet();
    return true;
  };
  return view;
}
} // namespace inde::ui

#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/accessibility.hpp"

#include <exception>
#include <utility>

namespace inde::ui {

IncrementalSelector::IncrementalSelector(std::string placeholder,
                                         Provider provider,
                                         std::string empty_message)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 4), provider_(std::move(provider)) {
  set_accessible_label(search_, placeholder);
  set_accessible_description(
      search_, "Digite para filtrar as opções e escolha um resultado abaixo.");
  search_.set_placeholder_text(std::move(placeholder));
  selected_.set_xalign(0.0F);
  selected_.set_wrap(true);
  selected_.add_css_class("dim-label");
  results_.add_css_class("boxed-list");
  results_scroll_.set_policy(Gtk::PolicyType::NEVER,
                             Gtk::PolicyType::AUTOMATIC);
  results_scroll_.set_min_content_height(84);
  results_scroll_.set_max_content_height(132);
  results_scroll_.set_propagate_natural_height(false);
  results_scroll_.set_child(results_);
  append(search_);
  append(selected_);
  append(results_scroll_);
  search_.signal_search_changed().connect(
      sigc::mem_fun(*this, &IncrementalSelector::refresh_results));
  selected_.set_text(std::move(empty_message));
  refresh_results();
}

std::optional<std::string> IncrementalSelector::selected_id() const {
  return selected_id_;
}

void IncrementalSelector::set_selected(std::string id, std::string label) {
  selected_id_ = std::move(id);
  selected_.set_text("Selecionado: " + std::move(label));
  signal_selection_changed_.emit();
}

void IncrementalSelector::clear_selection() {
  selected_id_.reset();
  selected_.set_text("Nenhuma escolha");
  signal_selection_changed_.emit();
}

void IncrementalSelector::refresh() { refresh_results(); }

void IncrementalSelector::refresh_results() {
  while (auto *child = results_.get_first_child())
    results_.remove(*child);
  std::vector<IncrementalSelection> values;
  try {
    values = provider_(search_.get_text(), result_limit_);
  } catch (const std::exception &error) {
    auto *failure =
        Gtk::make_managed<Gtk::Label>("Não foi possível carregar as opções");
    failure->set_xalign(0.0F);
    failure->set_wrap(true);
    failure->set_tooltip_text(error.what());
    failure->add_css_class("dim-label");
    results_.append(*failure);
    return;
  }
  if (values.empty()) {
    auto *empty = Gtk::make_managed<Gtk::Label>("Nenhum resultado encontrado");
    empty->set_xalign(0.0F);
    empty->add_css_class("dim-label");
    results_.append(*empty);
    return;
  }
  for (const auto &value : values) {
    auto *button = Gtk::make_managed<Gtk::Button>();
    auto *content = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 1);
    auto *title = Gtk::make_managed<Gtk::Label>(value.label);
    title->set_xalign(0.0F);
    title->set_ellipsize(Pango::EllipsizeMode::END);
    content->append(*title);
    if (!value.detail.empty()) {
      auto *detail = Gtk::make_managed<Gtk::Label>(value.detail);
      detail->set_xalign(0.0F);
      detail->set_wrap(true);
      detail->add_css_class("dim-label");
      content->append(*detail);
    }
    button->set_child(*content);
    button->set_halign(Gtk::Align::FILL);
    button->set_hexpand(true);
    button->signal_clicked().connect(
        [this, selection = value] { choose(selection); });
    results_.append(*button);
  }
}

void IncrementalSelector::choose(const IncrementalSelection &selection) {
  set_selected(selection.id, selection.label);
}

} // namespace inde::ui

#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::refresh_entities() {
  if (!service_.current())
    return;
  auto context = context_from_controls();
  explorer_snapshot_ = service_.planning().explore_entities(context);
  service_.set_planning_context(explorer_snapshot_.context);
  if (offset_ > 0 && offset_ >= explorer_snapshot_.matching_count) {
    offset_ = explorer_snapshot_.matching_count == 0
                  ? 0
                  : ((explorer_snapshot_.matching_count - 1) / page_size_) *
                        page_size_;
    context.offset = offset_;
    explorer_snapshot_ = service_.planning().explore_entities(context);
    service_.set_planning_context(explorer_snapshot_.context);
  }
  refresh_facets();
  refresh_context_summary();
  page_.clear();
  page_.reserve(explorer_snapshot_.items.size());
  while (auto *child = entity_list_.get_first_child())
    entity_list_.remove(*child);
  for (const auto &item : explorer_snapshot_.items) {
    page_.push_back(item.entity);
    const auto &entity = page_.back();
    auto *button = Gtk::make_managed<Gtk::Button>();
    auto *labels = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    labels->set_margin(12);
    auto *title = Gtk::make_managed<Gtk::Label>(
        entity.name + " — " + type_name(entity.entity_type_id));
    auto *reason = Gtk::make_managed<Gtk::Label>(item.primary_reason);
    title->set_xalign(0.0F);
    title->set_ellipsize(Pango::EllipsizeMode::END);
    title->set_max_width_chars(34);
    title->set_size_request(278, -1);
    reason->set_xalign(0.0F);
    reason->set_wrap(true);
    reason->set_lines(3);
    reason->set_ellipsize(Pango::EllipsizeMode::END);
    // Razões temporais podem citar eixo e os dois limites do período. Limitar
    // a largura de requisição evita que uma só frase force a FlowBox para uma
    // coluna gigantesca quando há poucos resultados.
    reason->set_max_width_chars(38);
    reason->set_width_chars(34);
    reason->set_size_request(278, -1);
    reason->add_css_class("dim-label");
    labels->append(*title);
    labels->append(*reason);
    button->set_child(*labels);
    button->set_tooltip_text(entity.name + "\n" + item.inclusion_reason);
    button->set_halign(Gtk::Align::FILL);
    button->set_valign(Gtk::Align::START);
    button->set_hexpand(true);
    button->set_vexpand(false);
    button->set_size_request(306, 116);
    button->add_css_class("card");
    button->add_css_class("entity-card");
    if (selected_entity_id_ == entity.id)
      button->add_css_class("suggested-action");
    button->signal_clicked().connect(
        [this, id = entity.id] { select_entity(id); });
    attach_context_menu(
        *button,
        {{"Abrir elemento da história", [this, id = entity.id] { select_entity(id); },
          "go-next-symbolic"},
         {"Editar",
          [this, id = entity.id] {
            selected_entity_id_ = id;
            edit_selected_entity();
          },
          "document-edit-symbolic"},
         {"Remover",
          [this, id = entity.id] {
            selected_entity_id_ = id;
            delete_selected_entity();
          },
          "user-trash-symbolic", true, true, true}});
    const auto card_index = static_cast<int>(page_.size() - 1);
    entity_list_.append(*button);
    if (auto *card = entity_list_.get_child_at_index(card_index)) {
      card->set_halign(Gtk::Align::FILL);
      card->set_valign(Gtk::Align::START);
      card->set_size_request(306, 116);
      card->set_hexpand(true);
    }
  }
  const bool no_results = page_.empty();
  explorer_empty_title_.set_visible(no_results);
  explorer_empty_message_.set_visible(no_results);
  entity_scroll_.set_visible(!no_results);
  if (selected_entity_id_ &&
      std::none_of(page_.begin(), page_.end(), [&](const auto &entity) {
        return entity.id == *selected_entity_id_;
      }))
    selected_entity_id_.reset();
  previous_button_.set_sensitive(offset_ > 0);
  next_button_.set_sensitive(explorer_snapshot_.has_next);
  const auto pages =
      explorer_snapshot_.matching_count == 0
          ? std::size_t{1}
          : (explorer_snapshot_.matching_count + page_size_ - 1) / page_size_;
  page_label_.set_text(std::to_string(explorer_snapshot_.matching_count) +
                       (explorer_snapshot_.matching_count == 1
                            ? " resultado • página "
                            : " resultados • página ") +
                       std::to_string(offset_ / page_size_ + 1) + " de " +
                       std::to_string(pages));
  if (explorer_snapshot_.context.related_entity_id) {
    const auto related = service_.narrative().entity(
        *explorer_snapshot_.context.related_entity_id);
    counterpart_filter_label_.set_text(
        "Outro elemento da relação: " +
        (related ? related->name : std::string("elemento da história indisponível")));
    context_expander_.set_expanded(true);
  } else {
    counterpart_filter_label_.set_text("Outro elemento da relação: qualquer elemento da história");
  }
  refresh_details();
  explorer_loaded_ = true;
}

void PlanningWorkspace::previous_page() {
  offset_ = offset_ >= page_size_ ? offset_ - page_size_ : 0;
  selected_entity_id_.reset();
  refresh_entities();
}

void PlanningWorkspace::next_page() {
  if (!explorer_snapshot_.has_next)
    return;
  offset_ += page_size_;
  selected_entity_id_.reset();
  refresh_entities();
}

} // namespace inde::ui

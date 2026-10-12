#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::refresh_relations() {
  const auto *current = selected_entity();
  const bool has_entity = current != nullptr;
  relations_section_.set_visible(has_entity);
  for (auto *widget :
       {static_cast<Gtk::Widget *>(&relation_detail_hint_),
        static_cast<Gtk::Widget *>(&context_relation_add_button_),
        static_cast<Gtk::Widget *>(&relation_navigation_toolbar_),
        static_cast<Gtk::Widget *>(&relation_list_)})
    widget->set_visible(has_entity);
  relations_.clear();
  selected_relation_id_.reset();
  while (auto *child = relation_list_.get_first_child())
    relation_list_.remove(*child);
  context_relation_add_button_.set_sensitive(false);
  relation_edit_button_.set_sensitive(false);
  relation_remove_button_.set_sensitive(false);
  relation_actions_button_.set_sensitive(false);
  relation_open_counterpart_button_.set_sensitive(false);
  relation_filter_counterpart_button_.set_sensitive(false);
  if (!current)
    return;
  if (relation_types_.empty())
    refresh_relation_type_controls();
  context_relation_add_button_.set_sensitive(!relation_types_.empty());

  persistence::RelationQuery query;
  query.entity_id = current->id;
  query.limit = 200;
  relations_ = service_.narrative().relations(query);
  for (const auto &relation : relations_) {
    const auto type = std::find_if(
        relation_types_.begin(), relation_types_.end(), [&](const auto &value) {
          return value.id == relation.relation_type_id;
        });
    const bool outgoing = relation.source_entity_id == current->id;
    const auto other_id =
        outgoing ? relation.target_entity_id : relation.source_entity_id;
    const auto other = service_.narrative().entity(other_id);
    std::string verb = "Relação";
    if (type != relation_types_.end()) {
      verb = outgoing || type->directionality ==
                             project::RelationDirectionality::Symmetric
                 ? type->name
                 : type->inverse_name;
    }
    auto *button = Gtk::make_managed<Gtk::Button>(
        verb + " — " + (other ? other->name : "Elemento da história ausente"));
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = relation.id] { select_relation(id); });
    attach_context_menu(*button, {{"Abrir outro elemento da relação",
                                   [this, id = relation.id] {
                                     select_relation(id);
                                     open_relation_counterpart();
                                   },
                                   "go-next-symbolic"},
                                  {"Abrir em Explorar relações",
                                   [this, id = relation.id] {
                                     select_relation(id);
                                     show_relations_page();
                                   },
                                   "go-jump-symbolic"},
                                  {"Filtrar por outro elemento da relação",
                                   [this, id = relation.id] {
                                     select_relation(id);
                                     filter_by_relation_counterpart();
                                   },
                                   "edit-find-symbolic"}});
    relation_list_.append(*button);
  }
}

void PlanningWorkspace::select_relation(std::string id) {
  selected_relation_id_ = std::move(id);
  const bool manager = page_stack_.get_visible_child_name() == "relations";
  relation_edit_button_.set_sensitive(manager);
  relation_remove_button_.set_sensitive(manager);
  relation_actions_button_.set_sensitive(manager);
  const bool has_counterpart = selected_relation_counterpart_id().has_value();
  relation_open_counterpart_button_.set_sensitive(has_counterpart);
  relation_filter_counterpart_button_.set_sensitive(has_counterpart);
}

std::optional<std::string>
PlanningWorkspace::selected_relation_counterpart_id() const {
  const auto *current = selected_entity();
  if (!current || !selected_relation_id_)
    return std::nullopt;
  const auto found = std::find_if(
      relations_.begin(), relations_.end(),
      [&](const auto &value) { return value.id == *selected_relation_id_; });
  if (found == relations_.end())
    return std::nullopt;
  if (found->source_entity_id == current->id)
    return found->target_entity_id;
  if (found->target_entity_id == current->id)
    return found->source_entity_id;
  return std::nullopt;
}

void PlanningWorkspace::open_relation_counterpart() {
  const auto id = selected_relation_counterpart_id();
  if (id)
    reveal_entity(*id);
}

void PlanningWorkspace::filter_by_relation_counterpart() {
  const auto id = selected_relation_counterpart_id();
  if (!id)
    return;
  remember_navigation();
  related_entity_filter_id_ = *id;
  offset_ = 0;
  refresh_entities();
  show_explorer_page();
  signal_status_message_.emit("Outro elemento da relação aplicada ao contexto do explorador");
}

} // namespace inde::ui

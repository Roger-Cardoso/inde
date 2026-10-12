#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/temporal_context.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>

namespace inde::ui {

void PlanningWorkspace::refresh_relation_filter_options() {
  const std::string selected = relation_type_filter_.get_active_id().raw();
  relation_type_filter_.remove_all();
  relation_type_filter_.append("all", "Todos os tipos de relação");
  for (const auto &type : relation_types_)
    relation_type_filter_.append(type.id, type.name);
  if (selected.empty() || !relation_type_filter_.set_active_id(selected))
    relation_type_filter_.set_active_id("all");
  relation_entity_filter_->refresh();
  relation_time_point_filter_->refresh();
  relation_location_filter_->refresh();
  relation_cause_filter_->refresh();
}

persistence::RelationQuery
PlanningWorkspace::relation_query_from_controls() const {
  persistence::RelationQuery query;
  query.search = relation_search_.get_text();
  const auto type = relation_type_filter_.get_active_id().raw();
  if (!type.empty() && type != "all")
    query.relation_type_id = type;
  query.entity_id = relation_entity_filter_->selected_id();
  query.fictional_time_point_id = relation_time_point_filter_->selected_id();
  query.location_entity_id = relation_location_filter_->selected_id();
  query.cause_entity_id = relation_cause_filter_->selected_id();
  query.limit = 300;
  return query;
}

std::string
PlanningWorkspace::relation_type_label(const std::string &id) const {
  const auto found =
      std::find_if(relation_types_.begin(), relation_types_.end(),
                   [&](const auto &type) { return type.id == id; });
  return found == relation_types_.end() ? "Relação" : found->name;
}

std::string
PlanningWorkspace::relation_entity_label(const std::string &id) const {
  const auto cached = relation_entity_labels_.find(id);
  if (cached != relation_entity_labels_.end())
    return cached->second;
  const auto entity = service_.narrative().entity(id);
  return entity ? entity->name : "Elemento da história ausente";
}

std::string PlanningWorkspace::relation_context_label(
    const project::NarrativeRelation &relation) const {
  std::vector<std::string> parts;
  if (relation.fictional_time_point_id) {
    const auto cached =
        relation_time_point_labels_.find(*relation.fictional_time_point_id);
    if (cached != relation_time_point_labels_.end()) {
      parts.push_back("Quando: " + cached->second);
    } else {
      const auto point =
          service_.planning().time_point(*relation.fictional_time_point_id);
      parts.push_back("Quando: " +
                      (point ? point->label : std::string("Ponto ausente")));
    }
  }
  if (relation.location_entity_id)
    parts.push_back("Onde: " +
                    relation_entity_label(*relation.location_entity_id));
  if (relation.cause_entity_id)
    parts.push_back("Por causa de: " +
                    relation_entity_label(*relation.cause_entity_id));
  if (!relation.description.empty())
    parts.push_back(relation.description);
  if (parts.empty())
    return "Sem qualificadores de contexto";
  std::string result;
  for (const auto &part : parts) {
    if (!result.empty())
      result += "  •  ";
    result += part;
  }
  return result;
}

void PlanningWorkspace::refresh_relation_explorer() {
  if (!service_.current())
    return;
  relation_page_results_ =
      service_.narrative().relations(relation_query_from_controls());

  // A grade usa os mesmos rotulos repetidas vezes. Precarrega entidades em
  // lotes e cada ponto temporal uma unica vez para impedir uma consulta SQL
  // por campo de cada card (o antigo N+1 do projeto integral).
  relation_entity_labels_.clear();
  persistence::EntityQuery entity_query;
  constexpr std::size_t entity_batch_size = 500;
  entity_query.limit = entity_batch_size;
  for (;;) {
    const auto entities = service_.narrative().entities(entity_query);
    for (const auto &entity : entities)
      relation_entity_labels_.insert_or_assign(entity.id, entity.name);
    if (entities.size() < entity_batch_size)
      break;
    entity_query.offset += entities.size();
  }
  relation_time_point_labels_.clear();
  for (const auto &relation : relation_page_results_) {
    if (!relation.fictional_time_point_id ||
        relation_time_point_labels_.contains(*relation.fictional_time_point_id))
      continue;
    const auto point =
        service_.planning().time_point(*relation.fictional_time_point_id);
    relation_time_point_labels_.emplace(*relation.fictional_time_point_id,
                                        point ? point->label
                                              : std::string("Ponto ausente"));
  }
  const bool selected_is_visible =
      selected_relation_id_ &&
      std::any_of(relation_page_results_.begin(), relation_page_results_.end(),
                  [this](const auto &relation) {
                    return relation.id == *selected_relation_id_;
                  });
  if (!selected_is_visible) {
    selected_relation_id_.reset();
    relation_edit_button_.set_sensitive(false);
    relation_remove_button_.set_sensitive(false);
    relation_actions_button_.set_sensitive(false);
  }
  while (auto *child = relation_card_list_.get_first_child())
    relation_card_list_.remove(*child);
  relation_empty_message_.set_text(
      relation_search_.get_text().empty()
          ? "Crie uma relação ou ajuste os filtros para começar a explorá-las."
          : "Nenhuma relação corresponde à pesquisa e aos filtros atuais.");
  if (relation_page_results_.empty()) {
    relation_results_stack_.set_visible_child("empty");
  } else {
    relation_results_stack_.set_visible_child("results");
    for (const auto &relation : relation_page_results_) {
      const auto type =
          std::find_if(relation_types_.begin(), relation_types_.end(),
                       [&](const auto &item) {
                         return item.id == relation.relation_type_id;
                       });
      const bool symmetric =
          type != relation_types_.end() &&
          type->directionality == project::RelationDirectionality::Symmetric;
      auto *panel = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 7);
      panel->set_margin(14);
      auto *title = Gtk::make_managed<Gtk::Label>(
          relation_entity_label(relation.source_entity_id) +
          (symmetric ? " ← " : " → ") +
          relation_type_label(relation.relation_type_id) +
          (symmetric ? " → " : " → ") +
          relation_entity_label(relation.target_entity_id));
      title->set_wrap(true);
      title->set_xalign(0.0F);
      title->add_css_class("title-3");
      auto *context =
          Gtk::make_managed<Gtk::Label>(relation_context_label(relation));
      context->set_wrap(true);
      context->set_xalign(0.0F);
      context->add_css_class("dim-label");
      panel->append(*title);
      panel->append(*context);
      auto *button = Gtk::make_managed<Gtk::Button>();
      button->set_child(*panel);
      button->set_size_request(320, 140);
      button->set_halign(Gtk::Align::FILL);
      button->add_css_class("catalog-card");
      button->signal_clicked().connect(
          [this, id = relation.id] { select_relation(id); });
      attach_context_menu(
          *button,
          {{"Abrir origem",
            [this, id = relation.source_entity_id] { reveal_entity(id); },
            "go-previous-symbolic"},
           {"Abrir destino",
            [this, id = relation.target_entity_id] { reveal_entity(id); },
            "go-next-symbolic"},
           {"Editar relação",
            [this, id = relation.id] {
              select_relation(id);
              edit_relation();
            },
            "document-edit-symbolic"},
           {"Datas e horários…", [this, id = relation.id] {
              if (owner_window()) show_temporal_context(*owner_window(), service_, "relation", id);
            }, "x-office-calendar-symbolic"},
           {"Excluir relação",
            [this, id = relation.id] {
              select_relation(id);
              delete_relation();
            },
            "user-trash-symbolic", true, true, true}});
      relation_card_list_.append(*button);
    }
  }
  const auto count = relation_page_results_.size();
  relation_context_summary_.set_text(
      std::to_string(count) +
      (count == 1 ? " relação encontrada" : " relações encontradas"));
}

void PlanningWorkspace::toggle_relation_filters() {
  relation_filters_visible_ = !relation_filters_visible_;
  set_overlay_revealer_open(relation_filters_revealer_,
                            relation_filters_visible_);
  relation_filters_button_.set_label(
      relation_filters_visible_ ? "Ocultar filtros" : "Filtros");
  if (relation_filters_visible_)
    relation_search_.grab_focus();
}

void PlanningWorkspace::clear_relation_filters() {
  relation_search_.set_text("");
  relation_type_filter_.set_active_id("all");
  relation_entity_filter_->clear_selection();
  relation_time_point_filter_->clear_selection();
  relation_location_filter_->clear_selection();
  relation_cause_filter_->clear_selection();
  refresh_relation_explorer();
}

} // namespace inde::ui

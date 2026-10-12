#include "inde/ui/planning_workspace.hpp"

#include <algorithm>

namespace inde::ui {

void PlanningWorkspace::refresh_context_filters() {
  const auto &context = service_.planning_context();
  refreshing_context_ = true;
  refreshing_temporal_filter_ = true;
  work_filter_.remove_all();
  work_filter_.append("all", "Todo o Projeto");
  for (const auto &work : service_.catalog().works)
    work_filter_.append(work.id, work.title);
  if (!context.work_id || !work_filter_.set_active_id(*context.work_id))
    work_filter_.set_active_id("all");

  temporal_axis_filter_.remove_all();
  temporal_axis_filter_.append("all", "Todas as linhas do tempo");
  for (const auto &axis : axes_)
    temporal_axis_filter_.append(
        axis.id, axis.name + (axis.is_default ? " — principal" : ""));
  if (!context.fictional_axis_id ||
      !temporal_axis_filter_.set_active_id(*context.fictional_axis_id))
    temporal_axis_filter_.set_active_id("all");
  if (context.fictional_time_point_id)
    temporal_window_filter_.set_active_id("point");
  else if (context.fictional_window_start_time_point_id)
    temporal_window_filter_.set_active_id("range");
  else
    temporal_window_filter_.set_active_id("all");
  refreshing_temporal_filter_ = false;
  refresh_context_time_points();
  const auto set_point_selection =
      [this](IncrementalSelector *selector,
             const std::optional<std::string> &id) {
        if (!id)
          return;
        const auto point = service_.planning().time_point(*id);
        selector->set_selected(*id, point ? std::to_string(point->ordinal) +
                                                " — " + point->label
                                          : "Ponto indisponível");
      };
  if (context.fictional_time_point_id)
    set_point_selection(temporal_point_filter_,
                        context.fictional_time_point_id);
  else if (context.fictional_window_start_time_point_id) {
    set_point_selection(temporal_point_filter_,
                        context.fictional_window_start_time_point_id);
    set_point_selection(temporal_window_end_filter_,
                        context.fictional_window_end_time_point_id);
  }
  related_entity_filter_id_ = context.related_entity_id;
  refresh_context_editorial_nodes();
  if (context.editorial_node_id) {
    const auto nodes =
        service_.structural_nodes_for_work(work_filter_.get_active_id().raw());
    const auto node =
        std::find_if(nodes.begin(), nodes.end(), [&](const auto &value) {
          return value.id == *context.editorial_node_id;
        });
    editorial_node_filter_->set_selected(
        *context.editorial_node_id,
        node == nodes.end() ? "Elemento do manuscrito indisponível" : node->title);
  } else
    editorial_node_filter_->clear_selection();
  if (context.document_id) {
    writing_usage_filter_.set_active_id("document");
    writing_document_filter_->set_sensitive(true);
    const auto document = service_.writing().document(*context.document_id);
    writing_document_filter_->set_selected(*context.document_id,
                                           document ? document->title
                                                    : "Documento indisponível");
  } else if (context.require_document_reference) {
    writing_usage_filter_.set_active_id("referenced");
    writing_document_filter_->set_sensitive(false);
    writing_document_filter_->clear_selection();
  } else {
    writing_usage_filter_.set_active_id("all");
    writing_document_filter_->set_sensitive(false);
    writing_document_filter_->clear_selection();
  }
  writing_document_filter_->refresh();
  refreshing_context_ = false;
}

void PlanningWorkspace::refresh_context_time_points() {
  refreshing_temporal_filter_ = true;
  const std::string axis_id = temporal_axis_filter_.get_active_id().raw();
  if (!axis_id.empty() && axis_id != "all" && service_.current()) {
    temporal_point_filter_->set_sensitive(true);
    temporal_window_end_filter_->set_sensitive(
        temporal_window_filter_.get_active_id() == "range");
  } else {
    temporal_point_filter_->set_sensitive(false);
    temporal_window_end_filter_->set_sensitive(false);
  }
  temporal_point_filter_->set_visible(temporal_window_filter_.get_active_id() !=
                                      "all");
  temporal_window_end_filter_->set_visible(
      temporal_window_filter_.get_active_id() == "range");
  temporal_point_filter_->clear_selection();
  temporal_window_end_filter_->clear_selection();
  temporal_point_filter_->refresh();
  temporal_window_end_filter_->refresh();
  refreshing_temporal_filter_ = false;
}

void PlanningWorkspace::refresh_context_editorial_nodes() {
  const bool was_refreshing = refreshing_context_;
  refreshing_context_ = true;
  const std::string work_id = work_filter_.get_active_id().raw();
  const bool can_select =
      service_.current() && !work_id.empty() && work_id != "all";
  editorial_node_filter_->set_sensitive(can_select);
  if (!can_select)
    editorial_node_filter_->clear_selection();
  editorial_node_filter_->refresh();
  refreshing_context_ = was_refreshing;
}

application::PlanningContext PlanningWorkspace::context_from_controls() const {
  application::PlanningContext context;
  context.search = search_.get_text();
  context.limit = page_size_;
  context.offset = offset_;
  context.entity_type_ids = selected_type_ids_;
  context.relation_type_ids = selected_relation_type_ids_;
  context.related_entity_id = related_entity_filter_id_;
  const std::string work_id = work_filter_.get_active_id().raw();
  if (!work_id.empty() && work_id != "all")
    context.work_id = work_id;
  const std::string axis_id = temporal_axis_filter_.get_active_id().raw();
  if (!axis_id.empty() && axis_id != "all")
    context.fictional_axis_id = axis_id;
  const std::string window = temporal_window_filter_.get_active_id().raw();
  if (window == "point") {
    context.fictional_time_point_id = temporal_point_filter_->selected_id();
  } else if (window == "range") {
    const auto start = temporal_point_filter_->selected_id();
    const auto end = temporal_window_end_filter_->selected_id();
    // A seleção do intervalo é necessariamente atômica para o serviço: ao
    // escolher apenas o início, o usuário ainda está compondo o filtro. Não
    // projete um período parcial, pois PlanningService o rejeita por contrato.
    if (start && end) {
      context.fictional_window_start_time_point_id = start;
      context.fictional_window_end_time_point_id = end;
    }
  }
  context.editorial_node_id = editorial_node_filter_->selected_id();
  const auto writing_usage = writing_usage_filter_.get_active_id().raw();
  context.require_document_reference =
      writing_usage == "referenced" || writing_usage == "document";
  if (writing_usage == "document")
    context.document_id = writing_document_filter_->selected_id();
  return context;
}

void PlanningWorkspace::refresh_facets() {
  if (refreshing_facets_)
    return;
  refreshing_facets_ = true;
  const auto rebuild =
      [this](Gtk::Box &box,
             const std::vector<application::PlanningFacet> &facets,
             std::vector<std::string> &selected, bool types) {
        while (auto *child = box.get_first_child())
          box.remove(*child);
        for (const auto &facet : facets) {
          // Facetas sem resultado continuam visíveis quando já escolhidas, para
          // que o autor sempre possa desfazer uma combinação que esvaziou o
          // contexto.
          if (facet.count == 0 && !facet.selected)
            continue;
          auto *check = Gtk::make_managed<Gtk::CheckButton>();
          auto *label = Gtk::make_managed<Gtk::Label>(
              facet.label + " (" + std::to_string(facet.count) + ")");
          label->set_xalign(0.0F);
          label->set_ellipsize(Pango::EllipsizeMode::END);
          label->set_max_width_chars(28);
          label->set_size_request(250, -1);
          check->set_child(*label);
          check->set_active(facet.selected);
          check->set_tooltip_text(
              facet.label + " — " +
              (types ? "incluir este tipo (OU entre tipos)"
                     : "incluir esta relação (OU entre relações)"));
          check->signal_toggled().connect([this, &selected, id = facet.id,
                                           check] {
            const auto found = std::find(selected.begin(), selected.end(), id);
            if (check->get_active() && found == selected.end())
              selected.push_back(id);
            else if (!check->get_active() && found != selected.end())
              selected.erase(found);
            if (&selected == &selected_type_ids_) {
              task_mode_filter_.set_active_id("explore");
            }
            offset_ = 0;
            refresh_entities();
          });
          box.append(*check);
        }
      };
  rebuild(type_facets_, explorer_snapshot_.entity_type_facets,
          selected_type_ids_, true);
  rebuild(relation_facets_, explorer_snapshot_.relation_type_facets,
          selected_relation_type_ids_, false);
  refreshing_facets_ = false;
}

} // namespace inde::ui

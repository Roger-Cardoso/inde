#include "inde/ui/planning_workspace.hpp"

namespace inde::ui {

void PlanningWorkspace::clear_planning_context() {
  remember_navigation();
  refreshing_context_ = true;
  refreshing_facets_ = true;
  refreshing_temporal_filter_ = true;
  search_.set_text("");
  selected_type_ids_.clear();
  selected_relation_type_ids_.clear();
  task_mode_filter_.set_active_id("explore");
  refreshing_facets_ = false;
  work_filter_.set_active_id("all");
  temporal_axis_filter_.set_active_id("all");
  temporal_window_filter_.set_active_id("all");
  temporal_point_filter_->clear_selection();
  temporal_window_end_filter_->clear_selection();
  temporal_point_filter_->set_sensitive(false);
  temporal_window_end_filter_->set_sensitive(false);
  temporal_window_end_filter_->set_visible(false);
  temporal_point_filter_->refresh();
  temporal_window_end_filter_->refresh();
  editorial_node_filter_->clear_selection();
  editorial_node_filter_->set_sensitive(false);
  editorial_node_filter_->refresh();
  writing_usage_filter_.set_active_id("all");
  writing_document_filter_->clear_selection();
  writing_document_filter_->set_sensitive(false);
  writing_document_filter_->refresh();
  related_entity_filter_id_.reset();
  refreshing_temporal_filter_ = false;
  refreshing_context_ = false;
  offset_ = 0;
  service_.clear_planning_context();
  refresh_entities();
  show_explorer_page();
  signal_status_message_.emit("Contexto do Planejamento limpo");
}

} // namespace inde::ui

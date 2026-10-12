#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/temporal_context.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::build_detail_page() {
  detail_page_.set_vexpand(true);
  detail_page_.set_hexpand(true);
  back_to_explorer_button_.set_halign(Gtk::Align::START);
  detail_context_.set_hexpand(true);
  detail_context_.set_halign(Gtk::Align::END);
  detail_context_.add_css_class("dim-label");
  detail_header_.append(back_to_explorer_button_);
  detail_header_.append(entity_actions_button_);
  detail_header_.append(cartography_button_);
  cartography_button_.signal_clicked().connect([this] {
    const auto entity = selected_entity();
    if (entity)
      signal_cartography_requested_.emit(entity->id);
  });
  detail_header_.append(detail_context_);
  detail_header_.add_css_class("detail-toolbar");
  detail_page_.append(detail_header_);
  content_scroll_.set_child(content_panel_);
  content_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC,
                             Gtk::PolicyType::AUTOMATIC);
  content_scroll_.set_vexpand(true);
  detail_page_.append(content_scroll_);
  content_panel_.set_hexpand(true);
  content_panel_.set_margin(12);
  content_panel_.add_css_class("detail-content");
  empty_title_.add_css_class("title-1");
  empty_title_.set_halign(Gtk::Align::START);
  empty_message_.set_wrap(true);
  empty_message_.set_xalign(0.0F);
  entity_name_.add_css_class("title-1");
  entity_name_.set_halign(Gtk::Align::START);
  entity_type_.add_css_class("dim-label");
  entity_type_.set_halign(Gtk::Align::START);
  entity_summary_.set_wrap(true);
  entity_summary_.set_xalign(0.0F);
  entity_summary_.set_halign(Gtk::Align::FILL);
  content_panel_.append(empty_title_);
  content_panel_.append(empty_message_);
  entity_summary_panel_.set_margin(14);
  entity_summary_panel_.append(entity_type_);
  entity_summary_panel_.append(entity_name_);
  entity_summary_panel_.append(entity_summary_);
  entity_alias_summary_.set_xalign(0);
  entity_alias_summary_.set_wrap(true);
  entity_alias_summary_.set_lines(3);
  entity_alias_summary_.set_ellipsize(Pango::EllipsizeMode::END);
  entity_alias_summary_.set_max_width_chars(65);
  entity_alias_summary_.add_css_class("dim-label");
  entity_summary_panel_.append(entity_alias_summary_);
  entity_dates_button_.set_halign(Gtk::Align::START);
  entity_summary_panel_.append(entity_dates_button_);
  entity_dates_button_.signal_clicked().connect([this] {
    if (auto *e = selected_entity(); e && owner_window())
      show_temporal_context(*owner_window(), service_, "entity", e->id,
                            sigc::mem_fun(*this,&PlanningWorkspace::refresh_details));
  });
  entity_summary_surface_.set_child(entity_summary_panel_);
  entity_summary_surface_.add_css_class("content-card");
  entity_summary_surface_.add_css_class("accent-card");
  content_panel_.append(entity_summary_surface_);
  work_scope_title_.add_css_class("title-2");
  work_scope_title_.set_halign(Gtk::Align::START);
  work_scope_toolbar_.append(work_scope_add_button_);
  work_scope_toolbar_.append(work_scope_actions_button_);
  work_scope_remove_button_.add_css_class("destructive-action");
  work_scope_list_.add_css_class("boxed-list");
  work_scope_panel_.set_margin(12);
  work_scope_panel_.append(work_scope_toolbar_);
  work_scope_panel_.append(work_scope_list_);
  work_scope_section_.set_child(work_scope_panel_);
  work_scope_section_.set_expanded(true);
  work_scope_section_.add_css_class("content-card");
  content_panel_.append(work_scope_section_);
  relation_detail_hint_.set_xalign(0.0F);
  relation_detail_hint_.set_wrap(true);
  relation_detail_hint_.add_css_class("dim-label");
  relation_detail_hint_.set_text(
      "Aplique relações deste elemento da história usando os tipos já definidos. Para gerenciar relações e tipos, abra Explorar relações.");
  relation_navigation_toolbar_.append(relation_open_counterpart_button_);
  relation_navigation_toolbar_.append(relation_filter_counterpart_button_);
  relation_list_.add_css_class("boxed-list");
  relations_panel_.set_margin(12);
  relations_panel_.append(relation_detail_hint_);
  relations_panel_.append(context_relation_add_button_);
  relations_panel_.append(relation_navigation_toolbar_);
  relations_panel_.append(relation_list_);
  relations_section_.set_child(relations_panel_);
  relations_section_.set_expanded(false);
  relations_section_.add_css_class("content-card");
  content_panel_.append(relations_section_);
  event_title_.add_css_class("title-2");
  event_title_.set_halign(Gtk::Align::START);
  event_status_.set_wrap(true);
  event_status_.set_xalign(0.0F);
  event_toolbar_.append(event_edit_button_);
  event_toolbar_.append(event_actions_button_);
  event_remove_button_.add_css_class("destructive-action");
  participants_title_.add_css_class("heading");
  participants_title_.set_halign(Gtk::Align::START);
  participant_toolbar_.append(participant_add_button_);
  participant_toolbar_.append(participant_actions_button_);
  participant_remove_button_.add_css_class("destructive-action");
  participant_list_.add_css_class("boxed-list");
  event_panel_.set_margin(12);
  event_panel_.append(event_status_);
  event_panel_.append(event_toolbar_);
  event_panel_.append(participants_title_);
  event_panel_.append(participant_toolbar_);
  event_panel_.append(participant_list_);
  event_section_.set_child(event_panel_);
  event_section_.set_expanded(true);
  event_section_.add_css_class("content-card");
  content_panel_.append(event_section_);
  presence_title_.add_css_class("title-2");
  presence_title_.set_halign(Gtk::Align::START);
  presence_toolbar_.append(presence_add_button_);
  presence_toolbar_.append(presence_actions_button_);
  presence_remove_button_.add_css_class("destructive-action");
  presence_list_.add_css_class("boxed-list");
  presence_panel_.set_margin(12);
  presence_panel_.append(presence_toolbar_);
  presence_panel_.append(presence_list_);
  presence_section_.set_child(presence_panel_);
  presence_section_.set_expanded(false);
  presence_section_.add_css_class("content-card");
  content_panel_.append(presence_section_);
  editorial_reference_title_.add_css_class("title-2");
  editorial_reference_title_.set_halign(Gtk::Align::START);
  editorial_reference_toolbar_.append(editorial_reference_add_button_);
  editorial_reference_toolbar_.append(editorial_reference_actions_button_);
  editorial_reference_remove_button_.add_css_class("destructive-action");
  editorial_reference_list_.add_css_class("boxed-list");
  editorial_reference_panel_.set_margin(12);
  editorial_reference_panel_.append(editorial_reference_toolbar_);
  editorial_reference_panel_.append(editorial_reference_list_);
  editorial_reference_section_.set_child(editorial_reference_panel_);
  editorial_reference_section_.set_expanded(false);
  editorial_reference_section_.add_css_class("content-card");
  content_panel_.append(editorial_reference_section_);
  writing_reference_hint_.set_xalign(0.0F);
  writing_reference_hint_.set_wrap(true);
  writing_reference_hint_.add_css_class("dim-label");
  writing_reference_list_.add_css_class("boxed-list");
  writing_reference_panel_.set_margin(12);
  writing_reference_panel_.append(writing_reference_hint_);
  writing_reference_panel_.append(writing_library_button_);
  writing_reference_panel_.append(writing_reference_list_);
  writing_reference_section_.set_child(writing_reference_panel_);
  writing_reference_section_.set_expanded(false);
  writing_reference_section_.add_css_class("content-card");
  content_panel_.append(writing_reference_section_);
}

} // namespace inde::ui

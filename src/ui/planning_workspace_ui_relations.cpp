#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::build_relations_page() {
  relations_page_.set_vexpand(true);
  relations_page_.set_hexpand(true);
  relation_search_.set_hexpand(true);
  relations_header_.add_css_class("organization-toolbar");
  relations_header_.append(relation_search_);
  relations_header_.append(relation_add_button_);
  relations_header_.append(relation_actions_button_);
  relations_header_.append(relation_filters_button_);
  relation_types_button_.set_label("Tipos");
  relation_types_button_.set_tooltip_text("Gerenciar tipos de relação");
  relation_types_popover_.set_child(relation_type_section_);
  relation_types_button_.set_popover(relation_types_popover_);
  relations_header_.append(relation_types_button_);
  relations_page_.append(relations_header_);
  for (auto *section : {&relation_crud_section_, &relation_type_section_}) {
    section->set_margin(10);
    section->set_halign(Gtk::Align::START);
    section->set_valign(Gtk::Align::START);
    section->add_css_class("tool-section");
  }
  for (auto *title : {&relation_crud_title_, &relation_type_section_title_}) {
    title->set_halign(Gtk::Align::START);
    title->add_css_class("heading");
  }
  relation_crud_section_.append(relation_crud_title_);
  relation_crud_section_.append(relation_crud_actions_);
  relation_type_actions_.append(relation_type_combo_);
  bound_choice_width(relation_type_combo_);
  relation_type_actions_.append(relation_type_add_button_);
  relation_type_actions_.append(relation_type_actions_button_);
  relation_type_section_.append(relation_type_section_title_);
  relation_type_section_.append(relation_type_actions_);
  relation_context_summary_.set_xalign(0.0F);
  relation_context_summary_.set_wrap(true);
  relation_context_summary_.add_css_class("dim-label");
  relations_page_.append(relation_context_summary_);

  relation_search_.set_placeholder_text(
      "Pesquisar relação, tipo, elemento da história ou contexto");
  relation_type_filter_.append("all", "Todos os tipos de relação");
  relation_type_filter_.set_active_id("all");
  relation_entity_filter_ = make_entity_selector(std::nullopt, false);
  relation_time_point_filter_ = make_time_point_selector();
  relation_location_filter_ =
      make_entity_selector(type_id_for_key("location"), false);
  relation_cause_filter_ = make_entity_selector(std::nullopt, false);
  relation_filters_panel_.set_margin(18);
  auto *relation_filter_title =
      Gtk::make_managed<Gtk::Label>("Pesquisa e filtros de relações");
  relation_filter_title->set_halign(Gtk::Align::START);
  relation_filter_title->add_css_class("title-2");
  relation_filters_panel_.append(*relation_filter_title);
  relation_filters_panel_.append(relation_type_filter_);
  relation_filters_panel_.append(*relation_entity_filter_);
  relation_filters_panel_.append(*relation_time_point_filter_);
  relation_filters_panel_.append(*relation_location_filter_);
  relation_filters_panel_.append(*relation_cause_filter_);
  relation_filter_actions_.set_halign(Gtk::Align::END);
  apply_relation_filters_button_.add_css_class("suggested-action");
  relation_filter_actions_.append(clear_relation_filters_button_);
  relation_filter_actions_.append(apply_relation_filters_button_);
  relation_filters_panel_.append(relation_filter_actions_);
  relation_filters_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                      Gtk::PolicyType::AUTOMATIC);
  relation_filters_scroll_.set_child(relation_filters_panel_);
  relation_filters_surface_.set_child(relation_filters_scroll_);
  relation_filters_surface_.set_margin(8);
  relation_filters_surface_.add_css_class("filter-sheet");
  relation_filters_revealer_.set_child(relation_filters_surface_);
  relation_filters_revealer_.set_hexpand(true);
  relation_filters_revealer_.set_vexpand(true);
  relation_filters_revealer_.set_halign(Gtk::Align::FILL);
  relation_filters_revealer_.set_valign(Gtk::Align::FILL);
  relation_filters_revealer_.set_transition_type(
      Gtk::RevealerTransitionType::SLIDE_DOWN);
  set_overlay_revealer_open(relation_filters_revealer_, false);

  relation_card_list_.set_selection_mode(Gtk::SelectionMode::NONE);
  relation_card_list_.set_min_children_per_line(1);
  relation_card_list_.set_max_children_per_line(3);
  relation_card_list_.set_column_spacing(12);
  relation_card_list_.set_row_spacing(12);
  relation_card_list_.set_homogeneous(true);
  relation_card_list_.set_valign(Gtk::Align::START);
  relation_card_list_.set_vexpand(false);
  relation_scroll_.set_policy(Gtk::PolicyType::NEVER,
                              Gtk::PolicyType::AUTOMATIC);
  relation_scroll_.set_child(relation_card_list_);
  relation_scroll_.set_vexpand(true);
  relation_scroll_.set_hexpand(true);
  relation_empty_state_.set_halign(Gtk::Align::CENTER);
  relation_empty_state_.set_valign(Gtk::Align::CENTER);
  relation_empty_state_.set_vexpand(true);
  relation_empty_state_.add_css_class("content-card");
  relation_empty_title_.add_css_class("title-2");
  relation_empty_message_.set_wrap(true);
  relation_empty_message_.set_max_width_chars(58);
  relation_empty_message_.set_justify(Gtk::Justification::CENTER);
  relation_empty_message_.add_css_class("dim-label");
  relation_empty_state_.append(relation_empty_title_);
  relation_empty_state_.append(relation_empty_message_);
  relation_results_stack_.set_vexpand(true);
  relation_results_stack_.set_hexpand(true);
  relation_results_stack_.add(relation_scroll_, "results");
  relation_results_stack_.add(relation_empty_state_, "empty");
  relations_overlay_.set_child(relation_results_stack_);
  relations_overlay_.add_overlay(relation_filters_revealer_);
  relations_page_.append(relations_overlay_);
}

} // namespace inde::ui

#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/work_setup.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <tuple>

namespace inde::ui {
namespace {
bool same_query(const application::PlanningContext &a,
                const application::PlanningContext &b) {
  const auto fields = [](const auto &c) {
    return std::tie(c.search, c.work_id, c.entity_type_ids, c.relation_type_ids,
        c.related_entity_id, c.fictional_axis_id, c.fictional_time_point_id,
        c.fictional_window_start_time_point_id, c.fictional_window_end_time_point_id,
        c.editorial_node_id, c.require_document_reference, c.document_id,
        c.limit, c.offset);
  };
  return fields(a) == fields(b);
}
}

PlanningWorkspace::PlanningWorkspace(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL), service_(service) {
  build_ui();
}

void PlanningWorkspace::build_ui() {
  set_vexpand(true);
  set_hexpand(true);
  append(planning_root_);
  planning_root_.set_margin(16);
  planning_root_.add_css_class("workspace-page");
  planning_root_.set_vexpand(true);
  planning_root_.set_hexpand(true);
  page_switcher_.set_halign(Gtk::Align::START);
  page_choices_.set_margin(10);
  page_choices_.append(explorer_page_button_);
  page_choices_.append(narrative_page_button_);
  page_choices_.append(relations_page_button_);
  page_choices_.append(time_page_button_);
  page_choices_popover_.set_child(page_choices_);
  page_choices_button_.set_popover(page_choices_popover_);
  page_choices_button_.set_tooltip_text("Escolher área de planejamento");
  page_switcher_.append(page_choices_button_);
  page_switcher_.append(temporal_library_button_);
  page_switcher_.append(history_back_button_);
  page_switcher_.append(history_forward_button_);
  planning_root_.append(page_switcher_);
  work_setup_text_.set_xalign(0);work_setup_text_.set_wrap(true);work_setup_text_.set_max_width_chars(100);
  work_setup_hint_.append(work_setup_text_);work_setup_button_.set_halign(Gtk::Align::START);work_setup_hint_.append(work_setup_button_);planning_root_.append(work_setup_hint_);
  work_setup_button_.signal_clicked().connect([this]{if(auto *owner=owner_window())show_work_setup(*owner,service_,[this](const std::string &){refresh();});});
  page_stack_.property_visible_child_name().signal_changed().connect(sigc::mem_fun(*this,&PlanningWorkspace::refresh_work_setup));
  narrative_structure_workspace_.signal_catalog_changed().connect([this]{refresh_work_setup();});
  page_stack_.set_vexpand(true);
  page_stack_.set_hexpand(true);
  page_stack_.set_hhomogeneous(false);
  page_stack_.set_vhomogeneous(false);
  page_stack_.set_transition_type(Gtk::StackTransitionType::NONE);
  planning_root_.append(page_stack_);

  build_explorer_header();
  build_explorer_filters();
  build_explorer_results();
  build_relations_page();
  build_explorer_pagination();
  build_detail_page();
  build_time_page();
  page_stack_.add(narrative_structure_workspace_, "narrative");
  page_stack_.add(explorer_page_, "explorer");
  page_stack_.add(relations_page_, "relations");
  page_stack_.add(detail_page_, "detail");
  page_stack_.add(time_page_, "time");
  page_stack_.add(temporal_library_, "temporal-library");

  configure_action_menus();
  connect_ui_signals();
  reset();
}

void PlanningWorkspace::refresh_work_setup() {
  work_setup_hint_.set_visible(service_.current() && service_.catalog().works.empty() && page_stack_.get_visible_child_name()!="narrative");
}

void PlanningWorkspace::remember_navigation() {
  if (restoring_navigation_ || !service_.current())
    return;
  NavigationState state;
  state.context = service_.planning_context();
  state.entity_id = selected_entity_id_;
  state.time_point_id = selected_time_point_id_;
  state.page_name = page_stack_.get_visible_child_name();
  if (state.page_name.empty())
    state.page_name = "explorer";
  navigation_backstack_.push_back(std::move(state));
  constexpr std::size_t history_limit = 32;
  if (navigation_backstack_.size() > history_limit)
    navigation_backstack_.erase(navigation_backstack_.begin());
  navigation_forwardstack_.clear();
  update_navigation_actions();
}

void PlanningWorkspace::go_back_in_navigation() {
  if (navigation_backstack_.empty()) {
    show_explorer_page();
    return;
  }
  NavigationState current;
  current.context = service_.planning_context();
  current.entity_id = selected_entity_id_;
  current.time_point_id = selected_time_point_id_;
  current.page_name = page_stack_.get_visible_child_name();
  if (current.page_name.empty())
    current.page_name = "explorer";
  navigation_forwardstack_.push_back(std::move(current));
  auto previous = std::move(navigation_backstack_.back());
  navigation_backstack_.pop_back();
  restore_navigation_state(std::move(previous));
}

void PlanningWorkspace::go_forward_in_navigation() {
  if (navigation_forwardstack_.empty())
    return;
  NavigationState current;
  current.context = service_.planning_context();
  current.entity_id = selected_entity_id_;
  current.time_point_id = selected_time_point_id_;
  current.page_name = page_stack_.get_visible_child_name();
  if (current.page_name.empty())
    current.page_name = "explorer";
  navigation_backstack_.push_back(std::move(current));
  auto next = std::move(navigation_forwardstack_.back());
  navigation_forwardstack_.pop_back();
  restore_navigation_state(std::move(next));
}

void PlanningWorkspace::restore_navigation_state(NavigationState state) {
  restoring_navigation_ = true;
  dismiss_filters();
  const bool reuse_results = explorer_loaded_ && state.page_name == "explorer" &&
      same_query(state.context, explorer_snapshot_.context);
  offset_ = state.context.offset;
  selected_type_ids_ = state.context.entity_type_ids;
  selected_relation_type_ids_ = state.context.relation_type_ids;
  refreshing_context_ = true;
  refreshing_facets_ = true;
  search_.set_text(state.context.search);
  task_mode_filter_.set_active_id("explore");
  refreshing_facets_ = false;
  refreshing_context_ = false;
  service_.set_planning_context(std::move(state.context));
  selected_entity_id_.reset();
  selected_time_point_id_.reset();

  if (reuse_results) {
    show_explorer_page();
    restoring_navigation_ = false;
    update_navigation_actions();
    return;
  }

  // Reconstruir somente quando a consulta salva não corresponde aos cartões.
  // Uma volta simples conserva os widgets e a rolagem já montados.
  page_stack_.set_visible_child(state.page_name == "detail" ? "explorer" : state.page_name);
  refresh();
  if (state.page_name == "detail" && state.entity_id) {
    selected_entity_id_ = std::move(state.entity_id);
    refresh_details();
    show_entity_detail_page();
  } else if (state.page_name == "time") {
    show_time_page();
    if (state.time_point_id)
      select_time_point(std::move(*state.time_point_id));
  } else if (state.page_name == "temporal-library") {
    show_temporal_library();
  } else if (state.page_name == "relations") {
    show_relations_page();
  } else if (state.page_name == "narrative") {
    show_narrative_page();
  } else {
    show_explorer_page();
  }
  restoring_navigation_ = false;
  update_navigation_actions();
}

void PlanningWorkspace::update_navigation_actions() {
  history_back_button_.set_sensitive(!navigation_backstack_.empty());
  history_forward_button_.set_sensitive(!navigation_forwardstack_.empty());
}

void PlanningWorkspace::show_narrative_page() {
  page_choices_popover_.popdown();
  page_choices_button_.set_label("Plano narrativo");
  detail_visible_ = false;
  selected_entity_id_.reset();
  dismiss_filters();
  page_switcher_.set_visible(true);
  page_stack_.set_visible_child("narrative");
  narrative_page_button_.add_css_class("suggested-action");
  explorer_page_button_.remove_css_class("suggested-action");
  relations_page_button_.remove_css_class("suggested-action");
  time_page_button_.remove_css_class("suggested-action");
  narrative_structure_workspace_.refresh();
  page_choices_button_.grab_focus();
}

void PlanningWorkspace::show_explorer_page() {
  page_choices_popover_.popdown();
  page_choices_button_.set_label("Personagens e mundo");
  detail_visible_ = false;
  // Voltar encerra o contexto operacional da entidade. Assim, no catálogo,
  // clicar em um card é sempre abrir conteúdo — nunca alternar seleção para
  // uma ação de CRUD posterior.
  selected_entity_id_.reset();
  page_switcher_.set_visible(true);
  page_stack_.set_visible_child("explorer");
  explorer_page_button_.add_css_class("suggested-action");
  narrative_page_button_.remove_css_class("suggested-action");
  relations_page_button_.remove_css_class("suggested-action");
  time_page_button_.remove_css_class("suggested-action");
  // Narrative is the initial page. Materialize the explorer on its first
  // activation, but reuse its cards when returning from an entity detail.
  if (!explorer_loaded_ && service_.current())
    refresh();
  page_choices_button_.grab_focus();
}

void PlanningWorkspace::show_relations_page() {
  page_choices_popover_.popdown();
  page_choices_button_.set_label("Relações");
  detail_visible_ = false;
  selected_entity_id_.reset();
  dismiss_filters();
  page_switcher_.set_visible(true);
  page_stack_.set_visible_child("relations");
  narrative_page_button_.remove_css_class("suggested-action");
  explorer_page_button_.remove_css_class("suggested-action");
  relations_page_button_.add_css_class("suggested-action");
  time_page_button_.remove_css_class("suggested-action");
  refresh_relation_type_controls();
  refresh_relation_filter_options();
  refresh_relation_explorer();
  page_choices_button_.grab_focus();
}

void PlanningWorkspace::show_time_page() {
  page_choices_popover_.popdown();
  page_choices_button_.set_label("Tempo da história");
  detail_visible_ = false;
  page_switcher_.set_visible(true);
  page_stack_.set_visible_child("time");
  narrative_page_button_.remove_css_class("suggested-action");
  explorer_page_button_.remove_css_class("suggested-action");
  relations_page_button_.remove_css_class("suggested-action");
  time_page_button_.add_css_class("suggested-action");
  refresh_axes();
  if (axis_combo_.get_sensitive())
    axis_combo_.grab_focus();
  else
    axis_add_button_.grab_focus();
}
void PlanningWorkspace::show_temporal_library() {
  page_choices_popover_.popdown();
  dismiss_filters();
  detail_visible_=false;
  page_switcher_.set_visible(true);
  page_stack_.set_visible_child("temporal-library");
  page_choices_button_.set_label("Planejar");
  temporal_library_.refresh();
}

void PlanningWorkspace::show_entity_detail_page() {
  if (!selected_entity_id_)
    return;
  detail_visible_ = true;
  page_switcher_.set_visible(false);
  dismiss_filters();
  detail_context_.set_text(page_label_.get_text());
  page_stack_.set_visible_child("detail");
  back_to_explorer_button_.grab_focus();
}

void PlanningWorkspace::dismiss_filters() {
  if (!filters_visible_)
    return;
  filters_visible_ = false;
  set_overlay_revealer_open(filters_revealer_, false);
  filters_button_.set_label("Filtros");
  filters_button_.grab_focus();
}

void PlanningWorkspace::refresh_context_summary() {
  const auto &context = explorer_snapshot_.context;
  std::vector<std::string> chips;
  if (!context.search.empty())
    chips.push_back("Busca: “" + context.search + "”");
  if (!context.entity_type_ids.empty())
    chips.push_back(std::to_string(context.entity_type_ids.size()) +
                    " tipo(s)");
  if (context.work_id)
    chips.push_back("Obra filtrada");
  if (!context.relation_type_ids.empty())
    chips.push_back(std::to_string(context.relation_type_ids.size()) +
                    " relação(ões)");
  if (context.related_entity_id)
    chips.push_back("Outro elemento da relação definida");
  if (context.fictional_axis_id)
    chips.push_back("Linha do tempo");
  if (context.fictional_time_point_id)
    chips.push_back("Marco na linha do tempo");
  if (context.fictional_window_start_time_point_id)
    chips.push_back("Período fechado");
  if (context.editorial_node_id)
    chips.push_back("Apresentação editorial");
  if (context.document_id)
    chips.push_back("Documento específico");
  else if (context.require_document_reference)
    chips.push_back("Usada em Escrita");
  std::string summary = chips.empty() ? "Todo o Projeto" : "Filtros: ";
  for (std::size_t index = 0; index < chips.size(); ++index) {
    if (index != 0)
      summary += "  •  ";
    summary += chips[index];
  }
  summary +=
      "  —  " + std::to_string(explorer_snapshot_.matching_count) +
      (explorer_snapshot_.matching_count == 1 ? " resultado" : " resultados");
  context_summary_.set_text(summary);
}

void PlanningWorkspace::install_actions(Gtk::ApplicationWindow &window) {
  window.add_action("create-entity",
                    sigc::mem_fun(*this, &PlanningWorkspace::create_entity));
  window.add_action(
      "edit-entity",
      sigc::mem_fun(*this, &PlanningWorkspace::edit_selected_entity));
  window.add_action(
      "remove-entity",
      sigc::mem_fun(*this, &PlanningWorkspace::delete_selected_entity));
}

void PlanningWorkspace::refresh() {
  refresh_work_setup();
  if (!service_.current()) {
    reset();
    return;
  }

  const auto visible_page = page_stack_.get_visible_child_name();
  if (visible_page == "narrative") {
    narrative_structure_workspace_.refresh();
    return;
  }
  if (visible_page == "temporal-library") {
    temporal_library_.refresh();
    return;
  }
  refresh_types();
  if (visible_page == "detail" && selected_entity_id_) {
    const auto entity = service_.narrative().entity(*selected_entity_id_);
    if (entity) {
      const auto found = std::find_if(page_.begin(), page_.end(),
          [&](const auto &value) { return value.id == entity->id; });
      if (found != page_.end()) *found = *entity;
      else page_.push_back(*entity);
      refresh_details();
      return;
    }
    selected_entity_id_.reset();
    show_explorer_page();
  }

  // Cada página atualiza o próprio conjunto pesado de consultas e widgets.
  // Manter Relações e a visão temporal fora do caminho crítico do explorador
  // evita reconstruir páginas invisíveis em todo retorno ao Planejamento.
  if (visible_page == "time") {
    refresh_axes();
    return;
  }

  axes_ = service_.planning().time_axes();
  if (visible_page == "relations") {
    refresh_relation_type_controls();
    refresh_relation_filter_options();
    refresh_relation_explorer();
    return;
  }

  refresh_context_filters();
  refresh_entities();
}

void PlanningWorkspace::reset() {
  work_setup_hint_.set_visible(false);
  selected_entity_id_.reset();
  offset_ = 0;
  types_.clear();
  page_.clear();
  explorer_snapshot_ = {};
  explorer_loaded_ = false;
  axes_.clear();
  time_points_.clear();
  participants_.clear();
  presences_.clear();
  work_scopes_.clear();
  editorial_references_.clear();
  relation_types_.clear();
  relation_page_results_.clear();
  relation_entity_labels_.clear();
  relation_time_point_labels_.clear();
  selected_type_ids_.clear();
  selected_relation_type_ids_.clear();
  relations_.clear();
  occurrence_.reset();
  selected_time_point_id_.reset();
  selected_participant_id_.reset();
  selected_presence_id_.reset();
  selected_work_scope_id_.reset();
  selected_editorial_reference_id_.reset();
  selected_relation_id_.reset();
  related_entity_filter_id_.reset();
  navigation_backstack_.clear();
  navigation_forwardstack_.clear();
  refreshing_context_ = true;
  search_.set_text("");
  refreshing_facets_ = true;
  task_mode_filter_.set_active_id("explore");
  refreshing_facets_ = false;
  work_filter_.remove_all();
  work_filter_.append("all", "Todo o Projeto");
  work_filter_.set_active_id("all");
  temporal_axis_filter_.remove_all();
  temporal_axis_filter_.append("all", "Todas as linhas do tempo");
  temporal_axis_filter_.set_active_id("all");
  temporal_point_filter_->clear_selection();
  temporal_point_filter_->set_sensitive(false);
  temporal_window_filter_.set_active_id("all");
  temporal_window_end_filter_->clear_selection();
  temporal_window_end_filter_->set_sensitive(false);
  temporal_window_end_filter_->set_visible(false);
  editorial_node_filter_->clear_selection();
  editorial_node_filter_->set_sensitive(false);
  writing_usage_filter_.set_active_id("all");
  writing_document_filter_->clear_selection();
  writing_document_filter_->set_sensitive(false);
  counterpart_filter_label_.set_text("Outro elemento da relação: qualquer elemento da história");
  refreshing_context_ = false;
  while (auto *child = entity_list_.get_first_child())
    entity_list_.remove(*child);
  while (auto *child = point_list_.get_first_child())
    point_list_.remove(*child);
  while (auto *child = participant_list_.get_first_child())
    participant_list_.remove(*child);
  while (auto *child = presence_list_.get_first_child())
    presence_list_.remove(*child);
  while (auto *child = work_scope_list_.get_first_child())
    work_scope_list_.remove(*child);
  while (auto *child = editorial_reference_list_.get_first_child())
    editorial_reference_list_.remove(*child);
  while (auto *child = writing_reference_list_.get_first_child())
    writing_reference_list_.remove(*child);
  while (auto *child = relation_list_.get_first_child())
    relation_list_.remove(*child);
  while (auto *child = relation_card_list_.get_first_child())
    relation_card_list_.remove(*child);
  relation_type_combo_.remove_all();
  entity_type_combo_.remove_all();
  entity_type_actions_button_.set_sensitive(false);
  relation_type_filter_.remove_all();
  relation_type_filter_.append("all", "Todos os tipos de relação");
  relation_type_filter_.set_active_id("all");
  relation_search_.set_text("");
  relation_entity_filter_->clear_selection();
  relation_time_point_filter_->clear_selection();
  relation_location_filter_->clear_selection();
  relation_cause_filter_->clear_selection();
  refreshing_axis_ = true;
  axis_combo_.remove_all();
  refreshing_axis_ = false;
  entity_name_.set_text("");
  entity_type_.set_text("");
  entity_summary_.set_text("");
  inspector_identity_.set_text("Nenhum elemento selecionado");
  inspector_dates_.set_text("");
  inspector_relations_.set_text("");
  inspector_presences_.set_text("");
  empty_title_.set_visible(true);
  empty_message_.set_visible(true);
  edit_button_.set_sensitive(false);
  remove_button_.set_sensitive(false);
  previous_button_.set_sensitive(false);
  next_button_.set_sensitive(false);
  page_label_.set_text("Página 1");
  axis_edit_button_.set_sensitive(false);
  axis_remove_button_.set_sensitive(false);
  axis_actions_button_.set_sensitive(false);
  point_add_button_.set_sensitive(false);
  point_edit_button_.set_sensitive(false);
  point_remove_button_.set_sensitive(false);
  point_actions_button_.set_sensitive(false);
  relation_open_counterpart_button_.set_sensitive(false);
  relation_filter_counterpart_button_.set_sensitive(false);
  refresh_temporal_context();
  refresh_editorial_context();
  refresh_relations();
  filters_visible_ = false;
  set_overlay_revealer_open(filters_revealer_, false);
  filters_button_.set_label("Filtros");
  relation_filters_visible_ = false;
  set_overlay_revealer_open(relation_filters_revealer_, false);
  relation_filters_button_.set_label("Filtros");
  relation_context_summary_.set_text("0 relações encontradas");
  explorer_empty_title_.set_visible(false);
  explorer_empty_message_.set_visible(false);
  context_summary_.set_text("Todo o Projeto");
  update_navigation_actions();
  narrative_structure_workspace_.reset();
  show_explorer_page();
}

void PlanningWorkspace::toggle_navigation() {
  if (detail_visible_)
    show_explorer_page();
  if (page_stack_.get_visible_child_name() != "explorer")
    show_explorer_page();
  filters_visible_ = !filters_visible_;
  set_overlay_revealer_open(filters_revealer_, filters_visible_);
  filters_button_.set_label(filters_visible_ ? "Ocultar filtros" : "Filtros");
  if (filters_visible_)
    search_.grab_focus();
}

void PlanningWorkspace::toggle_inspector() {
  if (detail_visible_) {
    show_explorer_page();
    return;
  }
  if (selected_entity_id_)
    show_entity_detail_page();
}

} // namespace inde::ui

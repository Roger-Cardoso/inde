#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

PlanningWorkspace::PlanningWorkspace(application::ProjectService &service)
    : Gtk::Paned(Gtk::Orientation::HORIZONTAL), service_(service) {
  build_ui();
}

void PlanningWorkspace::build_ui() {
  set_vexpand(true);
  set_hexpand(true);
  set_start_child(planning_root_);
  planning_root_.set_margin(16);
  planning_root_.add_css_class("workspace-page");
  planning_root_.set_vexpand(true);
  planning_root_.set_hexpand(true);
  page_switcher_.set_halign(Gtk::Align::START);
  narrative_page_button_.add_css_class("suggested-action");
  page_switcher_.append(narrative_page_button_);
  page_switcher_.append(explorer_page_button_);
  page_switcher_.append(relations_page_button_);
  page_switcher_.append(time_page_button_);
  page_switcher_.append(history_back_button_);
  page_switcher_.append(history_forward_button_);
  planning_root_.append(page_switcher_);
  page_stack_.set_vexpand(true);
  page_stack_.set_hexpand(true);
  page_stack_.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
  planning_root_.append(page_stack_);

  explorer_page_.set_vexpand(true);
  explorer_page_.set_hexpand(true);
  explorer_title_.add_css_class("title-1");
  explorer_title_.add_css_class("page-title");
  explorer_title_.set_hexpand(true);
  explorer_title_.set_halign(Gtk::Align::START);
  explorer_header_.append(explorer_title_);
  remove_button_.add_css_class("destructive-action");
  explorer_page_.append(explorer_header_);
  // O explorador não mantém seleção operacional: os cards abrem o detalhe.
  // Por isso, o único CRUD aqui é criar; editar/remover pertencem ao detalhe
  // da entidade efetivamente aberta.
  explorer_toolbar_.set_halign(Gtk::Align::START);
  for (auto *section : {&entity_crud_section_, &entity_type_section_,
                        &explorer_filter_section_}) {
    section->set_margin(10);
    section->add_css_class("tool-section");
    section->set_halign(Gtk::Align::START);
    section->set_valign(Gtk::Align::START);
  }
  for (auto *title : {&entity_crud_title_, &entity_type_section_title_,
                      &explorer_filter_title_}) {
    title->add_css_class("heading");
    title->set_halign(Gtk::Align::START);
  }
  entity_crud_actions_.append(add_button_);
  entity_crud_section_.append(entity_crud_title_);
  entity_crud_section_.append(entity_crud_actions_);
  entity_type_actions_.append(entity_type_combo_);
  entity_type_actions_.append(entity_type_add_button_);
  entity_type_actions_.append(entity_type_actions_button_);
  entity_type_section_.append(entity_type_section_title_);
  entity_type_section_.append(entity_type_actions_);
  explorer_filter_actions_.append(filters_button_);
  explorer_filter_section_.append(explorer_filter_title_);
  explorer_filter_section_.append(explorer_filter_actions_);
  explorer_toolbar_.append(entity_crud_section_);
  explorer_toolbar_.append(entity_type_section_);
  explorer_toolbar_.append(explorer_filter_section_);
  explorer_page_.append(explorer_toolbar_);
  context_summary_.set_wrap(true);
  context_summary_.set_xalign(0.0F);
  context_summary_.add_css_class("dim-label");
  context_summary_.add_css_class("context-summary");
  explorer_page_.append(context_summary_);

  navigation_panel_.set_margin(12);
  navigation_title_.add_css_class("heading");
  navigation_title_.set_halign(Gtk::Align::START);
  navigation_title_.set_text("Refinar resultados");
  search_.set_placeholder_text("Pesquisar nome ou resumo");
  set_accessible_label(search_, "Pesquisar entidades");
  set_accessible_description(search_, "Filtra entidades por nome ou resumo, "
                                      "sem expansão relacional implícita.");
  task_mode_filter_.append("explore", "Explorar tudo");
  task_mode_filter_.append("characters", "Modo: personagens");
  task_mode_filter_.append("events", "Modo: acontecimentos");
  task_mode_filter_.append("locations", "Modo: locais");
  task_mode_filter_.set_active_id("explore");
  work_filter_.append("all", "Todo o Projeto");
  counterpart_filter_label_.set_xalign(0.0F);
  counterpart_filter_label_.set_wrap(true);
  counterpart_filter_label_.add_css_class("dim-label");
  context_filters_.set_margin(8);
  context_filters_.append(work_filter_);
  temporal_filter_label_.set_halign(Gtk::Align::START);
  context_filters_.append(temporal_filter_label_);
  temporal_axis_filter_.set_tooltip_text(
      "Mostra entidades ligadas a acontecimentos, participações ou presenças "
      "no eixo");
  context_filters_.append(temporal_axis_filter_);
  temporal_window_filter_.append("all", "Eixo inteiro");
  temporal_window_filter_.append("point", "Um ponto temporal");
  temporal_window_filter_.append("range", "Período fechado");
  temporal_window_filter_.set_active_id("all");
  temporal_window_filter_.set_tooltip_text(
      "Escolha um ponto ou um período fechado no eixo selecionado");
  context_filters_.append(temporal_window_filter_);
  temporal_point_filter_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar ponto do eixo",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        const std::string axis_id = temporal_axis_filter_.get_active_id().raw();
        if (axis_id.empty() || axis_id == "all")
          return result;
        persistence::PlanningQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        for (const auto &point :
             service_.planning().time_points(axis_id, query))
          result.push_back({point.id,
                            std::to_string(point.ordinal) + " — " + point.label,
                            point.description});
        return result;
      },
      "Escolha um eixo para pesquisar pontos");
  temporal_point_filter_->set_tooltip_text(
      "No ponto, presenças precisam estar ativas; acontecimentos e "
      "participações ocorrem nele");
  temporal_point_filter_->set_sensitive(false);
  context_filters_.append(*temporal_point_filter_);
  temporal_window_end_filter_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar fim do período",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        const std::string axis_id = temporal_axis_filter_.get_active_id().raw();
        if (axis_id.empty() || axis_id == "all")
          return result;
        persistence::PlanningQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        for (const auto &point :
             service_.planning().time_points(axis_id, query))
          result.push_back({point.id,
                            std::to_string(point.ordinal) + " — " + point.label,
                            point.description});
        return result;
      },
      "Escolha o início antes de definir o fim");
  temporal_window_end_filter_->set_tooltip_text(
      "O período inclui os dois limites; presenças que o atravessam também "
      "entram");
  temporal_window_end_filter_->set_sensitive(false);
  temporal_window_end_filter_->set_visible(false);
  context_filters_.append(*temporal_window_end_filter_);
  editorial_filter_label_.set_halign(Gtk::Align::START);
  context_filters_.append(editorial_filter_label_);
  editorial_node_filter_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar unidade editorial",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        const std::string work_id = work_filter_.get_active_id().raw();
        if (work_id.empty() || work_id == "all")
          return result;
        const auto contains = [](const std::string &value,
                                 const std::string &needle) {
          if (needle.empty())
            return true;
          const auto folded = [](std::string text) {
            for (auto &character : text) {
              if (character >= 'A' && character <= 'Z')
                character = static_cast<char>(character - 'A' + 'a');
            }
            return text;
          };
          return folded(value).find(folded(needle)) != std::string::npos;
        };
        const auto nodes = service_.structural_nodes_for_work(work_id);
        const auto path_labels = project::structural_node_path_labels(nodes);
        for (const auto &node : nodes) {
          const auto &path = path_labels.at(node.id);
          if (!contains(path, search))
            continue;
          result.push_back(
              {node.id, path, project::structural_node_position_label(node)});
          if (result.size() >= limit)
            break;
        }
        return result;
      },
      "Escolha primeiro uma Obra para filtrar sua apresentação");
  editorial_node_filter_->set_tooltip_text(
      "Inclui referências da unidade selecionada e de suas unidades internas");
  editorial_node_filter_->set_sensitive(false);
  context_filters_.append(*editorial_node_filter_);
  writing_filter_label_.set_halign(Gtk::Align::START);
  context_filters_.append(writing_filter_label_);
  writing_usage_filter_.append("all", "Qualquer uso em Escrita");
  writing_usage_filter_.append("referenced", "Usadas em algum Documento");
  writing_usage_filter_.append("document", "Usadas num Documento específico");
  writing_usage_filter_.set_active_id("all");
  set_accessible_label(writing_usage_filter_, "Filtro de uso em Escrita");
  context_filters_.append(writing_usage_filter_);
  writing_document_filter_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar Documento",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        if (!service_.current())
          return result;
        persistence::DocumentQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        for (const auto &document :
             service_.writing().document_summaries(query))
          result.push_back({document.id, document.title,
                            std::to_string(document.entity_reference_count) +
                                (document.entity_reference_count == 1
                                     ? " entidade vinculada"
                                     : " entidades vinculadas")});
        return result;
      },
      "Escolha o modo Documento específico");
  writing_document_filter_->set_sensitive(false);
  context_filters_.append(*writing_document_filter_);
  context_filters_.append(relation_facets_label_);
  relation_facets_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                     Gtk::PolicyType::AUTOMATIC);
  relation_facets_scroll_.set_min_content_height(72);
  relation_facets_scroll_.set_max_content_height(144);
  relation_facets_scroll_.set_propagate_natural_height(false);
  relation_facets_scroll_.set_propagate_natural_width(false);
  relation_facets_scroll_.set_child(relation_facets_);
  context_filters_.append(relation_facets_scroll_);
  context_filters_.append(counterpart_filter_label_);
  context_filters_.append(clear_context_button_);
  context_expander_.set_child(context_filters_);
  context_expander_.set_expanded(true);
  navigation_panel_.append(navigation_title_);
  auto *filter_columns =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 18);
  auto *primary_filters =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *context_column =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  filter_columns->set_hexpand(true);
  filter_columns->set_vexpand(false);
  primary_filters->set_hexpand(true);
  context_column->set_hexpand(true);
  primary_filters->append(search_);
  primary_filters->append(task_mode_filter_);
  primary_filters->append(type_facets_label_);
  type_facets_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                 Gtk::PolicyType::AUTOMATIC);
  type_facets_scroll_.set_min_content_height(72);
  type_facets_scroll_.set_max_content_height(144);
  type_facets_scroll_.set_propagate_natural_height(false);
  type_facets_scroll_.set_propagate_natural_width(false);
  type_facets_scroll_.set_child(type_facets_);
  primary_filters->append(type_facets_scroll_);
  context_column->append(context_expander_);
  filter_columns->append(*primary_filters);
  filter_columns->append(*context_column);
  navigation_panel_.append(*filter_columns);
  apply_filters_button_.add_css_class("suggested-action");
  apply_filters_button_.set_halign(Gtk::Align::END);
  navigation_panel_.append(apply_filters_button_);
  // A camada cobre toda a área útil enquanto é usada. Só as coleções internas
  // (facetas, relações e seletores incrementais) conservam rolagem própria.
  navigation_panel_.set_vexpand(false);
  navigation_panel_.set_hexpand(true);
  filters_scroll_.set_policy(Gtk::PolicyType::NEVER,
                             Gtk::PolicyType::AUTOMATIC);
  filters_scroll_.set_propagate_natural_height(false);
  filters_scroll_.set_propagate_natural_width(false);
  filters_scroll_.set_child(navigation_panel_);
  filters_scroll_.set_hexpand(true);
  filters_scroll_.set_vexpand(true);
  filters_surface_.set_child(filters_scroll_);
  filters_surface_.add_css_class("filter-sheet");
  filters_surface_.set_margin(8);
  filters_surface_.set_vexpand(true);
  filters_revealer_.set_child(filters_surface_);
  filters_revealer_.set_hexpand(true);
  filters_revealer_.set_vexpand(true);
  filters_revealer_.set_halign(Gtk::Align::FILL);
  filters_revealer_.set_valign(Gtk::Align::FILL);
  filters_revealer_.set_transition_type(
      Gtk::RevealerTransitionType::SLIDE_DOWN);
  set_overlay_revealer_open(filters_revealer_, false);

  entity_scroll_.set_child(entity_list_);
  entity_scroll_.set_vexpand(true);
  entity_list_.set_selection_mode(Gtk::SelectionMode::NONE);
  entity_list_.set_min_children_per_line(1);
  entity_list_.set_max_children_per_line(4);
  entity_list_.set_column_spacing(12);
  entity_list_.set_row_spacing(12);
  entity_list_.set_homogeneous(true);
  // A grade distribui as colunas pela largura disponível, como Explorar
  // relações, sem transformar a altura livre em expansão vertical dos cards.
  entity_list_.set_halign(Gtk::Align::FILL);
  entity_list_.set_valign(Gtk::Align::START);
  entity_list_.set_hexpand(true);
  entity_list_.set_vexpand(false);
  entity_scroll_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  explorer_empty_title_.add_css_class("title-2");
  explorer_empty_title_.set_halign(Gtk::Align::START);
  explorer_empty_message_.set_wrap(true);
  explorer_empty_message_.set_xalign(0.0F);
  explorer_empty_message_.set_text(
      "Ajuste a busca ou remova filtros para voltar a explorar o Projeto.");
  explorer_page_.append(explorer_empty_title_);
  explorer_page_.append(explorer_empty_message_);
  explorer_overlay_.set_vexpand(true);
  explorer_overlay_.set_hexpand(true);
  explorer_overlay_.set_child(entity_scroll_);
  explorer_overlay_.add_overlay(filters_revealer_);
  explorer_page_.append(explorer_overlay_);

  relations_page_.set_vexpand(true);
  relations_page_.set_hexpand(true);
  relations_page_title_.add_css_class("title-1");
  relations_page_title_.add_css_class("page-title");
  relations_page_title_.set_hexpand(true);
  relations_page_title_.set_halign(Gtk::Align::START);
  relations_header_.append(relations_page_title_);
  relations_header_.append(relation_filters_button_);
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
  relation_crud_actions_.append(relation_add_button_);
  relation_crud_actions_.append(relation_actions_button_);
  relation_crud_section_.append(relation_crud_title_);
  relation_crud_section_.append(relation_crud_actions_);
  relation_type_actions_.append(relation_type_combo_);
  relation_type_actions_.append(relation_type_add_button_);
  relation_type_actions_.append(relation_type_actions_button_);
  relation_type_section_.append(relation_type_section_title_);
  relation_type_section_.append(relation_type_actions_);
  relations_toolbar_.append(relation_crud_section_);
  relations_toolbar_.append(relation_type_section_);
  relations_page_.append(relations_toolbar_);
  relation_context_summary_.set_xalign(0.0F);
  relation_context_summary_.set_wrap(true);
  relation_context_summary_.add_css_class("dim-label");
  relations_page_.append(relation_context_summary_);

  relation_search_.set_placeholder_text(
      "Pesquisar relação, tipo, entidade ou contexto");
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
  relation_filters_panel_.append(relation_search_);
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
  pagination_.append(previous_button_);
  pagination_.append(page_label_);
  pagination_.append(next_button_);
  page_label_.set_hexpand(true);
  page_label_.set_halign(Gtk::Align::CENTER);
  explorer_page_.append(pagination_);

  detail_page_.set_vexpand(true);
  detail_page_.set_hexpand(true);
  back_to_explorer_button_.set_halign(Gtk::Align::START);
  detail_context_.set_hexpand(true);
  detail_context_.set_halign(Gtk::Align::END);
  detail_context_.add_css_class("dim-label");
  detail_header_.append(back_to_explorer_button_);
  detail_header_.append(entity_actions_button_);
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
      "Aplique relações desta entidade usando os tipos já definidos. "
      "Para gerenciar relações e tipos, abra Explorar relações.");
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

  time_page_.set_vexpand(true);
  time_page_.set_hexpand(true);
  time_page_title_.add_css_class("title-1");
  time_page_title_.add_css_class("page-title");
  time_page_title_.set_hexpand(true);
  time_page_title_.set_halign(Gtk::Align::START);
  time_header_.append(time_page_title_);
  time_page_.append(time_header_);
  time_page_description_.set_wrap(true);
  time_page_description_.set_xalign(0.0F);
  time_page_description_.add_css_class("dim-label");
  time_page_description_.set_text(
      "Organize eixos, pontos e fatos temporais. A Timeline visual permanece "
      "no workspace Gráficos.");
  time_page_.append(time_page_description_);
  inspector_scroll_.set_child(inspector_panel_);
  inspector_scroll_.set_policy(Gtk::PolicyType::NEVER,
                               Gtk::PolicyType::AUTOMATIC);
  inspector_scroll_.set_vexpand(true);
  inspector_scroll_.set_hexpand(true);
  time_facts_scroll_.set_child(time_facts_panel_);
  time_facts_scroll_.set_policy(Gtk::PolicyType::NEVER,
                                Gtk::PolicyType::AUTOMATIC);
  time_facts_scroll_.set_vexpand(true);
  time_facts_scroll_.set_hexpand(true);
  time_columns_.append(inspector_scroll_);
  time_columns_.append(time_facts_scroll_);
  time_page_.append(time_columns_);
  inspector_panel_.set_margin(12);
  inspector_panel_.add_css_class("content-card");
  time_facts_panel_.set_margin(12);
  time_facts_panel_.add_css_class("content-card");
  time_title_.add_css_class("title-2");
  time_title_.set_halign(Gtk::Align::START);
  axis_toolbar_.append(axis_add_button_);
  axis_toolbar_.append(axis_actions_button_);
  axis_remove_button_.add_css_class("destructive-action");
  points_title_.add_css_class("heading");
  points_title_.set_halign(Gtk::Align::START);
  point_toolbar_.append(point_add_button_);
  point_toolbar_.append(point_actions_button_);
  point_remove_button_.add_css_class("destructive-action");
  point_list_.add_css_class("boxed-list");
  inspector_panel_.append(time_separator_);
  inspector_panel_.append(time_title_);
  inspector_panel_.append(axis_combo_);
  inspector_panel_.append(axis_toolbar_);
  inspector_panel_.append(points_title_);
  inspector_panel_.append(point_toolbar_);
  inspector_panel_.append(point_list_);
  time_events_title_.add_css_class("heading");
  time_events_title_.set_halign(Gtk::Align::START);
  time_events_list_.add_css_class("boxed-list");
  time_presences_title_.add_css_class("heading");
  time_presences_title_.set_halign(Gtk::Align::START);
  time_presences_list_.add_css_class("boxed-list");
  time_facts_panel_.append(time_events_title_);
  time_facts_panel_.append(time_events_list_);
  time_facts_panel_.append(time_presences_title_);
  time_facts_panel_.append(time_presences_list_);

  page_stack_.add(narrative_structure_workspace_, "narrative");
  page_stack_.add(explorer_page_, "explorer");
  page_stack_.add(relations_page_, "relations");
  page_stack_.add(detail_page_, "detail");
  page_stack_.add(time_page_, "time");

  configure_overflow_button(
      entity_actions_button_,
      {{"Editar entidade", [this] { edit_selected_entity(); },
        "document-edit-symbolic"},
       {"Remover entidade", [this] { delete_selected_entity(); },
        "user-trash-symbolic", true, true, true}},
      "Ações da entidade");
  configure_overflow_button(entity_type_actions_button_,
                            {{"Editar tipo", [this] { edit_entity_type(); },
                              "document-edit-symbolic"},
                             {"Remover tipo", [this] { delete_entity_type(); },
                              "user-trash-symbolic", true, true, true}},
                            "Ações do tipo de entidade selecionado");
  configure_overflow_button(
      work_scope_actions_button_,
      {{"Editar notas", [this] { edit_work_scope(); },
        "document-edit-symbolic"},
       {"Remover vínculo", [this] { delete_work_scope(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do vínculo selecionado");
  configure_overflow_button(
      relation_type_actions_button_,
      {{"Editar tipo", [this] { edit_relation_type(); },
        "document-edit-symbolic"},
       {"Remover tipo", [this] { delete_relation_type(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do tipo de relação");
  configure_overflow_button(
      relation_actions_button_,
      {{"Editar relação", [this] { edit_relation(); },
        "document-edit-symbolic"},
       {"Abrir contraparte", [this] { open_relation_counterpart(); },
        "go-next-symbolic"},
       {"Filtrar por contraparte", [this] { filter_by_relation_counterpart(); },
        "edit-find-symbolic"},
       {"Remover relação", [this] { delete_relation(); }, "user-trash-symbolic",
        true, true, true}},
      "Ações da relação selecionada");
  configure_overflow_button(
      event_actions_button_,
      {{"Editar ocorrência", [this] { edit_event_occurrence(); },
        "document-edit-symbolic"},
       {"Remover ocorrência", [this] { delete_event_occurrence(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do acontecimento");
  configure_overflow_button(
      participant_actions_button_,
      {{"Editar participante", [this] { edit_participant(); },
        "document-edit-symbolic"},
       {"Remover participante", [this] { delete_participant(); },
        "user-trash-symbolic", true, true, true}},
      "Ações do participante selecionado");
  configure_overflow_button(presence_actions_button_,
                            {{"Editar presença", [this] { edit_presence(); },
                              "document-edit-symbolic"},
                             {"Remover presença", [this] { delete_presence(); },
                              "user-trash-symbolic", true, true, true}},
                            "Ações da presença selecionada");
  configure_overflow_button(
      editorial_reference_actions_button_,
      {{"Editar referência", [this] { edit_editorial_reference(); },
        "document-edit-symbolic"},
       {"Remover referência", [this] { delete_editorial_reference(); },
        "user-trash-symbolic", true, true, true}},
      "Ações da referência selecionada");
  configure_overflow_button(
      axis_actions_button_,
      {{"Editar eixo", [this] { edit_time_axis(); }, "document-edit-symbolic"},
       {"Remover eixo", [this] { delete_time_axis(); }, "user-trash-symbolic",
        true, true, true}},
      "Ações do eixo atual");
  configure_overflow_button(point_actions_button_,
                            {{"Editar ponto", [this] { edit_time_point(); },
                              "document-edit-symbolic"},
                             {"Remover ponto", [this] { delete_time_point(); },
                              "user-trash-symbolic", true, true, true}},
                            "Ações do ponto selecionado");

  search_.signal_changed().connect([this] {
    if (refreshing_context_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  search_.signal_activate().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::dismiss_filters));
  task_mode_filter_.signal_changed().connect([this] {
    if (refreshing_facets_)
      return;
    selected_type_ids_.clear();
    const auto mode = task_mode_filter_.get_active_id().raw();
    if (mode == "characters")
      selected_type_ids_ = {*type_id_for_key("character")};
    else if (mode == "events")
      selected_type_ids_ = {*type_id_for_key("event")};
    else if (mode == "locations")
      selected_type_ids_ = {*type_id_for_key("location")};
    offset_ = 0;
    refresh_entities();
  });
  work_filter_.signal_changed().connect([this] {
    if (refreshing_context_)
      return;
    refresh_context_editorial_nodes();
    offset_ = 0;
    refresh_entities();
  });
  temporal_axis_filter_.signal_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    refresh_context_time_points();
    offset_ = 0;
    refresh_entities();
  });
  temporal_window_filter_.signal_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    refresh_context_time_points();
    offset_ = 0;
    refresh_entities();
  });
  temporal_point_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  temporal_window_end_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_ || refreshing_temporal_filter_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  editorial_node_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  writing_usage_filter_.signal_changed().connect([this] {
    if (refreshing_context_)
      return;
    const bool specific = writing_usage_filter_.get_active_id() == "document";
    writing_document_filter_->set_sensitive(specific);
    if (!specific)
      writing_document_filter_->clear_selection();
    writing_document_filter_->refresh();
    offset_ = 0;
    refresh_entities();
  });
  writing_document_filter_->signal_selection_changed().connect([this] {
    if (refreshing_context_)
      return;
    offset_ = 0;
    refresh_entities();
  });
  writing_library_button_.signal_clicked().connect([this] {
    if (const auto *entity = selected_entity())
      signal_filter_documents_requested_.emit(entity->id);
  });
  clear_context_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::clear_planning_context));
  apply_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::dismiss_filters));
  previous_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::previous_page));
  next_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::next_page));
  add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_entity));
  entity_type_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_entity_type));
  entity_type_combo_.signal_changed().connect([this] {
    const auto id = entity_type_combo_.get_active_id().raw();
    const auto found =
        std::find_if(types_.begin(), types_.end(),
                     [&](const auto &type) { return type.id == id; });
    entity_type_actions_button_.set_sensitive(found != types_.end() &&
                                              !found->is_builtin);
  });
  edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_selected_entity));
  remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_selected_entity));
  axis_combo_.signal_changed().connect([this] {
    if (!refreshing_axis_) {
      auto context = service_.planning_context();
      const auto axis_id = axis_combo_.get_active_id().raw();
      context.fictional_axis_id =
          axis_id.empty() ? std::nullopt : std::optional<std::string>{axis_id};
      context.fictional_time_point_id.reset();
      context.fictional_window_start_time_point_id.reset();
      context.fictional_window_end_time_point_id.reset();
      service_.set_planning_context(std::move(context));
      refresh_time_points();
    }
  });
  axis_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_time_axis));
  axis_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_time_axis));
  axis_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_time_axis));
  point_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_time_point));
  point_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_time_point));
  point_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_time_point));
  event_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_event_occurrence));
  event_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_event_occurrence));
  participant_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_participant));
  participant_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_participant));
  participant_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_participant));
  presence_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_presence));
  presence_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_presence));
  presence_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_presence));
  work_scope_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_work_scope));
  work_scope_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_work_scope));
  work_scope_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_work_scope));
  editorial_reference_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::add_editorial_reference));
  editorial_reference_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_editorial_reference));
  editorial_reference_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_editorial_reference));
  relation_type_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_relation_type));
  relation_type_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_relation_type));
  relation_type_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_relation_type));
  relation_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_relation));
  relation_edit_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::edit_relation));
  relation_remove_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::delete_relation));
  relation_open_counterpart_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::open_relation_counterpart));
  relation_filter_counterpart_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::filter_by_relation_counterpart));
  context_relation_add_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::create_relation));
  narrative_structure_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) {
        signal_status_message_.emit(message);
      });
  narrative_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "narrative")
      remember_navigation();
    show_narrative_page();
  });
  explorer_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "explorer")
      remember_navigation();
    show_explorer_page();
  });
  relations_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "relations")
      remember_navigation();
    show_relations_page();
  });
  time_page_button_.signal_clicked().connect([this] {
    if (page_stack_.get_visible_child_name() != "time")
      remember_navigation();
    show_time_page();
  });
  filters_button_.signal_clicked().connect([this] {
    filters_visible_ = !filters_visible_;
    set_overlay_revealer_open(filters_revealer_, filters_visible_);
    filters_button_.set_label(filters_visible_ ? "Ocultar filtros" : "Filtros");
    if (filters_visible_)
      search_.grab_focus();
  });
  relation_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::toggle_relation_filters));
  relation_search_.signal_search_changed().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::refresh_relation_explorer));
  relation_type_filter_.signal_changed().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::refresh_relation_explorer));
  for (auto *selector : {relation_entity_filter_, relation_time_point_filter_,
                         relation_location_filter_, relation_cause_filter_})
    selector->signal_selection_changed().connect(
        sigc::mem_fun(*this, &PlanningWorkspace::refresh_relation_explorer));
  clear_relation_filters_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::clear_relation_filters));
  apply_relation_filters_button_.signal_clicked().connect([this] {
    refresh_relation_explorer();
    if (relation_filters_visible_)
      toggle_relation_filters();
  });
  back_to_explorer_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::go_back_in_navigation));
  history_back_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::go_back_in_navigation));
  history_forward_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &PlanningWorkspace::go_forward_in_navigation));
  reset();
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
  service_.set_planning_context(std::move(state.context));
  selected_entity_id_.reset();
  selected_time_point_id_.reset();

  // Abrir detalhes nao invalida o catalogo que continua montado na pagina
  // anterior. Voltar para ele deve ser uma troca de pagina, nao uma nova
  // consulta seguida da reconstrucao de todos os cards e facetas.
  if (page_stack_.get_visible_child_name() == "detail" &&
      state.page_name == "explorer") {
    show_explorer_page();
    restoring_navigation_ = false;
    update_navigation_actions();
    return;
  }

  refresh();
  if (state.page_name == "detail" && state.entity_id) {
    selected_entity_id_ = std::move(state.entity_id);
    refresh_details();
    show_entity_detail_page();
  } else if (state.page_name == "time") {
    show_time_page();
    if (state.time_point_id)
      select_time_point(std::move(*state.time_point_id));
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
  narrative_page_button_.grab_focus();
}

void PlanningWorkspace::show_explorer_page() {
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
  explorer_page_button_.grab_focus();
}

void PlanningWorkspace::show_relations_page() {
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
  relations_page_button_.grab_focus();
}

void PlanningWorkspace::show_time_page() {
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
    chips.push_back("Contraparte definida");
  if (context.fictional_axis_id)
    chips.push_back("Eixo ficcional");
  if (context.fictional_time_point_id)
    chips.push_back("Ponto temporal");
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
  if (!service_.current()) {
    reset();
    return;
  }

  const auto visible_page = page_stack_.get_visible_child_name();
  if (visible_page == "narrative") {
    narrative_structure_workspace_.refresh();
    return;
  }
  refresh_types();

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
  selected_entity_id_.reset();
  offset_ = 0;
  types_.clear();
  page_.clear();
  explorer_snapshot_ = {};
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
  temporal_axis_filter_.append("all", "Todos os eixos ficcionais");
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
  counterpart_filter_label_.set_text("Contraparte: qualquer entidade");
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
  inspector_identity_.set_text("Nenhuma entidade selecionada");
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
  show_narrative_page();
}

void PlanningWorkspace::refresh_types() {
  types_ = service_.narrative().entity_types();
  refresh_entity_type_controls();
}

void PlanningWorkspace::refresh_entity_type_controls() {
  const auto selected = entity_type_combo_.get_active_id().raw();
  entity_type_combo_.remove_all();
  for (const auto &type : types_)
    entity_type_combo_.append(
        type.id,
        type.name + (type.is_builtin ? " — interno" : " — do usuário"));
  if (selected.empty() || !entity_type_combo_.set_active_id(selected)) {
    const auto custom =
        std::find_if(types_.begin(), types_.end(),
                     [](const auto &type) { return !type.is_builtin; });
    if (custom != types_.end())
      entity_type_combo_.set_active_id(custom->id);
    else if (!types_.empty())
      entity_type_combo_.set_active(0);
  }
  const auto active = entity_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == active; });
  entity_type_actions_button_.set_sensitive(found != types_.end() &&
                                            !found->is_builtin);
}

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
  temporal_axis_filter_.append("all", "Todos os eixos ficcionais");
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
        node == nodes.end() ? "Unidade editorial indisponível" : node->title);
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
        {{"Abrir entidade", [this, id = entity.id] { select_entity(id); },
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
        "Contraparte: " +
        (related ? related->name : std::string("entidade indisponível")));
    context_expander_.set_expanded(true);
  } else {
    counterpart_filter_label_.set_text("Contraparte: qualquer entidade");
  }
  refresh_details();
}

void PlanningWorkspace::refresh_details() {
  const auto found =
      selected_entity_id_
          ? std::find_if(page_.begin(), page_.end(),
                         [&](const auto &entity) {
                           return entity.id == *selected_entity_id_;
                         })
          : page_.end();
  const bool selected = found != page_.end();
  empty_title_.set_visible(!selected);
  empty_message_.set_visible(!selected);
  edit_button_.set_sensitive(selected);
  remove_button_.set_sensitive(selected);
  entity_actions_button_.set_sensitive(selected);
  if (!selected) {
    entity_name_.set_text("");
    entity_type_.set_text("");
    entity_summary_.set_text("");
    inspector_identity_.set_text("Nenhuma entidade selecionada");
    inspector_dates_.set_text("");
    inspector_relations_.set_text("");
    inspector_presences_.set_text("");
    refresh_temporal_context();
    refresh_editorial_context();
    refresh_writing_context();
    refresh_relations();
    return;
  }
  entity_name_.set_text(found->name);
  entity_type_.set_text(type_name(found->entity_type_id));
  entity_summary_.set_text(found->summary.empty() ? "Sem resumo."
                                                  : found->summary);
  inspector_identity_.set_text("UUID\n" + found->id);
  inspector_dates_.set_text("Criada em " + found->created_at +
                            "\nAtualizada em " + found->updated_at);
  persistence::RelationQuery relations;
  relations.entity_id = found->id;
  relations.limit = 200;
  inspector_relations_.set_text(
      "Relações (até 200): " +
      std::to_string(service_.narrative().relations(relations).size()));
  persistence::PresenceQuery presences;
  presences.entity_id = found->id;
  presences.limit = 100;
  inspector_presences_.set_text(
      "Presenças registradas (até 100): " +
      std::to_string(service_.planning().presences(presences).size()));
  refresh_temporal_context();
  refresh_editorial_context();
  refresh_writing_context();
  refresh_relations();
}

void PlanningWorkspace::refresh_axes() {
  const std::string previous = axis_combo_.get_active_id().raw();
  const auto shared_axis = service_.planning_context().fictional_axis_id;
  axes_ = service_.planning().time_axes();
  refreshing_axis_ = true;
  axis_combo_.remove_all();
  for (const auto &axis : axes_)
    axis_combo_.append(axis.id,
                       axis.name + (axis.is_default ? " — principal" : ""));
  if ((!shared_axis || !axis_combo_.set_active_id(*shared_axis)) &&
      (previous.empty() || !axis_combo_.set_active_id(previous))) {
    const auto preferred =
        std::find_if(axes_.begin(), axes_.end(),
                     [](const auto &axis) { return axis.is_default; });
    if (preferred != axes_.end())
      axis_combo_.set_active_id(preferred->id);
    else if (!axes_.empty())
      axis_combo_.set_active_id(axes_.front().id);
  }
  refreshing_axis_ = false;
  axis_edit_button_.set_sensitive(!axes_.empty());
  axis_actions_button_.set_sensitive(!axes_.empty());
  const std::string active = axis_combo_.get_active_id().raw();
  const auto selected_axis =
      std::find_if(axes_.begin(), axes_.end(),
                   [&](const auto &axis) { return axis.id == active; });
  axis_remove_button_.set_sensitive(selected_axis != axes_.end() &&
                                    !selected_axis->is_default);
  point_add_button_.set_sensitive(!active.empty());
  refresh_time_points();
}

void PlanningWorkspace::refresh_time_points() {
  selected_time_point_id_.reset();
  time_points_.clear();
  while (auto *child = point_list_.get_first_child())
    point_list_.remove(*child);
  const std::string axis_id = axis_combo_.get_active_id().raw();
  if (!service_.current() || axis_id.empty()) {
    point_edit_button_.set_sensitive(false);
    point_remove_button_.set_sensitive(false);
    point_actions_button_.set_sensitive(false);
    return;
  }
  persistence::PlanningQuery query;
  query.limit = 500;
  time_points_ = service_.planning().time_points(axis_id, query);
  for (const auto &point : time_points_) {
    auto *button = Gtk::make_managed<Gtk::Button>(
        std::to_string(point.ordinal) + " — " + point.label);
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = point.id] { select_time_point(id); });
    attach_context_menu(*button,
                        {{"Usar como recorte temporal",
                          [this, id = point.id] { select_time_point(id); },
                          "object-select-symbolic"},
                         {"Editar ponto",
                          [this, id = point.id] {
                            select_time_point(id);
                            edit_time_point();
                          },
                          "document-edit-symbolic"},
                         {"Remover ponto",
                          [this, id = point.id] {
                            select_time_point(id);
                            delete_time_point();
                          },
                          "user-trash-symbolic", true, true, true}});
    point_list_.append(*button);
  }
  const auto selected_axis =
      std::find_if(axes_.begin(), axes_.end(),
                   [&](const auto &axis) { return axis.id == axis_id; });
  axis_remove_button_.set_sensitive(selected_axis != axes_.end() &&
                                    !selected_axis->is_default);
  point_edit_button_.set_sensitive(false);
  point_remove_button_.set_sensitive(false);
  point_actions_button_.set_sensitive(false);
  refresh_time_overview();
}

void PlanningWorkspace::refresh_time_overview() {
  while (auto *child = time_events_list_.get_first_child())
    time_events_list_.remove(*child);
  while (auto *child = time_presences_list_.get_first_child())
    time_presences_list_.remove(*child);
  if (!service_.current()) {
    time_events_title_.set_text("Acontecimentos no recorte");
    time_presences_title_.set_text("Presenças no recorte");
    return;
  }
  try {
    const auto view =
        service_.planning().temporal_view(service_.planning_context());
    const auto &events = view.timeline.events;
    const auto &presences = view.timeline.presences;
    time_events_title_.set_text("Acontecimentos no recorte (" +
                                std::to_string(view.matching.events) + ")");
    for (const auto &event : events) {
      auto *button = Gtk::make_managed<Gtk::Button>(
          event.entity_name + " — ordinal " + std::to_string(event.ordinal));
      button->set_halign(Gtk::Align::FILL);
      button->set_tooltip_text(event.description.empty()
                                   ? event.inclusion_reason
                                   : event.description);
      button->signal_clicked().connect(
          [this, id = event.entity_id] { reveal_entity(id); });
      time_events_list_.append(*button);
    }
    if (events.empty()) {
      auto *empty = Gtk::make_managed<Gtk::Label>(
          "Nenhum Acontecimento corresponde ao recorte compartilhado.");
      empty->set_xalign(0.0F);
      empty->add_css_class("dim-label");
      time_events_list_.append(*empty);
    }

    time_presences_title_.set_text("Presenças no recorte (" +
                                   std::to_string(view.matching.presences) +
                                   ")");
    for (const auto &presence : presences) {
      auto *button =
          Gtk::make_managed<Gtk::Button>("Sujeito: " + presence.entity_name +
                                         " • Local: " + presence.location_name);
      button->set_halign(Gtk::Align::FILL);
      button->set_tooltip_text(presence.description.empty()
                                   ? presence.inclusion_reason
                                   : presence.description);
      button->signal_clicked().connect(
          [this, id = presence.entity_id] { reveal_entity(id); });
      time_presences_list_.append(*button);
    }
    if (presences.empty()) {
      auto *empty = Gtk::make_managed<Gtk::Label>(
          "Nenhuma presença corresponde ao recorte compartilhado.");
      empty->set_xalign(0.0F);
      empty->add_css_class("dim-label");
      time_presences_list_.append(*empty);
    }
  } catch (const std::exception &error) {
    time_events_title_.set_text("Acontecimentos no recorte");
    time_presences_title_.set_text("Presenças no recorte");
    auto *failure = Gtk::make_managed<Gtk::Label>(
        "Não foi possível projetar o recorte: " + std::string(error.what()));
    failure->set_xalign(0.0F);
    failure->add_css_class("dim-label");
    time_events_list_.append(*failure);
  }
}

const project::NarrativeEntity *PlanningWorkspace::selected_entity() const {
  if (!selected_entity_id_)
    return nullptr;
  const auto found =
      std::find_if(page_.begin(), page_.end(), [&](const auto &value) {
        return value.id == *selected_entity_id_;
      });
  return found == page_.end() ? nullptr : &*found;
}

std::optional<std::string>
PlanningWorkspace::type_id_for_key(const std::string &key) const {
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.key == key; });
  if (found == types_.end())
    return std::nullopt;
  return found->id;
}

void PlanningWorkspace::refresh_temporal_context() {
  const auto *entity = selected_entity();
  const auto event_type = type_id_for_key("event");
  const bool is_event =
      entity && event_type && entity->entity_type_id == *event_type;
  event_section_.set_visible(is_event);
  for (auto *widget : {static_cast<Gtk::Widget *>(&event_separator_),
                       static_cast<Gtk::Widget *>(&event_title_),
                       static_cast<Gtk::Widget *>(&event_status_),
                       static_cast<Gtk::Widget *>(&event_toolbar_),
                       static_cast<Gtk::Widget *>(&participants_title_),
                       static_cast<Gtk::Widget *>(&participant_toolbar_),
                       static_cast<Gtk::Widget *>(&participant_list_)})
    widget->set_visible(is_event);
  const bool has_entity = entity != nullptr;
  presence_section_.set_visible(has_entity);
  for (auto *widget : {static_cast<Gtk::Widget *>(&presence_separator_),
                       static_cast<Gtk::Widget *>(&presence_title_),
                       static_cast<Gtk::Widget *>(&presence_toolbar_),
                       static_cast<Gtk::Widget *>(&presence_list_)})
    widget->set_visible(has_entity);

  participants_.clear();
  presences_.clear();
  occurrence_.reset();
  selected_participant_id_.reset();
  selected_presence_id_.reset();
  while (auto *child = participant_list_.get_first_child())
    participant_list_.remove(*child);
  while (auto *child = presence_list_.get_first_child())
    presence_list_.remove(*child);
  participant_edit_button_.set_sensitive(false);
  participant_remove_button_.set_sensitive(false);
  participant_actions_button_.set_sensitive(false);
  presence_edit_button_.set_sensitive(false);
  presence_remove_button_.set_sensitive(false);
  presence_actions_button_.set_sensitive(false);
  presence_add_button_.set_sensitive(has_entity && !axes_.empty());
  if (!entity)
    return;

  if (is_event) {
    occurrence_ = service_.planning().event_occurrence_for_entity(entity->id);
    event_edit_button_.set_label(occurrence_ ? "Editar ocorrência" : "Situar");
    event_edit_button_.set_sensitive(!axes_.empty());
    event_remove_button_.set_sensitive(occurrence_.has_value());
    event_actions_button_.set_sensitive(occurrence_.has_value());
    participant_add_button_.set_sensitive(occurrence_.has_value());
    if (occurrence_) {
      const auto point =
          service_.planning().time_point(occurrence_->time_point_id);
      const auto point_description =
          point
              ? point->label + " (ordem " + std::to_string(point->ordinal) + ")"
              : occurrence_->time_point_id;
      event_status_.set_text("Situado em " + point_description +
                             (occurrence_->description.empty()
                                  ? ""
                                  : "\n" + occurrence_->description));
      persistence::PlanningQuery query;
      query.limit = 100;
      participants_ =
          service_.planning().event_participations(occurrence_->id, query);
      for (const auto &participant : participants_) {
        const auto referenced =
            service_.narrative().entity(participant.participant_entity_id);
        const auto name = referenced ? referenced->name : "Entidade ausente";
        auto *button =
            Gtk::make_managed<Gtk::Button>(name + " — " + participant.role);
        button->set_halign(Gtk::Align::FILL);
        button->signal_clicked().connect(
            [this, id = participant.id] { select_participant(id); });
        attach_context_menu(*button,
                            {{"Editar participante",
                              [this, id = participant.id] {
                                select_participant(id);
                                edit_participant();
                              },
                              "document-edit-symbolic"},
                             {"Remover participante",
                              [this, id = participant.id] {
                                select_participant(id);
                                delete_participant();
                              },
                              "user-trash-symbolic", true, true, true}});
        participant_list_.append(*button);
      }
    } else {
      event_status_.set_text(
          "Este acontecimento ainda não foi situado no tempo ficcional.");
    }
  }

  persistence::PresenceQuery presence_query;
  presence_query.entity_id = entity->id;
  presence_query.limit = 100;
  presences_ = service_.planning().presences(presence_query);
  for (const auto &presence : presences_) {
    const auto location =
        service_.narrative().entity(presence.location_entity_id);
    const auto name = location ? location->name : "Local ausente";
    auto *button = Gtk::make_managed<Gtk::Button>(
        name + (presence.end_time_point_id ? " — intervalo fechado"
                                           : " — intervalo aberto"));
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = presence.id] { select_presence(id); });
    attach_context_menu(*button, {{"Editar presença",
                                   [this, id = presence.id] {
                                     select_presence(id);
                                     edit_presence();
                                   },
                                   "document-edit-symbolic"},
                                  {"Remover presença",
                                   [this, id = presence.id] {
                                     select_presence(id);
                                     delete_presence();
                                   },
                                   "user-trash-symbolic", true, true, true}});
    presence_list_.append(*button);
  }
}

std::string PlanningWorkspace::type_name(const std::string &id) const {
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == id; });
  return found == types_.end() ? "Tipo desconhecido" : found->name;
}

void PlanningWorkspace::select_entity(std::string id) {
  // A única semântica do card é navegação. O id fica ativo somente enquanto a
  // página de detalhe estiver aberta, onde Editar e Remover são inequívocos.
  remember_navigation();
  selected_entity_id_ = std::move(id);
  refresh_details();
  show_entity_detail_page();
  signal_status_message_.emit("Entidade narrativa selecionada");
}

void PlanningWorkspace::reveal_entity(const std::string &id) {
  if (!service_.current())
    return;
  try {
    const auto entity = service_.narrative().entity(id);
    if (!entity)
      throw std::runtime_error("A entidade de origem não existe mais");
    remember_navigation();
    auto context = service_.planning_context();
    // Abrir uma origem conserva Obra, apresentação editorial e recorte
    // temporal. Só os filtros que competiriam diretamente com a entidade são
    // limpos, permitindo voltar à leitura sem perder o caminho contextual.
    context.search = entity->name;
    context.entity_type_ids.clear();
    context.relation_type_ids.clear();
    context.related_entity_id.reset();
    context.offset = 0;
    service_.set_planning_context(std::move(context));
    offset_ = 0;
    refresh();
    const auto found =
        std::find_if(page_.begin(), page_.end(),
                     [&](const auto &value) { return value.id == entity->id; });
    if (found == page_.end())
      throw std::runtime_error(
          "A entidade de origem não foi encontrada na lista atual");
    restoring_navigation_ = true;
    select_entity(entity->id);
    restoring_navigation_ = false;
    signal_status_message_.emit("Origem aberta no Planejamento");
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir a origem", error);
  }
}

void PlanningWorkspace::reveal_time_point(const std::string &id) {
  if (!service_.current())
    return;
  try {
    const auto point = service_.planning().time_point(id);
    if (!point)
      throw std::runtime_error("O ponto temporal de origem não existe mais");
    remember_navigation();
    auto context = service_.planning_context();
    context.fictional_axis_id = point->axis_id;
    context.fictional_time_point_id = point->id;
    context.fictional_window_start_time_point_id.reset();
    context.fictional_window_end_time_point_id.reset();
    service_.set_planning_context(std::move(context));
    refresh();
    axis_combo_.set_active_id(point->axis_id);
    refresh_time_points();
    select_time_point(point->id);
    show_time_page();
    signal_status_message_.emit("Ponto temporal aberto no Planejamento");
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir a origem", error);
  }
}

Gtk::Window *PlanningWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void PlanningWorkspace::show_error(const Glib::ustring &title,
                                   const std::exception &error) {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog(*owner, error.what(), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
}

void PlanningWorkspace::create_entity_type() {
  auto *owner = owner_window();
  if (!owner || !service_.current())
    return;
  auto *dialog = new OverlayDialog("Novo tipo de entidade", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *key = Gtk::make_managed<Gtk::Entry>();
  key->set_placeholder_text("Chave técnica: criatura-marinha");
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome exibido");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição e uso esperado");
  form->set_margin(16);
  append_labeled_form_field(
      *form, "Chave técnica", *key,
      "Identificador imutável em minúsculas; tipos do usuário não recebem "
      "capacidades internas por semelhança de nome.");
  append_labeled_form_field(*form, "Nome do tipo", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, key, name, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto created = service_.narrative().create_entity_type(
                key->get_text(), name->get_text(), description->get_text());
            refresh_types();
            refresh_context_filters();
            refresh_entities();
            entity_type_combo_.set_active_id(created.id);
            signal_status_message_.emit("Tipo de entidade criado");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o tipo de entidade", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_entity_type() {
  const auto id = entity_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == id; });
  auto *owner = owner_window();
  if (!owner || found == types_.end())
    return;
  if (found->is_builtin) {
    show_error("Tipo interno protegido",
               std::runtime_error(
                   "Tipos internos garantem integrações do sistema e não "
                   "podem ser alterados. Crie um tipo do usuário para outro "
                   "vocabulário."));
    return;
  }
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar tipo de entidade", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *key = Gtk::make_managed<Gtk::Label>("Chave imutável: " + original.key);
  key->set_halign(Gtk::Align::START);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  form->append(*key);
  append_labeled_form_field(*form, "Nome do tipo", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, name,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.name = name->get_text();
        original.description = description->get_text();
        const auto updated = service_.narrative().update_entity_type(original);
        refresh_types();
        refresh_context_filters();
        refresh_entities();
        entity_type_combo_.set_active_id(updated.id);
        signal_status_message_.emit("Tipo de entidade atualizado");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o tipo de entidade", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_entity_type() {
  const auto id = entity_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(types_.begin(), types_.end(),
                   [&](const auto &type) { return type.id == id; });
  auto *owner = owner_window();
  if (!owner || found == types_.end() || found->is_builtin)
    return;
  auto *dialog = new OverlayDialog(*owner, "Remover este tipo de entidade?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "Tipos usados por entidades são protegidos. Reclassifique ou remova "
      "essas entidades antes de remover o tipo.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().delete_entity_type(id);
        selected_type_ids_.erase(std::remove(selected_type_ids_.begin(),
                                             selected_type_ids_.end(), id),
                                 selected_type_ids_.end());
        refresh_types();
        refresh_context_filters();
        refresh_entities();
        signal_status_message_.emit("Tipo de entidade removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o tipo de entidade", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::create_entity() {
  auto *owner = owner_window();
  if (types_.empty() && service_.current())
    refresh_types();
  if (!owner || types_.empty())
    return;
  auto *dialog = new OverlayDialog("Nova entidade narrativa", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : types_)
    type->append(value.id, value.name);
  type->set_active(0);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome (obrigatório)");
  auto *summary = Gtk::make_managed<Gtk::Entry>();
  summary->set_placeholder_text("Resumo");
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo da entidade", *type,
                            "Escolha a categoria narrativa da entidade.");
  append_labeled_form_field(*form, "Nome da entidade", *name,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Resumo", *summary,
                            "Breve descrição da entidade.");
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, type, name,
                                     summary](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        const auto created = service_.narrative().create_entity(
            type->get_active_id().raw(), name->get_text(), summary->get_text());
        selected_entity_id_ = created.id;
        offset_ = 0;
        refresh_entities();
        if (std::none_of(page_.begin(), page_.end(), [&](const auto &value) {
              return value.id == created.id;
            }))
          page_.push_back(created);
        refresh_details();
        show_entity_detail_page();
        signal_status_message_.emit("Entidade narrativa criada e salva");
      } catch (const std::exception &error) {
        show_error("Não foi possível criar a entidade", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::edit_selected_entity() {
  if (!selected_entity_id_)
    return;
  const auto found =
      std::find_if(page_.begin(), page_.end(), [&](const auto &value) {
        return value.id == *selected_entity_id_;
      });
  auto *owner = owner_window();
  if (!owner || found == page_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar entidade narrativa", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : types_)
    type->append(value.id, value.name);
  type->set_active_id(original.entity_type_id);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *summary = Gtk::make_managed<Gtk::Entry>();
  summary->set_text(original.summary);
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo da entidade", *type,
                            "Escolha a categoria narrativa da entidade.");
  append_labeled_form_field(*form, "Nome da entidade", *name,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Resumo", *summary,
                            "Breve descrição da entidade.");
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, type, name,
                                     summary](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.entity_type_id = type->get_active_id().raw();
        original.name = name->get_text();
        original.summary = summary->get_text();
        static_cast<void>(service_.narrative().update_entity(original));
        refresh_entities();
        signal_status_message_.emit("Entidade narrativa atualizada e salva");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a entidade", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_selected_entity() {
  if (!selected_entity_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_entity_id_;
  auto *dialog = new OverlayDialog(*owner, "Remover esta entidade narrativa?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "Relações, ocorrências e presenças vinculadas protegem a entidade contra "
      "remoção acidental.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().delete_entity(id);
        selected_entity_id_.reset();
        refresh_entities();
        show_explorer_page();
        signal_status_message_.emit("Entidade narrativa removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a entidade", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
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

std::int64_t PlanningWorkspace::parse_ordinal(const Glib::ustring &text) {
  const auto source = text.raw();
  std::int64_t value{};
  const auto result =
      std::from_chars(source.data(), source.data() + source.size(), value);
  if (source.empty() || result.ec != std::errc{} ||
      result.ptr != source.data() + source.size())
    throw std::runtime_error("A ordem temporal precisa ser um número inteiro");
  return value;
}

void PlanningWorkspace::create_time_axis() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog("Novo eixo de tempo ficcional", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome do eixo (obrigatório)");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  form->set_margin(16);
  append_labeled_form_field(*form, "Nome do eixo", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, name, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto axis = service_.planning().create_time_axis(
                name->get_text(), description->get_text());
            refresh_axes();
            axis_combo_.set_active_id(axis.id);
            refresh_time_points();
            signal_status_message_.emit("Eixo de tempo ficcional criado");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o eixo temporal", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_time_axis() {
  const std::string id = axis_combo_.get_active_id().raw();
  const auto found =
      std::find_if(axes_.begin(), axes_.end(),
                   [&](const auto &axis) { return axis.id == id; });
  auto *owner = owner_window();
  if (!owner || found == axes_.end())
    return;
  auto original = *found;
  auto *dialog =
      new OverlayDialog("Editar eixo de tempo ficcional", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Nome do eixo", *name, "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, original, name, description](int response) mutable {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            original.name = name->get_text();
            original.description = description->get_text();
            static_cast<void>(service_.planning().update_time_axis(original));
            refresh_axes();
            axis_combo_.set_active_id(original.id);
            signal_status_message_.emit("Eixo de tempo ficcional atualizado");
          } catch (const std::exception &error) {
            show_error("Não foi possível editar o eixo temporal", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::delete_time_axis() {
  const std::string id = axis_combo_.get_active_id().raw();
  auto *owner = owner_window();
  if (!owner || id.empty())
    return;
  auto *dialog = new OverlayDialog(
      *owner, "Remover este eixo de tempo ficcional?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "O eixo principal e eixos com pontos vinculados são protegidos.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().delete_time_axis(id);
        refresh_axes();
        signal_status_message_.emit("Eixo de tempo ficcional removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o eixo temporal", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::select_time_point(std::string id) {
  selected_time_point_id_ = std::move(id);
  point_edit_button_.set_sensitive(true);
  point_remove_button_.set_sensitive(true);
  point_actions_button_.set_sensitive(true);
  signal_status_message_.emit("Ponto de tempo ficcional selecionado");
}

void PlanningWorkspace::create_time_point() {
  const std::string axis_id = axis_combo_.get_active_id().raw();
  auto *owner = owner_window();
  if (!owner || axis_id.empty())
    return;
  auto *dialog =
      new OverlayDialog("Novo ponto de tempo ficcional", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *ordinal = Gtk::make_managed<Gtk::Entry>();
  ordinal->set_placeholder_text("Ordem inteira, por exemplo 1000");
  auto *label = Gtk::make_managed<Gtk::Entry>();
  label->set_placeholder_text("Rótulo (obrigatório)");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  form->set_margin(16);
  append_labeled_form_field(*form, "Ordem temporal", *ordinal,
                            "Número inteiro; por exemplo, 1000.");
  append_labeled_form_field(*form, "Rótulo do ponto", *label,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, axis_id, ordinal, label, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto point = service_.planning().create_time_point(
                axis_id, parse_ordinal(ordinal->get_text()), label->get_text(),
                description->get_text());
            refresh_time_points();
            select_time_point(point.id);
            refresh_temporal_context();
            signal_status_message_.emit("Ponto de tempo ficcional criado");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o ponto temporal", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_time_point() {
  if (!selected_time_point_id_)
    return;
  const auto found = std::find_if(
      time_points_.begin(), time_points_.end(),
      [&](const auto &point) { return point.id == *selected_time_point_id_; });
  auto *owner = owner_window();
  if (!owner || found == time_points_.end())
    return;
  auto original = *found;
  auto *dialog =
      new OverlayDialog("Editar ponto de tempo ficcional", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *axis = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : axes_)
    axis->append(value.id, value.name);
  axis->set_active_id(original.axis_id);
  auto *ordinal = Gtk::make_managed<Gtk::Entry>();
  ordinal->set_text(std::to_string(original.ordinal));
  auto *label = Gtk::make_managed<Gtk::Entry>();
  label->set_text(original.label);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Eixo de tempo ficcional", *axis);
  append_labeled_form_field(*form, "Ordem temporal", *ordinal,
                            "Número inteiro.");
  append_labeled_form_field(*form, "Rótulo do ponto", *label,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, axis, ordinal,
                                     label, description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.axis_id = axis->get_active_id().raw();
        original.ordinal = parse_ordinal(ordinal->get_text());
        original.label = label->get_text();
        original.description = description->get_text();
        static_cast<void>(service_.planning().update_time_point(original));
        refresh_axes();
        axis_combo_.set_active_id(original.axis_id);
        refresh_time_points();
        signal_status_message_.emit("Ponto de tempo ficcional atualizado");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o ponto temporal", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_time_point() {
  if (!selected_time_point_id_)
    return;
  const auto id = *selected_time_point_id_;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(
      *owner, "Remover este ponto de tempo ficcional?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "Ocorrências e presenças vinculadas protegem o ponto contra remoção.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().delete_time_point(id);
        refresh_time_points();
        refresh_temporal_context();
        signal_status_message_.emit("Ponto de tempo ficcional removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o ponto temporal", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

IncrementalSelector *PlanningWorkspace::make_time_point_selector() const {
  return Gtk::make_managed<IncrementalSelector>(
      "Pesquisar ponto temporal",
      [this](const std::string &search, std::size_t limit) {
        std::vector<IncrementalSelection> result;
        for (const auto &axis : axes_) {
          persistence::PlanningQuery query;
          query.search = search;
          query.limit = std::min(limit - result.size(), std::size_t{50});
          if (query.limit == 0)
            break;
          for (const auto &point :
               service_.planning().time_points(axis.id, query)) {
            result.push_back({point.id,
                              axis.name + " — " +
                                  std::to_string(point.ordinal) + " — " +
                                  point.label,
                              point.description});
          }
        }
        return result;
      },
      "Pesquise ou escolha um ponto temporal");
}

IncrementalSelector *PlanningWorkspace::make_entity_selector(
    const std::optional<std::string> &type_id, bool exclude_events,
    const std::optional<std::string> &exclude_id) const {
  const auto event_type = type_id_for_key("event");
  return Gtk::make_managed<IncrementalSelector>(
      "Pesquisar entidade",
      [this, type_id, exclude_events, exclude_id,
       event_type](const std::string &search, std::size_t limit) {
        persistence::EntityQuery query;
        query.search = search;
        query.limit = std::min(limit, std::size_t{50});
        if (type_id)
          query.entity_type_ids = {*type_id};
        std::vector<IncrementalSelection> result;
        for (const auto &entity : service_.narrative().entities(query)) {
          if (exclude_id && entity.id == *exclude_id)
            continue;
          if (exclude_events && event_type &&
              entity.entity_type_id == *event_type)
            continue;
          result.push_back(
              {entity.id, entity.name, type_name(entity.entity_type_id)});
        }
        return result;
      },
      "Pesquise ou escolha uma entidade");
}

void PlanningWorkspace::edit_event_occurrence() {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!owner || !entity)
    return;
  auto original = occurrence_;
  auto *dialog = new OverlayDialog(
      original ? "Editar ocorrência" : "Situar acontecimento", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *point = make_time_point_selector();
  if (original)
    point->set_selected(original->time_point_id, "Ponto temporal atual");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição da ocorrência");
  if (original)
    description->set_text(original->description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Ponto de tempo ficcional", *point,
                            "Pesquise ou selecione o ponto da ocorrência.");
  append_labeled_form_field(*form, "Descrição da ocorrência", *description);
  dialog->get_content_area()->append(*form);
  const auto entity_id = entity->id;
  dialog->signal_response().connect([this, dialog, original, entity_id, point,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!point->selected_id())
          throw std::runtime_error("Crie e selecione um ponto temporal");
        if (original) {
          original->time_point_id = *point->selected_id();
          original->description = description->get_text();
          static_cast<void>(
              service_.planning().update_event_occurrence(*original));
        } else {
          static_cast<void>(service_.planning().place_event(
              entity_id, *point->selected_id(), description->get_text()));
        }
        refresh_details();
        signal_status_message_.emit("Ocorrência do acontecimento salva");
      } catch (const std::exception &error) {
        show_error("Não foi possível salvar a ocorrência", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_event_occurrence() {
  if (!occurrence_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = occurrence_->id;
  auto *dialog = new OverlayDialog(
      *owner, "Remover a ocorrência deste acontecimento?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "As participações desta ocorrência também serão removidas.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().remove_event_occurrence(id);
        refresh_details();
        signal_status_message_.emit("Ocorrência removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a ocorrência", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::select_participant(std::string id) {
  selected_participant_id_ = std::move(id);
  participant_edit_button_.set_sensitive(true);
  participant_remove_button_.set_sensitive(true);
  participant_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_participant() {
  auto *owner = owner_window();
  if (!owner || !occurrence_)
    return;
  auto *dialog = new OverlayDialog("Adicionar participante", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Adicionar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *entity = make_entity_selector(std::nullopt, true);
  auto *role = Gtk::make_managed<Gtk::Entry>();
  role->set_placeholder_text("Papel no acontecimento (obrigatório)");
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas");
  form->set_margin(16);
  append_labeled_form_field(*form, "Entidade participante", *entity,
                            "Pesquise e escolha a entidade participante.");
  append_labeled_form_field(*form, "Papel no acontecimento", *role,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Notas", *notes);
  dialog->get_content_area()->append(*form);
  const auto occurrence_id = occurrence_->id;
  dialog->signal_response().connect(
      [this, dialog, occurrence_id, entity, role, notes](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (!entity->selected_id())
              throw std::runtime_error("Selecione uma entidade participante");
            static_cast<void>(service_.planning().add_participant(
                occurrence_id, *entity->selected_id(), role->get_text(),
                notes->get_text()));
            refresh_details();
            signal_status_message_.emit("Participante adicionado");
          } catch (const std::exception &error) {
            show_error("Não foi possível adicionar o participante", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_participant() {
  if (!selected_participant_id_)
    return;
  const auto found = std::find_if(
      participants_.begin(), participants_.end(),
      [&](const auto &value) { return value.id == *selected_participant_id_; });
  auto *owner = owner_window();
  if (!owner || found == participants_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar participante", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *entity = make_entity_selector(std::nullopt, true);
  entity->set_selected(original.participant_entity_id, "Participante atual");
  auto *role = Gtk::make_managed<Gtk::Entry>();
  role->set_text(original.role);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_text(original.notes);
  form->set_margin(16);
  append_labeled_form_field(*form, "Entidade participante", *entity,
                            "Pesquise e escolha a entidade participante.");
  append_labeled_form_field(*form, "Papel no acontecimento", *role,
                            "Campo obrigatório.");
  append_labeled_form_field(*form, "Notas", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, entity, role,
                                     notes](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!entity->selected_id())
          throw std::runtime_error("Selecione uma entidade participante");
        original.participant_entity_id = *entity->selected_id();
        original.role = role->get_text();
        original.notes = notes->get_text();
        static_cast<void>(service_.planning().update_participation(original));
        refresh_details();
        signal_status_message_.emit("Participação atualizada");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a participação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_participant() {
  if (!selected_participant_id_)
    return;
  const auto id = *selected_participant_id_;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(
      *owner, "Remover esta participação do acontecimento?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().remove_participation(id);
        refresh_details();
        signal_status_message_.emit("Participante removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o participante", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::select_presence(std::string id) {
  selected_presence_id_ = std::move(id);
  presence_edit_button_.set_sensitive(true);
  presence_remove_button_.set_sensitive(true);
  presence_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_presence() {
  const auto *current = selected_entity();
  const auto location_type = type_id_for_key("location");
  auto *owner = owner_window();
  if (!owner || !current || !location_type)
    return;
  auto *dialog = new OverlayDialog("Nova presença", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *location = make_entity_selector(location_type, false);
  auto *start = make_time_point_selector();
  auto *end = make_time_point_selector();
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição da presença");
  form->set_margin(16);
  append_labeled_form_field(*form, "Local", *location,
                            "Pesquise e escolha uma entidade Local.");
  append_labeled_form_field(*form, "Início", *start,
                            "Ponto temporal em que a presença começa.");
  append_labeled_form_field(
      *form, "Fim opcional", *end,
      "Deixe sem seleção se a presença ainda estiver ativa.");
  append_labeled_form_field(*form, "Descrição da presença", *description);
  dialog->get_content_area()->append(*form);
  const auto entity_id = current->id;
  dialog->signal_response().connect([this, dialog, entity_id, location, start,
                                     end, description](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!location->selected_id())
          throw std::runtime_error("Crie e selecione uma entidade Local");
        if (!start->selected_id())
          throw std::runtime_error("Crie e selecione um ponto inicial");
        const auto optional_end = end->selected_id();
        static_cast<void>(service_.planning().create_presence(
            entity_id, *location->selected_id(), *start->selected_id(),
            optional_end, description->get_text()));
        refresh_details();
        signal_status_message_.emit("Presença narrativa criada");
      } catch (const std::exception &error) {
        show_error("Não foi possível criar a presença", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::edit_presence() {
  if (!selected_presence_id_)
    return;
  const auto found = std::find_if(
      presences_.begin(), presences_.end(),
      [&](const auto &value) { return value.id == *selected_presence_id_; });
  const auto location_type = type_id_for_key("location");
  auto *owner = owner_window();
  if (!owner || found == presences_.end() || !location_type)
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar presença", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *location = make_entity_selector(location_type, false);
  location->set_selected(original.location_entity_id, "Local atual");
  auto *start = make_time_point_selector();
  start->set_selected(original.start_time_point_id, "Início atual");
  auto *end = make_time_point_selector();
  if (original.end_time_point_id)
    end->set_selected(*original.end_time_point_id, "Fim atual");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Local", *location,
                            "Pesquise e escolha uma entidade Local.");
  append_labeled_form_field(*form, "Início", *start,
                            "Ponto temporal em que a presença começa.");
  append_labeled_form_field(
      *form, "Fim opcional", *end,
      "Deixe sem seleção se a presença ainda estiver ativa.");
  append_labeled_form_field(*form, "Descrição da presença", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, location, start,
                                     end, description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (!location->selected_id())
          throw std::runtime_error("Crie e selecione uma entidade Local");
        if (!start->selected_id())
          throw std::runtime_error("Crie e selecione um ponto inicial");
        original.location_entity_id = *location->selected_id();
        original.start_time_point_id = *start->selected_id();
        original.end_time_point_id = end->selected_id();
        original.description = description->get_text();
        static_cast<void>(service_.planning().update_presence(original));
        refresh_details();
        signal_status_message_.emit("Presença narrativa atualizada");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a presença", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_presence() {
  if (!selected_presence_id_)
    return;
  const auto id = *selected_presence_id_;
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog(*owner, "Remover esta presença narrativa?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.planning().remove_presence(id);
        refresh_details();
        signal_status_message_.emit("Presença narrativa removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a presença", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

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
        verb + " — " + (other ? other->name : "Entidade ausente"));
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = relation.id] { select_relation(id); });
    attach_context_menu(*button, {{"Abrir contraparte",
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
                                  {"Filtrar por contraparte",
                                   [this, id = relation.id] {
                                     select_relation(id);
                                     filter_by_relation_counterpart();
                                   },
                                   "edit-find-symbolic"}});
    relation_list_.append(*button);
  }
}

void PlanningWorkspace::refresh_relation_type_controls() {
  const std::string selected = relation_type_combo_.get_active_id().raw();
  relation_types_ = service_.narrative().relation_types();
  relation_type_combo_.remove_all();
  for (const auto &type : relation_types_)
    relation_type_combo_.append(type.id, type.name);
  if (!selected.empty() && relation_type_combo_.set_active_id(selected)) {
  } else if (!relation_types_.empty()) {
    relation_type_combo_.set_active(0);
  }
  const bool has_type = !relation_type_combo_.get_active_id().empty();
  relation_type_edit_button_.set_sensitive(has_type);
  relation_type_remove_button_.set_sensitive(has_type);
  relation_type_actions_button_.set_sensitive(has_type);
  relation_add_button_.set_sensitive(has_type);
}

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
  return entity ? entity->name : "Entidade ausente";
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
           {"Remover relação",
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
  signal_status_message_.emit("Contraparte aplicada ao contexto do explorador");
}

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

void PlanningWorkspace::create_relation_type() {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog = new OverlayDialog("Novo tipo de relação", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *key = Gtk::make_managed<Gtk::Entry>();
  key->set_placeholder_text("Chave técnica: vive-em");
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_placeholder_text("Nome no sentido origem → destino");
  auto *inverse = Gtk::make_managed<Gtk::Entry>();
  inverse->set_placeholder_text("Nome inverso; dispensável se simétrica");
  auto *direction = Gtk::make_managed<Gtk::ComboBoxText>();
  direction->append("directed", "Direcional");
  direction->append("symmetric", "Simétrica");
  direction->set_active_id("directed");
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição");
  form->set_margin(16);
  append_labeled_form_field(*form, "Chave técnica", *key,
                            "Identificador estável, por exemplo vive-em.");
  append_labeled_form_field(*form, "Nome de origem para destino", *name);
  append_labeled_form_field(*form, "Nome inverso", *inverse,
                            "Pode ficar vazio para relações simétricas.");
  append_labeled_form_field(*form, "Direcionalidade", *direction);
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, key, name, inverse, direction, description](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            const auto type = service_.narrative().create_relation_type(
                key->get_text(), name->get_text(), inverse->get_text(),
                project::relation_directionality_from_string(
                    direction->get_active_id().raw()),
                description->get_text());
            refresh_relation_type_controls();
            refresh_relation_filter_options();
            refresh_relation_explorer();
            refresh_context_filters();
            refresh_entities();
            relation_type_combo_.set_active_id(type.id);
            signal_status_message_.emit("Tipo de relação criado");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar o tipo de relação", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_relation_type() {
  const std::string id = relation_type_combo_.get_active_id().raw();
  const auto found =
      std::find_if(relation_types_.begin(), relation_types_.end(),
                   [&](const auto &value) { return value.id == id; });
  auto *owner = owner_window();
  if (!owner || found == relation_types_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar tipo de relação", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text(original.name);
  auto *inverse = Gtk::make_managed<Gtk::Entry>();
  inverse->set_text(original.inverse_name);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  form->append(
      *Gtk::make_managed<Gtk::Label>("Chave imutável: " + original.key));
  append_labeled_form_field(*form, "Nome de origem para destino", *name);
  append_labeled_form_field(*form, "Nome inverso", *inverse);
  append_labeled_form_field(*form, "Descrição", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, name, inverse,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        original.name = name->get_text();
        original.inverse_name = inverse->get_text();
        original.description = description->get_text();
        const auto type = service_.narrative().update_relation_type(original);
        refresh_relation_type_controls();
        refresh_relation_filter_options();
        refresh_relation_explorer();
        refresh_context_filters();
        refresh_entities();
        relation_type_combo_.set_active_id(type.id);
        signal_status_message_.emit("Tipo de relação atualizado");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar o tipo de relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_relation_type() {
  const std::string id = relation_type_combo_.get_active_id().raw();
  auto *owner = owner_window();
  if (!owner || id.empty())
    return;
  auto *dialog = new OverlayDialog(*owner, "Remover este tipo de relação?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "Tipos usados por relações são protegidos contra remoção.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().delete_relation_type(id);
        refresh_relation_type_controls();
        refresh_relation_filter_options();
        refresh_relation_explorer();
        refresh_context_filters();
        refresh_entities();
        signal_status_message_.emit("Tipo de relação removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o tipo de relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::create_relation() {
  const auto *current = selected_entity();
  auto *owner = owner_window();
  const bool contextual =
      current != nullptr && page_stack_.get_visible_child_name() == "detail";
  if (!owner || relation_types_.empty())
    return;
  auto *dialog = new OverlayDialog("Nova relação narrativa", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : relation_types_)
    type->append(value.id, value.name);
  type->set_active_id(relation_type_combo_.get_active_id());
  auto *source =
      contextual ? nullptr : make_entity_selector(std::nullopt, false);
  auto *target =
      contextual ? nullptr : make_entity_selector(std::nullopt, false);
  auto *other = contextual
                    ? make_entity_selector(std::nullopt, false, current->id)
                    : nullptr;
  auto *direction =
      contextual ? Gtk::make_managed<Gtk::ComboBoxText>() : nullptr;
  if (direction) {
    direction->append("outgoing", "Entidade atual → outra entidade");
    direction->append("incoming", "Outra entidade → entidade atual");
    direction->set_active_id("outgoing");
  }
  auto *when = make_time_point_selector();
  auto *where = make_entity_selector(type_id_for_key("location"), false);
  auto *cause = make_entity_selector(std::nullopt, false);
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_placeholder_text("Descrição da relação");
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo de relação", *type);
  if (contextual) {
    append_labeled_form_field(*form, "Outra entidade", *other,
                              "Pesquise e escolha a entidade relacionada.");
    append_labeled_form_field(*form, "Sentido da relação", *direction);
  } else {
    append_labeled_form_field(*form, "Entidade de origem", *source);
    append_labeled_form_field(*form, "Entidade de destino", *target);
  }
  append_labeled_form_field(*form, "Quando (opcional)", *when);
  append_labeled_form_field(*form, "Onde (opcional)", *where,
                            "Apenas entidades classificadas como Local.");
  append_labeled_form_field(*form, "Por causa de (opcional)", *cause);
  append_labeled_form_field(*form, "Notas de contexto", *description);
  dialog->get_content_area()->append(*form);
  const auto current_id =
      current ? std::optional<std::string>{current->id} : std::nullopt;
  dialog->signal_response().connect([this, dialog, contextual, current_id, type,
                                     source, target, other, direction, when,
                                     where, cause, description](int response) {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (type->get_active_id().empty())
          throw std::runtime_error("Selecione o tipo de relação");
        std::string source_id;
        std::string target_id;
        if (contextual) {
          if (!current_id || !other->selected_id())
            throw std::runtime_error("Selecione a outra entidade");
          const bool outgoing = direction->get_active_id().raw() == "outgoing";
          const auto other_id = *other->selected_id();
          source_id = outgoing ? *current_id : other_id;
          target_id = outgoing ? other_id : *current_id;
        } else {
          if (!source->selected_id() || !target->selected_id())
            throw std::runtime_error("Selecione origem e destino");
          source_id = *source->selected_id();
          target_id = *target->selected_id();
        }
        static_cast<void>(service_.narrative().create_relation(
            type->get_active_id().raw(), std::move(source_id),
            std::move(target_id), description->get_text(), when->selected_id(),
            where->selected_id(), cause->selected_id()));
        refresh_entities();
        refresh_relation_explorer();
        if (contextual)
          refresh_details();
        signal_status_message_.emit("Relação narrativa criada");
      } catch (const std::exception &error) {
        show_error("Não foi possível criar a relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::edit_relation() {
  if (!selected_relation_id_)
    return;
  const auto found = std::find_if(
      relation_page_results_.begin(), relation_page_results_.end(),
      [&](const auto &value) { return value.id == *selected_relation_id_; });
  auto *owner = owner_window();
  if (!owner || found == relation_page_results_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar relação narrativa", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *type = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : relation_types_)
    type->append(value.id, value.name);
  type->set_active_id(original.relation_type_id);
  auto *source = make_entity_selector(std::nullopt, false);
  source->set_selected(original.source_entity_id,
                       relation_entity_label(original.source_entity_id));
  auto *target = make_entity_selector(std::nullopt, false);
  target->set_selected(original.target_entity_id,
                       relation_entity_label(original.target_entity_id));
  auto *when = make_time_point_selector();
  if (original.fictional_time_point_id)
    when->set_selected(*original.fictional_time_point_id,
                       "Ponto temporal atual");
  auto *where = make_entity_selector(type_id_for_key("location"), false);
  if (original.location_entity_id)
    where->set_selected(*original.location_entity_id,
                        relation_entity_label(*original.location_entity_id));
  auto *cause = make_entity_selector(std::nullopt, false);
  if (original.cause_entity_id)
    cause->set_selected(*original.cause_entity_id,
                        relation_entity_label(*original.cause_entity_id));
  auto *description = Gtk::make_managed<Gtk::Entry>();
  description->set_text(original.description);
  form->set_margin(16);
  append_labeled_form_field(*form, "Tipo de relação", *type);
  append_labeled_form_field(*form, "Entidade de origem", *source);
  append_labeled_form_field(*form, "Entidade de destino", *target);
  append_labeled_form_field(*form, "Quando (opcional)", *when);
  append_labeled_form_field(*form, "Onde (opcional)", *where);
  append_labeled_form_field(*form, "Por causa de (opcional)", *cause);
  append_labeled_form_field(*form, "Notas de contexto", *description);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect([this, dialog, original, type, source,
                                     target, when, where, cause,
                                     description](int response) mutable {
    if (response == Gtk::ResponseType::ACCEPT) {
      try {
        if (type->get_active_id().empty() || !source->selected_id() ||
            !target->selected_id())
          throw std::runtime_error("Selecione tipo, origem e destino");
        original.relation_type_id = type->get_active_id().raw();
        original.source_entity_id = *source->selected_id();
        original.target_entity_id = *target->selected_id();
        original.fictional_time_point_id = when->selected_id();
        original.location_entity_id = where->selected_id();
        original.cause_entity_id = cause->selected_id();
        original.description = description->get_text();
        static_cast<void>(service_.narrative().update_relation(original));
        refresh_entities();
        refresh_relation_explorer();
        signal_status_message_.emit("Relação narrativa atualizada");
      } catch (const std::exception &error) {
        show_error("Não foi possível editar a relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::delete_relation() {
  if (!selected_relation_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_relation_id_;
  auto *dialog = new OverlayDialog(*owner, "Remover esta relação narrativa?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().delete_relation(id);
        refresh_entities();
        refresh_relation_explorer();
        refresh_details();
        signal_status_message_.emit("Relação narrativa removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a relação", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::refresh_editorial_context() {
  const auto *entity = selected_entity();
  const bool has_entity = entity != nullptr;
  work_scope_section_.set_visible(has_entity);
  editorial_reference_section_.set_visible(has_entity);
  for (auto *widget :
       {static_cast<Gtk::Widget *>(&work_scope_separator_),
        static_cast<Gtk::Widget *>(&work_scope_title_),
        static_cast<Gtk::Widget *>(&work_scope_toolbar_),
        static_cast<Gtk::Widget *>(&work_scope_list_),
        static_cast<Gtk::Widget *>(&editorial_reference_separator_),
        static_cast<Gtk::Widget *>(&editorial_reference_title_),
        static_cast<Gtk::Widget *>(&editorial_reference_toolbar_),
        static_cast<Gtk::Widget *>(&editorial_reference_list_)})
    widget->set_visible(has_entity);

  work_scopes_.clear();
  editorial_references_.clear();
  selected_work_scope_id_.reset();
  selected_editorial_reference_id_.reset();
  while (auto *child = work_scope_list_.get_first_child())
    work_scope_list_.remove(*child);
  while (auto *child = editorial_reference_list_.get_first_child())
    editorial_reference_list_.remove(*child);
  work_scope_edit_button_.set_sensitive(false);
  work_scope_remove_button_.set_sensitive(false);
  work_scope_actions_button_.set_sensitive(false);
  editorial_reference_edit_button_.set_sensitive(false);
  editorial_reference_remove_button_.set_sensitive(false);
  editorial_reference_actions_button_.set_sensitive(false);
  work_scope_add_button_.set_sensitive(has_entity &&
                                       !service_.catalog().works.empty());
  editorial_reference_add_button_.set_sensitive(false);
  if (!entity)
    return;

  persistence::EntityWorkScopeQuery scope_query;
  scope_query.entity_id = entity->id;
  scope_query.limit = 100;
  work_scopes_ = service_.narrative().work_scopes(scope_query);
  for (const auto &scope : work_scopes_) {
    const auto work = std::find_if(
        service_.catalog().works.begin(), service_.catalog().works.end(),
        [&](const auto &value) { return value.id == scope.work_id; });
    const auto label = (work == service_.catalog().works.end() ? "Obra ausente"
                                                               : work->title) +
                       (scope.notes.empty() ? "" : " — " + scope.notes);
    auto *button = Gtk::make_managed<Gtk::Button>(label);
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = scope.id] { select_work_scope(id); });
    attach_context_menu(*button, {{"Editar notas",
                                   [this, id = scope.id] {
                                     select_work_scope(id);
                                     edit_work_scope();
                                   },
                                   "document-edit-symbolic"},
                                  {"Remover vínculo",
                                   [this, id = scope.id] {
                                     select_work_scope(id);
                                     delete_work_scope();
                                   },
                                   "user-trash-symbolic", true, true, true}});
    work_scope_list_.append(*button);
  }

  persistence::EditorialReferenceQuery reference_query;
  reference_query.entity_id = entity->id;
  reference_query.limit = 100;
  editorial_references_ =
      service_.narrative().editorial_references(reference_query);
  for (const auto &reference : editorial_references_) {
    const auto nodes = service_.structural_nodes_for_work(reference.work_id);
    const auto node =
        std::find_if(nodes.begin(), nodes.end(), [&](const auto &value) {
          return value.id == reference.editorial_node_id;
        });
    const auto node_name =
        node == nodes.end() ? "Unidade ausente" : node->title;
    auto *button =
        Gtk::make_managed<Gtk::Button>(node_name + " — " + reference.purpose);
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect(
        [this, id = reference.id] { select_editorial_reference(id); });
    attach_context_menu(*button, {{"Editar referência",
                                   [this, id = reference.id] {
                                     select_editorial_reference(id);
                                     edit_editorial_reference();
                                   },
                                   "document-edit-symbolic"},
                                  {"Remover referência",
                                   [this, id = reference.id] {
                                     select_editorial_reference(id);
                                     delete_editorial_reference();
                                   },
                                   "user-trash-symbolic", true, true, true}});
    editorial_reference_list_.append(*button);
  }
  editorial_reference_add_button_.set_sensitive(!work_scopes_.empty());
}

void PlanningWorkspace::refresh_writing_context() {
  while (auto *child = writing_reference_list_.get_first_child())
    writing_reference_list_.remove(*child);
  const auto *entity = selected_entity();
  writing_reference_section_.set_visible(entity != nullptr);
  writing_library_button_.set_sensitive(entity != nullptr);
  if (!entity) {
    writing_reference_hint_.set_text("Selecione uma entidade.");
    return;
  }
  persistence::DocumentQuery query;
  query.entity_id = entity->id;
  query.limit = 200;
  const auto documents = service_.writing().document_summaries(query);
  writing_reference_hint_.set_text(
      documents.empty()
          ? "Esta entidade ainda não foi vinculada explicitamente a um "
            "Documento."
          : std::to_string(documents.size()) +
                (documents.size() == 1 ? " Documento usa esta entidade"
                                       : " Documentos usam esta entidade"));
  for (const auto &document : documents) {
    auto *button = Gtk::make_managed<Gtk::Button>(document.title);
    button->set_halign(Gtk::Align::FILL);
    button->set_tooltip_text("Abrir Documento; o vínculo pode apontar ao texto "
                             "inteiro ou a uma âncora");
    button->signal_clicked().connect(
        [this, id = document.id] { signal_open_document_requested_.emit(id); });
    writing_reference_list_.append(*button);
  }
}

void PlanningWorkspace::select_work_scope(std::string id) {
  selected_work_scope_id_ = std::move(id);
  work_scope_edit_button_.set_sensitive(true);
  work_scope_remove_button_.set_sensitive(true);
  work_scope_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_work_scope() {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!owner || !entity)
    return;
  auto *dialog = new OverlayDialog("Vincular entidade à Obra", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Vincular", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *work = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &value : service_.catalog().works) {
    const bool linked = std::any_of(
        work_scopes_.begin(), work_scopes_.end(),
        [&](const auto &scope) { return scope.work_id == value.id; });
    if (!linked)
      work->append(value.id, value.title);
  }
  work->set_active(0);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas sobre o escopo nesta Obra");
  form->set_margin(16);
  append_labeled_form_field(
      *form, "Obra", *work,
      "Escolha a obra que passa a incluir esta entidade.");
  append_labeled_form_field(*form, "Notas sobre o escopo", *notes);
  dialog->get_content_area()->append(*form);
  const auto entity_id = entity->id;
  dialog->signal_response().connect(
      [this, dialog, entity_id, work, notes](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (work->get_active_id().empty())
              throw std::runtime_error(
                  "A entidade já está vinculada a todas as Obras disponíveis");
            static_cast<void>(service_.narrative().add_entity_to_work(
                entity_id, work->get_active_id().raw(), notes->get_text()));
            refresh_entities();
            signal_status_message_.emit("Entidade vinculada à Obra");
          } catch (const std::exception &error) {
            show_error("Não foi possível vincular a entidade", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_work_scope() {
  if (!selected_work_scope_id_)
    return;
  const auto found = std::find_if(
      work_scopes_.begin(), work_scopes_.end(),
      [&](const auto &value) { return value.id == *selected_work_scope_id_; });
  auto *owner = owner_window();
  if (!owner || found == work_scopes_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar vínculo com a Obra", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_text(original.notes);
  notes->set_placeholder_text("Notas sobre o escopo nesta Obra");
  form->set_margin(16);
  append_labeled_form_field(*form, "Notas sobre o escopo", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, original, notes](int response) mutable {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            original.notes = notes->get_text();
            static_cast<void>(service_.narrative().update_work_scope(original));
            refresh_details();
            signal_status_message_.emit("Vínculo com a Obra atualizado");
          } catch (const std::exception &error) {
            show_error("Não foi possível editar o vínculo", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::delete_work_scope() {
  if (!selected_work_scope_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_work_scope_id_;
  auto *dialog = new OverlayDialog(
      *owner, "Remover o vínculo desta entidade com a Obra?", false,
      Gtk::MessageType::QUESTION, Gtk::ButtonsType::YES_NO, true);
  dialog->set_secondary_text(
      "Referências em unidades editoriais protegem este vínculo.");
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().remove_entity_from_work(id);
        refresh_entities();
        signal_status_message_.emit("Vínculo com a Obra removido");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover o vínculo", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
}

void PlanningWorkspace::select_editorial_reference(std::string id) {
  selected_editorial_reference_id_ = std::move(id);
  editorial_reference_edit_button_.set_sensitive(true);
  editorial_reference_remove_button_.set_sensitive(true);
  editorial_reference_actions_button_.set_sensitive(true);
}

void PlanningWorkspace::add_editorial_reference() {
  const auto *entity = selected_entity();
  auto *owner = owner_window();
  if (!owner || !entity)
    return;
  auto *dialog = new OverlayDialog("Nova referência editorial", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Criar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *node = Gtk::make_managed<Gtk::ComboBoxText>();
  for (const auto &scope : work_scopes_) {
    const auto work = std::find_if(
        service_.catalog().works.begin(), service_.catalog().works.end(),
        [&](const auto &value) { return value.id == scope.work_id; });
    const auto work_name =
        work == service_.catalog().works.end() ? "Obra" : work->title;
    for (const auto &value : service_.structural_nodes_for_work(scope.work_id))
      node->append(value.id, work_name + " — " + value.title);
  }
  node->set_active(0);
  auto *purpose = Gtk::make_managed<Gtk::Entry>();
  purpose->set_placeholder_text(
      "Finalidade, por exemplo: aparece, mencionada, revelada");
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_placeholder_text("Notas editoriais");
  form->set_margin(16);
  append_labeled_form_field(*form, "Unidade editorial", *node,
                            "Escolha onde a entidade será apresentada.");
  append_labeled_form_field(*form, "Finalidade da apresentação", *purpose);
  append_labeled_form_field(*form, "Notas editoriais", *notes);
  dialog->get_content_area()->append(*form);
  const auto entity_id = entity->id;
  dialog->signal_response().connect(
      [this, dialog, entity_id, node, purpose, notes](int response) {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            if (node->get_active_id().empty())
              throw std::runtime_error(
                  "Crie uma unidade editorial em uma Obra vinculada");
            static_cast<void>(service_.narrative().add_editorial_reference(
                entity_id, node->get_active_id().raw(), purpose->get_text(),
                notes->get_text()));
            refresh_details();
            signal_status_message_.emit("Referência editorial criada");
          } catch (const std::exception &error) {
            show_error("Não foi possível criar a referência", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::edit_editorial_reference() {
  if (!selected_editorial_reference_id_)
    return;
  const auto found =
      std::find_if(editorial_references_.begin(), editorial_references_.end(),
                   [&](const auto &value) {
                     return value.id == *selected_editorial_reference_id_;
                   });
  auto *owner = owner_window();
  if (!owner || found == editorial_references_.end())
    return;
  auto original = *found;
  auto *dialog = new OverlayDialog("Editar referência editorial", *owner, true);
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Salvar", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *purpose = Gtk::make_managed<Gtk::Entry>();
  purpose->set_text(original.purpose);
  auto *notes = Gtk::make_managed<Gtk::Entry>();
  notes->set_text(original.notes);
  form->set_margin(16);
  append_labeled_form_field(*form, "Finalidade da apresentação", *purpose);
  append_labeled_form_field(*form, "Notas editoriais", *notes);
  dialog->get_content_area()->append(*form);
  dialog->signal_response().connect(
      [this, dialog, original, purpose, notes](int response) mutable {
        if (response == Gtk::ResponseType::ACCEPT) {
          try {
            original.purpose = purpose->get_text();
            original.notes = notes->get_text();
            static_cast<void>(
                service_.narrative().update_editorial_reference(original));
            refresh_details();
            signal_status_message_.emit("Referência editorial atualizada");
          } catch (const std::exception &error) {
            show_error("Não foi possível editar a referência", error);
          }
        }
        dialog->hide();
      });
  dialog->present();
}

void PlanningWorkspace::delete_editorial_reference() {
  if (!selected_editorial_reference_id_)
    return;
  auto *owner = owner_window();
  if (!owner)
    return;
  const auto id = *selected_editorial_reference_id_;
  auto *dialog = new OverlayDialog(*owner, "Remover esta referência editorial?",
                                   false, Gtk::MessageType::QUESTION,
                                   Gtk::ButtonsType::YES_NO, true);
  dialog->signal_response().connect([this, dialog, id](int response) {
    if (response == Gtk::ResponseType::YES) {
      try {
        service_.narrative().remove_editorial_reference(id);
        refresh_details();
        signal_status_message_.emit("Referência editorial removida");
      } catch (const std::exception &error) {
        show_error("Não foi possível remover a referência", error);
      }
    }
    dialog->hide();
  });
  dialog->present();
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

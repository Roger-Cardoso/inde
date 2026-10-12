#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/bounded_choices.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace inde::ui {

void PlanningWorkspace::build_explorer_header() {
  explorer_page_.set_vexpand(true);
  explorer_page_.set_hexpand(true);
  search_.set_hexpand(true);
  explorer_header_.add_css_class("organization-toolbar");
  explorer_header_.append(search_);
  explorer_header_.append(add_button_);
  explorer_header_.append(filters_button_);
  entity_types_button_.set_label("Categorias");
  entity_types_button_.set_tooltip_text("Gerenciar categorias de elemento");
  entity_types_popover_.set_child(entity_type_section_);
  entity_types_button_.set_popover(entity_types_popover_);
  explorer_header_.append(entity_types_button_);
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
  entity_crud_section_.append(entity_crud_title_);
  entity_crud_section_.append(entity_crud_actions_);
  entity_type_actions_.append(entity_type_combo_);
  bound_choice_width(entity_type_combo_);
  entity_type_actions_.append(entity_type_add_button_);
  entity_type_actions_.append(entity_type_actions_button_);
  entity_type_section_.append(entity_type_section_title_);
  entity_type_section_.append(entity_type_actions_);
  explorer_filter_section_.append(explorer_filter_title_);
  explorer_filter_section_.append(explorer_filter_actions_);
  // Tipos são gestão sob demanda, não uma segunda barra permanente.
  context_summary_.set_wrap(true);
  context_summary_.set_xalign(0.0F);
  context_summary_.add_css_class("dim-label");
  context_summary_.add_css_class("context-summary");
  explorer_page_.append(context_summary_);
}

void PlanningWorkspace::build_explorer_filters() {
  navigation_panel_.set_margin(12);
  navigation_title_.add_css_class("heading");
  navigation_title_.set_halign(Gtk::Align::START);
  navigation_title_.set_text("Refinar resultados");
  search_.set_placeholder_text("Pesquisar nome, nome alternativo ou resumo");
  set_accessible_label(search_, "Pesquisar elementos da história");
  set_accessible_description(search_, "Filtra elementos da história por nome ou resumo, sem expansão relacional implícita.");
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
      "Mostra elementos da história ligados a acontecimentos, participações ou presenças na linha do tempo");
  context_filters_.append(temporal_axis_filter_);
  temporal_window_filter_.append("all", "Linha do tempo inteira");
  temporal_window_filter_.append("point", "Um marco na linha do tempo");
  temporal_window_filter_.append("range", "Período fechado");
  temporal_window_filter_.set_active_id("all");
  temporal_window_filter_.set_tooltip_text(
      "Escolha um ponto ou um período fechado na linha do tempo selecionada");
  context_filters_.append(temporal_window_filter_);
  temporal_point_filter_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar ponto da linha do tempo",
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
      "Escolha uma linha do tempo para pesquisar pontos");
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
      "Pesquisar elemento do manuscrito",
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
                                     ? " elemento associado"
                                     : " elementos associados")});
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
}

void PlanningWorkspace::build_explorer_results() {
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
}

void PlanningWorkspace::build_explorer_pagination() {
  pagination_.append(previous_button_);
  pagination_.append(page_label_);
  pagination_.append(next_button_);
  page_label_.set_hexpand(true);
  page_label_.set_halign(Gtk::Align::CENTER);
  explorer_page_.append(pagination_);
}

} // namespace inde::ui

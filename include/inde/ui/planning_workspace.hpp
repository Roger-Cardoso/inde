#pragma once

#include "inde/application/project_service.hpp"
#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"

#include <cstdint>
#include <gtkmm.h>
#include <optional>
#include <unordered_map>
#include <vector>

namespace inde::ui {

class PlanningWorkspace final : public Gtk::Paned {
public:
  explicit PlanningWorkspace(application::ProjectService &service);

  void install_actions(Gtk::ApplicationWindow &window);
  void refresh();
  void reset();
  void reveal_entity(const std::string &id);
  void reveal_time_point(const std::string &id);
  void toggle_navigation();
  void toggle_inspector();
  [[nodiscard]] sigc::signal<void(const Glib::ustring &)> &
  signal_status_message() {
    return signal_status_message_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_open_document_requested() {
    return signal_open_document_requested_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_filter_documents_requested() {
    return signal_filter_documents_requested_;
  }
  sigc::signal<void(const std::string &)> &signal_cartography_requested() {
    return signal_cartography_requested_;
  }

private:
  struct NavigationState;
  static constexpr std::size_t page_size_ = 100;

  void build_ui();
  void show_narrative_page();
  void show_explorer_page();
  void show_relations_page();
  void show_time_page();
  void show_entity_detail_page();
  void remember_navigation();
  void go_back_in_navigation();
  void go_forward_in_navigation();
  void restore_navigation_state(NavigationState state);
  void update_navigation_actions();
  void dismiss_filters();
  void refresh_context_summary();
  void refresh_types();
  void refresh_entity_type_controls();
  void refresh_context_filters();
  void refresh_context_time_points();
  void refresh_context_editorial_nodes();
  [[nodiscard]] application::PlanningContext context_from_controls() const;
  void refresh_facets();
  void refresh_entities();
  void refresh_details();
  void refresh_axes();
  void refresh_time_points();
  void refresh_time_overview();
  void refresh_temporal_context();
  void refresh_editorial_context();
  void refresh_writing_context();
  void refresh_relations();
  void refresh_relation_explorer();
  void refresh_relation_type_controls();
  void refresh_relation_filter_options();
  void clear_relation_filters();
  void toggle_relation_filters();
  [[nodiscard]] persistence::RelationQuery relation_query_from_controls() const;
  [[nodiscard]] std::string relation_type_label(const std::string &id) const;
  [[nodiscard]] std::string relation_entity_label(const std::string &id) const;
  [[nodiscard]] std::string
  relation_context_label(const project::NarrativeRelation &relation) const;
  void create_entity();
  void create_entity_type();
  void edit_entity_type();
  void delete_entity_type();
  void edit_selected_entity();
  void delete_selected_entity();
  void select_entity(std::string id);
  void previous_page();
  void next_page();
  void create_time_axis();
  void edit_time_axis();
  void delete_time_axis();
  void select_time_point(std::string id);
  void create_time_point();
  void edit_time_point();
  void delete_time_point();
  void edit_event_occurrence();
  void delete_event_occurrence();
  void add_participant();
  void edit_participant();
  void delete_participant();
  void select_participant(std::string id);
  void add_presence();
  void edit_presence();
  void delete_presence();
  void select_presence(std::string id);
  void add_work_scope();
  void edit_work_scope();
  void delete_work_scope();
  void select_work_scope(std::string id);
  void add_editorial_reference();
  void edit_editorial_reference();
  void delete_editorial_reference();
  void select_editorial_reference(std::string id);
  void create_relation_type();
  void edit_relation_type();
  void delete_relation_type();
  void create_relation();
  void edit_relation();
  void delete_relation();
  void select_relation(std::string id);
  void open_relation_counterpart();
  void filter_by_relation_counterpart();
  void clear_planning_context();
  [[nodiscard]] IncrementalSelector *make_time_point_selector() const;
  [[nodiscard]] IncrementalSelector *make_entity_selector(
      const std::optional<std::string> &type_id, bool exclude_events,
      const std::optional<std::string> &exclude_id = std::nullopt) const;
  void show_error(const Glib::ustring &title, const std::exception &error);
  [[nodiscard]] Gtk::Window *owner_window();
  [[nodiscard]] std::string type_name(const std::string &id) const;
  [[nodiscard]] std::optional<std::string>
  type_id_for_key(const std::string &key) const;
  [[nodiscard]] const project::NarrativeEntity *selected_entity() const;
  [[nodiscard]] std::optional<std::string>
  selected_relation_counterpart_id() const;
  [[nodiscard]] static std::int64_t parse_ordinal(const Glib::ustring &text);

  application::ProjectService &service_;
  NarrativeStructureWorkspace narrative_structure_workspace_{service_};
  Gtk::Box planning_root_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box page_switcher_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button narrative_page_button_{"Estrutura narrativa"};
  Gtk::Button explorer_page_button_{"Explorar entidades"};
  Gtk::Button relations_page_button_{"Explorar relações"};
  Gtk::Button time_page_button_{"Tempo ficcional"};
  Gtk::Button history_back_button_{"← Voltar"};
  Gtk::Button history_forward_button_{"Avançar →"};
  Gtk::Stack page_stack_;
  Gtk::Box explorer_page_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box relations_page_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box detail_page_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box time_page_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box explorer_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label explorer_title_{"Explorar entidades"};
  Gtk::Box explorer_toolbar_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Box entity_crud_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label entity_crud_title_{"Entidades"};
  Gtk::Box entity_crud_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Box entity_type_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label entity_type_section_title_{"Tipos de entidade"};
  Gtk::Box entity_type_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::ComboBoxText entity_type_combo_;
  Gtk::Button entity_type_add_button_{"Novo tipo"};
  Gtk::MenuButton entity_type_actions_button_;
  Gtk::Box explorer_filter_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label explorer_filter_title_{"Resultados"};
  Gtk::Box explorer_filter_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button filters_button_{"Filtros"};
  Gtk::Label context_summary_;
  Gtk::Overlay explorer_overlay_;
  Gtk::Revealer filters_revealer_;
  Gtk::Frame filters_surface_;
  Gtk::ScrolledWindow filters_scroll_;
  Gtk::Label explorer_empty_title_{"Nenhuma entidade encontrada"};
  Gtk::Label explorer_empty_message_;
  Gtk::Box relations_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label relations_page_title_{"Explorar relações"};
  Gtk::Button relation_filters_button_{"Filtros"};
  Gtk::Box relations_toolbar_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Box relation_crud_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label relation_crud_title_{"Relações"};
  Gtk::Box relation_crud_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Box relation_type_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label relation_type_section_title_{"Tipos de relação"};
  Gtk::Box relation_type_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Label relation_context_summary_;
  Gtk::Overlay relations_overlay_;
  Gtk::Revealer relation_filters_revealer_;
  Gtk::Frame relation_filters_surface_;
  Gtk::ScrolledWindow relation_filters_scroll_;
  Gtk::Box relation_filters_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::SearchEntry relation_search_;
  Gtk::ComboBoxText relation_type_filter_;
  IncrementalSelector *relation_entity_filter_{};
  IncrementalSelector *relation_time_point_filter_{};
  IncrementalSelector *relation_location_filter_{};
  IncrementalSelector *relation_cause_filter_{};
  Gtk::Box relation_filter_actions_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button clear_relation_filters_button_{"Limpar"};
  Gtk::Button apply_relation_filters_button_{"Ver relações"};
  Gtk::ScrolledWindow relation_scroll_;
  Gtk::FlowBox relation_card_list_;
  Gtk::Box relation_empty_state_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label relation_empty_title_{"Nenhuma relação encontrada"};
  Gtk::Label relation_empty_message_;
  Gtk::Stack relation_results_stack_;
  Gtk::Box detail_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button back_to_explorer_button_{"← Voltar aos resultados"};
  Gtk::Button cartography_button_{"Ver no mapa"};
  Gtk::Label detail_context_;
  Gtk::Box time_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label time_page_title_{"Tempo ficcional"};
  Gtk::Label time_page_description_;
  Gtk::Box navigation_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box content_panel_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box inspector_panel_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Box time_columns_{Gtk::Orientation::HORIZONTAL, 12};
  Gtk::ScrolledWindow content_scroll_;
  Gtk::ScrolledWindow inspector_scroll_;
  Gtk::ScrolledWindow time_facts_scroll_;
  Gtk::Box time_facts_panel_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Label navigation_title_{"Entidades"};
  Gtk::Entry search_;
  Gtk::ComboBoxText task_mode_filter_;
  Gtk::Label type_facets_label_{"Tipos"};
  Gtk::ScrolledWindow type_facets_scroll_;
  Gtk::Box type_facets_{Gtk::Orientation::VERTICAL, 2};
  Gtk::Expander context_expander_{"Contexto do explorador"};
  Gtk::Box context_filters_{Gtk::Orientation::VERTICAL, 6};
  Gtk::ComboBoxText work_filter_;
  Gtk::Label temporal_filter_label_{"Tempo ficcional"};
  Gtk::ComboBoxText temporal_axis_filter_;
  Gtk::ComboBoxText temporal_window_filter_;
  IncrementalSelector *temporal_point_filter_{};
  IncrementalSelector *temporal_window_end_filter_{};
  Gtk::Label editorial_filter_label_{"Apresentação editorial"};
  IncrementalSelector *editorial_node_filter_{};
  Gtk::Label writing_filter_label_{"Uso em Escrita"};
  Gtk::ComboBoxText writing_usage_filter_;
  IncrementalSelector *writing_document_filter_{};
  Gtk::Label relation_facets_label_{"Relações"};
  Gtk::ScrolledWindow relation_facets_scroll_;
  Gtk::Box relation_facets_{Gtk::Orientation::VERTICAL, 2};
  Gtk::Label counterpart_filter_label_{"Contraparte: qualquer entidade"};
  Gtk::Button clear_context_button_{"Limpar contexto"};
  Gtk::Button apply_filters_button_{"Ver resultados"};
  Gtk::Button add_button_{"Nova entidade"};
  Gtk::Button edit_button_{"Editar"};
  Gtk::Button remove_button_{"Remover"};
  Gtk::MenuButton entity_actions_button_;
  Gtk::ScrolledWindow entity_scroll_;
  Gtk::FlowBox entity_list_;
  Gtk::Box pagination_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button previous_button_{"Anterior"};
  Gtk::Label page_label_;
  Gtk::Button next_button_{"Próxima"};
  Gtk::Label empty_title_{"Planejamento narrativo"};
  Gtk::Label empty_message_{
      "Crie ou selecione uma entidade para começar a modelar a realidade "
      "ficcional."};
  Gtk::Label entity_name_;
  Gtk::Label entity_type_;
  Gtk::Label entity_summary_;
  Gtk::Frame entity_summary_surface_;
  Gtk::Box entity_summary_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Expander work_scope_section_{"Obras e escopo"};
  Gtk::Box work_scope_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Separator work_scope_separator_;
  Gtk::Label work_scope_title_{"Obras da entidade"};
  Gtk::Box work_scope_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button work_scope_add_button_{"Vincular à obra"};
  Gtk::Button work_scope_edit_button_{"Editar notas"};
  Gtk::Button work_scope_remove_button_{"Remover vínculo"};
  Gtk::MenuButton work_scope_actions_button_;
  Gtk::ListBox work_scope_list_;
  Gtk::Expander relations_section_{"Relações narrativas"};
  Gtk::Box relations_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label relation_detail_hint_;
  Gtk::ComboBoxText relation_type_combo_;
  Gtk::Box relation_type_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button relation_type_add_button_{"Novo tipo"};
  Gtk::Button relation_type_edit_button_{"Editar tipo"};
  Gtk::Button relation_type_remove_button_{"Remover tipo"};
  Gtk::MenuButton relation_type_actions_button_;
  Gtk::Box relation_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button relation_add_button_{"Nova relação"};
  Gtk::Button relation_edit_button_{"Editar"};
  Gtk::Button relation_remove_button_{"Remover"};
  Gtk::MenuButton relation_actions_button_;
  Gtk::Button context_relation_add_button_{"Estabelecer relação"};
  Gtk::Box relation_navigation_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button relation_open_counterpart_button_{"Abrir contraparte"};
  Gtk::Button relation_filter_counterpart_button_{"Filtrar por contraparte"};
  Gtk::ListBox relation_list_;
  Gtk::Expander event_section_{"Acontecimento e participantes"};
  Gtk::Box event_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Separator event_separator_;
  Gtk::Label event_title_{"Acontecimento"};
  Gtk::Label event_status_;
  Gtk::Box event_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button event_edit_button_{"Situar"};
  Gtk::Button event_remove_button_{"Remover ocorrência"};
  Gtk::MenuButton event_actions_button_;
  Gtk::Label participants_title_{"Participantes"};
  Gtk::Box participant_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button participant_add_button_{"Adicionar"};
  Gtk::Button participant_edit_button_{"Editar"};
  Gtk::Button participant_remove_button_{"Remover"};
  Gtk::MenuButton participant_actions_button_;
  Gtk::ListBox participant_list_;
  Gtk::Expander presence_section_{"Presença e localização"};
  Gtk::Box presence_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Separator presence_separator_;
  Gtk::Label presence_title_{"Presença e localização"};
  Gtk::Box presence_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button presence_add_button_{"Nova presença"};
  Gtk::Button presence_edit_button_{"Editar"};
  Gtk::Button presence_remove_button_{"Remover"};
  Gtk::MenuButton presence_actions_button_;
  Gtk::ListBox presence_list_;
  Gtk::Expander editorial_reference_section_{"Apresentação editorial"};
  Gtk::Box editorial_reference_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Separator editorial_reference_separator_;
  Gtk::Label editorial_reference_title_{"Apresentação editorial"};
  Gtk::Box editorial_reference_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button editorial_reference_add_button_{"Nova referência"};
  Gtk::Button editorial_reference_edit_button_{"Editar"};
  Gtk::Button editorial_reference_remove_button_{"Remover"};
  Gtk::MenuButton editorial_reference_actions_button_;
  Gtk::ListBox editorial_reference_list_;
  Gtk::Expander writing_reference_section_{"Documentos relacionados"};
  Gtk::Box writing_reference_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label writing_reference_hint_;
  Gtk::Button writing_library_button_{"Ver na Biblioteca"};
  Gtk::ListBox writing_reference_list_;
  Gtk::Label inspector_title_{"Inspetor"};
  Gtk::Label inspector_identity_;
  Gtk::Label inspector_dates_;
  Gtk::Label inspector_relations_;
  Gtk::Label inspector_presences_;
  Gtk::Separator time_separator_;
  Gtk::Label time_title_{"Tempo ficcional"};
  Gtk::ComboBoxText axis_combo_;
  Gtk::Box axis_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button axis_add_button_{"Novo eixo"};
  Gtk::Button axis_edit_button_{"Editar"};
  Gtk::Button axis_remove_button_{"Remover"};
  Gtk::MenuButton axis_actions_button_;
  Gtk::Label points_title_{"Pontos temporais"};
  Gtk::Box point_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button point_add_button_{"Novo ponto"};
  Gtk::Button point_edit_button_{"Editar"};
  Gtk::Button point_remove_button_{"Remover"};
  Gtk::MenuButton point_actions_button_;
  Gtk::ListBox point_list_;
  Gtk::Separator time_facts_separator_;
  Gtk::Label time_events_title_{"Acontecimentos neste eixo"};
  Gtk::ListBox time_events_list_;
  Gtk::Label time_presences_title_{"Presenças neste eixo"};
  Gtk::ListBox time_presences_list_;
  std::vector<project::EntityType> types_;
  std::vector<project::NarrativeEntity> page_;
  application::PlanningExplorerSnapshot explorer_snapshot_;
  std::vector<project::FictionalTimeAxis> axes_;
  std::vector<project::FictionalTimePoint> time_points_;
  std::vector<project::EventParticipation> participants_;
  std::vector<project::EntityPresence> presences_;
  std::vector<project::EntityWorkScope> work_scopes_;
  std::vector<project::EditorialEntityReference> editorial_references_;
  std::vector<project::RelationType> relation_types_;
  std::vector<std::string> selected_type_ids_;
  std::vector<std::string> selected_relation_type_ids_;
  std::vector<project::NarrativeRelation> relations_;
  std::vector<project::NarrativeRelation> relation_page_results_;
  std::unordered_map<std::string, std::string> relation_entity_labels_;
  std::unordered_map<std::string, std::string> relation_time_point_labels_;
  std::optional<project::EventOccurrence> occurrence_;
  std::optional<std::string> selected_entity_id_;
  std::optional<std::string> selected_time_point_id_;
  std::optional<std::string> selected_participant_id_;
  std::optional<std::string> selected_presence_id_;
  std::optional<std::string> selected_work_scope_id_;
  std::optional<std::string> selected_editorial_reference_id_;
  std::optional<std::string> selected_relation_id_;
  std::optional<std::string> related_entity_filter_id_;
  struct NavigationState {
    application::PlanningContext context;
    std::optional<std::string> entity_id;
    std::optional<std::string> time_point_id;
    std::string page_name;
  };
  std::vector<NavigationState> navigation_backstack_;
  std::vector<NavigationState> navigation_forwardstack_;
  std::size_t offset_{};
  bool refreshing_context_{};
  bool refreshing_facets_{};
  bool refreshing_axis_{};
  bool refreshing_temporal_filter_{};
  bool filters_visible_{};
  bool relation_filters_visible_{};
  bool detail_visible_{};
  bool restoring_navigation_{};
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
  sigc::signal<void(const std::string &)> signal_open_document_requested_;
  sigc::signal<void(const std::string &)> signal_filter_documents_requested_;
  sigc::signal<void(const std::string &)> signal_cartography_requested_;
};

} // namespace inde::ui

#pragma once
#include "inde/ui/composition_page.hpp"

#include "inde/application/project_service.hpp"
#include "inde/application/text_analysis_coordinator.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/editor_interaction.hpp"
#include "inde/application/inline_commands.hpp"
#include "inde/application/document_recovery_coordinator.hpp"
#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/view_state.hpp"

#include <gtkmm.h>
#include <optional>
#include <future>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace inde::ui {
class KeyboardCommands;

// Editor consolidation: one stable layout shared by every production UI unit.
class WritingWorkspace final : public Gtk::Box {
public:
  explicit WritingWorkspace(application::ProjectService &service);
  ~WritingWorkspace() override;

  void install_actions(Gtk::ApplicationWindow &window);
  void install_keyboard(KeyboardCommands &);
  std::vector<Gtk::Widget *> keyboard_regions();
  void leave_view();
  bool composing_input() const { return ime_active_ || ime_commit_pending_; }
  void refresh();
  void reset();
  void flush_changes();
  [[nodiscard]] project::Document apply_editorial_change(const project::Document &expected,
                                                        const project::Document &after,const std::string &label);
  void suggest_editorial_change();
  void reveal_document(const std::string &id);
  void reveal_realization(const application::BuilderRealization &);
  void reveal_usage_offset(std::size_t offset);
  void open_removal_use_tools(const project::RemovalUse &);
  void show_library_for_editorial_node(const std::string &node_id);
  void show_library_for_entity(const std::string &entity_id);
  void create_document_for_editorial_node(const std::string &node_id);
  void set_focus_mode(bool focused);
  [[nodiscard]] ViewState capture_view();
  void restore_view(const ViewState &);
  [[nodiscard]] std::string current_document_id() const {
    return current_document_ ? current_document_->id : std::string{};
  }
  void review_document(const std::string &id);
  [[nodiscard]] sigc::signal<void(const Glib::ustring &)> &
  signal_status_message() {
    return signal_status_message_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_entity_source_requested() {
    return signal_entity_source_requested_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_editorial_source_requested() {
    return signal_editorial_source_requested_;
  }
  sigc::signal<void(const project::RemovalUse &,const ViewState &)> &signal_narrative_source_requested() {return signal_narrative_source_requested_;}

private:
  struct InteractionContext {
    application::DocumentEditState state;
    application::TextRange range;
    std::string project_id;
    std::uint64_t epoch{},revision{};
  };
  [[nodiscard]] InteractionContext interaction_context(bool selection=true);
  void require_interaction_context(const InteractionContext &);
  void show_text_entity(bool create, bool materialization=false);
  void show_text_relation();
  void show_narrative_association();
  void show_narrative_uses(bool composition = false);
  void show_semantic_distribution();
  void preview_project_inline(InteractionContext,application::TextRange,
      project::NarrativeEntity,project::EntityType);
  void commit_text_date(const InteractionContext &, project::Document,
                        const project::TextDateCreation &);
  void commit_text_entity(const InteractionContext &,project::Document,
      const project::TextEntityCreation &);
  std::vector<project::NarrativeEntity> inline_project_entities(const std::string &);
  void append_project_inline(const application::InlineQuery &);
  struct AnchorMarks {
    std::string id;
    std::string label;
    Glib::RefPtr<Gtk::TextBuffer::Mark> start;
    Glib::RefPtr<Gtk::TextBuffer::Mark> end;
  };

  void build_ui();
  void build_formatting_tags();
  void build_editor_controls();
  void apply_editor_format(project::DocumentTextStyle, const std::string &value={});
  void apply_paragraph_body();
  void show_text_color();
  void show_tool_section(const std::string &);
  void show_inline_commands(bool manager=false);
  void edit_inline_command(std::optional<std::size_t> index={});
  void schedule_inline_suggestions();
  void refresh_inline_suggestions();
  void close_inline_suggestions();
  void refresh_library();
  void refresh_library_filter_options();
  void toggle_library_filters();
  void dismiss_library_filters();
  void clear_library_filters();
  void create_document_group();
  void edit_current_group();
  void delete_current_group();
  void open_document_group(const std::string &id);
  void leave_document_group();
  void edit_document_organization();
  [[nodiscard]] persistence::DocumentQuery library_query() const;
  void refresh_placement_options();
  void show_library();
  void open_document(const std::string &id);
  void remember_document_position();
  void restore_document_position();
  void schedule_position_save();
  void export_document();
  sigc::connection position_save_, position_restore_;
  void create_document();
  void create_document_at(std::optional<std::string> editorial_node_id);
  void save_document();
  void delete_document();

  void apply_text_style(project::DocumentTextStyle style);
  void clear_text_formatting();
  void refresh_formatting_toolbar();
  [[nodiscard]] bool style_active_at_selection(
      project::DocumentTextStyle style, bool &mixed);
  [[nodiscard]] std::vector<project::DocumentFormatSpan> capture_formatting();
  void restore_formatting(
      const std::vector<project::DocumentFormatSpan> &formatting);
  [[nodiscard]] Glib::RefPtr<Gtk::TextTag>
  tag_for_style(project::DocumentTextStyle style) const;

  void toggle_tools();
  void refresh_text_tools();
  void apply_text_statistics(project::DocumentTextStatistics statistics);
  void invalidate_text_analysis();
  void update_goal_state();
  void navigate_to_offset(std::size_t offset);
  void refresh_long_paragraphs(
      const std::vector<project::LongParagraph> &paragraphs);
  void refresh_anchor_list();
  void add_anchor();
  void rename_anchor();
  void remove_anchor();
  void select_anchor(std::string id);
  void open_selected_anchor();
  void clear_anchor_marks();
  void
  restore_anchor_marks(const std::vector<project::DocumentAnchor> &anchors);
  [[nodiscard]] std::vector<project::DocumentAnchor> capture_anchors();
  [[nodiscard]] std::optional<std::string>
  selected_anchor_label(const std::optional<std::string> &id) const;

  void refresh_entity_references();
  void add_entity_reference();
  void remove_entity_reference();
  void select_entity_reference(std::string id);
  void open_reference_entity();
  void show_semantic_dictionary();
  void show_linguistic_entities();
  void show_text_references(bool incoming = false,
                            std::optional<std::string> anchor_id = std::nullopt,
                            std::size_t offset = 0);
  void add_text_reference();
  void build_search_ui();
  void open_find(bool replace);
  void close_find();
  void invalidate_find();
  void schedule_find();
  void request_find();
  void select_find_match(std::size_t index, bool focus = true);
  void navigate_find(bool previous);
  void replace_find(bool all);
  bool apply_edit_transaction(application::DocumentEditState after,
                               const std::string &label);
  [[nodiscard]] application::DocumentEditState capture_edit_state();
  void restore_edit_state(const application::DocumentEditState &state);

  [[nodiscard]] project::Document capture_editor_state();
  void restore_editor_state(const project::Document &state, bool incremental = false);
  void mark_dirty();
  void schedule_autosave();
  void queue_autosave();
  void receive_save_result();
  void build_recovery_ui();
  void build_semantic_ui();
  void show_semantic_layers(std::size_t offset=0);
  void show_semantic_annotation(const std::string &id);
  void register_semantic_type();
  void edit_semantic_fields(std::optional<std::string> annotation_id={},std::optional<std::string> type_key={},project::AnnotationScope scope=project::AnnotationScope::Interval);
  void show_document_perspective();
  void edit_semantic_time(const std::string &annotation);
  void edit_semantic_dates(const std::string &annotation);
  void show_temporal_notices();
  std::string semantic_time_label(const project::SemanticTemporalBinding &) const;
  void show_semantic_uses(persistence::SemanticUseQuery query={});
  void render_semantic_uses(Gtk::Box &, persistence::SemanticUseQuery);
  bool semantic_tooltip(int, int, bool, const Glib::RefPtr<Gtk::Tooltip> &);
  void edit_semantic_relations(const std::string&source, std::size_t offset=0);
  void pick_semantic_relation(const std::string &source, bool project_scope=false,
                             std::size_t offset=0, const std::string &role={});
  void preview_semantic_relation(const std::string &source, const std::string &target,
                                const std::string &target_document);
  void refresh_semantic_lenses();
  void semantic_before_edit(std::size_t start,std::size_t end,const std::string&replacement);
  void apply_semantic_document(project::Document,const std::string&label);
  Gtk::Button semantic_button_{"Marcações do texto"};
  Gtk::ToggleButton semantic_lens_button_{"Realçar marcações"};
  Gtk::Button semantic_mark_button_{"Marcar seleção"};
  Gtk::Box semantic_page_{Gtk::Orientation::VERTICAL, 12};
  struct SemanticExplorerFilter {
    std::string search, role, status, anchor;
    bool issues_only{};
  } semantic_filter_;
  bool semantic_project_scope_{};
  persistence::SemanticUseQuery semantic_use_filter_;
  std::string semantic_explorer_document_id_;
  std::size_t semantic_offset_{};
  std::unordered_map<std::string,Glib::RefPtr<Gtk::TextTag>> semantic_tags_;
  void begin_recovery();
  void build_ime_guard();
  void on_preedit_changed(const char *);
  void require_completed_composition() const;
  bool ime_active_{}, ime_commit_pending_{};
  std::uint64_t ime_start_revision_{};
  sigc::connection ime_commit_delivery_;
  void show_recovery_retention(unsigned keep_days=30);
  void render_recovery_retention(const project::Project &, unsigned,
                                project::RecoveryRetentionPage);
  void run_recovery_maintenance(std::function<std::string()>);
  std::future<std::string> recovery_maintenance_task_;
  std::future<project::RecoveryRetentionPage> recovery_inventory_task_;
  sigc::connection recovery_maintenance_delivery_;
  void end_recovery();
  void schedule_recovery();
  void queue_recovery(bool force=false);
  void receive_recovery_result();
  void update_recovery_status();
  void refresh_recovery_notice();
  void show_recovery_records();
  void preview_recovery(const std::string &session);
  struct ClipboardRead;
  void build_clipboard_ui();
  void copy_selection(bool styles, bool cut = false);
  void paste_clipboard(bool styles);
  void read_clipboard_chunk(const std::shared_ptr<ClipboardRead> &request);
  void invalidate_clipboard();
  void commit_undo_group();
  void undo();
  void redo();
  void update_undo_actions();
  void update_editor_status();
  void show_error(const Glib::ustring &title, const std::exception &error);
  [[nodiscard]] Gtk::Window *owner_window();
  [[nodiscard]] std::string
  placement_label(const std::optional<std::string> &node_id) const;
  [[nodiscard]] std::string entity_label(const std::string &entity_id) const;
  [[nodiscard]] static std::string
  search_match_label(project::DocumentSearchMatch match);

  application::ProjectService &service_;
  Gtk::Stack page_stack_;
  CompositionPage composition_page_{service_};
  Gtk::Button compositions_button_{"Composições…"};

  Gtk::Box library_page_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box library_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Box library_heading_{Gtk::Orientation::VERTICAL, 2};
  Gtk::Label library_title_{"Documentos"};
  Gtk::Label library_summary_;
  Gtk::Label library_context_;
  Gtk::Button library_filters_button_{"Pesquisa e filtros"};
  Gtk::Button leave_group_button_{"← Biblioteca"};
  Gtk::Button new_group_button_{"Novo grupo"};
  Gtk::MenuButton group_actions_button_;
  Gtk::Button new_document_button_{"Novo Documento"};
  Gtk::Overlay library_overlay_;
  Gtk::Stack library_results_stack_;
  Gtk::ScrolledWindow library_scroll_;
  Gtk::FlowBox document_list_;
  Gtk::Box empty_state_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label empty_title_{"Comece a escrever"};
  Gtk::Label empty_message_{
      "Crie um Documento livre agora. Você pode situá-lo na organização do manuscrito quando isso for útil."};
  Gtk::Revealer library_filters_revealer_;
  Gtk::Frame library_filters_surface_;
  Gtk::Box library_filters_panel_{Gtk::Orientation::VERTICAL, 10};
  Gtk::SearchEntry search_;
  Gtk::ComboBoxText placement_filter_;
  Gtk::ComboBoxText group_filter_;
  Gtk::ComboBoxText purpose_filter_;
  Gtk::Entry perspective_filter_;
  Gtk::CheckButton revisions_filter_{"Somente versões de revisão dos documentos"};
  IncrementalSelector *entity_filter_{};
  Gtk::Box library_filter_actions_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button clear_library_filters_button_{"Limpar"};
  Gtk::Button apply_library_filters_button_{"Ver resultados"};

  Gtk::Box editor_page_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box editor_toolbar_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::MenuButton format_menu_button_, context_menu_button_, document_menu_button_;
  Gtk::Popover format_popover_, context_popover_, document_popover_;
  Gtk::ScrolledWindow context_menu_scroll_;
  Gtk::Box document_menu_{Gtk::Orientation::VERTICAL, 8};
  bool focus_mode_{};
  Gtk::Button back_button_{"← Voltar"};
  Gtk::Button tools_button_{"Ferramentas"};
  Gtk::Button organization_button_{"Detalhes"};
  Gtk::Button save_button_{"Salvar"};
  Gtk::Button delete_button_{"Excluir"};
  Gtk::Entry title_entry_;
  Gtk::ComboBoxText placement_;
  Gtk::Button open_placement_button_{"Abrir Unidade"};
  Gtk::Box formatting_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Box contextual_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::ComboBoxText font_family_, paragraph_style_;
  Gtk::Entry font_size_;
  Gtk::Button size_down_{"−"}, size_up_{"+"};
  Gtk::Button align_left_{"⟸"}, align_center_{"⟺"}, align_right_{"⟹"}, align_justify_{"☰"};
  Gtk::Button color_button_{"A"}, cut_button_{"✂"}, consult_button_{"Consultar"};
  Gtk::Button anchors_button_{"Trechos marcados"}, entities_button_{"Elementos da história"}, comment_button_{"Comentar"};
  Gtk::Button statistics_button_{"Estatísticas"}, goal_button_{"Meta"}, repeated_button_{"Palavras repetidas"};
  Gtk::Button long_paragraphs_button_{"Parágrafos Longos"}, inline_button_{"⌘"};
  bool refreshing_format_controls_{};
  std::string active_tool_section_;
  struct ValueTag {project::DocumentTextStyle style; std::string value; Glib::RefPtr<Gtk::TextTag> tag;};
  std::vector<ValueTag> value_tags_;
  Glib::RefPtr<Gtk::TextTag> value_tag(project::DocumentTextStyle,const std::string &);
  Gtk::Popover inline_popover_;
  Gtk::Popover *editor_context_popover_{};
  Gtk::Box inline_suggestions_{Gtk::Orientation::VERTICAL, 2};
  sigc::connection inline_debounce_;
  Gtk::Revealer find_revealer_;
  Gtk::Box find_panel_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Box find_row_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Box replace_row_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Entry find_query_;
  Gtk::Entry find_replacement_;
  Gtk::CheckButton find_case_{"Diferenciar maiúsculas"};
  Gtk::CheckButton find_word_{"Palavra inteira"};
  Gtk::Button find_button_{"Localizar"};
  Gtk::MenuButton clipboard_button_;
  Gtk::Button recovery_library_button_{"Recuperação"};
  Gtk::Button recovery_editor_button_{"Recuperação"};
  Gtk::Label recovery_notice_, recovery_status_;
  Gtk::Button find_previous_{"Anterior"};
  Gtk::Button find_next_{"Próximo"};
  Gtk::Button find_close_{"Fechar busca"};
  Gtk::Button find_replace_mode_{"Substituir…"};
  Gtk::Button replace_one_{"Substituir"};
  Gtk::Button replace_all_{"Substituir tudo"};
  Gtk::Label find_status_;
  Gtk::Button undo_button_{"⮐"};
  Gtk::Button redo_button_{"⮑"};
  Gtk::Button bold_button_{"B"};
  Gtk::Button italic_button_{"I"};
  Gtk::Button underline_button_{"U"};
  Gtk::Button strike_button_{"S"};
  Gtk::Button heading_button_{"Título"};
  Gtk::Button subheading_button_{"Subtítulo"};
  Gtk::Button quote_button_{"Citação"};
  Gtk::Button clear_format_button_{"Limpar"};
  Gtk::Overlay editor_body_overlay_;
  Gtk::Overlay editor_surface_;
  Gtk::ScrolledWindow editor_scroll_;
  Gtk::DrawingArea editor_size_observer_;
  Gtk::TextView editor_;
  Gtk::Revealer tools_revealer_;
  Gtk::Frame tools_surface_;
  Gtk::ScrolledWindow tools_scroll_;
  Gtk::Box tools_panel_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box stats_grid_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label stats_title_{"Estatísticas do texto"};
  Gtk::Label stats_words_;
  Gtk::Label stats_characters_;
  Gtk::Label stats_structure_;
  Gtk::Label stats_reading_time_;
  Gtk::Separator goal_separator_;
  Gtk::Label goal_title_{"Meta do Documento"};
  Gtk::CheckButton goal_enabled_{"Usar meta de palavras"};
  Gtk::SpinButton word_goal_;
  Gtk::ProgressBar goal_progress_;
  Gtk::Separator repeated_separator_;
  Gtk::Label repeated_title_{"Palavras repetidas"};
  Gtk::Label repeated_words_;
  Gtk::Separator linguistic_separator_;
  Gtk::Label linguistic_title_{"Dicionário"};
  Gtk::Label linguistic_summary_{
      "Consulta explícita e somente leitura; nenhuma interpretação é "
      "escolhida automaticamente."};
  Gtk::Button semantic_dictionary_button_{"Consultar palavra selecionada"};
  Gtk::Button linguistic_entities_button_{"Construções e cenários de significado"};
  Gtk::Separator paragraphs_separator_;
  Gtk::Label paragraphs_title_{"Parágrafos longos"};
  Gtk::ListBox long_paragraphs_list_;
  Gtk::Separator anchors_separator_;
  Gtk::Label anchors_title_{"Trechos marcados"};
  Gtk::Box anchor_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button add_anchor_button_{"Marcar trecho"};
  Gtk::Button rename_anchor_button_{"Renomear"};
  Gtk::Button remove_anchor_button_{"Remover"};
  Gtk::ListBox anchors_list_;
  Gtk::Button text_references_button_{"Referências textuais"};
  Gtk::Button anchor_usages_button_{"Onde este trecho é usado"};
  Gtk::Separator references_separator_;
  Gtk::Label references_title_{"Elementos da história usadas"};
  Gtk::Box reference_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button add_reference_button_{"Vincular elemento da história"};
  Gtk::Button open_reference_button_{"Abrir"};
  Gtk::Button remove_reference_button_{"Desvincular"};
  Gtk::ListBox references_list_;
  Gtk::Box editor_footer_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label editor_status_;
  Gtk::Label word_count_;

  std::vector<std::pair<project::DocumentTextStyle, Glib::RefPtr<Gtk::TextTag>>>
      formatting_tags_;
  std::vector<project::DocumentSummary> documents_;
  std::vector<std::pair<std::string, std::string>> placement_options_;
  std::unordered_map<std::string, std::string> placement_labels_;
  std::vector<project::DocumentGroup> document_groups_;
  std::optional<std::string> current_group_id_;
  project::DocumentTextStatistics current_statistics_;
  std::optional<project::Document> current_document_;
  std::vector<AnchorMarks> anchor_marks_;
  std::vector<project::DocumentEntityReference> entity_references_;
  std::optional<std::string> selected_anchor_id_;
  std::optional<std::string> selected_entity_reference_id_;
  application::DocumentEditSession edit_session_;
  application::DocumentSaveCoordinator save_worker_;
  application::DocumentRecoveryCoordinator recovery_worker_;
  std::string recovery_session_, recovery_error_;
  std::uint64_t saved_revision_{}, checkpoint_submitted_{}, checkpoint_confirmed_{};
  bool checkpoint_requested_{};
  sigc::connection recovery_debounce_, recovery_periodic_, recovery_delivery_;
  std::optional<project::Document> saved_document_;
  std::uint64_t edit_epoch_{}, edit_revision_{}, submitted_revision_{};
  bool save_submitted_{};
  std::string save_error_;
  sigc::connection save_delivery_;
  std::shared_ptr<int> clipboard_lifetime_{std::make_shared<int>(0)};
  Glib::RefPtr<Gio::Cancellable> clipboard_cancel_;
  std::uint64_t clipboard_generation_{};
  application::TextAnalysisCoordinator find_analysis_;
  application::TextSearchResult find_result_;
  std::size_t find_index_{};
  sigc::connection find_debounce_, find_delivery_;
  sigc::connection undo_group_connection_;
  sigc::connection autosave_connection_;
  sigc::connection analysis_connection_;
  sigc::connection analysis_delivery_connection_;
  application::TextAnalysisCoordinator text_analysis_;
  guint editor_frame_tick_{};
  std::chrono::steady_clock::time_point editor_change_started_;
  bool loading_editor_{};
  bool refreshing_library_controls_{};
  bool restoring_undo_{};
  bool library_filters_visible_{};
  bool tools_visible_{};
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
  sigc::signal<void(const std::string &)> signal_entity_source_requested_;
  sigc::signal<void(const std::string &)> signal_editorial_source_requested_;
  sigc::signal<void(const project::RemovalUse &,const ViewState &)> signal_narrative_source_requested_;
};

} // namespace inde::ui

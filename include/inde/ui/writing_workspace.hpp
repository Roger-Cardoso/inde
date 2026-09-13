#pragma once

#include "inde/application/project_service.hpp"
#include "inde/ui/incremental_selector.hpp"

#include <gtkmm.h>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace inde::ui {

class WritingWorkspace final : public Gtk::Box {
public:
  explicit WritingWorkspace(application::ProjectService &service);

  void install_actions(Gtk::ApplicationWindow &window);
  void refresh();
  void reset();
  void flush_changes();
  void reveal_document(const std::string &id);
  void show_library_for_editorial_node(const std::string &node_id);
  void show_library_for_entity(const std::string &entity_id);
  void create_document_for_editorial_node(const std::string &node_id);
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

private:
  struct AnchorMarks {
    std::string id;
    std::string label;
    Glib::RefPtr<Gtk::TextBuffer::Mark> start;
    Glib::RefPtr<Gtk::TextBuffer::Mark> end;
  };

  void build_ui();
  void build_formatting_tags();
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
  void show_text_references(bool incoming = false,
                            std::optional<std::string> anchor_id = std::nullopt,
                            std::size_t offset = 0);
  void add_text_reference();

  [[nodiscard]] project::Document capture_editor_state();
  void restore_editor_state(const project::Document &state);
  void mark_dirty();
  void schedule_autosave();
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
      "Crie um Documento livre agora. Você pode situá-lo na estrutura "
      "editorial quando isso for útil."};
  Gtk::Revealer library_filters_revealer_;
  Gtk::Frame library_filters_surface_;
  Gtk::Box library_filters_panel_{Gtk::Orientation::VERTICAL, 10};
  Gtk::SearchEntry search_;
  Gtk::ComboBoxText placement_filter_;
  Gtk::ComboBoxText group_filter_;
  Gtk::ComboBoxText purpose_filter_;
  Gtk::Entry perspective_filter_;
  Gtk::CheckButton revisions_filter_{"Somente revisões documentais"};
  IncrementalSelector *entity_filter_{};
  Gtk::Box library_filter_actions_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button clear_library_filters_button_{"Limpar"};
  Gtk::Button apply_library_filters_button_{"Ver resultados"};

  Gtk::Box editor_page_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box editor_toolbar_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button back_button_{"← Documentos"};
  Gtk::Button tools_button_{"Ferramentas"};
  Gtk::Button organization_button_{"Organização"};
  Gtk::Button save_button_{"Salvar"};
  Gtk::Button delete_button_{"Remover"};
  Gtk::Entry title_entry_;
  Gtk::ComboBoxText placement_;
  Gtk::Button open_placement_button_{"Abrir unidade"};
  Gtk::Box formatting_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button undo_button_{"Desfazer"};
  Gtk::Button redo_button_{"Refazer"};
  Gtk::Button bold_button_{"B"};
  Gtk::Button italic_button_{"I"};
  Gtk::Button underline_button_{"U"};
  Gtk::Button strike_button_{"S"};
  Gtk::Button heading_button_{"Título"};
  Gtk::Button subheading_button_{"Subtítulo"};
  Gtk::Button quote_button_{"Citação"};
  Gtk::Button clear_format_button_{"Limpar formato"};
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
  Gtk::Label repeated_title_{"Palavras recorrentes"};
  Gtk::Label repeated_words_;
  Gtk::Separator paragraphs_separator_;
  Gtk::Label paragraphs_title_{"Parágrafos longos"};
  Gtk::ListBox long_paragraphs_list_;
  Gtk::Separator anchors_separator_;
  Gtk::Label anchors_title_{"Âncoras textuais"};
  Gtk::Box anchor_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button add_anchor_button_{"Nova âncora"};
  Gtk::Button rename_anchor_button_{"Renomear"};
  Gtk::Button remove_anchor_button_{"Remover"};
  Gtk::ListBox anchors_list_;
  Gtk::Button text_references_button_{"Referências textuais"};
  Gtk::Button anchor_usages_button_{"Usos desta âncora"};
  Gtk::Separator references_separator_;
  Gtk::Label references_title_{"Entidades usadas"};
  Gtk::Box reference_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button add_reference_button_{"Vincular entidade"};
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
  std::vector<project::Document> undo_stack_;
  std::vector<project::Document> redo_stack_;
  std::optional<project::Document> undo_baseline_;
  sigc::connection undo_group_connection_;
  sigc::connection autosave_connection_;
  sigc::connection analysis_connection_;
  bool loading_editor_{};
  bool refreshing_library_controls_{};
  bool restoring_undo_{};
  bool dirty_{};
  bool library_filters_visible_{};
  bool tools_visible_{};
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
  sigc::signal<void(const std::string &)> signal_entity_source_requested_;
  sigc::signal<void(const std::string &)> signal_editorial_source_requested_;
};

} // namespace inde::ui

#pragma once

#include "inde/application/project_service.hpp"

#include <gtkmm.h>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace inde::ui {

class StructuralNodeItem final : public Glib::Object {
public:
  static Glib::RefPtr<StructuralNodeItem> create(project::StructuralNode node);
  [[nodiscard]] const project::StructuralNode &node() const noexcept {
    return node_;
  }

protected:
  explicit StructuralNodeItem(project::StructuralNode node);

private:
  project::StructuralNode node_;
};

class StructuralEditor final : public Gtk::Box {
public:
  explicit StructuralEditor(application::ProjectService &service);

  void install_actions(Gtk::ApplicationWindow &window);
  void set_work(std::optional<std::string> work_id);
  void refresh();
  void reset();
  void show_structure();
  void reveal_node(const std::string &node_id);
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
  signal_create_document_requested() {
    return signal_create_document_requested_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_filter_documents_requested() {
    return signal_filter_documents_requested_;
  }

private:
  void build_ui();
  void show_structure_page();
  void show_detail_page();
  void dismiss_filters();
  void setup_structure_model();
  void capture_structure_view_state();
  void restore_structure_view_state();
  Glib::RefPtr<Gio::ListModel>
  create_child_model(const Glib::RefPtr<Glib::ObjectBase> &item);
  void on_structure_selection_changed();
  void update_inspector();
  void refresh_editorial_entity_type_facets();
  void refresh_filter_summary();
  void refresh_editorial_references();
  void refresh_documents();
  void add_editorial_reference();
  void edit_editorial_reference();
  void delete_editorial_reference();
  void select_editorial_reference(std::string id);
  void create_selected_template();
  void duplicate_selected_branch();
  void refresh_structural_types();
  void create_structural_type();
  void edit_structural_type();
  void delete_structural_type();
  bool is_structure_node_visible(const project::StructuralNode &node) const;
  void create_structural_node(std::optional<std::string> parent_id);
  void edit_structural_node(std::string id);
  void delete_structural_node(std::string id);
  void show_error(const Glib::ustring &title, const std::exception &error);
  [[nodiscard]] Gtk::Window *owner_window();

  application::ProjectService &service_;
  Gtk::Stack page_stack_;
  Gtk::Box content_area_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box detail_page_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box detail_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button back_to_structure_button_{"← Voltar à estrutura"};
  Gtk::Label detail_hint_{"Detalhe da unidade editorial"};
  Gtk::Stack structure_stack_;
  Gtk::Box empty_structure_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label empty_structure_title_{"Estrutura editorial"};
  Gtk::Label empty_structure_hint_{
      "Crie e selecione uma Obra para organizar sua estrutura editorial."};
  Gtk::Box structure_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box structure_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label structure_title_{"Estrutura editorial"};
  Gtk::Label structure_hint_{"Selecione uma obra"};
  Gtk::Button filters_button_{"Filtros"};
  Gtk::Label structure_filter_summary_;
  Gtk::Overlay structure_overlay_;
  Gtk::Revealer filters_revealer_;
  Gtk::Frame filters_surface_;
  Gtk::Box filters_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::SearchEntry structure_search_;
  Gtk::Label structural_type_filter_label_{"Tipo de unidade"};
  Gtk::ComboBoxText structural_type_filter_;
  Gtk::Label status_filter_label_{"Status editorial"};
  Gtk::ComboBoxText status_filter_;
  Gtk::Label entity_type_facets_label_{"Entidades apresentadas"};
  Gtk::ScrolledWindow entity_type_facets_scroll_;
  Gtk::Box entity_type_facets_{Gtk::Orientation::VERTICAL, 2};
  Gtk::Button clear_filters_button_{"Limpar filtros"};
  Gtk::Button apply_filters_button_{"Ver estrutura"};
  Gtk::ScrolledWindow structure_toolbar_scroll_;
  Gtk::FlowBox structure_toolbar_;
  Gtk::Box filter_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label filter_section_title_{"Pesquisa e filtros"};
  Gtk::Box filter_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Box crud_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label crud_section_title_{"Elementos"};
  Gtk::Box crud_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button add_root_button_{"Novo elemento raiz"};
  Gtk::Button structure_add_child_button_{"Novo filho"};
  Gtk::Button structure_edit_button_{"Editar"};
  Gtk::Button structure_duplicate_button_{"Duplicar"};
  Gtk::Button structure_remove_button_{"Remover"};
  Gtk::MenuButton structure_actions_button_;
  Gtk::Box type_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label type_section_title_{"Tipos de elemento"};
  Gtk::Box type_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::ComboBoxText structural_type_manager_;
  Gtk::Button structural_type_add_button_{"Novo tipo"};
  Gtk::Button structural_type_edit_button_{"Editar"};
  Gtk::Button structural_type_remove_button_{"Remover"};
  Gtk::Box position_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label position_section_title_{"Posição"};
  Gtk::Box position_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button structure_up_button_{"Mover para cima"};
  Gtk::Button structure_down_button_{"Mover para baixo"};
  Gtk::Box template_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label template_section_title_{"Templates"};
  Gtk::Box template_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::ComboBoxText structure_template_combo_;
  Gtk::Button structure_template_button_{"Aplicar template"};
  Gtk::Box recovery_section_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label recovery_section_title_{"Recuperação"};
  Gtk::Box recovery_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button structure_restore_button_{"Desfazer exclusão"};
  Gtk::ScrolledWindow structure_scroll_;
  Gtk::ListView structure_list_;
  Gtk::Box inspector_panel_{Gtk::Orientation::VERTICAL, 10};
  Gtk::ScrolledWindow inspector_scroll_;
  Gtk::Label inspector_title_{"Detalhe do elemento"};
  Gtk::Frame inspector_summary_surface_;
  Gtk::Overlay inspector_profile_overlay_;
  Gtk::Box inspector_summary_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Overlay inspector_cover_overlay_;
  Gtk::Box inspector_cover_banner_{Gtk::Orientation::VERTICAL};
  Gtk::Picture inspector_cover_;
  Gtk::Box inspector_identity_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Label inspector_type_;
  Gtk::Label inspector_name_;
  Gtk::Label inspector_status_;
  Gtk::Label inspector_synopsis_;
  Gtk::Button inspector_edit_button_{"Editar selecionado"};
  Gtk::MenuButton inspector_actions_button_;
  Gtk::Separator reference_separator_;
  Gtk::Frame direct_references_surface_;
  Gtk::Box direct_references_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label reference_title_{"Entidades apresentadas"};
  Gtk::Box reference_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button reference_add_button_{"Adicionar"};
  Gtk::Button reference_edit_button_{"Editar"};
  Gtk::Button reference_remove_button_{"Remover"};
  Gtk::MenuButton reference_actions_button_;
  Gtk::ListBox reference_list_;
  Gtk::Frame inherited_references_surface_;
  Gtk::Box inherited_references_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label inherited_references_title_{
      "Entidades herdadas de elementos filhos"};
  Gtk::Label inherited_references_hint_{
      "Cada referência indica o elemento em que foi apresentada."};
  Gtk::ListBox inherited_references_list_;
  Gtk::Frame documents_surface_;
  Gtk::Box documents_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label documents_title_{"Documentos desta unidade"};
  Gtk::Label documents_hint_;
  Gtk::Box documents_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button create_document_button_{"Novo Documento"};
  Gtk::Button filter_documents_button_{"Ver na Biblioteca"};
  Gtk::ListBox documents_list_;
  Glib::RefPtr<Gio::ListStore<StructuralNodeItem>> structure_root_model_;
  Glib::RefPtr<Gtk::TreeListModel> structure_tree_model_;
  Glib::RefPtr<Gtk::SingleSelection> structure_selection_;
  Glib::RefPtr<Gtk::SignalListItemFactory> structure_factory_;
  std::vector<project::StructuralNode> structure_model_nodes_;
  std::vector<project::StructuralElementType> structural_types_;
  struct StructureViewState {
    std::unordered_set<std::string> expanded_node_ids;
    double scroll_value{};
    bool initialized{};
  };
  std::unordered_map<std::string, StructureViewState> structure_view_states_;
  std::optional<std::string> rendered_work_id_;
  std::optional<std::string> selected_work_id_;
  std::optional<std::string> selected_structural_node_id_;
  std::optional<std::string> selected_editorial_reference_id_;
  std::vector<project::EditorialEntityReference> editorial_references_;
  std::string structure_filter_;
  std::vector<std::string> selected_entity_type_ids_;
  std::unordered_set<std::string> matching_editorial_node_ids_;
  bool filters_visible_{};
  bool refreshing_filters_{};
  bool refreshing_structure_model_{};
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
  sigc::signal<void(const std::string &)> signal_open_document_requested_;
  sigc::signal<void(const std::string &)> signal_create_document_requested_;
  sigc::signal<void(const std::string &)> signal_filter_documents_requested_;
};

} // namespace inde::ui

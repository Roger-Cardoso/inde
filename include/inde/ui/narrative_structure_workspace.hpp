#pragma once
#include <map>

#include "inde/application/project_service.hpp"
#include "inde/ui/view_state.hpp"

#include <gtkmm.h>
#include <optional>
#include <string>
#include <vector>

namespace inde::ui {
class KeyboardCommands;

class NarrativeStructureWorkspace final : public Gtk::Box {
public:
  explicit NarrativeStructureWorkspace(application::ProjectService &service);
  void refresh();
  void install_keyboard(KeyboardCommands &);
  void reset();
  void reveal_removal_use(const project::RemovalUse &);
  ViewState capture_view() const;
  void restore_view(const ViewState &);
  sigc::signal<void(const std::string &)> &signal_write_requested() {
    return signal_write_requested_;
  }
  sigc::signal<void(const application::BuilderRealization &)> &signal_realization_requested() { return signal_realization_requested_; }
  [[nodiscard]] sigc::signal<void(const Glib::ustring &)> &
  signal_status_message() {
    return signal_status_message_;
  }

private:
  void build_ui();
  void refresh_works();
  void refresh_library();
  void refresh_detail();
  void refresh_roles();
  void show_library();
  void show_detail(std::string id);
  void show_roles();
  void create_blank();
  void instantiate_model();
  void duplicate_selected(bool derived);
  void choose_comparison();
  void render_comparison();
  void choose_dependency_scope();
  void start_work();
  void start_comparison_selection(const std::string &);
  void toggle_comparison_target(const std::string &);
  void cancel_comparison_selection();
  void confirm_comparison_selection();
  void update_comparison_selection();
  void add_structure_row(const project::NarrativeStructure &, const std::string &);
  void add_model_row(const project::StructureModel &);
  sigc::signal<void()> signal_catalog_changed_;
public:
  sigc::signal<void()> &signal_catalog_changed() { return signal_catalog_changed_; }
private:
  std::string selection_source_;
  std::string selection_source_name_;
  std::vector<std::string> selection_targets_;
  std::vector<std::string> comparison_targets_;
  Gtk::ComboBoxText comparison_target_combo_;
  bool changing_comparison_target_{};
  bool updating_selection_{};
  std::map<std::string,Gtk::CheckButton *> comparison_checks_;
  application::BuilderComparison comparison_;
  std::string comparison_source_, comparison_target_;
  std::size_t comparison_page_index_{};
  Gtk::Box comparison_page_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box comparison_header_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button comparison_back_{"← Alternativas"};
  Gtk::Button comparison_previous_{"Página anterior"},
      comparison_next_{"Próxima página"};
  Gtk::Label comparison_title_;
  Gtk::ScrolledWindow comparison_scroll_;
  Gtk::Box comparison_body_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Button dependency_scope_button_{"A qual ordem esta regra se aplica?…"};
  bool refreshing_detail_{};
  void capture_model();
  void delete_model();
  void activate_selected();
  void delete_selected();
  void add_line();
  void add_unit();
  void build_unit_page();
  void show_unit();
  void edit_unit();
  void edit_line();
  void refresh_unit();
  void move_unit(int);
  void choose_location();
  void choose_relation();
  void choose_document(bool create, bool prefer_existing = false);
  void choose_text_target(const std::string &replace = {});
  void realization_actions(const application::BuilderRealization &);
  void show_continuity();
  void render_continuity();
  void justify_notice(application::BuilderNotice);
  Gtk::Box continuity_page_{Gtk::Orientation::VERTICAL,8}, continuity_body_{Gtk::Orientation::VERTICAL,10};
  Gtk::ScrolledWindow continuity_scroll_;
  Gtk::Label continuity_title_;
  Gtk::Button continuity_back_{"← Planejamento"},continuity_previous_{"Anteriores"},continuity_next_{"Próximos"};
  Gtk::ComboBoxText continuity_filter_;
  std::vector<application::BuilderNotice> continuity_notices_;
  std::size_t continuity_page_index_{};
  std::string continuity_origin_{"unit"};
  bool refreshing_works_{};
  application::NarrativeBuilderSnapshot builder_snapshot_;
  std::string displayed_unit_id_;
  Gtk::ComboBoxText order_combo_;
  Gtk::Button open_unit_button_{"Abrir parte"};
  Gtk::Button edit_line_button_{"Editar linha"};
  Gtk::Box unit_page_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box unit_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button unit_back_{"← Montagem"};
  Gtk::Label unit_title_;
  Gtk::ScrolledWindow unit_scroll_;
  Gtk::Box unit_body_{Gtk::Orientation::VERTICAL, 10};
  sigc::signal<void(const std::string &)> signal_write_requested_;
  sigc::signal<void(const application::BuilderRealization &)> signal_realization_requested_;
  void delete_line();
  void delete_unit();
  void connect_unit_line();
  void connect_unit_entity();
  void add_link();
  void delete_link();
  void create_role();
  void delete_role();
  void assign_role();
  void delete_assignment();
  void show_error(const std::exception &error);
  [[nodiscard]] Gtk::Window *owner_window();
  [[nodiscard]] std::string selected_work_id() const;

  application::ProjectService &service_;
  Gtk::Box header_{Gtk::Orientation::VERTICAL, 6};
  Gtk::FlowBox header_actions_;
  Gtk::Box heading_{Gtk::Orientation::VERTICAL, 2};
  Gtk::Label title_{"Plano narrativo"};
  Gtk::Label hint_{
      "Organize seleção, encadeamento e revelação sem alterar a publicação."};
  Gtk::ComboBoxText work_combo_;
  Gtk::Button library_button_{"Planos"};
  Gtk::Button roles_button_{"Papéis na história"};
  Gtk::Stack stack_;

  Gtk::Box library_page_{Gtk::Orientation::VERTICAL, 10};
  Gtk::FlowBox library_toolbar_;
  Gtk::Button create_button_{"Nova vazia"};
  Gtk::MenuButton models_button_;
  Gtk::Popover models_popover_;
  Gtk::Label library_help_{"Clique para selecionar. Duplo clique ou Enter para abrir. Os três pontinhos contêm as ações da estrutura."};
  Gtk::Box setup_box_{Gtk::Orientation::VERTICAL,12};
  Gtk::Label setup_hint_{"Para planejar uma narrativa, crie uma Obra e seu universo. Faça isso aqui em uma única etapa; não precisa procurar outra área. Escrever Documentos e explorar o mundo não exigem uma Obra."};
  Gtk::Button setup_button_{"Começar minha Obra…"};
  Gtk::Overlay library_overlay_;
  Gtk::Box selection_bar_{Gtk::Orientation::VERTICAL,6};
  Gtk::Label selection_hint_;
  Gtk::Box selection_actions_{Gtk::Orientation::HORIZONTAL,6};
  Gtk::Button selection_compare_{"Comparar"}, selection_cancel_{"Cancelar comparação"}, selection_origin_{"Origem registrada…"};
  Gtk::Box models_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box structures_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::ScrolledWindow models_scroll_;
  Gtk::ListBox models_list_;
  Gtk::ScrolledWindow structures_scroll_;
  Gtk::ListBox structures_list_;
  std::optional<std::string> selected_structure_id_;
  std::optional<std::string> selected_model_id_;
  std::vector<std::string> structure_row_ids_;
  std::vector<std::string> model_row_ids_;

  Gtk::Box detail_page_{Gtk::Orientation::VERTICAL, 10};
  Gtk::FlowBox detail_header_;
  Gtk::Button detail_back_{"← Planos"};
  Gtk::Label detail_title_;
  Gtk::Stack detail_sections_;
  Gtk::StackSwitcher detail_switcher_;
  Gtk::Box links_panel_{Gtk::Orientation::VERTICAL,6};
  Gtk::Box lines_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box units_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box lines_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Box units_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button add_line_button_{"Nova linha"};
  Gtk::Button remove_line_button_{"Remover linha"};
  Gtk::Button add_unit_button_{"Nova parte"};
  Gtk::Button remove_unit_button_{"Remover parte"};
  Gtk::Button membership_button_{"Vincular à linha"};
  Gtk::Button reality_button_{"Vincular elemento da história"};
  Gtk::Button link_button_{"Novo vínculo"};
  Gtk::Button remove_link_button_{"Desvincular"};
  Gtk::ScrolledWindow lines_scroll_;
  Gtk::ScrolledWindow units_scroll_;
  Gtk::ScrolledWindow links_scroll_;
  Gtk::ListBox lines_list_;
  Gtk::ListBox units_list_;
  Gtk::ListBox links_list_;
  std::optional<std::string> selected_line_id_;
  std::optional<std::string> selected_unit_id_;
  std::optional<std::string> selected_link_id_;
  std::vector<std::string> line_row_ids_;
  std::vector<std::string> unit_row_ids_;
  std::vector<std::string> link_row_ids_;

  Gtk::Box roles_page_{Gtk::Orientation::VERTICAL, 10};
  Gtk::FlowBox roles_toolbar_;
  Gtk::Button create_role_button_{"Novo papel"};
  Gtk::Button delete_role_button_{"Remover papel"};
  Gtk::Button assign_role_button_{"Definir papel"};
  Gtk::Button delete_assignment_button_{"Retirar este papel"};
  Gtk::Stack roles_sections_;
  Gtk::StackSwitcher roles_switcher_;
  Gtk::ScrolledWindow roles_scroll_;
  Gtk::ScrolledWindow assignments_scroll_;
  Gtk::ListBox roles_list_;
  Gtk::ListBox assignments_list_;
  std::optional<std::string> selected_role_id_;
  std::optional<std::string> selected_assignment_id_;
  std::vector<std::string> role_row_ids_;
  std::vector<std::string> assignment_row_ids_;
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
};

} // namespace inde::ui

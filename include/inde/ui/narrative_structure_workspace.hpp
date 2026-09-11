#pragma once

#include "inde/application/project_service.hpp"

#include <gtkmm.h>
#include <optional>
#include <string>
#include <vector>

namespace inde::ui {

class NarrativeStructureWorkspace final : public Gtk::Box {
public:
  explicit NarrativeStructureWorkspace(application::ProjectService &service);
  void refresh();
  void reset();
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
  void capture_model();
  void delete_model();
  void activate_selected();
  void delete_selected();
  void add_line();
  void add_unit();
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
  Gtk::Box header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Box heading_{Gtk::Orientation::VERTICAL, 2};
  Gtk::Label title_{"Estrutura narrativa"};
  Gtk::Label hint_{
      "Organize seleção, encadeamento e revelação sem alterar a publicação."};
  Gtk::ComboBoxText work_combo_;
  Gtk::Button library_button_{"Estruturas"};
  Gtk::Button roles_button_{"Papéis narrativos"};
  Gtk::Stack stack_;

  Gtk::Box library_page_{Gtk::Orientation::VERTICAL, 10};
  Gtk::FlowBox library_toolbar_;
  Gtk::Button create_button_{"Nova vazia"};
  Gtk::Button instantiate_button_{"Aplicar modelo"};
  Gtk::Button duplicate_button_{"Duplicar"};
  Gtk::Button derive_button_{"Derivar"};
  Gtk::Button capture_button_{"Salvar como modelo"};
  Gtk::Button activate_button_{"Ativar"};
  Gtk::Button delete_button_{"Remover"};
  Gtk::Paned library_paned_{Gtk::Orientation::HORIZONTAL};
  Gtk::Box models_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box structures_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Button delete_model_button_{"Remover modelo do usuário"};
  Gtk::ScrolledWindow models_scroll_;
  Gtk::ListBox models_list_;
  Gtk::ScrolledWindow structures_scroll_;
  Gtk::ListBox structures_list_;
  std::optional<std::string> selected_structure_id_;
  std::optional<std::string> selected_model_id_;
  std::vector<std::string> structure_row_ids_;
  std::vector<std::string> model_row_ids_;

  Gtk::Box detail_page_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Box detail_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button detail_back_{"← Modelos"};
  Gtk::Label detail_title_;
  Gtk::Paned detail_paned_{Gtk::Orientation::HORIZONTAL};
  Gtk::Box lines_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box units_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box lines_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Box units_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Button add_line_button_{"Nova linha"};
  Gtk::Button remove_line_button_{"Remover linha"};
  Gtk::Button add_unit_button_{"Nova unidade"};
  Gtk::Button remove_unit_button_{"Remover unidade"};
  Gtk::Button membership_button_{"Vincular à linha"};
  Gtk::Button reality_button_{"Vincular entidade"};
  Gtk::Button link_button_{"Novo vínculo"};
  Gtk::Button remove_link_button_{"Remover vínculo"};
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
  Gtk::Box roles_toolbar_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button create_role_button_{"Novo papel"};
  Gtk::Button delete_role_button_{"Remover papel"};
  Gtk::Button assign_role_button_{"Atribuir papel"};
  Gtk::Button delete_assignment_button_{"Remover atribuição"};
  Gtk::Paned roles_paned_{Gtk::Orientation::HORIZONTAL};
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

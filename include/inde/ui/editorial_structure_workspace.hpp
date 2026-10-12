#pragma once

#include "inde/application/project_service.hpp"
#include "inde/ui/structural_editor.hpp"
#include "inde/ui/production_page.hpp"

#include <exception>
#include <gtkmm.h>
#include <optional>
#include <string>
#include <vector>

namespace inde::ui {
class KeyboardCommands;

// Workspace exclusivo para a construção editorial de uma Obra. O catálogo de
// IPs e Obras pertence ao CatalogWorkspace e é compartilhado pelo projeto.
class EditorialStructureWorkspace final : public Gtk::Box {
public:
  explicit EditorialStructureWorkspace(application::ProjectService &service);

  void install_actions(Gtk::ApplicationWindow &window);
  void refresh();
  void install_keyboard(KeyboardCommands &);
  void reset();
  void reveal_work(const std::string &work_id);
  void reveal_node(const std::string &node_id);
  void reveal_removal_use(const project::RemovalUse &);
  void toggle_inspector();
  [[nodiscard]] ViewState capture_view();
  void restore_view(const ViewState &);
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
  void show_library();
  void show_editor();
  void show_production();
  void refresh_library();
  void create_blank_structure();
  void instantiate_model();
  void duplicate_structure(bool derived);
  void capture_model();
  void delete_model();
  void activate_structure();
  void delete_structure();
  void open_structure();
  [[nodiscard]] Gtk::Window *owner_window();
  void show_error(const std::exception &error);

  application::ProjectService &service_;
  Gtk::Box header_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Label title_{"Manuscrito"};
  Gtk::Label hint_{
      "Abra uma obra no Catálogo para organizar seus elementos editoriais."};
  Gtk::Box library_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::ComboBoxText work_combo_;
  Gtk::Button setup_work_button_{"Começar uma Obra"};
  Gtk::Button back_to_library_{"Modelos e alternativas"};
  Gtk::Button back_to_manuscript_{"← Manuscrito"};
  Gtk::Button production_button_{"Produzir obra…"};
  Gtk::Stack pages_;
  Gtk::Box library_page_{Gtk::Orientation::VERTICAL, 10};
  Gtk::FlowBox library_toolbar_;
  Gtk::Button create_structure_button_{"Nova vazia"};
  Gtk::Button instantiate_button_{"Aplicar modelo"};
  Gtk::Button duplicate_button_{"Duplicar"};
  Gtk::Button derive_button_{"Derivar"};
  Gtk::Button capture_button_{"Salvar como modelo"};
  Gtk::Button activate_button_{"Ativar"};
  Gtk::Button open_button_{"Abrir ativa"};
  Gtk::Button delete_structure_button_{"Remover"};
  Gtk::Stack library_views_;
  Gtk::StackSwitcher library_switcher_;
  Gtk::MenuButton alternatives_actions_;
  Gtk::Popover alternatives_popover_;
  Gtk::Box alternatives_options_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box models_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Box structures_panel_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Button delete_model_button_{"Remover modelo do usuário"};
  Gtk::ScrolledWindow models_scroll_;
  Gtk::ListBox models_list_;
  Gtk::ScrolledWindow structures_scroll_;
  Gtk::ListBox structures_list_;
  StructuralEditor structural_editor_;
  ProductionPage production_;
  std::optional<std::string> selected_work_id_;
  std::optional<std::string> selected_structure_id_;
  std::optional<std::string> selected_model_id_;
  std::vector<std::string> structure_row_ids_;
  std::vector<std::string> model_row_ids_;
  bool refreshing_work_{};
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
  sigc::signal<void(const std::string &)> signal_open_document_requested_;
  sigc::signal<void(const std::string &)> signal_create_document_requested_;
  sigc::signal<void(const std::string &)> signal_filter_documents_requested_;
};

} // namespace inde::ui

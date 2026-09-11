#pragma once

#include "inde/application/project_service.hpp"

#include <gtkmm.h>
#include <optional>

namespace inde::ui {

class EditorialWorkspace final : public Gtk::Box {
public:
  explicit EditorialWorkspace(application::ProjectService &service);

  void install_actions(Gtk::ApplicationWindow &window);
  void refresh();
  void reset();
  void toggle_navigation();
  void toggle_inspector();
  [[nodiscard]] sigc::signal<void(const Glib::ustring &)> &
  signal_status_message() {
    return signal_status_message_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_open_work_requested() { return signal_open_work_requested_; }

private:
  void build_ui();
  void refresh_catalog();
  void create_intellectual_property();
  void create_work();
  void edit_intellectual_property(std::string id);
  void edit_work(std::string id);
  void delete_intellectual_property(std::string id);
  void delete_work(std::string id);
  void select_work_structure(std::string work_id);
  void show_error(const Glib::ustring &title, const std::exception &error);
  [[nodiscard]] Gtk::Window *owner_window();

  application::ProjectService &service_;
  Gtk::Box catalog_page_{Gtk::Orientation::VERTICAL, 12};
  Gtk::ScrolledWindow catalog_scroll_;
  Gtk::Box catalog_content_{Gtk::Orientation::VERTICAL, 16};
  Gtk::Box catalog_header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label catalog_title_{"Catálogo do projeto"};
  Gtk::Button filters_button_{"Filtros"};
  Gtk::Revealer filters_revealer_;
  Gtk::Frame filters_surface_;
  Gtk::Box filters_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::SearchEntry catalog_search_;
  Gtk::Button clear_filters_button_{"Limpar filtros"};
  Gtk::Button apply_filters_button_{"Ver catálogo"};
  Gtk::Box navigation_panel_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Label project_name_;
  Gtk::Label project_details_;
  Gtk::Box ip_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box work_panel_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label ip_title_{"Propriedades intelectuais"};
  Gtk::Label work_title_{"Obras"};
  Gtk::Label ip_empty_;
  Gtk::Label work_empty_;
  Gtk::Button add_ip_button_{"Nova propriedade intelectual"};
  Gtk::Button add_work_button_{"Nova obra"};
  Gtk::MenuButton ip_actions_button_;
  Gtk::MenuButton work_actions_button_;
  Gtk::Button edit_ip_button_{"Editar"};
  Gtk::Button remove_ip_button_{"Remover"};
  Gtk::Button edit_work_button_{"Editar"};
  Gtk::Button remove_work_button_{"Remover"};
  Gtk::Box ip_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::Box work_toolbar_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::FlowBox ip_list_;
  Gtk::FlowBox work_list_;
  std::optional<std::string> selected_work_id_;
  std::optional<std::string> selected_ip_id_;
  bool filters_visible_{};
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
  sigc::signal<void(const std::string &)> signal_open_work_requested_;
};

} // namespace inde::ui

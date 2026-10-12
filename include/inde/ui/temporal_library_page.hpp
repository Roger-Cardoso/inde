#pragma once
#include "inde/application/project_service.hpp"
#include <gtkmm.h>
namespace inde::ui {
class KeyboardCommands;
class TemporalLibraryPage final : public Gtk::Box {
public:
  explicit TemporalLibraryPage(application::ProjectService &);
  void refresh();
  void install_keyboard(KeyboardCommands &);
  void edit_calendar(std::optional<project::FictionalCalendar> = {});
  void show_usage();
  void date_links(std::optional<project::TemporalReference> = {});
  void configure_axis(const std::string &axis);
  void open_reference(const std::string &axis, const std::string &id);

private:
  void clear();
  void later(const std::function<void()> &);
  void adoption(const std::string &);
  void references(const std::string &);
  void parallel_dates(const std::string &);
  void edit_reference(const std::string &,
                      std::optional<project::TemporalReference> = {});
  void error(const std::exception &);
  application::ProjectService &service_;
  Gtk::Box heading_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button back_{"Calendários e relógios"};
  Gtk::Label title_{"Tempo"};
  Gtk::ScrolledWindow scroll_;
  Gtk::Box body_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Box footer_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Label status_;
  Gtk::Button save_{"Salvar calendário e relógio"};
  sigc::connection save_connection_;
};
} // namespace inde::ui

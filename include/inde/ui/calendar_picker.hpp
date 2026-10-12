#pragma once
#include "inde/project/calendar_view.hpp"
#include <gtkmm.h>
#include <map>
namespace inde::ui {
// Reusable draft editor. Visible month and chosen boundaries are independent.
class CalendarPicker final : public Gtk::Box {
public:
  explicit CalendarPicker(bool partial = true, bool clock = true);
  void set_calendar(project::FictionalCalendar);
  void set_selection(std::optional<project::TemporalBoundary>,
                     std::optional<project::TemporalBoundary> = {});
  void set_range(bool);
  project::TemporalBoundary boundary(bool upper = false) const;
  project::CalendarDate date() const;
  project::CalendarDate visible_month() const { return visible_; }
  void show_month(project::CalendarDate);
  void set_marks(const std::map<int, std::size_t> &);
  void set_day_notes(const std::map<int, std::string> &);
  sigc::signal<void()> &signal_changed() { return changed_; }
  sigc::signal<void()> &signal_month_changed() { return month_changed_; }

private:
  void render();
  void summarize();
  void choose(std::optional<int>, std::optional<int>);
  void clock_fields();
  void navigate(int);
  void focus_day(int);
  bool key(guint, guint, Gdk::ModifierType);
  project::FictionalCalendar calendar_;
  project::CalendarDate visible_;
  std::optional<project::TemporalBoundary> lower_, upper_;
  bool available_{}, range_{}, upper_active_{}, partial_{}, clock_{},
      updating_{};
  int focus_index_{};
  Gtk::Box header_{Gtk::Orientation::HORIZONTAL, 6},
      endpoints_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button previous_{"‹"}, next_{"›"}, lower_button_{"Início"},
      upper_button_{"Fim"}, clear_{"Limpar escolha"};
  Gtk::Entry year_;
  Gtk::ComboBoxText month_;
  Gtk::Label summary_, status_;
  Gtk::Grid grid_;
  Gtk::Box options_{Gtk::Orientation::HORIZONTAL, 6},
      clock_box_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Button year_only_{"Usar só este ano"}, month_only_{"Usar só este mês"};
  Gtk::Expander clock_expander_{"Adicionar horário"};
  Gtk::Expander direct_expander_{"Entrada direta (opcional)"};
  Gtk::Box direct_box_{Gtk::Orientation::VERTICAL,4};
  Gtk::Entry direct_;
  std::vector<Gtk::Button *> days_;
  std::vector<Gtk::Entry *> clock_fields_;
  std::map<int, std::size_t> marks_;
  std::map<int, std::string> day_notes_;
  sigc::signal<void()> changed_, month_changed_;
};
} // namespace inde::ui

#pragma once
#include "inde/ui/calendar_picker.hpp"
namespace inde::ui {
// One compact draft surface shared by writing and contextual details.
class TemporalReferenceEditor final : public Gtk::Box {
public:
  explicit TemporalReferenceEditor(
      const std::vector<project::FictionalCalendar> &);
  project::TemporalReference draft(const std::string &axis) const;
  project::FictionalCalendar calendar() const;

private:
  void summarize();
  std::vector<project::FictionalCalendar> calendars_;
  Gtk::ComboBoxText system_, precision_;
  Gtk::Entry name_, uncertainty_;
  Gtk::Expander selector_{"Escolher data ou intervalo…"},
      advanced_{"Precisão e incerteza"};
  Gtk::Box details_{Gtk::Orientation::VERTICAL, 6};
  CalendarPicker picker_;
};
} // namespace inde::ui

#pragma once

#include <gtkmm.h>
#include <iomanip>
#include <sstream>

namespace inde::ui::cartography_ui_detail {
inline constexpr auto local_type = "00000000-0000-4000-9000-000000000002";
inline void label(Gtk::Label &v) {
  v.set_wrap(true);
  v.set_xalign(0);
}
inline Gtk::SpinButton *spin(double low, double high, double step, double value,
                      int digits = 0) {
  auto *s = Gtk::make_managed<Gtk::SpinButton>();
  s->set_range(low, high);
  s->set_increments(step, step * 10);
  s->set_digits(digits);
  s->set_value(value);
  return s;
}
inline std::string number(double v, int digits = 2) {
  std::ostringstream s;
  s << std::fixed << std::setprecision(digits) << v;
  return s.str();
}
} // namespace inde::ui::cartography_ui_detail

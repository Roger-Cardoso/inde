#pragma once
#include "inde/ui/accessibility.hpp"
namespace inde::ui {
inline void constrain_temporal_combo(Gtk::ComboBoxText &combo) {
  for (auto *cell : combo.get_cells())
    if (auto *text = dynamic_cast<Gtk::CellRendererText *>(cell)) {
      text->property_ellipsize() = Pango::EllipsizeMode::END;
      text->property_width_chars() = 36;
      text->property_max_width_chars() = 48;
    }
  combo.signal_changed().connect(sigc::track_obj(
      [&combo] {
        combo.set_tooltip_text(combo.get_active_text());
        set_accessible_description(combo, combo.get_active_text());
      },
      combo));
}
} // namespace inde::ui

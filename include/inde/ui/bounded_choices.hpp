#pragma once
#include <gtkmm.h>

namespace inde::ui {
// Bound presentation only; the model retains the full authorial name and ID.
inline void bound_choice_width(Gtk::ComboBoxText &choice, int characters = 28) {
  for (auto *cell : choice.get_cells())
    if (auto *text = dynamic_cast<Gtk::CellRendererText *>(cell)) {
      text->property_ellipsize() = Pango::EllipsizeMode::END;
      text->property_width_chars() = 20;
      text->property_max_width_chars() = characters;
    }
}
} // namespace inde::ui

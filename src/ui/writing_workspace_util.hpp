#pragma once

#include <gtkmm.h>

namespace inde::ui {

inline void prepare_tool_label(Gtk::Label &label) {
  label.set_xalign(0.0F);
  label.set_wrap(true);
  label.add_css_class("dim-label");
}

inline void clear_list(Gtk::ListBox &list) {
  while (auto *child = list.get_first_child())
    list.remove(*child);
}

} // namespace inde::ui

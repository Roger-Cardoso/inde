#pragma once

#include <gtkmm.h>

namespace inde::ui::narrative_structure_ui_detail {

inline void clear(Gtk::ListBox &list) {
  while (auto *child = list.get_first_child())
    list.remove(*child);
}

inline Gtk::Box *row_content(const Glib::ustring &title, const Glib::ustring &detail) {
  auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
  box->set_margin(8);
  auto *heading = Gtk::make_managed<Gtk::Label>(title);
  heading->set_halign(Gtk::Align::FILL);
  heading->set_ellipsize(Pango::EllipsizeMode::END);
  heading->set_max_width_chars(55);
  heading->set_tooltip_text(title);
  heading->set_xalign(0.0F);
  heading->add_css_class("heading");
  auto *secondary = Gtk::make_managed<Gtk::Label>(detail);
  secondary->set_halign(Gtk::Align::FILL);
  secondary->set_max_width_chars(80);
  secondary->set_xalign(0.0F);
  secondary->set_wrap(true);
  secondary->add_css_class("dim-label");
  box->append(*heading);
  box->append(*secondary);
  return box;
}

} // namespace inde::ui::narrative_structure_ui_detail

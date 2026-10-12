#pragma once
#include <gtkmm.h>
#include <optional>
namespace inde::ui {
// Embedded HSV/palette built with current GTK controls and Cairo drawing.
class EditorColorPicker final : public Gtk::Box {
public:
  explicit EditorColorPicker(const std::string &initial);
  ~EditorColorPicker() override;
  std::optional<std::string> color_hex() const;

private:
  void set_color(const Gdk::RGBA &color);
  void set_hsv(double hue, double saturation, double value);
  void hsv_changed();
  void channels_changed();
  void hex_changed();
  Gtk::Stack choices_;
  Gtk::Box hsv_box_{Gtk::Orientation::VERTICAL, 6};
  Gtk::FlowBox palette_;
  Gtk::DrawingArea saturation_value_;
  Gtk::Scale hue_{Gtk::Orientation::HORIZONTAL};
  Gtk::Scale saturation_{Gtk::Orientation::HORIZONTAL};
  Gtk::Scale value_{Gtk::Orientation::HORIZONTAL};
  Gtk::Box channels_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::SpinButton red_, green_, blue_;
  Gtk::Entry hex_;
  Gtk::Label error_, preview_title_{"Prévia da cor no fundo do editor"};
  Gtk::DrawingArea preview_;
  Gdk::RGBA color_;
  bool syncing_{}, valid_{true};
};
} // namespace inde::ui

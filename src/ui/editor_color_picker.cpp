#include "editor_color_picker.hpp"
#include "inde/project/writing.hpp"
#include "inde/ui/accessibility.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace inde::ui {
namespace {
std::string hex_color(const Gdk::RGBA &c) {
  std::ostringstream s;
  s << '#' << std::hex << std::setfill('0');
  for (auto value : {c.get_red(), c.get_green(), c.get_blue()})
    s << std::setw(2)
      << std::clamp(static_cast<int>(std::lround(value * 255)), 0, 255);
  return s.str();
}
Gdk::RGBA hsv(double h, double s, double v) {
  const double c = v * s, x = c * (1 - std::abs(std::fmod(h / 60, 2) - 1)),
               m = v - c;
  double r{}, g{}, b{};
  if (h < 60) {
    r = c;
    g = x;
  } else if (h < 120) {
    r = x;
    g = c;
  } else if (h < 180) {
    g = c;
    b = x;
  } else if (h < 240) {
    g = x;
    b = c;
  } else if (h < 300) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }
  Gdk::RGBA color;
  color.set_rgba(r + m, g + m, b + m, 1);
  return color;
}
} // namespace
EditorColorPicker::EditorColorPicker(const std::string &initial)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 10) {
  saturation_value_.set_content_height(140);
  saturation_value_.set_hexpand(true);
  set_accessible_label(saturation_value_,
                       "Área visual de saturação e luminosidade");
  set_accessible_description(saturation_value_,
                             "Clique para escolher. Os controles HSV e RGB "
                             "oferecem a mesma seleção por teclado.");
  saturation_value_.set_draw_func(
      [this](const auto &cr, int width, int height) {
        const auto pure = hsv(hue_.get_value(), 1, 1);
        cr->set_source_rgb(pure.get_red(), pure.get_green(), pure.get_blue());
        cr->paint();
        auto white = Cairo::LinearGradient::create(0, 0, width, 0);
        white->add_color_stop_rgba(0, 1, 1, 1, 1);
        white->add_color_stop_rgba(1, 1, 1, 1, 0);
        cr->set_source(white);
        cr->paint();
        auto black = Cairo::LinearGradient::create(0, 0, 0, height);
        black->add_color_stop_rgba(0, 0, 0, 0, 0);
        black->add_color_stop_rgba(1, 0, 0, 0, 1);
        cr->set_source(black);
        cr->paint();
        const auto x = saturation_.get_value() / 100 * width,
                   y = (1 - value_.get_value() / 100) * height;
        cr->arc(x, y, 6, 0, 6.283185307);
        cr->set_source_rgb(0, 0, 0);
        cr->set_line_width(3);
        cr->stroke_preserve();
        cr->set_source_rgb(1, 1, 1);
        cr->set_line_width(1);
        cr->stroke();
      });
  auto pointer = Gtk::GestureDrag::create();
  pointer->set_button(1);
  auto choose = [this](double x, double y) {
    const auto width = saturation_value_.get_width(),
               height = saturation_value_.get_height();
    if (width <= 0 || height <= 0)
      return;
    set_hsv(hue_.get_value(), std::clamp(x / width, 0.0, 1.0),
            1 - std::clamp(y / height, 0.0, 1.0));
  };
  pointer->signal_drag_begin().connect(choose);
  pointer->signal_drag_update().connect(
      [drag=pointer.get(), choose](double dx, double dy) {
        double x{}, y{};
        if (drag->get_start_point(x, y))
          choose(x + dx, y + dy);
      });
  saturation_value_.add_controller(pointer);
  hsv_box_.append(saturation_value_);
  for (auto [scale, label] : {std::pair{&hue_, "Tonalidade — H"},
                              std::pair{&saturation_, "Saturação — S"},
                              std::pair{&value_, "Luminosidade — V"}}) {
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    auto *caption = Gtk::make_managed<Gtk::Label>(label);
    caption->set_xalign(0);
    caption->set_width_chars(18);
    row->append(*caption);
    scale->set_range(0, scale == &hue_ ? 360 : 100);
    scale->set_increments(1, 10);
    scale->set_digits(0);
    scale->set_hexpand(true);
    scale->set_tooltip_text(label);
    set_accessible_label(*scale, label);
    row->append(*scale);
    hsv_box_.append(*row);
    scale->signal_value_changed().connect(
        sigc::mem_fun(*this, &EditorColorPicker::hsv_changed));
  }
  palette_.set_selection_mode(Gtk::SelectionMode::NONE);
  palette_.set_min_children_per_line(1);
  palette_.set_max_children_per_line(4);
  palette_.set_column_spacing(6);
  palette_.set_row_spacing(6);
  for (const auto *hex : {"#20242d", "#ffffff", "#88aaff", "#da5959", "#eeaa77",
                          "#6cbd9a", "#b7a0da", "#eeee77"}) {
    auto *button = Gtk::make_managed<Gtk::Button>(hex);
    auto *sample = Gtk::make_managed<Gtk::DrawingArea>();
    sample->set_content_width(28);
    sample->set_content_height(28);
    sample->set_draw_func([color = Gdk::RGBA(hex)](const auto &cr, int, int) {
      cr->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(),
                          1);
      cr->paint();
    });
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    box->append(*sample);
    box->append(*Gtk::make_managed<Gtk::Label>(hex));
    button->set_child(*box);
    set_accessible_label(*button, Glib::ustring("Cor ") + hex);
    button->signal_clicked().connect(
        [this, color = Gdk::RGBA(hex)] { set_color(color); });
    palette_.append(*button);
  }
  choices_.set_hhomogeneous(false);
  choices_.set_vhomogeneous(false);
  choices_.add(hsv_box_, "hsv");
  choices_.add(palette_, "palette");
  append(choices_);
  auto *palette =
      Gtk::make_managed<Gtk::Button>("Mostrar paleta / ajustar tonalidade");
  palette->signal_clicked().connect([this] {
    choices_.set_visible_child(
        choices_.get_visible_child_name() == "hsv" ? "palette" : "hsv");
  });
  append(*palette);
  for (const auto &item :
       {std::pair{&red_, "R — Vermelho"}, std::pair{&green_, "G — Verde"},
        std::pair{&blue_, "B — Azul"}}) {
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
    auto *label = Gtk::make_managed<Gtk::Label>(item.second);
    item.first->set_range(0, 255);
    item.first->set_increments(1, 16);
    item.first->set_digits(0);
    item.first->set_numeric(true);
    item.first->set_width_chars(3);
    set_accessible_label(*item.first, item.second);
    item.first->set_tooltip_text(item.second);
    box->append(*label);
    box->append(*item.first);
    channels_.append(*box);
    item.first->signal_value_changed().connect(
        sigc::mem_fun(*this, &EditorColorPicker::channels_changed));
  }
  append(channels_);
  hex_.set_placeholder_text("Cor #RRGGBB");
  hex_.set_max_length(7);
  set_accessible_label(hex_, "Cor hexadecimal RGB");
  append(hex_);
  error_.set_wrap(true);
  error_.add_css_class("error");
  append(error_);
  preview_title_.set_xalign(0);
  append(preview_title_);
  preview_.set_content_height(54);
  set_accessible_label(preview_,
                       "Amostra de cor com texto sobre o fundo do editor");
  preview_.set_draw_func([this](const auto &cr, int width, int height) {
    Gdk::RGBA background;
    if (!get_style_context()->lookup_color("inde_paper", background))
      background = Gdk::RGBA("#ffffff");
    cr->set_source_rgb(background.get_red(), background.get_green(),
                       background.get_blue());
    cr->paint();
    cr->set_source_rgb(color_.get_red(), color_.get_green(), color_.get_blue());
    cr->rectangle(12, 12, 30, height - 24);
    cr->fill();
    auto text = create_pango_layout("Água, palavras e histórias — Aa 123");
    int tw{}, th{};
    text->get_pixel_size(tw, th);
    cr->move_to(54, (height - th) / 2.0);
    text->show_in_cairo_context(cr);
    (void)width;
  });
  append(preview_);
  hex_.signal_changed().connect(
      sigc::mem_fun(*this, &EditorColorPicker::hex_changed));
  set_color(Gdk::RGBA(initial));
}
EditorColorPicker::~EditorColorPicker() {
  gtk_drawing_area_set_draw_func(saturation_value_.gobj(), nullptr, nullptr,
                                 nullptr);
  gtk_drawing_area_set_draw_func(preview_.gobj(), nullptr, nullptr, nullptr);
}
void EditorColorPicker::set_color(const Gdk::RGBA &color) {
  syncing_ = true;
  color_ = color;
  color_.set_alpha(1);
  const double r = color_.get_red(), g = color_.get_green(),
               b = color_.get_blue();
  const double maximum = std::max({r, g, b}), minimum = std::min({r, g, b}),
               delta = maximum - minimum;
  double h = hue_.get_value();
  if (delta > 0) {
    if (maximum == r)
      h = 60 * std::fmod((g - b) / delta, 6);
    else if (maximum == g)
      h = 60 * ((b - r) / delta + 2);
    else
      h = 60 * ((r - g) / delta + 4);
    if (h < 0)
      h += 360;
  }
  hue_.set_value(h);
  saturation_.set_value(maximum == 0 ? 0 : delta / maximum * 100);
  value_.set_value(maximum * 100);
  saturation_value_.queue_draw();
  red_.set_value(std::lround(color_.get_red() * 255));
  green_.set_value(std::lround(color_.get_green() * 255));
  blue_.set_value(std::lround(color_.get_blue() * 255));
  hex_.set_text(hex_color(color_));
  valid_ = true;
  error_.set_visible(false);
  preview_.queue_draw();
  syncing_ = false;
}
void EditorColorPicker::set_hsv(double h, double s, double v) {
  set_color(hsv(h, s, v));
  // RGB cannot encode hue at zero saturation or saturation at zero value.
  // Keep the user's HSV coordinates, so raising brightness restores the color.
  syncing_ = true;
  hue_.set_value(h);
  saturation_.set_value(s * 100);
  value_.set_value(v * 100);
  syncing_ = false;
}
void EditorColorPicker::hsv_changed() {
  if (syncing_)
    return;
  set_hsv(hue_.get_value(), saturation_.get_value() / 100,
          value_.get_value() / 100);
}
void EditorColorPicker::channels_changed() {
  if (syncing_)
    return;
  Gdk::RGBA value;
  value.set_rgba(red_.get_value() / 255, green_.get_value() / 255,
                 blue_.get_value() / 255, 1);
  set_color(value);
}
void EditorColorPicker::hex_changed() {
  if (syncing_)
    return;
  try {
    project::validate_format_span(
        {project::DocumentTextStyle::Foreground, 0, 1, hex_.get_text().raw()});
    set_color(Gdk::RGBA(hex_.get_text()));
  } catch (const std::exception &) {
    valid_ = false;
    error_.set_text(
        "Informe uma cor RGB completa: #RRGGBB, por exemplo #88aaff.");
    error_.set_visible(true);
  }
}
std::optional<std::string> EditorColorPicker::color_hex() const {
  if (!valid_)
    return {};
  return hex_color(color_);
}
} // namespace inde::ui

#pragma once
#include "widget_probe.hpp"
#include <cmath>
#include <cstring>
#include <cstdint>

namespace inde::test::ui {
// Inspect pixels from the actual allocated native widget, including its CSS
// background. WidgetPaintable replaces the deprecated style-context renderer.
inline void require_surface_background(Gtk::Widget &widget,
                                       const char *color_name = "inde_surface") {
  auto *snapshot = gtk_snapshot_new();
  auto *paintable=gtk_widget_paintable_new(widget.gobj());
  gdk_paintable_snapshot(paintable,GDK_SNAPSHOT(snapshot),20,20);
  g_object_unref(paintable);
  auto *node = gtk_snapshot_free_to_node(snapshot);
  require(node, "Notebook content has no theme background");
  auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32, 20, 20);
  auto context = Cairo::Context::create(surface);
  gsk_render_node_draw(node, context->cobj());
  gsk_render_node_unref(node); surface->flush();
  Gdk::RGBA expected;
  require(widget.get_style_context()->lookup_color(color_name, expected), "Missing surface color");
  bool painted=false;
  for(int y=0;y<20;++y)for(int x=0;x<20;++x) {
    std::uint32_t pixel{};
    std::memcpy(&pixel,surface->get_data()+y*surface->get_stride()+x*4,sizeof(pixel));
    painted=painted || ((pixel >> 24) == 255 &&
      std::abs(static_cast<double>((pixel >> 16) & 255) / 255 - expected.get_red()) < 0.01 &&
      std::abs(static_cast<double>((pixel >> 8) & 255) / 255 - expected.get_green()) < 0.01 &&
      std::abs(static_cast<double>(pixel & 255) / 255 - expected.get_blue()) < 0.01);
  }
  require(painted,"Allocated notebook does not paint the application surface color");
}
}

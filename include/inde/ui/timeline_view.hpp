#pragma once

#include "inde/application/timeline_projection.hpp"

#include <gtkmm.h>
#include <optional>

namespace inde::ui {

class TimelineView final : public Gtk::DrawingArea {
public:
  TimelineView();

  void set_snapshot(application::TimelineSnapshot snapshot);
  void clear();
  void select_item(application::TimelineVisualKind kind, std::string id);
  void set_zoom(double zoom, int viewport_width);
  [[nodiscard]] double zoom() const { return zoom_; }
  [[nodiscard]] sigc::signal<void(application::TimelineVisualKind,
                                  const std::string &)> &
  signal_item_selected() {
    return signal_item_selected_;
  }
  [[nodiscard]] sigc::signal<void(double)> &signal_painted() {
    return signal_painted_;
  }

private:
  void draw(const Cairo::RefPtr<Cairo::Context> &context, int width,
            int height);
  void select_at(double x, double y);
  [[nodiscard]] bool is_selected(application::TimelineVisualKind kind,
                                 const std::string &id) const;

  std::optional<application::TimelineSnapshot> snapshot_;
  application::TimelineLayout layout_;
  double zoom_{1.0};
  int viewport_width_{640};
  std::optional<application::TimelineVisualKind> selected_kind_;
  std::string selected_id_;
  Glib::RefPtr<Gtk::GestureClick> click_;
  sigc::signal<void(application::TimelineVisualKind, const std::string &)>
      signal_item_selected_;
  sigc::signal<void(double)> signal_painted_;
};

} // namespace inde::ui

#include "inde/ui/timeline_view.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

namespace inde::ui {
namespace {

void set_color(const Cairo::RefPtr<Cairo::Context> &context, double red,
               double green, double blue, double alpha = 1.0) {
  context->set_source_rgba(red, green, blue, alpha);
}

std::string elide(const std::string &text) {
  const Glib::ustring unicode{text};
  return (unicode.size() > 34 ? unicode.substr(0, 31) + Glib::ustring{"..."}
                              : unicode)
      .raw();
}

void configure_font(const Cairo::RefPtr<Cairo::Context> &context, double size) {
  context->select_font_face("Sans", Cairo::ToyFontFace::Slant::NORMAL,
                            Cairo::ToyFontFace::Weight::NORMAL);
  context->set_font_size(size);
}

void draw_text(const Cairo::RefPtr<Cairo::Context> &context,
               const std::string &text, double x, double y, double size,
               double red = 0.86, double green = 0.88, double blue = 0.91) {
  configure_font(context, size);
  set_color(context, red, green, blue);
  context->move_to(x, y);
  context->show_text(elide(text));
}

void draw_centered_text(const Cairo::RefPtr<Cairo::Context> &context,
                        const std::string &text, double center_x, double y,
                        double size, double minimum_x, double maximum_x,
                        double red = 0.86, double green = 0.88,
                        double blue = 0.91) {
  configure_font(context, size);
  const auto visible = elide(text);
  Cairo::TextExtents extents;
  context->get_text_extents(visible, extents);
  const auto desired = center_x - extents.width / 2.0 - extents.x_bearing;
  const auto x = std::clamp(desired, minimum_x,
                            maximum_x - extents.width - extents.x_bearing);
  set_color(context, red, green, blue);
  context->move_to(x, y);
  context->show_text(visible);
}

} // namespace

TimelineView::TimelineView() {
  set_hexpand(true);
  set_vexpand(true);
  set_content_width(640);
  set_content_height(420);
  set_draw_func(sigc::mem_fun(*this, &TimelineView::draw));
  click_ = Gtk::GestureClick::create();
  click_->set_button(1);
  click_->signal_pressed().connect(
      [this](int, double x, double y) { select_at(x, y); });
  add_controller(click_);
}

void TimelineView::set_snapshot(application::TimelineSnapshot snapshot) {
  snapshot_ = std::move(snapshot);
  selected_kind_.reset();
  selected_id_.clear();
  const auto provisional = application::layout_timeline(*snapshot_, 900.0);
  set_content_height(static_cast<int>(std::max(420.0, provisional.height)));
  set_content_width(static_cast<int>(std::ceil(
      application::recommended_timeline_content_width(
          *snapshot_, static_cast<double>(viewport_width_), zoom_))));
  queue_draw();
}

void TimelineView::clear() {
  snapshot_.reset();
  selected_kind_.reset();
  selected_id_.clear();
  set_content_height(420);
  queue_draw();
}

void TimelineView::select_item(application::TimelineVisualKind kind,
                               std::string id) {
  selected_kind_ = kind;
  selected_id_ = std::move(id);
  queue_draw();
}

void TimelineView::set_zoom(double zoom, int viewport_width) {
  zoom_ = std::clamp(zoom, 1.0, 8.0);
  viewport_width_ = std::max(640, viewport_width);
  const auto content_width =
      snapshot_ ? application::recommended_timeline_content_width(
                      *snapshot_, static_cast<double>(viewport_width_), zoom_)
                : static_cast<double>(viewport_width_) * zoom_;
  set_content_width(static_cast<int>(std::ceil(content_width)));
  queue_draw();
}

bool TimelineView::is_selected(application::TimelineVisualKind kind,
                               const std::string &id) const {
  return selected_kind_ && *selected_kind_ == kind && selected_id_ == id;
}

void TimelineView::draw(const Cairo::RefPtr<Cairo::Context> &context, int width,
                        int height) {
  const auto started = std::chrono::steady_clock::now();
  const auto report_paint = [this, started] {
    signal_painted_.emit(
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started)
            .count());
  };
  set_color(context, 0.105, 0.115, 0.13);
  context->paint();
  if (!snapshot_) {
    report_paint();
    return;
  }

  layout_ = application::layout_timeline(*snapshot_, width);
  set_color(context, 0.43, 0.47, 0.54);
  context->set_line_width(2.0);
  context->move_to(layout_.left, layout_.axis_y);
  context->line_to(layout_.right, layout_.axis_y);
  context->stroke();

  for (const auto &item : layout_.items) {
    const bool selected = is_selected(item.kind, item.id);
    if (item.kind == application::TimelineVisualKind::Point) {
      set_color(context, selected ? 1.0 : 0.58, selected ? 0.77 : 0.67,
                selected ? 0.2 : 0.82);
      context->arc(item.x + item.width / 2.0, item.y + item.height / 2.0,
                   selected ? 8.0 : 6.0, 0.0, 2.0 * std::numbers::pi);
      context->fill();
      const auto point =
          std::find_if(snapshot_->points.begin(), snapshot_->points.end(),
                       [&](const auto &value) { return value.id == item.id; });
      if (point != snapshot_->points.end()) {
        const auto center_x = item.x + item.width / 2.0;
        draw_centered_text(context, point->label, center_x,
                           layout_.axis_y + 27.0, 12.0, layout_.left - 24.0,
                           layout_.right + 24.0);
        draw_centered_text(context, std::to_string(point->ordinal), center_x,
                           layout_.axis_y + 44.0, 10.0, layout_.left - 24.0,
                           layout_.right + 24.0, 0.55, 0.58, 0.63);
      }
    } else if (item.kind == application::TimelineVisualKind::Event) {
      set_color(context, selected ? 1.0 : 0.88, selected ? 0.65 : 0.39,
                selected ? 0.18 : 0.24);
      const auto center_x = item.x + item.width / 2.0;
      const auto center_y = item.y + item.height / 2.0;
      context->move_to(center_x, item.y);
      context->line_to(item.x + item.width, center_y);
      context->line_to(center_x, item.y + item.height);
      context->line_to(item.x, center_y);
      context->close_path();
      context->fill();
      const auto event = std::find_if(
          snapshot_->events.begin(), snapshot_->events.end(),
          [&](const auto &value) { return value.occurrence_id == item.id; });
      if (event != snapshot_->events.end())
        draw_text(context, event->entity_name, item.x + 12.0, item.y + 5.0,
                  12.0);
    } else {
      set_color(context, selected ? 0.34 : 0.22, selected ? 0.76 : 0.54,
                selected ? 0.95 : 0.75, selected ? 0.9 : 0.72);
      context->rectangle(item.x, item.y, item.width, item.height);
      context->fill();
      const auto presence = std::find_if(
          snapshot_->presences.begin(), snapshot_->presences.end(),
          [&](const auto &value) { return value.presence_id == item.id; });
      if (presence != snapshot_->presences.end()) {
        if (!presence->end_ordinal) {
          const auto end = item.x + item.width;
          context->move_to(end + 10.0, item.y + item.height / 2.0);
          context->line_to(end, item.y);
          context->line_to(end, item.y + item.height);
          context->close_path();
          context->fill();
        }
        draw_text(context,
                  presence->entity_name + " em " + presence->location_name,
                  item.x + 6.0, item.y + 14.0, 11.0, 0.96, 0.97, 0.99);
      }
    }
  }

  if (snapshot_->points.empty()) {
    const auto message_y =
        std::max(90.0, static_cast<double>(height) / 2.0);
    draw_text(context, "Este eixo ainda não possui pontos.", 48.0, message_y,
              15.0);
    draw_text(context, "Crie pontos no Planejamento.", 48.0,
              message_y + 24.0, 13.0, 0.66, 0.69, 0.74);
  }
  report_paint();
}

void TimelineView::select_at(double x, double y) {
  const auto selected = application::timeline_hit_test(layout_, x, y);
  if (!selected)
    return;
  selected_kind_ = selected->kind;
  selected_id_ = selected->id;
  queue_draw();
  signal_item_selected_.emit(selected->kind, selected->id);
}

} // namespace inde::ui

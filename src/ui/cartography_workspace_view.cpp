#include "cartography_ui_helpers.hpp"
#include "inde/ui/cartography_workspace.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <set>
#include <sstream>

namespace inde::ui {
namespace geo = project::geo;
using cartography_ui_detail::label;
using cartography_ui_detail::number;

void CartographyWorkspace::zoom(double factor) {
  if (!planet())
    return;
  camera_.zoom = std::clamp(camera_.zoom * factor, 1.0, 64.0);
  queue_scene();
}
void CartographyWorkspace::queue_scene() {
  if (scene_request_.connected())
    return;
  scene_request_ = Glib::signal_timeout().connect(
      [this] {
        rebuild_scene();
        return false;
      },
      20);
}
void CartographyWorkspace::rebuild_scene() {
  try {
    const auto *p = planet();
    if (!p) {
      scene_ = {};
      canvas_.queue_draw();
      return;
    }
    const int w = std::max(1, canvas_.get_width()),
              h = std::max(1, canvas_.get_height());
    const auto layer =
        layer_.get_active_id() == "land"    ? application::MapLayer::LandWater
        : layer_.get_active_id() == "depth" ? application::MapLayer::OceanDepth
                                            : application::MapLayer::Relief;
    const auto started = std::chrono::steady_clock::now();
    if (preview_) {
      const int level = application::map_level(*p, camera_, w, h);
      std::set<geo::ChunkKey> keys;
      for (auto b : application::map_window(camera_, w, h).ranges)
        for (auto k : geo::visible_chunks(b, level))
          keys.insert(k);
      std::vector<geo::TerrainChunk> chunks;
      for (const auto &t : preview_->chunks)
        if (keys.contains(t.key))
          chunks.push_back(t);
      scene_ =
          application::build_map_scene(*p, camera_, w, h, layer, chunks, {});
    } else
      scene_ = service_.cartography().scene(
          *p, camera_, w, h, layer,
          terrain_preview_ ? &*terrain_preview_ : nullptr);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - started)
                          .count();
    metrics_.set_text(
        "Zoom " + number(camera_.zoom, 1) + "× · Nível de detalhe " +
        std::to_string(scene_.level) + "/" + std::to_string(p->detail) + "\n" +
        std::to_string(scene_.active_chunks) + " chunks ativos · " +
        std::to_string(scene_.loaded_chunks) + " lidos\n" +
        std::to_string(scene_.cells.size()) + " células · " +
        std::to_string(scene_.markers.size()) + " Locais\nCache " +
        number(scene_.cache_bytes / 1024.0, 0) + " KiB · render ~" +
        number(scene_.render_bytes / 1024.0, 0) + " KiB · cena " + number(ms) +
        " ms" +
        (camera_.zoom > 8 ? "\nLimite de detalhe disponível: zoom adicional "
                            "não cria terreno."
                          : "") +
        (scene_.limited ? "\nVista limitada pelo orçamento cartográfico."
                        : "") +
        (scene_.missing_chunks
             ? "\nATENÇÃO: chunks ausentes; não foram regenerados."
             : ""));
    while (auto *child = visible_locals_.get_first_child())
      visible_locals_.remove(*child);
    auto *heading = Gtk::make_managed<Gtk::Label>("Locais da vista (até 30)");
    label(*heading);
    visible_locals_.append(*heading);
    for (std::size_t i = 0;
         i < std::min(std::size_t{30}, scene_.markers.size()); ++i) {
      const auto &m = scene_.markers[i];
      auto *b = Gtk::make_managed<Gtk::Button>();
      auto *l = Gtk::make_managed<Gtk::Label>(m.local.name);
      l->set_ellipsize(Pango::EllipsizeMode::END);
      l->set_max_width_chars(24);
      b->set_child(*l);
      b->signal_clicked().connect(
          [this, id = m.local.position.entity_id, name = m.local.name] {
            selected_local_ = id;
            local_selector_->set_selected(id, name);
            reveal_entity(id);
          });
      visible_locals_.append(*b);
    }
    canvas_.queue_draw();
  } catch (const std::exception &e) {
    scene_ = {};
    canvas_.queue_draw();
    error(e);
  }
}
void CartographyWorkspace::draw(const Cairo::RefPtr<Cairo::Context> &cr, int w,
                                int h) {
  Gdk::RGBA background;
  get_style_context()->lookup_color("inde_paper", background);
  cr->set_source_rgb(background.get_red(), background.get_green(), background.get_blue());
  cr->paint();
  if (!planet()) {
    const auto foreground = get_style_context()->get_color();
    cr->set_source_rgb(foreground.get_red(), foreground.get_green(), foreground.get_blue());
    cr->set_font_size(20);
    cr->move_to(30, h / 2.0);
    cr->show_text("Seu mundo começa com um planeta.");
    return;
  }
  cr->save();
  cr->rectangle(0, 0, w, h);
  cr->clip();
  if (!scene_.terrain_tiles.empty()) {
    const auto filter = rendering_.get_active_id() == "raster"
                            ? Cairo::SurfacePattern::Filter::NEAREST
                            : Cairo::SurfacePattern::Filter::BILINEAR;
    for (auto &tile : scene_.terrain_tiles) {
      auto surface = Cairo::ImageSurface::create(
          reinterpret_cast<unsigned char *>(tile.argb.data()),
          Cairo::Surface::Format::ARGB32, tile.side, tile.side, tile.side * 4);
      auto pattern = Cairo::SurfacePattern::create(surface);
      pattern->set_filter(filter);
      cr->save();
      constexpr double seam_overlap = 0.75;
      cr->set_antialias(Cairo::ANTIALIAS_NONE);
      cr->rectangle(tile.clip_x - seam_overlap, tile.clip_y - seam_overlap,
                    tile.clip_width + 2 * seam_overlap,
                    tile.clip_height + 2 * seam_overlap);
      cr->clip();
      cr->set_antialias(Cairo::ANTIALIAS_DEFAULT);
      cr->translate(tile.paint_x, tile.paint_y);
      cr->scale(tile.sample_pixels, tile.sample_pixels);
      cr->set_source(pattern);
      cr->paint();
      cr->restore();
    }
  } else {
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    for (const auto &r : scene_.cells) {
      cr->set_source_rgb(r.color.red, r.color.green, r.color.blue);
      cr->rectangle(r.x, r.y, r.width + 0.5, r.height + 0.5);
      cr->fill();
    }
  }
  cr->set_antialias(Cairo::ANTIALIAS_DEFAULT);
  if (coastlines_.get_active()) {
    cr->set_source_rgba(0.98, 0.86, 0.65, 0.78);
    cr->set_line_width(1.25);
    for (const auto &line : scene_.coastlines) {
      cr->move_to(line.x1, line.y1);
      cr->line_to(line.x2, line.y2);
    }
    cr->stroke();
  }
  for (const auto &m : scene_.markers) {
    cr->set_source_rgb(1, 0.89, 0.64);
    if (m.local.position.symbol == "mountain") {
      cr->move_to(m.x, m.y - 7);
      cr->line_to(m.x - 6, m.y + 5);
      cr->line_to(m.x + 6, m.y + 5);
      cr->close_path();
    } else if (m.local.position.symbol == "city")
      cr->rectangle(m.x - 5, m.y - 5, 10, 10);
    else
      cr->arc(m.x, m.y, 5, 0, 2 * std::numbers::pi);
    cr->fill_preserve();
    cr->set_source_rgb(0.10, 0.13, 0.18);
    cr->set_line_width(2);
    cr->stroke();
    if (m.local.position.approximate) {
      cr->set_source_rgb(1, 0.89, 0.64);
      cr->set_dash(std::vector<double>{2, 3}, 0);
      cr->arc(m.x, m.y, 10, 0, 2 * std::numbers::pi);
      cr->stroke();
      cr->unset_dash();
    }
    if (m.label) {
      cr->set_font_size(12);
      cr->move_to(m.x + 10, m.y - 5);
      cr->text_path(m.label_text);
      cr->set_source_rgb(0.06, 0.09, 0.12);
      cr->set_line_width(3);
      cr->stroke_preserve();
      cr->set_source_rgb(1, 0.96, 0.84);
      cr->fill();
    }
  }
  const auto view = application::map_window(camera_, w, h);
  const auto ring = [&](geo::Coordinate center, double radius, double red,
                        double green, double blue) {
    const double x = w / 2.0 + geo::normalize_longitude(center.longitude -
                                                        camera_.longitude) *
                                   view.scale;
    const double y =
        h / 2.0 + (camera_.latitude - center.latitude) * view.scale;
    cr->set_source_rgba(red, green, blue, 0.9);
    cr->set_line_width(2);
    cr->set_dash(std::vector<double>{5, 4}, 0);
    cr->arc(x, y, radius * view.scale, 0, 2 * std::numbers::pi);
    cr->stroke();
    cr->unset_dash();
  };
  for (const auto &lock : terrain_locks_)
    ring(lock.center, lock.radius_degrees, 1.0, 0.55, 0.35);
  if (terrain_center_)
    ring(*terrain_center_, terrain_radius_.get_value(), 0.45, 0.9, 1.0);
  cr->restore();
}
void CartographyWorkspace::click(double x, double y) {
  if (!planet())
    return;
  canvas_.grab_focus();
  const auto point = application::map_coordinate(camera_, canvas_.get_width(),
                                                 canvas_.get_height(), x, y);
  if (!point)
    return;
  if (mode_.get_active_id() == "measure") {
    if (!measure_start_) {
      measure_start_ = point;
      message_.set_text("Primeiro ponto definido. Clique no segundo ponto.");
    } else {
      const double km =
          geo::distance_m(*measure_start_, *point, planet()->radius_m) / 1000.0;
      message_.set_text("Distância sobre a esfera: " + number(km) +
                        " km. Não é distância de rota nem tempo de viagem.");
      measure_start_.reset();
    }
    return;
  }
  if (mode_.get_active_id() == "place") {
    place_local(*point);
    return;
  }
  if (mode_.get_active_id() == "raise" || mode_.get_active_id() == "lower" ||
      mode_.get_active_id() == "smooth") {
    preview_terrain(*point);
    return;
  }
  for (const auto &m : scene_.markers)
    if (std::hypot(x - m.x, y - m.y) < 12) {
      selected_local_ = m.local.position.entity_id;
      local_selector_->set_selected(*selected_local_, m.local.name);
      reveal_entity(*selected_local_);
      return;
    }
  selection_.set_text("Longitude " + number(point->longitude, 4) +
                      "°\nLatitude " + number(point->latitude, 4) + "°");
}
void CartographyWorkspace::export_svg() {
  if (!planet() || !owner())
    return;
  try {
    rebuild_scene();
    const auto data = application::map_svg(
        scene_, planet()->name + (preview_ ? " — prévia" : ""),
        rendering_.get_active_id() != "raster", coastlines_.get_active());
    auto dialog = Gtk::FileChooserNative::create(
        "Exportar vista cartográfica", *owner(), Gtk::FileChooser::Action::SAVE,
        "Exportar", "Cancelar");
    dialog->set_current_name("mapa-inde.svg");
    dialog->signal_response().connect([this, dialog, data](int r) {
      if (r == Gtk::ResponseType::ACCEPT)
        try {
          auto file = dialog->get_file();
          if (!file)
            throw std::runtime_error("Destino inválido");
          std::string etag;
          file->replace_contents(data, "", etag, false,
                                 Gio::File::CreateFlags::NONE);
          message_.set_text(
              "Vista exportada como SVG. O planeta continua no Projeto.");
        } catch (const std::exception &e) {
          error(e);
        }
    });
    dialog->show();
  } catch (const std::exception &e) {
    error(e);
  }
}
} // namespace inde::ui

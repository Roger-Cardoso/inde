#include "inde/application/cartography_service.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <locale>
#include <set>
#include <sstream>

namespace inde::application {
namespace {
constexpr std::size_t cell_budget = 16384, marker_budget = 500,
                      label_budget = 80, coastline_budget = 32768;
std::pair<double, double> screen(geo::Coordinate p, MapCamera c, int w, int h,
                                 double scale) {
  return {w / 2.0 + geo::normalize_longitude(p.longitude - c.longitude) * scale,
          h / 2.0 + (c.latitude - p.latitude) * scale};
}
MapColor blend(MapColor a, MapColor b, double t) {
  t = std::clamp(t, 0.0, 1.0);
  return {a.red + (b.red - a.red) * t, a.green + (b.green - a.green) * t,
          a.blue + (b.blue - a.blue) * t};
}
MapColor color(int elevation, const geo::Planet &p, MapLayer layer) {
  if (elevation < 0)
    return blend({0.16, 0.44, 0.58}, {0.035, 0.13, 0.24},
                 -static_cast<double>(elevation) / p.depth_m);
  if (layer == MapLayer::OceanDepth)
    return {0.52, 0.58, 0.53};
  if (layer == MapLayer::LandWater)
    return blend({0.57, 0.65, 0.36}, {0.23, 0.38, 0.26},
                 static_cast<double>(elevation) / p.height_m);
  const double t = static_cast<double>(elevation) / p.height_m;
  return t < 0.5 ? blend({0.40, 0.55, 0.29}, {0.57, 0.43, 0.29}, t * 2)
                 : blend({0.57, 0.43, 0.29}, {0.94, 0.93, 0.87}, (t - 0.5) * 2);
}
std::uint32_t argb(MapColor value) {
  const auto component = [](double channel) {
    return static_cast<std::uint32_t>(
        std::lround(std::clamp(channel, 0.0, 1.0) * 255));
  };
  return 0xff000000U | (component(value.red) << 16) |
         (component(value.green) << 8) | component(value.blue);
}
std::string xml(std::string_view text) {
  std::string out;
  for (unsigned char c : text) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&apos;";
      break;
    default:
      if (c >= 32 || c == '\n' || c == '\t')
        out += static_cast<char>(c);
    }
  }
  return out;
}
} // namespace
int map_level(const geo::Planet &p, MapCamera c, int width, int height) {
  geo::validate(p);
  const auto window = map_window(c, width, height);
  int level = p.detail;
  while (level > 0) {
    std::set<geo::ChunkKey> keys;
    for (auto bounds : window.ranges)
      for (auto key : geo::visible_chunks(bounds, level))
        keys.insert(key);
    if (keys.size() <= 32)
      break;
    --level;
  }
  return level;
}
MapWindow map_window(MapCamera c, int width, int height) {
  if (width < 1 || height < 1 || width > 16384 || height > 16384 ||
      !std::isfinite(c.zoom) || c.zoom < 1 || c.zoom > 64)
    throw std::runtime_error("Câmera cartográfica inválida");
  geo::validate(geo::Coordinate{c.longitude, c.latitude});
  MapWindow out;
  out.scale = std::min(width / 360.0, height / 180.0) * c.zoom;
  const double half_lon = std::min(180.0, width / out.scale / 2),
               half_lat = height / out.scale / 2;
  const double south = std::max(-90.0, c.latitude - half_lat),
               north = std::min(90.0, c.latitude + half_lat);
  if (half_lon >= 180)
    out.ranges.push_back({-180, south, 180, north});
  else {
    const double west = geo::normalize_longitude(c.longitude - half_lon),
                 east = geo::normalize_longitude(c.longitude + half_lon);
    if (west < east)
      out.ranges.push_back({west, south, east, north});
    else {
      if (west < 180)
        out.ranges.push_back({west, south, 180, north});
      if (east > -180)
        out.ranges.push_back({-180, south, east, north});
    }
  }
  return out;
}
std::optional<geo::Coordinate> map_coordinate(MapCamera c, int w, int h,
                                              double x, double y) {
  const auto view = map_window(c, w, h);
  if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x > w ||
      y > h)
    return std::nullopt;
  const double lat = c.latitude - (y - h / 2.0) / view.scale;
  // The fitted world may leave margins; these aren't additional copies.
  if (lat < -90 || lat > 90 || std::abs(x - w / 2.0) > 180 * view.scale)
    return std::nullopt;
  return geo::Coordinate{
      geo::normalize_longitude(c.longitude + (x - w / 2.0) / view.scale), lat};
}
MapScene build_map_scene(const geo::Planet &p, MapCamera c, int w, int h,
                         MapLayer layer,
                         const std::vector<geo::TerrainChunk> &chunks,
                         std::vector<geo::PositionedLocal> locals) {
  geo::validate(p);
  const auto view = map_window(c, w, h);
  MapScene scene;
  scene.width = w;
  scene.height = h;
  scene.level = map_level(p, c, w, h);
  if (chunks.size() > 32 || locals.size() > 1002)
    throw std::runtime_error("Conjunto cartográfico excede orçamento");
  const int grid_width = geo::chunk_side * (2 << scene.level);
  const int grid_height = grid_width / 2;
  std::vector<std::int16_t> elevation(static_cast<std::size_t>(grid_width) *
                                      grid_height);
  std::vector<std::uint8_t> present(elevation.size());
  for (const auto &chunk : chunks) {
    geo::validate(chunk.key);
    if (chunk.key.level != scene.level)
      continue;
    for (int y = 0; y < geo::chunk_side; ++y)
      for (int x = 0; x < geo::chunk_side; ++x) {
        const int global_x = chunk.key.x * geo::chunk_side + x;
        const int global_y = chunk.key.y * geo::chunk_side + y;
        const auto at =
            static_cast<std::size_t>(global_y) * grid_width + global_x;
        elevation[at] = chunk.elevation[y * geo::chunk_side + x];
        present[at] = 1;
      }
  }
  constexpr int tile_side = geo::chunk_side + 2;
  const double degrees_per_sample = 360.0 / grid_width;
  const double sample_pixels = degrees_per_sample * view.scale;
  for (const auto &chunk : chunks) {
    if (chunk.key.level != scene.level)
      continue;
    const auto bounds = geo::chunk_bounds(chunk.key);
    const auto [center_x, center_y] = screen(
        {(bounds.west + bounds.east) / 2, (bounds.south + bounds.north) / 2}, c,
        w, h, view.scale);
    const double chunk_pixels = geo::chunk_side * sample_pixels;
    MapTerrainTile tile;
    tile.key = chunk.key;
    tile.clip_x = center_x - chunk_pixels / 2;
    tile.clip_y = center_y - chunk_pixels / 2;
    tile.clip_width = chunk_pixels;
    tile.clip_height = chunk_pixels;
    tile.paint_x = tile.clip_x - sample_pixels;
    tile.paint_y = tile.clip_y - sample_pixels;
    tile.sample_pixels = sample_pixels;
    tile.side = tile_side;
    tile.argb.resize(tile_side * tile_side);
    for (int y = -1; y <= geo::chunk_side; ++y)
      for (int x = -1; x <= geo::chunk_side; ++x) {
        int global_x = chunk.key.x * geo::chunk_side + x;
        global_x = (global_x % grid_width + grid_width) % grid_width;
        int global_y =
            std::clamp(chunk.key.y * geo::chunk_side + y, 0, grid_height - 1);
        auto at = static_cast<std::size_t>(global_y) * grid_width + global_x;
        if (!present[at]) {
          global_x = chunk.key.x * geo::chunk_side +
                     std::clamp(x, 0, geo::chunk_side - 1);
          global_y = chunk.key.y * geo::chunk_side +
                     std::clamp(y, 0, geo::chunk_side - 1);
          at = static_cast<std::size_t>(global_y) * grid_width + global_x;
        }
        tile.argb[static_cast<std::size_t>(y + 1) * tile_side + x + 1] =
            argb(color(elevation[at], p, layer));
      }
    scene.terrain_tiles.push_back(std::move(tile));
  }

  const auto add_coastline = [&](MapCoastline line) {
    if (scene.coastlines.size() < coastline_budget)
      scene.coastlines.push_back(line);
    else
      scene.limited = true;
  };
  const double world_pixels = 360.0 * view.scale;
  for (const auto &chunk : chunks) {
    if (chunk.key.level != scene.level)
      continue;
    for (int y = 0; y < geo::chunk_side; ++y)
      for (int x = 0; x < geo::chunk_side; ++x) {
        const int global_x = chunk.key.x * geo::chunk_side + x;
        const int global_y = chunk.key.y * geo::chunk_side + y;
        const auto at =
            static_cast<std::size_t>(global_y) * grid_width + global_x;
        const bool land = elevation[at] >= 0;
        const double longitude = -180.0 + (global_x + 0.5) * degrees_per_sample;
        const double latitude = 90.0 - (global_y + 0.5) * degrees_per_sample;
        const auto [x_center, y_center] =
            screen({longitude, latitude}, c, w, h, view.scale);
        const int right_x = (global_x + 1) % grid_width;
        const auto right_at =
            static_cast<std::size_t>(global_y) * grid_width + right_x;
        if (present[right_at] && (elevation[right_at] >= 0) != land) {
          const double edge_x = x_center + sample_pixels / 2;
          add_coastline({edge_x, y_center - sample_pixels / 2, edge_x,
                         y_center + sample_pixels / 2});
          if (global_x == grid_width - 1)
            add_coastline({edge_x - world_pixels, y_center - sample_pixels / 2,
                           edge_x - world_pixels,
                           y_center + sample_pixels / 2});
        }
        if (global_y + 1 < grid_height) {
          const auto below_at =
              static_cast<std::size_t>(global_y + 1) * grid_width + global_x;
          if (present[below_at] && (elevation[below_at] >= 0) != land) {
            const double edge_y = y_center + sample_pixels / 2;
            add_coastline({x_center - sample_pixels / 2, edge_y,
                           x_center + sample_pixels / 2, edge_y});
          }
        }
      }
  }
  int stride = 1;
  const double pixel =
      180.0 / (1 << scene.level) / geo::chunk_side * view.scale;
  const double visible_cells = (w / pixel + 2) * (h / pixel + 2);
  while (visible_cells / (stride * stride) > cell_budget ||
         (pixel * stride < 2 && stride < geo::chunk_side))
    stride *= 2;
  stride = std::min(stride, geo::chunk_side);
  for (const auto &chunk : chunks) {
    geo::validate(chunk.key);
    if (chunk.key.level != scene.level)
      continue;
    const auto b = geo::chunk_bounds(chunk.key);
    const double degrees = (b.east - b.west) / geo::chunk_side;
    for (int y = 0; y < geo::chunk_side; y += stride)
      for (int x = 0; x < geo::chunk_side; x += stride) {
        const auto [cx, cy] = screen({b.west + (x + stride / 2.0) * degrees,
                                      b.north - (y + stride / 2.0) * degrees},
                                     c, w, h, view.scale);
        const double side = stride * degrees * view.scale;
        const double left = std::max(0.0, cx - side / 2),
                     right = std::min(static_cast<double>(w), cx + side / 2);
        const double top = std::max(0.0, cy - side / 2),
                     bottom = std::min(static_cast<double>(h), cy + side / 2);
        if (right <= left || bottom <= top)
          continue;
        if (scene.cells.size() >= cell_budget) {
          scene.limited = true;
          continue;
        }
        scene.cells.push_back(
            {left, top, right - left, bottom - top,
             color(chunk.elevation[y * geo::chunk_side + x], p, layer)});
      }
  }
  std::sort(locals.begin(), locals.end(), [](const auto &a, const auto &b) {
    return a.position.importance != b.position.importance
               ? a.position.importance > b.position.importance
               : a.position.entity_id < b.position.entity_id;
  });
  std::set<std::string> seen;
  struct LabelBox {
    double x, y, w;
  };
  std::vector<LabelBox> placed;
  for (auto &local : locals) {
    geo::validate(local.position);
    if (!seen.insert(local.position.entity_id).second ||
        local.position.min_level >
            static_cast<int>(std::floor(std::log2(c.zoom))))
      continue;
    auto [x, y] = screen(local.position.coordinate, c, w, h, view.scale);
    if (x < 0 || y < 0 || x > w || y > h)
      continue;
    if (scene.markers.size() == marker_budget) {
      scene.limited = true;
      break;
    }
    // Bound UTF-8 labels without cutting a code point; retain the complete
    // entity name independently of its screen label.
    std::size_t end = 0, characters = 0;
    while (end < local.name.size() && characters < 20) {
      ++end;
      while (end < local.name.size() &&
             (static_cast<unsigned char>(local.name[end]) & 0xc0) == 0x80)
        ++end;
      ++characters;
    }
    std::string label_text = local.name.substr(0, end);
    if (end < local.name.size()) {
      label_text += "…";
      ++characters;
    }
    const double label_width = characters * 12.0;
    bool label =
        placed.size() < label_budget && x + 10 + label_width < w && y > 16;
    for (const auto &box : placed)
      if (x + 10 < box.x + box.w && x + 10 + label_width > box.x &&
          std::abs(y - box.y) < 18) {
        label = false;
        break;
      }
    if (label)
      placed.push_back({x + 10, y, label_width});
    scene.markers.push_back(
        {std::move(local), x, y, label, std::move(label_text)});
  }
  scene.active_chunks = chunks.size();
  scene.render_bytes = scene.cells.size() * sizeof(MapCell) +
                       scene.coastlines.size() * sizeof(MapCoastline);
  for (const auto &tile : scene.terrain_tiles)
    scene.render_bytes += tile.argb.size() * sizeof(std::uint32_t);
  return scene;
}
std::string map_svg(const MapScene &s, const std::string &title, bool smooth,
                    bool coastlines) {
  if (s.cells.size() > cell_budget || s.markers.size() > marker_budget ||
      s.coastlines.size() > coastline_budget || s.width < 1 || s.height < 1)
    throw std::runtime_error("Cena SVG inválida");
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::fixed << std::setprecision(3);
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << s.width
      << "\" height=\"" << s.height << "\" viewBox=\"0 0 " << s.width << ' '
      << s.height << "\">\n<title>" << xml(title) << "</title>\n";
  out << "<desc>INDE C-01: projeção equiretangular de esfera; vista limitada, "
         "não o planeta completo. Sem dados temporais ou inferência "
         "política.</desc>\n";
  if (smooth) {
    const double sample_pixels =
        s.terrain_tiles.empty() ? 1 : s.terrain_tiles.front().sample_pixels;
    out << "<defs><filter id=\"terrain-smooth\" x=\"-2%\" y=\"-2%\" "
           "width=\"104%\" height=\"104%\"><feGaussianBlur "
           "stdDeviation=\""
        << std::clamp(sample_pixels / 5, 0.35, 3.0) << "\"/></filter></defs>\n";
  }
  out << "<rect width=\"100%\" height=\"100%\" fill=\"#101820\"/>\n<g "
         "id=\"terrain\" shape-rendering=\""
      << (smooth ? "auto\" filter=\"url(#terrain-smooth)" : "crispEdges")
      << "\">\n";
  for (const auto &r : s.cells)
    out << "<rect x=\"" << r.x << "\" y=\"" << r.y << "\" width=\""
        << r.width + 0.02 << "\" height=\"" << r.height + 0.02
        << "\" fill=\"rgb(" << static_cast<int>(r.color.red * 255) << ','
        << static_cast<int>(r.color.green * 255) << ','
        << static_cast<int>(r.color.blue * 255) << ")\"/>\n";
  out << "</g>\n";
  if (coastlines) {
    out << "<g id=\"coastlines\" fill=\"none\" stroke=\"#fadb9f\" "
           "stroke-opacity=\"0.78\" stroke-width=\"1.25\">\n";
    for (const auto &line : s.coastlines)
      out << "<path d=\"M " << line.x1 << ' ' << line.y1 << " L " << line.x2
          << ' ' << line.y2 << "\"/>\n";
    out << "</g>\n";
  }
  out << "<g id=\"locals\" fill=\"#ffe4a3\" stroke=\"#202735\" "
         "stroke-width=\"2\">\n";
  for (const auto &m : s.markers) {
    out << "<g data-inde-entity=\"" << xml(m.local.position.entity_id)
        << "\"><title>" << xml(m.local.name) << "</title>";
    if (m.local.position.symbol == "mountain")
      out << "<path d=\"M " << m.x << ' ' << m.y - 7 << " L " << m.x - 6 << ' '
          << m.y + 5 << " L " << m.x + 6 << ' ' << m.y + 5 << " Z\"/>";
    else if (m.local.position.symbol == "city")
      out << "<rect x=\"" << m.x - 5 << "\" y=\"" << m.y - 5
          << "\" width=\"10\" height=\"10\"/>";
    else
      out << "<circle cx=\"" << m.x << "\" cy=\"" << m.y << "\" r=\"5\"/>";
    if (m.local.position.approximate)
      out << "<circle fill=\"none\" stroke=\"#ffe4a3\" stroke-dasharray=\"2 "
             "3\" cx=\""
          << m.x << "\" cy=\"" << m.y << "\" r=\"10\"/>";
    out << "</g>\n";
  }
  out << "</g>\n<g id=\"labels\" font-family=\"sans-serif\" font-size=\"12\" "
         "fill=\"#fff4d5\" stroke=\"#101820\" stroke-width=\"3\" "
         "paint-order=\"stroke\">\n";
  for (const auto &m : s.markers)
    if (m.label)
      out << "<text x=\"" << m.x + 10 << "\" y=\"" << m.y - 5 << "\">"
          << xml(m.label_text) << "</text>\n";
  out << "</g>\n</svg>\n";
  return out.str();
}
const project::Project &CartographyService::active() const {
  if (!session_.current())
    throw std::runtime_error("Abra um Projeto para usar Cartografia");
  return *session_.current();
}
std::vector<geo::Planet> CartographyService::planets() const {
  return store_.planets(active().path());
}
void CartographyService::accept(const geo::GeneratedPlanet &g) {
  store_.create(active().path(), g);
  clear_cache();
}
void CartographyService::clear_cache() noexcept {
  cache_.clear();
  cache_project_.clear();
  cache_planet_.clear();
}
MapScene CartographyService::scene(const geo::Planet &p, MapCamera c, int w,
                                   int h, MapLayer layer,
                                   const geo::TerrainPatch *preview) {
  const auto path = active().path();
  geo::validate(p);
  const auto view = map_window(c, w, h);
  const int level = map_level(p, c, w, h);
  if (cache_project_ != path.string() || cache_planet_ != p.id) {
    clear_cache();
    cache_project_ = path.string();
    cache_planet_ = p.id;
  }
  std::set<geo::ChunkKey> needed;
  for (const auto b : view.ranges)
    for (auto key : geo::visible_chunks(b, level))
      needed.insert(key);
  if (needed.size() > 32)
    throw std::runtime_error("A câmera excedeu o orçamento de chunks");
  std::vector<geo::ChunkKey> missing;
  for (const auto k : needed) {
    auto it = std::find_if(cache_.begin(), cache_.end(),
                           [&](const auto &t) { return t.key == k; });
    if (it == cache_.end())
      missing.push_back(k);
    else
      cache_.splice(cache_.begin(), cache_, it);
  }
  auto loaded = store_.chunks(path, p.id, missing);
  for (auto &chunk : loaded)
    cache_.push_front(std::move(chunk));
  // Don't evict an active chunk when filling this frame's working set.
  while (cache_.size() > 32) {
    auto it = std::prev(cache_.end());
    while (needed.contains(it->key))
      --it;
    cache_.erase(it);
  }
  std::vector<geo::TerrainChunk> chunks;
  for (const auto &t : cache_)
    if (needed.contains(t.key))
      chunks.push_back(t);
  if (preview) {
    if (preview->planet_id != p.id)
      throw std::runtime_error("Prévia pertence a outro planeta");
    for (auto &chunk : chunks) {
      const auto found = std::find_if(
          preview->chunks.begin(), preview->chunks.end(),
          [&](const auto &delta) { return delta.key == chunk.key; });
      if (found != preview->chunks.end())
        chunk = found->after;
    }
  }
  std::vector<geo::PositionedLocal> locals;
  for (const auto b : view.ranges) {
    auto rows = store_.locals(
        path, p.id, b, static_cast<int>(std::floor(std::log2(c.zoom))), 501);
    locals.insert(locals.end(), rows.begin(), rows.end());
  }
  auto result = build_map_scene(p, c, w, h, layer, chunks, std::move(locals));
  result.loaded_chunks = loaded.size();
  result.missing_chunks = needed.size() - chunks.size();
  result.cache_bytes = cache_.size() * sizeof(geo::TerrainChunk);
  return result;
}

std::vector<geo::TerrainChunk>
CartographyService::all_chunks(const geo::Planet &planet) const {
  geo::validate(planet);
  std::vector<geo::ChunkKey> keys;
  for (int level = 0; level <= planet.detail; ++level) {
    const auto at_level = geo::visible_chunks({}, level);
    keys.insert(keys.end(), at_level.begin(), at_level.end());
  }
  std::vector<geo::TerrainChunk> result;
  for (std::size_t offset = 0; offset < keys.size(); offset += 32) {
    const auto end = std::min(keys.size(), offset + 32);
    std::vector<geo::ChunkKey> batch(keys.begin() + offset, keys.begin() + end);
    auto chunks = store_.chunks(active().path(), planet.id, batch);
    result.insert(result.end(), std::make_move_iterator(chunks.begin()),
                  std::make_move_iterator(chunks.end()));
  }
  return result;
}

geo::TerrainPatch
CartographyService::preview_terrain(const geo::Planet &planet,
                                    const geo::TerrainBrush &brush) const {
  for (const auto &lock : terrain_locks(planet.id))
    if (geo::overlaps(brush, lock))
      throw std::runtime_error("O pincel intersecta a região bloqueada: " +
                               lock.name);
  return geo::preview_terrain_edit(planet, all_chunks(planet), brush);
}

int CartographyService::apply_terrain(const geo::TerrainPatch &patch,
                                      int expected_revision, bool reverse) {
  const auto revision =
      store_.apply_terrain(active().path(), patch, expected_revision, reverse);
  clear_cache();
  return revision;
}

std::vector<geo::TerrainLock>
CartographyService::terrain_locks(const std::string &planet) const {
  return store_.terrain_locks(active().path(), planet);
}

void CartographyService::add_terrain_lock(const geo::TerrainLock &lock) {
  store_.add_terrain_lock(active().path(), lock);
}

void CartographyService::remove_terrain_lock(const geo::TerrainLock &lock) {
  store_.remove_terrain_lock(active().path(), lock);
}
std::optional<geo::Position>
CartographyService::position(const std::string &planet,
                             const std::string &entity) const {
  return store_.position(active().path(), planet, entity);
}
void CartographyService::change_position(
    const std::string &planet, const std::string &entity,
    const std::optional<geo::Position> &expected,
    const std::optional<geo::Position> &desired) {
  store_.change_position(active().path(), planet, entity, expected, desired);
}
} // namespace inde::application

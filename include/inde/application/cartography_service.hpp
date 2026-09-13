#pragma once
#include "inde/application/project_session.hpp"
#include "inde/persistence/cartography_store.hpp"
#include <cstdint>
#include <functional>
#include <list>
#include <vector>

namespace inde::application {
namespace geo = project::geo;
struct MapCamera {
  double longitude{}, latitude{}, zoom{1};
};
enum class MapLayer { LandWater, Relief, OceanDepth };
struct MapColor {
  double red{}, green{}, blue{};
};
struct MapCell {
  double x{}, y{}, width{}, height{};
  MapColor color;
};
struct MapTerrainTile {
  geo::ChunkKey key;
  double clip_x{}, clip_y{}, clip_width{}, clip_height{};
  double paint_x{}, paint_y{}, sample_pixels{};
  int side{};
  std::vector<std::uint32_t> argb;
};
struct MapCoastline {
  double x1{}, y1{}, x2{}, y2{};
};
struct MapMarker {
  geo::PositionedLocal local;
  double x{}, y{};
  bool label{};
  std::string label_text;
};
struct MapScene {
  int width{}, height{}, level{};
  std::vector<MapCell> cells;
  std::vector<MapTerrainTile> terrain_tiles;
  std::vector<MapCoastline> coastlines;
  std::vector<MapMarker> markers;
  std::size_t active_chunks{}, loaded_chunks{}, missing_chunks{}, cache_bytes{},
      render_bytes{};
  bool limited{};
};
struct MapWindow {
  double scale{};
  std::vector<geo::Bounds> ranges;
};
MapWindow map_window(MapCamera camera, int width, int height);
int map_level(const geo::Planet &, MapCamera, int width, int height);
std::optional<geo::Coordinate> map_coordinate(MapCamera camera, int width,
                                              int height, double x, double y);
// Both GTK/Cairo and SVG consume this bounded scene; neither knows SQLite.
MapScene build_map_scene(const geo::Planet &, MapCamera, int width, int height,
                         MapLayer, const std::vector<geo::TerrainChunk> &,
                         std::vector<geo::PositionedLocal>);
std::string map_svg(const MapScene &, const std::string &title,
                    bool smooth = true, bool coastlines = true);

class CartographyService {
public:
  CartographyService(ProjectSession &session,
                     persistence::CartographyStore &store)
      : session_(session), store_(store) {}
  std::vector<geo::Planet> planets() const;
  void accept(const geo::GeneratedPlanet &);
  MapScene scene(const geo::Planet &, MapCamera, int, int, MapLayer,
                 const geo::TerrainPatch *preview = nullptr);
  geo::TerrainPatch preview_terrain(const geo::Planet &,
                                    const geo::TerrainBrush &) const;
  int apply_terrain(const geo::TerrainPatch &, int expected_revision,
                    bool reverse = false);
  std::vector<geo::TerrainLock> terrain_locks(const std::string &planet) const;
  void add_terrain_lock(const geo::TerrainLock &);
  void remove_terrain_lock(const geo::TerrainLock &);
  std::optional<geo::Position> position(const std::string &planet,
                                        const std::string &entity) const;
  void change_position(const std::string &planet, const std::string &entity,
                       const std::optional<geo::Position> &expected,
                       const std::optional<geo::Position> &desired);
  void clear_cache() noexcept;

private:
  const project::Project &active() const;
  std::vector<geo::TerrainChunk> all_chunks(const geo::Planet &) const;
  ProjectSession &session_;
  persistence::CartographyStore &store_;
  std::string cache_project_, cache_planet_;
  std::list<geo::TerrainChunk> cache_; // LRU, at most 32 x 8 KiB.
};
} // namespace inde::application

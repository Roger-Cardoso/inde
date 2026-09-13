#include "inde/application/project_service.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numbers>
#include <set>

namespace {
using namespace inde;
namespace geo = project::geo;
template <class F> void rejects(F action) {
  bool failed = false;
  try {
    action();
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
}
struct Temporary {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
                               ("inde-cartography-" + project::new_uuid());
  Temporary() { std::filesystem::create_directory(path); }
  ~Temporary() { std::filesystem::remove_all(path); }
};
geo::Planet planet() {
  geo::Planet p;
  p.id = project::new_uuid();
  p.name = "Mundo Áureo & <Ilhas>";
  p.created_at = project::utc_now();
  p.detail = 3;
  p.seed = 41852791;
  return p;
}
void geometry() {
  rejects([] {
    geo::validate(geo::Coordinate{0, std::numeric_limits<double>::quiet_NaN()});
  });
  rejects([] { geo::validate(geo::Coordinate{181, 0}); });
  rejects([] { geo::validate(geo::ChunkKey{4, 0, 0}); });
  assert(geo::normalize_longitude(180) == -180);
  assert(geo::normalize_longitude(-540) == -180);
  const double quarter = geo::distance_m({0, 0}, {90, 0}, 6371000);
  assert(std::abs(quarter - 6371000 * std::numbers::pi / 2) < 0.001);
  assert(std::abs(geo::distance_m({0, 90}, {120, 90}, 6371000)) < 0.001);
  assert(std::abs(geo::distance_m({179, 0}, {-179, 0}, 6371000) -
                  quarter / 45) < 0.001);
  assert(std::isfinite(geo::distance_m({0, 0}, {180, 0}, 6371000)));
  const auto w = application::map_window({179, 0, 8}, 1000, 500);
  assert(w.ranges.size() == 2);
  const auto edge = application::map_window({157.5, 0, 8}, 1000, 500);
  assert(edge.ranges.size() == 1);
  for (auto b : edge.ranges)
    geo::validate(b);
  const auto center =
      application::map_coordinate({179, 10, 8}, 1000, 500, 500, 250);
  assert(center && center->longitude == 179 && center->latitude == 10);
  assert(!application::map_coordinate({}, 1000, 1000, 500, 0));
  assert(geo::visible_chunks({}, 0).size() == 2);
  assert(geo::visible_chunks({-180, 0, 0, 90}, 1).size() == 2);
  const auto resolution_planet = planet();
  assert(application::map_level(resolution_planet, {}, 1000, 500) == 2);
  assert(application::map_level(resolution_planet, {0, 0, 2}, 1000, 500) == 3);
}
geo::GeneratedPlanet generation() {
  auto p = planet();
  const auto started = std::chrono::steady_clock::now();
  auto g = geo::generate(p);
  const auto elapsed = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - started)
                           .count();
  std::cout << "C-01 generation_ms=" << elapsed << " chunks=" << g.chunks.size()
            << '\n';
  assert(g.chunks.size() == 170);
  assert(geo::generate(p).chunks == g.chunks);
  ++p.seed;
  assert(geo::generate(p).chunks != g.chunks);
  auto copy = p;
  copy.detail = 4;
  rejects([&] { geo::generate(copy); });
  std::stop_source stop;
  stop.request_stop();
  rejects([&] { geo::generate(p, stop.get_token()); });
  double water = 0, total = 0;
  for (const auto &t : g.chunks)
    if (t.key.level == 3) {
      auto b = geo::chunk_bounds(t.key);
      for (int y = 0; y < 64; ++y) {
        const double latitude = b.north - (y + 0.5) * (b.north - b.south) / 64;
        const double weight = std::cos(latitude * std::numbers::pi / 180);
        for (int x = 0; x < 64; ++x) {
          const auto h = t.elevation[y * 64 + x];
          assert(h >= -p.depth_m && h <= p.height_m);
          total += weight;
          if (h < 0)
            water += weight;
        }
      }
    }
  assert(std::abs(water / total * 100 - g.planet.water_percent) < 0.1);
  // Every coarse cell is the 2x2 mean of the authoritative next level.
  for (const auto &t : g.chunks)
    if (t.key.level < 3)
      for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
          geo::ChunkKey key{t.key.level + 1, t.key.x * 2 + x / 32,
                            t.key.y * 2 + y / 32};
          const auto child =
              std::find_if(g.chunks.begin(), g.chunks.end(),
                           [&](const auto &v) { return v.key == key; });
          assert(child != g.chunks.end());
          const int at = (y % 32) * 2 * 64 + (x % 32) * 2;
          const int expected =
              (child->elevation[at] + child->elevation[at + 1] +
               child->elevation[at + 64] + child->elevation[at + 65]) /
              4;
          assert(t.elevation[y * 64 + x] == expected);
        }
  return g;
}
void verify_pyramid(const std::vector<geo::TerrainChunk> &chunks, int detail) {
  for (const auto &t : chunks)
    if (t.key.level < detail)
      for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
          const geo::ChunkKey key{t.key.level + 1, t.key.x * 2 + x / 32,
                                  t.key.y * 2 + y / 32};
          const auto child =
              std::find_if(chunks.begin(), chunks.end(),
                           [&](const auto &v) { return v.key == key; });
          assert(child != chunks.end());
          const int at = (y % 32) * 2 * 64 + (x % 32) * 2;
          assert(t.elevation[y * 64 + x] ==
                 (child->elevation[at] + child->elevation[at + 1] +
                  child->elevation[at + 64] + child->elevation[at + 65]) /
                     4);
        }
}
void terrain_editing(const geo::GeneratedPlanet &generated) {
  const auto original = generated.chunks;
  const geo::TerrainBrush raise{
      generated.planet.id, {179, 0}, 4, 900, geo::TerrainEditMode::Raise};
  const auto started = std::chrono::steady_clock::now();
  const auto patch =
      geo::preview_terrain_edit(generated.planet, generated.chunks, raise);
  std::cout << "C-02 preview_ms="
            << std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - started)
                   .count()
            << " samples=" << patch.changed_samples
            << " chunks=" << patch.chunks.size() << '\n';
  assert(generated.chunks == original && patch.changed_samples > 0 &&
         !patch.chunks.empty() &&
         patch.chunks.size() < generated.chunks.size());
  std::map<geo::ChunkKey, geo::TerrainChunk> changed;
  for (const auto &chunk : original)
    changed.emplace(chunk.key, chunk);
  for (const auto &delta : patch.chunks) {
    assert(delta.before == changed.at(delta.key));
    changed[delta.key] = delta.after;
  }
  std::vector<geo::TerrainChunk> after;
  for (auto &[key, chunk] : changed)
    after.push_back(std::move(chunk));
  verify_pyramid(after, generated.planet.detail);
  assert(geo::preview_terrain_edit(generated.planet, generated.chunks, raise)
             .chunks == patch.chunks);
  auto lower = raise;
  lower.center = {-179, 0};
  lower.mode = geo::TerrainEditMode::Lower;
  assert(!geo::preview_terrain_edit(generated.planet, generated.chunks, lower)
              .chunks.empty());
  auto smooth = raise;
  smooth.center = {40, 89};
  smooth.mode = geo::TerrainEditMode::Smooth;
  assert(!geo::preview_terrain_edit(generated.planet, generated.chunks, smooth)
              .chunks.empty());
  auto invalid = raise;
  invalid.radius_degrees = 20;
  rejects([&] {
    geo::preview_terrain_edit(generated.planet, generated.chunks, invalid);
  });
  auto incomplete = generated.chunks;
  incomplete.pop_back();
  rejects(
      [&] { geo::preview_terrain_edit(generated.planet, incomplete, raise); });
  const geo::TerrainLock lock{
      project::new_uuid(), generated.planet.id, "Costa Ω", {-178, 0}, 2,
      project::utc_now()};
  assert(geo::overlaps(raise, lock));
  auto other = lock;
  other.planet_id = project::new_uuid();
  assert(!geo::overlaps(raise, other));
}
void persistence_and_scene(const geo::GeneratedPlanet &g) {
  Temporary temp;
  setenv("XDG_CONFIG_HOME", (temp.path / "config").c_str(), 1);
  application::ProjectService service;
  const auto path =
      service.create(temp.path / "Atlas", "Atlas cartográfico").path();
  auto document = service.writing().create_document("Texto preservado");
  document.content = "Ação 🌍";
  document = service.writing().update_document(document);
  const auto db_path =
      persistence::ProjectDatabaseRepository::database_path(path);
  // Reconstruct a real pre-cartography v13 database and migrate it on reopen.
  service.close();
  {
    persistence::SqliteDatabase db(db_path);
    db.execute("DROP TABLE cartographic_terrain_locks;ALTER TABLE "
               "cartographic_planets DROP COLUMN terrain_revision;DELETE FROM "
               "schema_migrations WHERE version=15;");
    assert(persistence::SchemaMigrator{}.current_version(db) == 14);
    db.execute(
        "DROP TRIGGER cartographic_local_type_guard;DROP TABLE "
        "cartographic_positions;DROP TABLE cartographic_position_index;DROP "
        "TABLE cartographic_chunks;DROP TABLE cartographic_planets;DELETE FROM "
        "schema_migrations WHERE version=14;");
    assert(persistence::SchemaMigrator{}.current_version(db) == 13);
  }
  service.open(path);
  assert(service.writing().document(document.id)->content == document.content);
  assert(service.cartography().planets().empty());
  auto incomplete = g;
  incomplete.chunks.pop_back();
  rejects([&] { service.cartography().accept(incomplete); });
  assert(service.cartography().planets().empty());
  // Force failure midway through chunk insertion; no metadata or log survives.
  {
    persistence::SqliteDatabase db(db_path);
    db.execute("CREATE TRIGGER fail_chunk BEFORE INSERT ON cartographic_chunks "
               "WHEN NEW.x=1 BEGIN SELECT RAISE(ABORT,'test fault'); END;");
  }
  rejects([&] { service.cartography().accept(g); });
  assert(service.cartography().planets().empty());
  {
    persistence::SqliteDatabase db(db_path);
    assert(db.query_integer("SELECT count(*) FROM cartographic_chunks") == 0);
    assert(db.query_integer("SELECT count(*) FROM change_log WHERE "
                            "command_name='create_cartographic_planet'") == 0);
    db.execute("DROP TRIGGER fail_chunk;");
  }
  service.cartography().accept(g);
  rejects([&] { service.cartography().accept(g); });
  assert(service.cartography().planets().size() == 1);
  persistence::SqliteCartographyRepository store;
  auto world = service.cartography().planets().front();
  assert(world.terrain_revision == 0);
  const geo::TerrainBrush brush{
      world.id, {12, -15}, 3, 700, geo::TerrainEditMode::Raise};
  const auto patch = service.cartography().preview_terrain(world, brush);
  const auto edited_key = patch.chunks.front().key;
  const auto terrain_before = store.chunks(path, world.id, {edited_key});
  {
    persistence::SqliteDatabase db(db_path);
    db.execute("CREATE TRIGGER fail_terrain BEFORE UPDATE OF elevation ON "
               "cartographic_chunks BEGIN SELECT RAISE(ABORT,'terrain test "
               "fault'); END;");
  }
  rejects([&] {
    service.cartography().apply_terrain(patch, world.terrain_revision);
  });
  assert(store.chunks(path, world.id, {edited_key}) == terrain_before);
  assert(service.cartography().planets().front().terrain_revision == 0);
  {
    persistence::SqliteDatabase db(db_path);
    assert(db.query_integer("SELECT count(*) FROM change_log WHERE "
                            "command_name='edit_cartographic_terrain'") == 0);
    db.execute("DROP TRIGGER fail_terrain;");
  }
  world.terrain_revision =
      service.cartography().apply_terrain(patch, world.terrain_revision);
  assert(world.terrain_revision == 1);
  assert(store.chunks(path, world.id, {edited_key}).front() ==
         patch.chunks.front().after);
  rejects([&] { service.cartography().apply_terrain(patch, 0); });
  world.terrain_revision =
      service.cartography().apply_terrain(patch, world.terrain_revision, true);
  assert(world.terrain_revision == 2 &&
         store.chunks(path, world.id, {edited_key}) == terrain_before);
  world.terrain_revision =
      service.cartography().apply_terrain(patch, world.terrain_revision);
  assert(world.terrain_revision == 3);

  const geo::TerrainLock terrain_lock{
      project::new_uuid(), world.id, "Cordilheira Ω",
      {12, -15},           2,        project::utc_now()};
  service.cartography().add_terrain_lock(terrain_lock);
  assert(service.cartography().terrain_locks(world.id) ==
         std::vector<geo::TerrainLock>{terrain_lock});
  rejects([&] { service.cartography().preview_terrain(world, brush); });
  rejects([&] {
    service.cartography().apply_terrain(patch, world.terrain_revision, true);
  });
  service.cartography().remove_terrain_lock(terrain_lock);
  assert(service.cartography().terrain_locks(world.id).empty());
  rejects([&] { service.cartography().remove_terrain_lock(terrain_lock); });
  service.cartography().add_terrain_lock(terrain_lock);
  const auto stored = store.chunks(path, g.planet.id, {{3, 0, 0}});
  assert(stored.front() == g.chunks.front());
  rejects([&] {
    store.chunks(path, g.planet.id, std::vector<geo::ChunkKey>(33, {0, 0, 0}));
  });
  const auto local = service.narrative().create_entity(
      "00000000-0000-4000-9000-000000000002", "Arkan & <Porto>");
  const auto other = service.narrative().create_entity(
      "00000000-0000-4000-9000-000000000001", "Pessoa");
  geo::Position p{g.planet.id, local.id, {179.9999999, 0}, true, 90, 0, "city"};
  service.cartography().change_position(p.planet_id, p.entity_id, {}, p);
  auto current = service.cartography().position(p.planet_id, p.entity_id);
  assert(current && current->coordinate.longitude == -180);
  rejects([&] {
    service.cartography().change_position(p.planet_id, p.entity_id, {}, p);
  });
  auto invalid = p;
  invalid.entity_id = other.id;
  rejects([&] {
    store.change_position(path, invalid.planet_id, invalid.entity_id, {},
                          invalid);
  });
  invalid = p;
  invalid.entity_id = "missing";
  rejects([&] {
    store.change_position(path, invalid.planet_id, invalid.entity_id, {},
                          invalid);
  });
  invalid = p;
  invalid.coordinate.latitude = 91;
  rejects([&] {
    service.cartography().change_position(p.planet_id, p.entity_id, current,
                                          invalid);
  });
  auto changed_entity = local;
  changed_entity.entity_type_id = other.entity_type_id;
  rejects([&] {
    static_cast<void>(service.narrative().update_entity(changed_entity));
  });
  auto wrong_planet = p;
  wrong_planet.planet_id = project::new_uuid();
  rejects([&] {
    store.change_position(path, wrong_planet.planet_id, p.entity_id, {},
                          wrong_planet);
  });
  p.coordinate = {-179, 2};
  service.cartography().change_position(p.planet_id, p.entity_id, current, p);
  current = service.cartography().position(p.planet_id, p.entity_id);
  assert(store.locals(path, g.planet.id, {-180, -10, -170, 10}, 0, 10).size() ==
         1);
  assert(store.locals(path, g.planet.id, {0, -10, 10, 10}, 0, 10).empty());
  auto first = service.cartography().scene(g.planet, {}, 1000, 500,
                                           application::MapLayer::Relief);
  auto second = service.cartography().scene(g.planet, {}, 1000, 500,
                                            application::MapLayer::Relief);
  assert(first.level == 2 && first.loaded_chunks == 32 &&
         second.loaded_chunks == 0 && second.cells.size() <= 16384);
  assert(first.terrain_tiles.size() == 32 && !first.coastlines.empty());
  std::cout << "C-02 render_kib=" << first.render_bytes / 1024.0
            << " coastlines=" << first.coastlines.size() << '\n';
  assert(first.render_bytes > 0 && first.render_bytes <= 3 * 1024 * 1024);
  for (const auto &tile : first.terrain_tiles)
    assert(tile.side == geo::chunk_side + 2 &&
           tile.argb.size() ==
               static_cast<std::size_t>(tile.side * tile.side) &&
           tile.sample_pixels > 0);
  const double coastline_step = first.terrain_tiles.front().sample_pixels;
  for (const auto &line : first.coastlines) {
    assert(std::isfinite(line.x1) && std::isfinite(line.y1) &&
           std::isfinite(line.x2) && std::isfinite(line.y2));
    assert(std::hypot(line.x2 - line.x1, line.y2 - line.y1) <=
           coastline_step * 1.01);
  }
  const auto tile_at =
      [&](geo::ChunkKey key) -> const application::MapTerrainTile & {
    const auto found =
        std::find_if(first.terrain_tiles.begin(), first.terrain_tiles.end(),
                     [&](const auto &tile) { return tile.key == key; });
    assert(found != first.terrain_tiles.end());
    return *found;
  };
  const auto &west = tile_at({2, 0, 0});
  const auto &east = tile_at({2, 7, 0});
  for (int y = 1; y <= geo::chunk_side; ++y)
    assert(east.argb[static_cast<std::size_t>(y) * east.side + east.side - 1] ==
           west.argb[static_cast<std::size_t>(y) * west.side + 1]);
  assert(first.markers.size() == 1);
  auto long_local = first.markers.front().local;
  long_local.name = "Águias e montanhas de um continente muito distante Ω";
  long_local.position.coordinate = {0, 0};
  const auto labels = application::build_map_scene(
      g.planet, {}, 1000, 500, application::MapLayer::Relief, {}, {long_local});
  assert(labels.markers.front().label);
  assert(labels.markers.front().label_text.ends_with("…"));
  assert(labels.markers.front().local.name == long_local.name);
  assert(application::map_svg(labels, "Rótulos")
             .find(labels.markers.front().label_text) != std::string::npos);
  assert(second.cache_bytes <= 32 * sizeof(geo::TerrainChunk));
  const auto before = store.chunks(path, g.planet.id, {{3, 0, 0}});
  const auto thin = service.cartography().scene(g.planet, {0, 0, 64}, 32, 4096,
                                                application::MapLayer::Relief);
  assert(thin.active_chunks <= 32 && thin.cells.size() <= 16384);
  const auto started = std::chrono::steady_clock::now();
  for (int i = 0; i < 100; ++i) {
    application::MapCamera c{geo::normalize_longitude(i * 37),
                             static_cast<double>((i * 17) % 180 - 90),
                             std::pow(2.0, i % 7)};
    auto scene = service.cartography().scene(g.planet, c, 1200, 600,
                                             application::MapLayer::LandWater);
    assert(scene.cells.size() <= 16384 && scene.active_chunks <= 32 &&
           scene.missing_chunks == 0 &&
           scene.cache_bytes <= 32 * sizeof(geo::TerrainChunk) &&
           scene.render_bytes <= 3 * 1024 * 1024);
  }
  std::cout << "C-01 100 camera changes avg_ms="
            << std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - started)
                       .count() /
                   100
            << '\n';
  assert(before == store.chunks(path, g.planet.id, {{3, 0, 0}}));
  const auto svg = application::map_svg(first, g.planet.name);
  assert(svg.find("&amp;") != std::string::npos &&
         svg.find("&lt;Ilhas&gt;") != std::string::npos &&
         svg.find("data-inde-entity") != std::string::npos &&
         svg.find("terrain-smooth") != std::string::npos &&
         svg.find("id=\"coastlines\"") != std::string::npos);
  const auto raster_svg =
      application::map_svg(first, g.planet.name, false, false);
  assert(raster_svg.find("crispEdges") != std::string::npos &&
         raster_svg.find("terrain-smooth") == std::string::npos &&
         raster_svg.find("id=\"coastlines\"") == std::string::npos);
  service.close();
  service.open(path);
  assert(service.cartography().position(p.planet_id, p.entity_id) == current);
  const auto copied = service.save_as(temp.path / "Cópia").path();
  assert(copied != path);
  assert(service.cartography().position(p.planet_id, p.entity_id) == current);
  assert(store.chunks(copied, g.planet.id, {{3, 0, 0}}) == before);
  assert(store.terrain_locks(copied, world.id) ==
         std::vector<geo::TerrainLock>{terrain_lock});
  service.cartography().change_position(p.planet_id, p.entity_id, current, {});
  assert(!service.cartography().position(p.planet_id, p.entity_id));
  service.cartography().change_position(p.planet_id, p.entity_id, {},
                                        current); // undo
  service.narrative().delete_entity(local.id);
  assert(!service.cartography().position(p.planet_id, p.entity_id));
  service.close();
  service.open(path);
  assert(service.cartography().position(p.planet_id, p.entity_id) == current);
  for (const auto &project_path : {path, copied}) {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(project_path));
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    assert(db.query_text("SELECT rtreecheck('cartographic_position_index')") ==
           "ok");
    auto fk = db.prepare("PRAGMA foreign_key_check");
    assert(!fk.step());
  }
  application::ProjectService foreign;
  const auto foreign_path = foreign.create(temp.path / "Fora", "Fora").path();
  foreign.cartography().accept(g);
  rejects([&] {
    store.change_position(foreign_path, p.planet_id, p.entity_id, {}, p);
  });
  {
    persistence::SqliteDatabase db(db_path);
    db.execute("INSERT INTO schema_migrations VALUES(16,'future');");
  }
  service.close();
  rejects([&] { service.open(path); });
  assert(!service.current());
}
} // namespace
int main() {
  geometry();
  auto g = generation();
  terrain_editing(g);
  persistence_and_scene(g);
  std::cout << "Cartography C-01/C-02 tests passed.\n";
}

#include "inde/persistence/sqlite_cartography_repository.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <cmath>
#include <set>

namespace inde::persistence {
namespace {
namespace geo = project::geo;
SqliteDatabase open(const std::filesystem::path &path) {
  const auto file = ProjectDatabaseRepository::database_path(path);
  if (!std::filesystem::is_regular_file(file))
    throw std::runtime_error("Banco do Projeto indisponível");
  return SqliteDatabase(file);
}
std::string owner(SqliteDatabase &db) {
  return db.query_text("SELECT id FROM projects");
}
void log(SqliteDatabase &db, const std::string &project_id,
         const std::string &command) {
  auto q = db.prepare(
      "INSERT INTO change_log(id,project_id,command_name,system_created_at) "
      "VALUES(?,?,?,?)");
  q.bind(1, project::new_uuid());
  q.bind(2, project_id);
  q.bind(3, command);
  q.bind(4, project::utc_now());
  q.run();
}
geo::Position read_position(SqliteStatement &q) {
  return {q.column_text(0),
          q.column_text(1),
          {q.column_integer(2) / 1000000.0, q.column_integer(3) / 1000000.0},
          q.column_integer(4) != 0,
          static_cast<int>(q.column_integer(5)),
          static_cast<int>(q.column_integer(6)),
          q.column_text(7)};
}
std::optional<geo::Position> position_in(SqliteDatabase &db,
                                         const std::string &planet,
                                         const std::string &entity) {
  auto q = db.prepare("SELECT "
                      "planet_id,entity_id,longitude_e6,latitude_e6,"
                      "approximate,importance,min_level,symbol "
                      "FROM cartographic_positions WHERE planet_id=? AND "
                      "entity_id=? AND project_id=?");
  q.bind(1, planet);
  q.bind(2, entity);
  q.bind(3, owner(db));
  if (!q.step())
    return std::nullopt;
  return read_position(q);
}
std::array<std::uint8_t, geo::chunk_side * geo::chunk_side * 2>
encode(const geo::TerrainChunk &chunk) {
  std::array<std::uint8_t, geo::chunk_side * geo::chunk_side * 2> bytes{};
  for (std::size_t i = 0; i < chunk.elevation.size(); ++i) {
    const auto bits = static_cast<std::uint16_t>(chunk.elevation[i]);
    bytes[2 * i] = static_cast<std::uint8_t>(bits & 255);
    bytes[2 * i + 1] = static_cast<std::uint8_t>(bits >> 8);
  }
  return bytes;
}
geo::TerrainChunk decode(geo::ChunkKey key,
                         const std::vector<std::uint8_t> &bytes) {
  if (bytes.size() != geo::chunk_side * geo::chunk_side * 2)
    throw std::runtime_error("Chunk de terreno corrompido");
  geo::TerrainChunk chunk{key, {}};
  for (std::size_t i = 0; i < chunk.elevation.size(); ++i) {
    const auto bits = static_cast<unsigned>(bytes[2 * i]) |
                      (static_cast<unsigned>(bytes[2 * i + 1]) << 8);
    chunk.elevation[i] = static_cast<std::int16_t>(
        bits <= 32767 ? static_cast<int>(bits)
                      : static_cast<int>(bits) - 65536);
  }
  return chunk;
}
std::vector<geo::TerrainLock> read_locks(SqliteDatabase &db,
                                         const std::string &planet,
                                         const std::string &project_id) {
  auto q = db.prepare(
      "SELECT id,planet_id,name,longitude_e6,latitude_e6,radius_e6,created_at "
      "FROM cartographic_terrain_locks WHERE planet_id=? AND project_id=? "
      "ORDER BY created_at,id LIMIT 101");
  q.bind(1, planet);
  q.bind(2, project_id);
  std::vector<geo::TerrainLock> out;
  while (q.step()) {
    geo::TerrainLock lock{
        q.column_text(0),
        q.column_text(1),
        q.column_text(2),
        {q.column_integer(3) / 1000000.0, q.column_integer(4) / 1000000.0},
        q.column_integer(5) / 1000000.0,
        q.column_text(6)};
    geo::validate(lock);
    out.push_back(std::move(lock));
  }
  if (out.size() > 100)
    throw std::runtime_error("C-02 suporta até 100 bloqueios por planeta");
  return out;
}
} // namespace

std::vector<project::geo::Planet>
SqliteCartographyRepository::planets(const std::filesystem::path &path) const {
  auto db = open(path);
  auto q = db.prepare("SELECT "
                      "id,name,radius_m,seed,water_percent,fragmentation,"
                      "height_m,depth_m,detail,generator,created_at,"
                      "terrain_revision "
                      "FROM cartographic_planets WHERE project_id=? ORDER BY "
                      "name,id LIMIT 101");
  q.bind(1, owner(db));
  std::vector<geo::Planet> out;
  while (q.step()) {
    geo::Planet p{q.column_text(0),
                  q.column_text(1),
                  q.column_integer(2),
                  q.column_integer(3),
                  static_cast<int>(q.column_integer(4)),
                  static_cast<int>(q.column_integer(5)),
                  static_cast<int>(q.column_integer(6)),
                  static_cast<int>(q.column_integer(7)),
                  static_cast<int>(q.column_integer(8)),
                  static_cast<int>(q.column_integer(9)),
                  q.column_text(10),
                  static_cast<int>(q.column_integer(11))};
    geo::validate(p);
    out.push_back(std::move(p));
  }
  if (out.size() > 100)
    throw std::runtime_error("C-01 suporta até 100 planetas por Projeto");
  return out;
}
void SqliteCartographyRepository::create(const std::filesystem::path &path,
                                         const geo::GeneratedPlanet &g) const {
  geo::validate(g.planet);
  std::set<geo::ChunkKey> required;
  for (int l = 0; l <= g.planet.detail; ++l)
    for (const auto k : geo::visible_chunks({}, l))
      required.insert(k);
  if (g.chunks.size() != required.size())
    throw std::runtime_error("Pirâmide de terreno incompleta");
  for (const auto &chunk : g.chunks) {
    geo::validate(chunk.key);
    if (required.erase(chunk.key) != 1)
      throw std::runtime_error("Chunk repetido ou fora da pirâmide");
    for (auto h : chunk.elevation)
      if (h < -g.planet.depth_m || h > g.planet.height_m)
        throw std::runtime_error("Elevação fora da escala declarada");
  }
  auto db = open(path);
  SqliteTransaction tx(db);
  const auto project_id = owner(db);
  auto count = db.prepare(
      "SELECT count(*) FROM cartographic_planets WHERE project_id=?");
  count.bind(1, project_id);
  count.step();
  if (count.column_integer(0) >= 100)
    throw std::runtime_error("Limite de 100 planetas neste incremento");
  auto q = db.prepare(
      "INSERT INTO "
      "cartographic_planets(id,project_id,name,radius_m,seed,water_percent,"
      "fragmentation,height_m,depth_m,detail,generator,created_at) "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
  const auto &p = g.planet;
  q.bind(1, p.id);
  q.bind(2, project_id);
  q.bind(3, p.name);
  q.bind(4, p.radius_m);
  q.bind(5, p.seed);
  q.bind(6, std::int64_t{p.water_percent});
  q.bind(7, std::int64_t{p.fragmentation});
  q.bind(8, std::int64_t{p.height_m});
  q.bind(9, std::int64_t{p.depth_m});
  q.bind(10, std::int64_t{p.detail});
  q.bind(11, std::int64_t{p.generator});
  q.bind(12, p.created_at);
  q.run();
  auto insert = db.prepare(
      "INSERT INTO cartographic_chunks(planet_id,level,x,y,elevation) "
      "VALUES(?,?,?,?,?)");
  for (const auto &chunk : g.chunks) {
    const auto bytes = encode(chunk);
    insert.bind(1, p.id);
    insert.bind(2, std::int64_t{chunk.key.level});
    insert.bind(3, std::int64_t{chunk.key.x});
    insert.bind(4, std::int64_t{chunk.key.y});
    insert.bind_blob(5, bytes);
    insert.run();
    insert.reset();
  }
  log(db, project_id, "create_cartographic_planet");
  tx.commit();
}
std::vector<project::geo::TerrainChunk> SqliteCartographyRepository::chunks(
    const std::filesystem::path &path, const std::string &planet,
    const std::vector<geo::ChunkKey> &keys) const {
  if (keys.size() > 32)
    throw std::runtime_error("Consulta excede 32 chunks ativos");
  if (keys.empty())
    return {};
  for (auto key : keys)
    geo::validate(key);
  auto db = open(path);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  auto q = db.prepare("SELECT c.elevation FROM cartographic_chunks c JOIN "
                      "cartographic_planets p ON p.id=c.planet_id "
                      "WHERE c.planet_id=? AND c.level=? AND c.x=? AND c.y=? "
                      "AND p.project_id=?");
  const auto project_id = owner(db);
  std::vector<geo::TerrainChunk> out;
  for (const auto key : keys) {
    q.bind(1, planet);
    q.bind(2, std::int64_t{key.level});
    q.bind(3, std::int64_t{key.x});
    q.bind(4, std::int64_t{key.y});
    q.bind(5, project_id);
    if (q.step()) {
      const auto bytes = q.column_blob(0);
      out.push_back(decode(key, bytes));
    }
    q.reset();
  }
  tx.commit();
  return out;
}
std::vector<project::geo::PositionedLocal>
SqliteCartographyRepository::locals(const std::filesystem::path &path,
                                    const std::string &planet, geo::Bounds b,
                                    int level, std::size_t limit) const {
  geo::validate(b);
  if (level < 0 || level > 6 || limit < 1 || limit > 501)
    throw std::runtime_error("Consulta espacial inválida");
  auto db = open(path);
  auto q = db.prepare(
      "SELECT "
      "p.planet_id,p.entity_id,p.longitude_e6,p.latitude_e6,p.approximate,p."
      "importance,p.min_level,p.symbol,e.name "
      "FROM cartographic_position_index r JOIN cartographic_positions p ON "
      "p.id=r.id JOIN entities e ON e.id=p.entity_id "
      "WHERE r.min_lon>=? AND r.max_lon<=? AND r.min_lat>=? AND r.max_lat<=? "
      "AND p.planet_id=? AND p.project_id=? AND p.min_level<=? ORDER BY "
      "p.importance DESC,p.entity_id LIMIT ?");
  q.bind(1, static_cast<std::int64_t>(std::ceil(b.west * 1000000)));
  q.bind(2, static_cast<std::int64_t>(std::floor(b.east * 1000000)));
  q.bind(3, static_cast<std::int64_t>(std::ceil(b.south * 1000000)));
  q.bind(4, static_cast<std::int64_t>(std::floor(b.north * 1000000)));
  q.bind(5, planet);
  q.bind(6, owner(db));
  q.bind(7, std::int64_t{level});
  q.bind(8, static_cast<std::int64_t>(limit));
  std::vector<geo::PositionedLocal> out;
  while (q.step())
    out.push_back({read_position(q), q.column_text(8)});
  return out;
}
std::optional<project::geo::Position>
SqliteCartographyRepository::position(const std::filesystem::path &path,
                                      const std::string &planet,
                                      const std::string &entity) const {
  auto db = open(path);
  return position_in(db, planet, entity);
}
void SqliteCartographyRepository::change_position(
    const std::filesystem::path &path, const std::string &planet,
    const std::string &entity, const std::optional<geo::Position> &expected,
    const std::optional<geo::Position> &desired) const {
  if (planet.empty() || entity.empty())
    throw std::runtime_error("Destino espacial inválido");
  if (desired) {
    geo::validate(*desired);
    if (desired->planet_id != planet || desired->entity_id != entity)
      throw std::runtime_error("Identidade da posição divergente");
  }
  auto db = open(path);
  SqliteTransaction tx(db);
  const auto project_id = owner(db);
  if (position_in(db, planet, entity) != expected)
    throw std::runtime_error(
        "A posição mudou. Reabra o Local antes de editar ou desfazer.");
  if (desired) {
    auto q = db.prepare(
        "INSERT INTO "
        "cartographic_positions(project_id,planet_id,entity_id,longitude_e6,"
        "latitude_e6,approximate,importance,min_level,symbol) "
        "VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(planet_id,entity_id) DO UPDATE "
        "SET "
        "longitude_e6=excluded.longitude_e6,latitude_e6=excluded.latitude_e6,"
        "approximate=excluded.approximate,importance=excluded.importance,min_"
        "level=excluded.min_level,symbol=excluded.symbol");
    const auto &p = *desired;
    const auto lon = static_cast<std::int64_t>(std::llround(
        geo::normalize_longitude(p.coordinate.longitude) * 1000000));
    q.bind(1, project_id);
    q.bind(2, planet);
    q.bind(3, entity);
    q.bind(4, lon == 180000000 ? -180000000 : lon);
    q.bind(5, static_cast<std::int64_t>(
                  std::llround(p.coordinate.latitude * 1000000)));
    q.bind(6, std::int64_t{p.approximate});
    q.bind(7, std::int64_t{p.importance});
    q.bind(8, std::int64_t{p.min_level});
    q.bind(9, p.symbol);
    q.run();
  } else {
    if (!expected)
      throw std::runtime_error("Local sem posição para remover");
    auto q = db.prepare("DELETE FROM cartographic_positions WHERE planet_id=? "
                        "AND entity_id=? AND project_id=?");
    q.bind(1, planet);
    q.bind(2, entity);
    q.bind(3, project_id);
    q.run();
  }
  log(db, project_id,
      desired ? "place_cartographic_local" : "unplace_cartographic_local");
  tx.commit();
}

int SqliteCartographyRepository::apply_terrain(
    const std::filesystem::path &path, const geo::TerrainPatch &patch,
    int expected_revision, bool reverse) const {
  if (patch.planet_id.empty() || patch.chunks.empty() ||
      patch.chunks.size() > 170 || patch.changed_samples == 0 ||
      expected_revision < 0 || !std::isfinite(patch.radius_degrees) ||
      patch.radius_degrees < 0.5 || patch.radius_degrees > 15)
    throw std::runtime_error("Delta de terreno inválido");
  geo::validate(patch.center);
  std::set<geo::ChunkKey> keys;
  for (const auto &delta : patch.chunks) {
    geo::validate(delta.key);
    if (delta.before.key != delta.key || delta.after.key != delta.key ||
        delta.before == delta.after || !keys.insert(delta.key).second)
      throw std::runtime_error("Delta de terreno incoerente");
  }

  auto db = open(path);
  SqliteTransaction tx(db);
  const auto project_id = owner(db);
  auto world = db.prepare(
      "SELECT radius_m,height_m,depth_m,detail,generator,created_at,"
      "terrain_revision FROM cartographic_planets WHERE id=? AND project_id=?");
  world.bind(1, patch.planet_id);
  world.bind(2, project_id);
  if (!world.step())
    throw std::runtime_error("Planeta da edição não existe");
  geo::Planet planet{patch.planet_id,
                     "Planeta",
                     world.column_integer(0),
                     0,
                     65,
                     3,
                     static_cast<int>(world.column_integer(1)),
                     static_cast<int>(world.column_integer(2)),
                     static_cast<int>(world.column_integer(3)),
                     static_cast<int>(world.column_integer(4)),
                     world.column_text(5),
                     static_cast<int>(world.column_integer(6))};
  if (planet.terrain_revision != expected_revision)
    throw std::runtime_error(
        "O terreno mudou. Recarregue o planeta antes de aplicar ou desfazer.");
  const auto finest_samples =
      static_cast<std::size_t>(geo::chunk_side * (2 << planet.detail)) *
      (geo::chunk_side * (1 << planet.detail));
  if (patch.changed_samples > finest_samples)
    throw std::runtime_error("Contagem de amostras do delta é impossível");

  const geo::TerrainBrush footprint{patch.planet_id, patch.center,
                                    patch.radius_degrees, 50,
                                    geo::TerrainEditMode::Raise};
  for (const auto &lock : read_locks(db, patch.planet_id, project_id))
    if (geo::overlaps(footprint, lock))
      throw std::runtime_error("A edição intersecta a região bloqueada: " +
                               lock.name);

  auto select = db.prepare(
      "SELECT elevation FROM cartographic_chunks WHERE planet_id=? AND "
      "level=? AND x=? AND y=?");
  auto update = db.prepare(
      "UPDATE cartographic_chunks SET elevation=? WHERE planet_id=? AND "
      "level=? AND x=? AND y=?");
  for (const auto &delta : patch.chunks) {
    const auto &expected = reverse ? delta.after : delta.before;
    const auto &desired = reverse ? delta.before : delta.after;
    if (delta.key.level > planet.detail)
      throw std::runtime_error("Delta usa nível ausente no planeta");
    for (const auto h : desired.elevation)
      if (h < -planet.depth_m || h > planet.height_m)
        throw std::runtime_error("Delta excede a escala do planeta");
    select.bind(1, patch.planet_id);
    select.bind(2, std::int64_t{delta.key.level});
    select.bind(3, std::int64_t{delta.key.x});
    select.bind(4, std::int64_t{delta.key.y});
    if (!select.step() || decode(delta.key, select.column_blob(0)) != expected)
      throw std::runtime_error(
          "Um chunk do terreno mudou; a edição não foi aplicada.");
    select.reset();
    const auto bytes = encode(desired);
    update.bind_blob(1, bytes);
    update.bind(2, patch.planet_id);
    update.bind(3, std::int64_t{delta.key.level});
    update.bind(4, std::int64_t{delta.key.x});
    update.bind(5, std::int64_t{delta.key.y});
    update.run();
    if (db.changes() != 1)
      throw std::runtime_error("Chunk do terreno desapareceu durante a edição");
    update.reset();
  }
  std::vector<geo::TerrainChunk> candidate;
  auto all = db.prepare(
      "SELECT level,x,y,elevation FROM cartographic_chunks WHERE planet_id=? "
      "ORDER BY level,x,y");
  all.bind(1, patch.planet_id);
  while (all.step()) {
    const geo::ChunkKey key{static_cast<int>(all.column_integer(0)),
                            static_cast<int>(all.column_integer(1)),
                            static_cast<int>(all.column_integer(2))};
    candidate.push_back(decode(key, all.column_blob(3)));
  }
  geo::validate_terrain_pyramid(planet, candidate);
  auto revision = db.prepare(
      "UPDATE cartographic_planets SET terrain_revision=terrain_revision+1 "
      "WHERE id=? AND project_id=? AND terrain_revision=?");
  revision.bind(1, patch.planet_id);
  revision.bind(2, project_id);
  revision.bind(3, std::int64_t{expected_revision});
  revision.run();
  if (db.changes() != 1)
    throw std::runtime_error("Revisão cartográfica concorrente");
  log(db, project_id,
      reverse ? "undo_cartographic_terrain" : "edit_cartographic_terrain");
  tx.commit();
  return expected_revision + 1;
}

std::vector<project::geo::TerrainLock>
SqliteCartographyRepository::terrain_locks(const std::filesystem::path &path,
                                           const std::string &planet) const {
  if (planet.empty())
    throw std::runtime_error("Planeta inválido para consultar bloqueios");
  auto db = open(path);
  return read_locks(db, planet, owner(db));
}

void SqliteCartographyRepository::add_terrain_lock(
    const std::filesystem::path &path, const geo::TerrainLock &lock) const {
  geo::validate(lock);
  auto db = open(path);
  SqliteTransaction tx(db);
  const auto project_id = owner(db);
  auto count = db.prepare(
      "SELECT count(*) FROM cartographic_terrain_locks WHERE planet_id=? AND "
      "project_id=?");
  count.bind(1, lock.planet_id);
  count.bind(2, project_id);
  count.step();
  if (count.column_integer(0) >= 100)
    throw std::runtime_error("Limite de 100 bloqueios neste planeta");
  auto q = db.prepare(
      "INSERT INTO cartographic_terrain_locks(id,project_id,planet_id,name,"
      "longitude_e6,latitude_e6,radius_e6,created_at) VALUES(?,?,?,?,?,?,?,?)");
  q.bind(1, lock.id);
  q.bind(2, project_id);
  q.bind(3, lock.planet_id);
  q.bind(4, lock.name);
  const auto lon = static_cast<std::int64_t>(
      std::llround(geo::normalize_longitude(lock.center.longitude) * 1000000));
  q.bind(5, lon == 180000000 ? -180000000 : lon);
  q.bind(6, static_cast<std::int64_t>(
                std::llround(lock.center.latitude * 1000000)));
  q.bind(7, static_cast<std::int64_t>(
                std::llround(lock.radius_degrees * 1000000)));
  q.bind(8, lock.created_at);
  q.run();
  log(db, project_id, "lock_cartographic_terrain");
  tx.commit();
}

void SqliteCartographyRepository::remove_terrain_lock(
    const std::filesystem::path &path, const geo::TerrainLock &lock) const {
  geo::validate(lock);
  auto db = open(path);
  SqliteTransaction tx(db);
  const auto project_id = owner(db);
  auto q = db.prepare(
      "DELETE FROM cartographic_terrain_locks WHERE id=? AND planet_id=? AND "
      "project_id=? AND name=? AND longitude_e6=? AND latitude_e6=? AND "
      "radius_e6=?");
  q.bind(1, lock.id);
  q.bind(2, lock.planet_id);
  q.bind(3, project_id);
  q.bind(4, lock.name);
  const auto lon = static_cast<std::int64_t>(
      std::llround(geo::normalize_longitude(lock.center.longitude) * 1000000));
  q.bind(5, lon == 180000000 ? -180000000 : lon);
  q.bind(6, static_cast<std::int64_t>(
                std::llround(lock.center.latitude * 1000000)));
  q.bind(7, static_cast<std::int64_t>(
                std::llround(lock.radius_degrees * 1000000)));
  q.run();
  if (db.changes() != 1)
    throw std::runtime_error("O bloqueio mudou ou não existe mais");
  log(db, project_id, "unlock_cartographic_terrain");
  tx.commit();
}
} // namespace inde::persistence

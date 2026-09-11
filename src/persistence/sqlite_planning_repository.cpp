#include "inde/persistence/sqlite_planning_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <limits>
#include <stdexcept>
#include <string_view>

namespace inde::persistence {
namespace {

SqliteDatabase open_database(const std::filesystem::path &project_path) {
  const auto path = ProjectDatabaseRepository::database_path(project_path);
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("O banco de dados do projeto não foi encontrado");
  return SqliteDatabase(path);
}

std::string project_id(SqliteDatabase &database) {
  auto statement = database.prepare("SELECT id FROM projects");
  if (!statement.step())
    throw std::runtime_error("O banco não contém a identidade do projeto");
  const auto result = statement.column_text(0);
  if (statement.step())
    throw std::runtime_error(
        "O banco contém mais de uma identidade de projeto");
  return result;
}

bool exists(SqliteDatabase &database, std::string_view table,
            const std::string &id) {
  auto statement = database.prepare("SELECT count(*) FROM " +
                                    std::string(table) + " WHERE id = ?");
  statement.bind(1, id);
  return statement.step() && statement.column_integer(0) == 1;
}

void record_change(SqliteDatabase &database, const std::string &owner,
                   std::string_view command) {
  auto statement = database.prepare(
      "INSERT INTO change_log(id, project_id, command_name, system_created_at) "
      "VALUES (?, ?, ?, ?)");
  statement.bind(1, project::new_uuid());
  statement.bind(2, owner);
  statement.bind(3, command);
  statement.bind(4, project::utc_now());
  statement.run();
}

void require_changed(SqliteDatabase &database, const char *message) {
  if (database.changes() != 1)
    throw std::runtime_error(message);
}

std::int64_t checked_limit(std::size_t value) {
  if (value == 0 || value > 500)
    throw std::runtime_error("Limite da consulta de planejamento inválido");
  return static_cast<std::int64_t>(value);
}

std::int64_t checked_offset(std::size_t value) {
  if (value >
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
    throw std::runtime_error(
        "Deslocamento da consulta de planejamento inválido");
  return static_cast<std::int64_t>(value);
}

std::string like_pattern(const std::string &search) {
  std::string pattern{"%"};
  pattern.reserve(search.size() + 2);
  for (const char character : search) {
    if (character == '%' || character == '_' || character == '\\')
      pattern.push_back('\\');
    pattern.push_back(character);
  }
  pattern.push_back('%');
  return pattern;
}

project::FictionalTimeAxis read_axis(SqliteStatement &statement) {
  project::FictionalTimeAxis value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_integer(3) != 0,
      statement.column_text(4), statement.column_text(5)};
  project::validate(value);
  return value;
}

project::FictionalTimePoint read_point(SqliteStatement &statement) {
  project::FictionalTimePoint value{
      statement.column_text(0),    statement.column_text(1),
      statement.column_integer(2), statement.column_text(3),
      statement.column_text(4),    statement.column_text(5),
      statement.column_text(6)};
  project::validate(value);
  return value;
}

project::EventOccurrence read_occurrence(SqliteStatement &statement) {
  project::EventOccurrence value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5)};
  project::validate(value);
  return value;
}

project::EventParticipation read_participation(SqliteStatement &statement) {
  project::EventParticipation value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5),
      statement.column_text(6)};
  project::validate(value);
  return value;
}

project::EntityPresence read_presence(SqliteStatement &statement) {
  project::EntityPresence value{
      statement.column_text(0),
      statement.column_text(1),
      statement.column_text(2),
      statement.column_text(3),
      statement.column_is_null(4)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(4)},
      statement.column_text(5),
      statement.column_text(6),
      statement.column_text(7)};
  project::validate(value);
  return value;
}

} // namespace

void SqlitePlanningRepository::initialize(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  SchemaMigrator{}.migrate(database);
  const auto owner = project_id(database);
  if (database.query_integer(
          "SELECT count(*) FROM fictional_time_axes WHERE is_default = 1") == 1)
    return;
  SqliteTransaction transaction(database);
  auto statement = database.prepare(
      "INSERT INTO fictional_time_axes "
      "(id, project_id, name, description, is_default, created_at, updated_at) "
      "VALUES (?, ?, 'Cronologia principal', "
      "'Eixo padrão do tempo ficcional', 1, ?, ?)");
  const auto now = project::utc_now();
  statement.bind(1, project::new_uuid());
  statement.bind(2, owner);
  statement.bind(3, now);
  statement.bind(4, now);
  statement.run();
  transaction.commit();
}

std::vector<project::FictionalTimeAxis>
SqlitePlanningRepository::time_axes(const std::filesystem::path &path) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement =
      db.prepare("SELECT id,name,description,is_default,created_at,updated_at "
                 "FROM fictional_time_axes WHERE project_id=? "
                 "ORDER BY is_default DESC,name COLLATE NOCASE,id");
  statement.bind(1, owner);
  std::vector<project::FictionalTimeAxis> result;
  while (statement.step())
    result.push_back(read_axis(statement));
  return result;
}

std::optional<project::FictionalTimeAxis>
SqlitePlanningRepository::time_axis(const std::filesystem::path &path,
                                    const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement =
      db.prepare("SELECT id,name,description,is_default,created_at,updated_at "
                 "FROM fictional_time_axes WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  return read_axis(statement);
}

std::vector<project::FictionalTimePoint>
SqlitePlanningRepository::time_points(const std::filesystem::path &path,
                                      const std::string &axis_id,
                                      const PlanningQuery &query) const {
  const auto limit = checked_limit(query.limit);
  const auto offset = checked_offset(query.offset);
  auto db = open_database(path);
  const auto owner = project_id(db);
  std::string sql =
      "SELECT id,axis_id,ordinal,label,description,created_at,updated_at "
      "FROM fictional_time_points WHERE project_id=? AND axis_id=? "
      ;
  if (!query.search.empty())
    sql += "AND (label LIKE ? ESCAPE '\\' OR description LIKE ? ESCAPE '\\') ";
  sql += "ORDER BY ordinal,id LIMIT ? OFFSET ?";
  auto statement = db.prepare(sql);
  statement.bind(1, owner);
  statement.bind(2, axis_id);
  int parameter = 3;
  if (!query.search.empty()) {
    const auto pattern = like_pattern(query.search);
    statement.bind(parameter++, pattern);
    statement.bind(parameter++, pattern);
  }
  statement.bind(parameter++, limit);
  statement.bind(parameter, offset);
  std::vector<project::FictionalTimePoint> result;
  while (statement.step())
    result.push_back(read_point(statement));
  return result;
}

std::optional<project::FictionalTimePoint>
SqlitePlanningRepository::time_point(const std::filesystem::path &path,
                                     const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement = db.prepare(
      "SELECT id,axis_id,ordinal,label,description,created_at,updated_at "
      "FROM fictional_time_points WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  return read_point(statement);
}

std::vector<project::EventOccurrence>
SqlitePlanningRepository::event_occurrences(
    const std::filesystem::path &path,
    const EventOccurrenceQuery &query) const {
  const auto limit = checked_limit(query.limit);
  const auto offset = checked_offset(query.offset);
  auto db = open_database(path);
  const auto owner = project_id(db);
  std::string sql =
      "SELECT o.id,o.event_entity_id,o.time_point_id,o.description,"
      "o.created_at,o.updated_at FROM event_occurrences o "
      "JOIN fictional_time_points p ON p.id=o.time_point_id AND "
      "p.project_id=o.project_id WHERE o.project_id=?";
  if (query.time_axis_id)
    sql += " AND p.axis_id=?";
  sql += " ORDER BY p.ordinal,o.id LIMIT ? OFFSET ?";
  auto statement = db.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  if (query.time_axis_id)
    statement.bind(parameter++, *query.time_axis_id);
  statement.bind(parameter++, limit);
  statement.bind(parameter, offset);
  std::vector<project::EventOccurrence> result;
  while (statement.step())
    result.push_back(read_occurrence(statement));
  return result;
}

std::optional<project::EventOccurrence>
SqlitePlanningRepository::event_occurrence(const std::filesystem::path &path,
                                           const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement = db.prepare(
      "SELECT "
      "id,event_entity_id,time_point_id,description,created_at,updated_at "
      "FROM event_occurrences WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  return read_occurrence(statement);
}

std::optional<project::EventOccurrence>
SqlitePlanningRepository::event_occurrence_for_entity(
    const std::filesystem::path &path, const std::string &entity_id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement = db.prepare(
      "SELECT "
      "id,event_entity_id,time_point_id,description,created_at,updated_at "
      "FROM event_occurrences WHERE project_id=? AND event_entity_id=?");
  statement.bind(1, owner);
  statement.bind(2, entity_id);
  if (!statement.step())
    return std::nullopt;
  return read_occurrence(statement);
}

std::vector<project::EventParticipation>
SqlitePlanningRepository::event_participations(
    const std::filesystem::path &path, const std::string &occurrence_id,
    const PlanningQuery &query) const {
  const auto limit = checked_limit(query.limit);
  const auto offset = checked_offset(query.offset);
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement = db.prepare(
      "SELECT id,event_occurrence_id,participant_entity_id,role,notes,"
      "created_at,updated_at FROM event_participations "
      "WHERE project_id=? AND event_occurrence_id=? ORDER BY role,id "
      "LIMIT ? OFFSET ?");
  statement.bind(1, owner);
  statement.bind(2, occurrence_id);
  statement.bind(3, limit);
  statement.bind(4, offset);
  std::vector<project::EventParticipation> result;
  while (statement.step())
    result.push_back(read_participation(statement));
  return result;
}

std::optional<project::EventParticipation>
SqlitePlanningRepository::event_participation(const std::filesystem::path &path,
                                              const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement = db.prepare(
      "SELECT id,event_occurrence_id,participant_entity_id,role,notes,"
      "created_at,updated_at FROM event_participations "
      "WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  return read_participation(statement);
}

std::vector<project::EntityPresence>
SqlitePlanningRepository::presences(const std::filesystem::path &path,
                                    const PresenceQuery &query) const {
  const auto limit = checked_limit(query.limit);
  const auto offset = checked_offset(query.offset);
  auto db = open_database(path);
  const auto owner = project_id(db);
  std::string sql =
      "SELECT p.id,p.entity_id,p.location_entity_id,p.start_time_point_id,"
      "p.end_time_point_id,p.description,p.created_at,p.updated_at "
      "FROM entity_presences p JOIN fictional_time_points t "
      "ON t.id=p.start_time_point_id AND t.project_id=p.project_id "
      "WHERE p.project_id=?";
  if (query.entity_id)
    sql += " AND p.entity_id=?";
  if (query.location_entity_id)
    sql += " AND p.location_entity_id=?";
  if (query.time_axis_id)
    sql += " AND t.axis_id=?";
  sql += " ORDER BY t.axis_id,t.ordinal,p.id LIMIT ? OFFSET ?";
  auto statement = db.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  if (query.entity_id)
    statement.bind(parameter++, *query.entity_id);
  if (query.location_entity_id)
    statement.bind(parameter++, *query.location_entity_id);
  if (query.time_axis_id)
    statement.bind(parameter++, *query.time_axis_id);
  statement.bind(parameter++, limit);
  statement.bind(parameter, offset);
  std::vector<project::EntityPresence> result;
  while (statement.step())
    result.push_back(read_presence(statement));
  return result;
}

std::optional<project::EntityPresence>
SqlitePlanningRepository::presence(const std::filesystem::path &path,
                                   const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  auto statement =
      db.prepare("SELECT id,entity_id,location_entity_id,start_time_point_id,"
                 "end_time_point_id,description,created_at,updated_at "
                 "FROM entity_presences WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  return read_presence(statement);
}

void SqlitePlanningRepository::save(
    const std::filesystem::path &path,
    const project::FictionalTimeAxis &value) const {
  project::validate(value);
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  const bool updating = exists(db, "fictional_time_axes", value.id);
  auto statement = db.prepare(
      "INSERT INTO "
      "fictional_time_axes(id,project_id,name,description,is_default,created_"
      "at,updated_at) "
      "VALUES(?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET name=excluded.name,"
      "description=excluded.description,updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.name);
  statement.bind(4, value.description);
  statement.bind(5, std::int64_t{value.is_default ? 1 : 0});
  statement.bind(6, value.created_at);
  statement.bind(7, value.updated_at);
  statement.run();
  record_change(db, owner,
                updating ? "update_fictional_time_axis"
                         : "create_fictional_time_axis");
  tx.commit();
}

void SqlitePlanningRepository::save(
    const std::filesystem::path &path,
    const project::FictionalTimePoint &value) const {
  project::validate(value);
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  const bool updating = exists(db, "fictional_time_points", value.id);
  auto statement =
      db.prepare("INSERT INTO "
                 "fictional_time_points(id,project_id,axis_id,ordinal,label,"
                 "description,created_at,updated_at) "
                 "VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
                 "axis_id=excluded.axis_id,"
                 "ordinal=excluded.ordinal,label=excluded.label,description="
                 "excluded.description,updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.axis_id);
  statement.bind(4, value.ordinal);
  statement.bind(5, value.label);
  statement.bind(6, value.description);
  statement.bind(7, value.created_at);
  statement.bind(8, value.updated_at);
  statement.run();
  record_change(db, owner,
                updating ? "update_fictional_time_point"
                         : "create_fictional_time_point");
  tx.commit();
}

void SqlitePlanningRepository::save(
    const std::filesystem::path &path,
    const project::EventOccurrence &value) const {
  project::validate(value);
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  const bool updating = exists(db, "event_occurrences", value.id);
  auto statement =
      db.prepare("INSERT INTO "
                 "event_occurrences(id,project_id,event_entity_id,time_point_"
                 "id,description,created_at,updated_at) "
                 "VALUES(?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
                 "event_entity_id=excluded.event_entity_id,"
                 "time_point_id=excluded.time_point_id,description=excluded."
                 "description,updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.event_entity_id);
  statement.bind(4, value.time_point_id);
  statement.bind(5, value.description);
  statement.bind(6, value.created_at);
  statement.bind(7, value.updated_at);
  statement.run();
  record_change(db, owner,
                updating ? "update_event_occurrence"
                         : "create_event_occurrence");
  tx.commit();
}

void SqlitePlanningRepository::save(
    const std::filesystem::path &path,
    const project::EventParticipation &value) const {
  project::validate(value);
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  const bool updating = exists(db, "event_participations", value.id);
  auto statement = db.prepare(
      "INSERT INTO "
      "event_participations(id,project_id,event_occurrence_id,participant_"
      "entity_id,role,notes,created_at,updated_at) "
      "VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
      "event_occurrence_id=excluded.event_occurrence_id,"
      "participant_entity_id=excluded.participant_entity_id,role=excluded.role,"
      "notes=excluded.notes,updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.event_occurrence_id);
  statement.bind(4, value.participant_entity_id);
  statement.bind(5, value.role);
  statement.bind(6, value.notes);
  statement.bind(7, value.created_at);
  statement.bind(8, value.updated_at);
  statement.run();
  record_change(db, owner,
                updating ? "update_event_participation"
                         : "create_event_participation");
  tx.commit();
}

void SqlitePlanningRepository::save(
    const std::filesystem::path &path,
    const project::EntityPresence &value) const {
  project::validate(value);
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  const bool updating = exists(db, "entity_presences", value.id);
  auto statement = db.prepare(
      "INSERT INTO entity_presences(id,project_id,entity_id,location_entity_id,"
      "start_time_point_id,end_time_point_id,description,created_at,updated_at)"
      " "
      "VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
      "entity_id=excluded.entity_id,location_entity_id=excluded.location_"
      "entity_id,"
      "start_time_point_id=excluded.start_time_point_id,"
      "end_time_point_id=excluded.end_time_point_id,"
      "description=excluded.description,updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.entity_id);
  statement.bind(4, value.location_entity_id);
  statement.bind(5, value.start_time_point_id);
  if (value.end_time_point_id)
    statement.bind(6, *value.end_time_point_id);
  else
    statement.bind_null(6);
  statement.bind(7, value.description);
  statement.bind(8, value.created_at);
  statement.bind(9, value.updated_at);
  statement.run();
  record_change(db, owner,
                updating ? "update_entity_presence" : "create_entity_presence");
  tx.commit();
}

void SqlitePlanningRepository::remove_time_axis(
    const std::filesystem::path &path, const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  auto statement =
      db.prepare("DELETE FROM fictional_time_axes WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  statement.run();
  require_changed(db, "Eixo de tempo ficcional não encontrado");
  record_change(db, owner, "delete_fictional_time_axis");
  tx.commit();
}
void SqlitePlanningRepository::remove_time_point(
    const std::filesystem::path &path, const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  auto statement = db.prepare(
      "DELETE FROM fictional_time_points WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  statement.run();
  require_changed(db, "Ponto de tempo ficcional não encontrado");
  record_change(db, owner, "delete_fictional_time_point");
  tx.commit();
}
void SqlitePlanningRepository::remove_event_occurrence(
    const std::filesystem::path &path, const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  auto statement =
      db.prepare("DELETE FROM event_occurrences WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  statement.run();
  require_changed(db, "Ocorrência de acontecimento não encontrada");
  record_change(db, owner, "delete_event_occurrence");
  tx.commit();
}
void SqlitePlanningRepository::remove_event_participation(
    const std::filesystem::path &path, const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  auto statement = db.prepare(
      "DELETE FROM event_participations WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  statement.run();
  require_changed(db, "Participação em acontecimento não encontrada");
  record_change(db, owner, "delete_event_participation");
  tx.commit();
}

void SqlitePlanningRepository::remove_presence(
    const std::filesystem::path &path, const std::string &id) const {
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db);
  auto statement =
      db.prepare("DELETE FROM entity_presences WHERE project_id=? AND id=?");
  statement.bind(1, owner);
  statement.bind(2, id);
  statement.run();
  require_changed(db, "Presença narrativa não encontrada");
  record_change(db, owner, "delete_entity_presence");
  tx.commit();
}

} // namespace inde::persistence

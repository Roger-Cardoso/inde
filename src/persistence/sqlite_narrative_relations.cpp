#include "inde/persistence/sqlite_narrative_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "narrative_repository_support.hpp"
#include "narrative_query_support.hpp"
#include "narrative_row_codec.hpp"

namespace inde::persistence {

using namespace narrative_detail;

std::vector<project::RelationType> SqliteNarrativeRepository::relation_types(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, key, name, inverse_name, description, directionality, "
      "is_builtin, created_at, updated_at FROM relation_types "
      "WHERE project_id = ? ORDER BY is_builtin DESC, name, id");
  statement.bind(1, owner);
  std::vector<project::RelationType> values;
  while (statement.step())
    values.push_back(read_relation_type(statement));
  return values;
}

std::optional<project::RelationType> SqliteNarrativeRepository::relation_type(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, key, name, inverse_name, description, directionality, "
      "is_builtin, created_at, updated_at FROM relation_types "
      "WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  auto value = read_relation_type(statement);
  if (statement.step())
    throw std::runtime_error("Tipo de relação duplicado no banco");
  return value;
}

std::vector<project::NarrativeRelation> SqliteNarrativeRepository::relations(
    const std::filesystem::path &project_path,
    const RelationQuery &query) const {
  const auto limit = checked_limit(query.limit, 1000, "Limite de relações");
  const auto offset = checked_offset(query.offset);
  const auto require_id = [](const std::optional<std::string> &id,
                             const char *label) {
    if (id && id->empty())
      throw std::runtime_error(std::string(label) +
                               " contém identificador vazio");
  };
  require_id(query.entity_id, "Filtro de entidade da relação");
  require_id(query.relation_type_id, "Filtro de tipo de relação");
  require_id(query.fictional_time_point_id,
             "Filtro de quando da relação");
  require_id(query.location_entity_id, "Filtro de onde da relação");
  require_id(query.cause_entity_id, "Filtro de causa da relação");
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql =
      "SELECT r.id, r.relation_type_id, r.source_entity_id, r.target_entity_id, "
      "r.description, r.created_at, r.updated_at, context.fictional_time_point_id, "
      "context.location_entity_id, context.cause_entity_id "
      "FROM relations r "
      "JOIN relation_types rt ON rt.id = r.relation_type_id "
      "AND rt.project_id = r.project_id "
      "JOIN entities source ON source.id = r.source_entity_id "
      "AND source.project_id = r.project_id "
      "JOIN entities target ON target.id = r.target_entity_id "
      "AND target.project_id = r.project_id "
      "LEFT JOIN relation_contexts context ON context.relation_id = r.id "
      "AND context.project_id = r.project_id WHERE r.project_id = ?";
  if (query.entity_id)
    sql += " AND (r.source_entity_id = ? OR r.target_entity_id = ?)";
  if (query.relation_type_id)
    sql += " AND r.relation_type_id = ?";
  if (query.fictional_time_point_id)
    sql += " AND context.fictional_time_point_id = ?";
  if (query.location_entity_id)
    sql += " AND context.location_entity_id = ?";
  if (query.cause_entity_id)
    sql += " AND context.cause_entity_id = ?";
  if (!query.search.empty()) {
    sql += " AND (r.description LIKE ? ESCAPE '\\' "
           "OR rt.key LIKE ? ESCAPE '\\' OR rt.name LIKE ? ESCAPE '\\' "
           "OR rt.inverse_name LIKE ? ESCAPE '\\' "
           "OR source.name LIKE ? ESCAPE '\\' OR source.summary LIKE ? ESCAPE '\\' "
           "OR target.name LIKE ? ESCAPE '\\' OR target.summary LIKE ? ESCAPE '\\')";
  }
  sql += " ORDER BY r.updated_at DESC, r.id LIMIT ? OFFSET ?";
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  if (query.entity_id) {
    statement.bind(parameter++, *query.entity_id);
    statement.bind(parameter++, *query.entity_id);
  }
  if (query.relation_type_id)
    statement.bind(parameter++, *query.relation_type_id);
  if (query.fictional_time_point_id)
    statement.bind(parameter++, *query.fictional_time_point_id);
  if (query.location_entity_id)
    statement.bind(parameter++, *query.location_entity_id);
  if (query.cause_entity_id)
    statement.bind(parameter++, *query.cause_entity_id);
  if (!query.search.empty()) {
    const auto pattern = like_pattern(query.search);
    for (int index = 0; index < 8; ++index)
      statement.bind(parameter++, pattern);
  }
  statement.bind(parameter++, limit);
  statement.bind(parameter, offset);
  std::vector<project::NarrativeRelation> values;
  while (statement.step())
    values.push_back(read_relation(statement));
  return values;
}

std::optional<project::NarrativeRelation> SqliteNarrativeRepository::relation(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT r.id, r.relation_type_id, r.source_entity_id, r.target_entity_id, "
      "r.description, r.created_at, r.updated_at, context.fictional_time_point_id, "
      "context.location_entity_id, context.cause_entity_id "
      "FROM relations r LEFT JOIN relation_contexts context "
      "ON context.relation_id = r.id AND context.project_id = r.project_id "
      "WHERE r.project_id = ? AND r.id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  auto value = read_relation(statement);
  if (statement.step())
    throw std::runtime_error("Relação narrativa duplicada no banco");
  return value;
}

} // namespace inde::persistence

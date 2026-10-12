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

std::vector<project::EntityType> SqliteNarrativeRepository::entity_types(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, key, name, description, is_builtin, created_at, updated_at "
      "FROM entity_types WHERE project_id = ? ORDER BY is_builtin DESC, name, id");
  statement.bind(1, owner);
  std::vector<project::EntityType> values;
  while (statement.step())
    values.push_back(read_entity_type(statement));
  return values;
}

std::optional<project::EntityType> SqliteNarrativeRepository::entity_type(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, key, name, description, is_builtin, created_at, updated_at "
      "FROM entity_types WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  auto value = read_entity_type(statement);
  if (statement.step())
    throw std::runtime_error("Tipo de entidade duplicado no banco");
  return value;
}

std::vector<project::NarrativeEntity> SqliteNarrativeRepository::entities(
    const std::filesystem::path &project_path, const EntityQuery &query) const {
  const auto limit = checked_limit(query.limit, 500, "Limite de entidades");
  const auto offset = checked_offset(query.offset);
  validate_entity_filters(query);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql =
      "SELECT id, entity_type_id, name, summary, created_at, updated_at "
      "FROM entities e WHERE e.project_id = ?";
  append_entity_filters(sql, query);
  if (!query.search.empty()) {
    sql += " ORDER BY CASE WHEN e.name LIKE ? ESCAPE '\\' THEN 0 "
           "WHEN e.name LIKE ? ESCAPE '\\' THEN 1 "
           "WHEN e.summary LIKE ? ESCAPE '\\' THEN 2 ELSE 3 END, "
           "e.name COLLATE NOCASE, e.id LIMIT ? OFFSET ?";
  } else {
    sql += " ORDER BY e.name COLLATE NOCASE, e.id LIMIT ? OFFSET ?";
  }
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  parameter = bind_entity_filters(statement, parameter, query);
  if (!query.search.empty()) {
    statement.bind(parameter++, like_prefix_pattern(query.search));
    statement.bind(parameter++, like_pattern(query.search));
    statement.bind(parameter++, like_pattern(query.search));
  }
  statement.bind(parameter++, limit);
  statement.bind(parameter, offset);
  std::vector<project::NarrativeEntity> values;
  while (statement.step())
    values.push_back(read_entity(statement));
  return values;
}

std::size_t SqliteNarrativeRepository::entity_count(
    const std::filesystem::path &project_path, const EntityQuery &query) const {
  validate_entity_filters(query);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql = "SELECT count(*) FROM entities e WHERE e.project_id = ?";
  append_entity_filters(sql, query);
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  static_cast<void>(bind_entity_filters(statement, parameter, query));
  if (!statement.step())
    throw std::runtime_error("A contagem de entidades nao retornou resultado");
  const auto count = statement.column_integer(0);
  if (count < 0)
    throw std::runtime_error("A contagem de entidades retornou valor invalido");
  return static_cast<std::size_t>(count);
}

std::vector<EntityFacetCount> SqliteNarrativeRepository::entity_type_facets(
    const std::filesystem::path &project_path, const EntityQuery &input) const {
  validate_entity_filters(input);
  auto query = input;
  // A faceta responde "o que ocorreria se este tipo fosse escolhido"; portanto
  // a categoria de tipos atual não limita a sua própria agregação.
  query.entity_type_ids.clear();
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql =
      "SELECT e.entity_type_id, count(*) FROM entities e "
      "WHERE e.project_id = ?";
  append_entity_filters(sql, query);
  sql += " GROUP BY e.entity_type_id";
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  static_cast<void>(bind_entity_filters(statement, parameter, query));
  std::vector<EntityFacetCount> result;
  while (statement.step()) {
    const auto count = statement.column_integer(1);
    if (count < 0)
      throw std::runtime_error("Faceta de tipo retornou valor invalido");
    result.push_back({statement.column_text(0), static_cast<std::size_t>(count)});
  }
  return result;
}

std::vector<EntityFacetCount>
SqliteNarrativeRepository::relation_type_facets(
    const std::filesystem::path &project_path, const EntityQuery &input) const {
  validate_entity_filters(input);
  auto query = input;
  // A relação agrupada é a candidata: não a filtramos antecipadamente.
  query.relation_type_ids.clear();
  query.related_entity_id.reset();
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql =
      "SELECT r.relation_type_id, count(DISTINCT e.id) FROM entities e "
      "JOIN relations r ON r.project_id=e.project_id AND "
      "(r.source_entity_id=e.id OR r.target_entity_id=e.id) "
      "WHERE e.project_id = ?";
  append_entity_filters(sql, query);
  if (input.related_entity_id) {
    sql += " AND ((r.source_entity_id=e.id AND r.target_entity_id=?) "
           "OR (r.target_entity_id=e.id AND r.source_entity_id=?))";
  }
  sql += " GROUP BY r.relation_type_id";
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  parameter = bind_entity_filters(statement, parameter, query);
  if (input.related_entity_id) {
    statement.bind(parameter++, *input.related_entity_id);
    statement.bind(parameter++, *input.related_entity_id);
  }
  std::vector<EntityFacetCount> result;
  while (statement.step()) {
    const auto count = statement.column_integer(1);
    if (count < 0)
      throw std::runtime_error("Faceta de relação retornou valor invalido");
    result.push_back({statement.column_text(0), static_cast<std::size_t>(count)});
  }
  return result;
}

std::optional<project::NarrativeEntity> SqliteNarrativeRepository::entity(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, entity_type_id, name, summary, created_at, updated_at "
      "FROM entities WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  auto value = read_entity(statement);
  if (statement.step())
    throw std::runtime_error("Entidade narrativa duplicada no banco");
  return value;
}

} // namespace inde::persistence

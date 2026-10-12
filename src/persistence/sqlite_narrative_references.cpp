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

std::vector<project::EntityWorkScope> SqliteNarrativeRepository::work_scopes(
    const std::filesystem::path &project_path,
    const EntityWorkScopeQuery &query) const {
  const auto limit = checked_limit(query.limit, 500, "Limite de vínculos com obras");
  const auto offset = checked_offset(query.offset);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql =
      "SELECT id, entity_id, work_id, notes, created_at, updated_at "
      "FROM entity_work_scopes WHERE project_id = ?";
  if (query.entity_id)
    sql += " AND entity_id = ?";
  if (query.work_id)
    sql += " AND work_id = ?";
  sql += " ORDER BY created_at, id LIMIT ? OFFSET ?";
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  if (query.entity_id)
    statement.bind(parameter++, *query.entity_id);
  if (query.work_id)
    statement.bind(parameter++, *query.work_id);
  statement.bind(parameter++, limit);
  statement.bind(parameter, offset);
  std::vector<project::EntityWorkScope> values;
  while (statement.step())
    values.push_back(read_work_scope(statement));
  return values;
}

std::optional<project::EntityWorkScope> SqliteNarrativeRepository::work_scope(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, entity_id, work_id, notes, created_at, updated_at "
      "FROM entity_work_scopes WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  auto value = read_work_scope(statement);
  if (statement.step())
    throw std::runtime_error("Vínculo entre entidade e obra duplicado");
  return value;
}

std::vector<project::EditorialEntityReference>
SqliteNarrativeRepository::editorial_references(
    const std::filesystem::path &project_path,
    const EditorialReferenceQuery &query) const {
  const auto limit =
      checked_limit(query.limit, 500, "Limite de referências editoriais");
  const auto offset = checked_offset(query.offset);
  validate_filter_ids(query.entity_type_ids,
                      "Filtro de tipos de entidade apresentados");
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql =
      "SELECT r.id, r.entity_id, r.work_id, r.editorial_node_id, r.purpose, "
      "r.notes, r.created_at, r.updated_at FROM editorial_entity_references r";
  if (!query.entity_type_ids.empty())
    sql += " INNER JOIN entities e ON e.id = r.entity_id "
           "AND e.project_id = r.project_id";
  sql += " WHERE r.project_id = ?";
  if (query.entity_id)
    sql += " AND r.entity_id = ?";
  if (query.work_id)
    sql += " AND r.work_id = ?";
  if (query.editorial_node_id)
    sql += " AND r.editorial_node_id = ?";
  if (!query.entity_type_ids.empty()) {
    sql += " AND e.entity_type_id IN (";
    for (std::size_t index = 0; index < query.entity_type_ids.size(); ++index)
      sql += index == 0 ? "?" : ", ?";
    sql += ")";
  }
  sql += " ORDER BY r.created_at, r.id LIMIT ? OFFSET ?";
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  if (query.entity_id)
    statement.bind(parameter++, *query.entity_id);
  if (query.work_id)
    statement.bind(parameter++, *query.work_id);
  if (query.editorial_node_id)
    statement.bind(parameter++, *query.editorial_node_id);
  for (const auto &type_id : query.entity_type_ids)
    statement.bind(parameter++, type_id);
  statement.bind(parameter++, limit);
  statement.bind(parameter, offset);
  std::vector<project::EditorialEntityReference> values;
  while (statement.step())
    values.push_back(read_editorial_reference(statement));
  return values;
}

std::vector<EntityFacetCount>
SqliteNarrativeRepository::editorial_reference_entity_type_facets(
    const std::filesystem::path &project_path,
    const EditorialReferenceQuery &input) const {
  // A faceta responde aos tipos candidatos no recorte atual, portanto a sua
  // própria seleção não limita a agregação.
  validate_filter_ids(input.entity_type_ids,
                      "Filtro de tipos de entidade apresentados");
  auto query = input;
  query.entity_type_ids.clear();
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string sql =
      "SELECT e.entity_type_id, count(DISTINCT r.editorial_node_id) "
      "FROM editorial_entity_references r "
      "INNER JOIN entities e ON e.id = r.entity_id "
      "AND e.project_id = r.project_id "
      "WHERE r.project_id = ?";
  if (query.entity_id)
    sql += " AND r.entity_id = ?";
  if (query.work_id)
    sql += " AND r.work_id = ?";
  if (query.editorial_node_id)
    sql += " AND r.editorial_node_id = ?";
  sql += " GROUP BY e.entity_type_id";
  auto statement = database.prepare(sql);
  int parameter = 1;
  statement.bind(parameter++, owner);
  if (query.entity_id)
    statement.bind(parameter++, *query.entity_id);
  if (query.work_id)
    statement.bind(parameter++, *query.work_id);
  if (query.editorial_node_id)
    statement.bind(parameter++, *query.editorial_node_id);
  std::vector<EntityFacetCount> result;
  while (statement.step()) {
    const auto count = statement.column_integer(1);
    if (count < 0)
      throw std::runtime_error(
          "Faceta de tipo editorial retornou valor inválido");
    result.push_back(
        {statement.column_text(0), static_cast<std::size_t>(count)});
  }
  return result;
}

std::optional<project::EditorialEntityReference>
SqliteNarrativeRepository::editorial_reference(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, entity_id, work_id, editorial_node_id, purpose, notes, "
      "created_at, updated_at FROM editorial_entity_references "
      "WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  auto value = read_editorial_reference(statement);
  if (statement.step())
    throw std::runtime_error("Referência editorial de entidade duplicada");
  return value;
}

std::vector<project::ChangeLogEntry> SqliteNarrativeRepository::change_log(
    const std::filesystem::path &project_path, std::size_t requested_limit) const {
  const auto limit = checked_limit(requested_limit, 1000, "Limite do histórico");
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT sequence, id, command_name, system_created_at FROM change_log "
      "WHERE project_id = ? ORDER BY sequence DESC LIMIT ?");
  statement.bind(1, owner);
  statement.bind(2, limit);
  std::vector<project::ChangeLogEntry> values;
  while (statement.step())
    values.push_back({statement.column_integer(0), statement.column_text(1),
                      statement.column_text(2), statement.column_text(3)});
  return values;
}

} // namespace inde::persistence

#include "inde/persistence/sqlite_narrative_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <algorithm>
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
  auto result = statement.column_text(0);
  if (statement.step())
    throw std::runtime_error("O banco contém mais de uma identidade de projeto");
  return result;
}

bool exists(SqliteDatabase &database, std::string_view table,
            const std::string &id) {
  // `table` é escolhido apenas entre literais internos abaixo.
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

std::int64_t checked_limit(std::size_t value, std::size_t maximum,
                           const char *label) {
  if (value == 0 || value > maximum)
    throw std::runtime_error(std::string(label) + " fora do limite permitido");
  return static_cast<std::int64_t>(value);
}

std::int64_t checked_offset(std::size_t value) {
  if (value > static_cast<std::size_t>(
                  std::numeric_limits<std::int64_t>::max()))
    throw std::runtime_error("Deslocamento de consulta fora do limite");
  return static_cast<std::int64_t>(value);
}

std::string like_pattern(const std::string &search) {
  std::string result{"%"};
  result.reserve(search.size() + 2);
  for (const char character : search) {
    if (character == '%' || character == '_' || character == '\\')
      result.push_back('\\');
    result.push_back(character);
  }
  result.push_back('%');
  return result;
}

std::string like_prefix_pattern(const std::string &search) {
  std::string result;
  result.reserve(search.size() + 1);
  for (const char character : search) {
    if (character == '%' || character == '_' || character == '\\')
      result.push_back('\\');
    result.push_back(character);
  }
  result.push_back('%');
  return result;
}

void validate_filter_ids(const std::vector<std::string> &ids,
                         const char *label) {
  constexpr std::size_t maximum_filters = 32;
  if (ids.size() > maximum_filters)
    throw std::runtime_error(std::string(label) +
                             " excede 32 valores por categoria");
  if (std::any_of(ids.begin(), ids.end(),
                  [](const auto &id) { return id.empty(); }))
    throw std::runtime_error(std::string(label) + " contem identificador vazio");
}

void append_placeholders(std::string &sql, std::size_t count) {
  sql += " (";
  for (std::size_t index = 0; index < count; ++index) {
    if (index != 0)
      sql += ", ";
    sql += '?';
  }
  sql += ')';
}

void bind_optional(SqliteStatement &statement, int index,
                   const std::optional<std::string> &value) {
  if (value)
    statement.bind(index, *value);
  else
    statement.bind_null(index);
}

void append_entity_filters(std::string &sql, const EntityQuery &query) {
  if (!query.entity_type_ids.empty()) {
    sql += " AND e.entity_type_id IN";
    append_placeholders(sql, query.entity_type_ids.size());
  }
  if (!query.search.empty())
    sql += " AND (e.name LIKE ? ESCAPE '\\' OR e.summary LIKE ? ESCAPE '\\')";
  if (query.work_id) {
    sql +=
        " AND EXISTS (SELECT 1 FROM entity_work_scopes ws "
        "WHERE ws.project_id = e.project_id AND ws.entity_id = e.id "
        "AND ws.work_id = ?)";
  }
  if (!query.relation_type_ids.empty() || query.related_entity_id) {
    sql +=
        " AND EXISTS (SELECT 1 FROM relations r "
        "WHERE r.project_id = e.project_id "
        "AND (r.source_entity_id = e.id OR r.target_entity_id = e.id)";
    if (!query.relation_type_ids.empty()) {
      sql += " AND r.relation_type_id IN";
      append_placeholders(sql, query.relation_type_ids.size());
    }
    if (query.related_entity_id) {
      sql +=
          " AND ((r.source_entity_id = e.id AND r.target_entity_id = ?) "
          "OR (r.target_entity_id = e.id AND r.source_entity_id = ?))";
    }
    sql += ')';
  }
  if (query.fictional_axis_id) {
    sql +=
        " AND EXISTS (SELECT 1 FROM fictional_time_axes axis "
        "WHERE axis.project_id = e.project_id AND axis.id = ? AND ("
        "EXISTS (SELECT 1 FROM event_occurrences occurrence "
        "JOIN fictional_time_points point ON point.id = occurrence.time_point_id "
        "AND point.project_id = occurrence.project_id "
        "WHERE occurrence.project_id = e.project_id "
        "AND occurrence.event_entity_id = e.id AND point.axis_id = axis.id) "
        "OR EXISTS (SELECT 1 FROM event_participations participation "
        "JOIN event_occurrences occurrence ON occurrence.id = participation.event_occurrence_id "
        "AND occurrence.project_id = participation.project_id "
        "JOIN fictional_time_points point ON point.id = occurrence.time_point_id "
        "AND point.project_id = occurrence.project_id "
        "WHERE participation.project_id = e.project_id "
        "AND participation.participant_entity_id = e.id "
        "AND point.axis_id = axis.id) "
        "OR EXISTS (SELECT 1 FROM entity_presences presence "
        "JOIN fictional_time_points start_point ON start_point.id = presence.start_time_point_id "
        "AND start_point.project_id = presence.project_id "
        "WHERE presence.project_id = e.project_id "
        "AND (presence.entity_id = e.id OR presence.location_entity_id = e.id) "
        "AND start_point.axis_id = axis.id)))";
  }
  if (query.fictional_time_point_id) {
    sql +=
        " AND (EXISTS (SELECT 1 FROM event_occurrences occurrence "
        "LEFT JOIN event_participations participation "
        "ON participation.event_occurrence_id = occurrence.id "
        "AND participation.project_id = occurrence.project_id "
        "WHERE occurrence.project_id = e.project_id AND occurrence.time_point_id = ? "
        "AND (occurrence.event_entity_id = e.id OR participation.participant_entity_id = e.id)) "
        "OR EXISTS (SELECT 1 FROM entity_presences presence "
        "JOIN fictional_time_points selected_point "
        "ON selected_point.id = ? AND selected_point.project_id = presence.project_id "
        "JOIN fictional_time_points start_point ON start_point.id = presence.start_time_point_id "
        "AND start_point.project_id = presence.project_id "
        "LEFT JOIN fictional_time_points end_point ON end_point.id = presence.end_time_point_id "
        "AND end_point.project_id = presence.project_id "
        "WHERE presence.project_id = e.project_id "
        "AND (presence.entity_id = e.id OR presence.location_entity_id = e.id) "
        "AND start_point.axis_id = selected_point.axis_id "
        "AND start_point.ordinal <= selected_point.ordinal "
        "AND (end_point.id IS NULL OR end_point.ordinal >= selected_point.ordinal)))";
  }
  if (query.fictional_window_start_time_point_id) {
    sql +=
        " AND (EXISTS (SELECT 1 FROM event_occurrences occurrence "
        "JOIN fictional_time_points occurrence_point "
        "ON occurrence_point.id = occurrence.time_point_id "
        "AND occurrence_point.project_id = occurrence.project_id "
        "JOIN fictional_time_points window_start "
        "ON window_start.id = ? AND window_start.project_id = occurrence.project_id "
        "JOIN fictional_time_points window_end "
        "ON window_end.id = ? AND window_end.project_id = occurrence.project_id "
        "WHERE occurrence.project_id = e.project_id "
        "AND occurrence.event_entity_id = e.id "
        "AND occurrence_point.axis_id = window_start.axis_id "
        "AND window_end.axis_id = window_start.axis_id "
        "AND occurrence_point.ordinal BETWEEN window_start.ordinal "
        "AND window_end.ordinal) "
        "OR EXISTS (SELECT 1 FROM event_participations participation "
        "JOIN event_occurrences occurrence "
        "ON occurrence.id = participation.event_occurrence_id "
        "AND occurrence.project_id = participation.project_id "
        "JOIN fictional_time_points occurrence_point "
        "ON occurrence_point.id = occurrence.time_point_id "
        "AND occurrence_point.project_id = occurrence.project_id "
        "JOIN fictional_time_points window_start "
        "ON window_start.id = ? AND window_start.project_id = participation.project_id "
        "JOIN fictional_time_points window_end "
        "ON window_end.id = ? AND window_end.project_id = participation.project_id "
        "WHERE participation.project_id = e.project_id "
        "AND participation.participant_entity_id = e.id "
        "AND occurrence_point.axis_id = window_start.axis_id "
        "AND window_end.axis_id = window_start.axis_id "
        "AND occurrence_point.ordinal BETWEEN window_start.ordinal "
        "AND window_end.ordinal) "
        "OR EXISTS (SELECT 1 FROM entity_presences presence "
        "JOIN fictional_time_points start_point "
        "ON start_point.id = presence.start_time_point_id "
        "AND start_point.project_id = presence.project_id "
        "LEFT JOIN fictional_time_points end_point "
        "ON end_point.id = presence.end_time_point_id "
        "AND end_point.project_id = presence.project_id "
        "JOIN fictional_time_points window_start "
        "ON window_start.id = ? AND window_start.project_id = presence.project_id "
        "JOIN fictional_time_points window_end "
        "ON window_end.id = ? AND window_end.project_id = presence.project_id "
        "WHERE presence.project_id = e.project_id "
        "AND (presence.entity_id = e.id OR presence.location_entity_id = e.id) "
        "AND start_point.axis_id = window_start.axis_id "
        "AND window_end.axis_id = window_start.axis_id "
        "AND start_point.ordinal <= window_end.ordinal "
        "AND (end_point.id IS NULL OR end_point.ordinal >= window_start.ordinal)))";
  }
  if (query.editorial_node_id) {
    // O seletor editorial trata a unidade como uma apresentação composta: uma
    // referência em qualquer descendente é visível no contexto do ancestral.
    sql +=
        " AND EXISTS (WITH RECURSIVE visible_nodes(id, work_id) AS ("
        "SELECT id, work_id FROM editorial_nodes WHERE id = ? "
        "UNION ALL SELECT node.id, node.work_id FROM editorial_nodes node "
        "JOIN visible_nodes parent ON node.parent_id = parent.id "
        "AND node.work_id = parent.work_id) "
        "SELECT 1 FROM editorial_entity_references reference "
        "JOIN visible_nodes visible ON visible.id = reference.editorial_node_id "
        "WHERE reference.project_id = e.project_id "
        "AND reference.entity_id = e.id)";
  }
  if (query.require_document_reference || query.document_id) {
    sql +=
        " AND EXISTS (SELECT 1 FROM document_entity_references doc_reference "
        "WHERE doc_reference.project_id = e.project_id "
        "AND doc_reference.entity_id = e.id";
    if (query.document_id)
      sql += " AND doc_reference.document_id = ?";
    sql += ')';
  }
}

int bind_entity_filters(SqliteStatement &statement, int parameter,
                        const EntityQuery &query) {
  for (const auto &id : query.entity_type_ids)
    statement.bind(parameter++, id);
  if (!query.search.empty()) {
    const auto pattern = like_pattern(query.search);
    statement.bind(parameter++, pattern);
    statement.bind(parameter++, pattern);
  }
  if (query.work_id)
    statement.bind(parameter++, *query.work_id);
  for (const auto &id : query.relation_type_ids)
    statement.bind(parameter++, id);
  if (query.related_entity_id) {
    statement.bind(parameter++, *query.related_entity_id);
    statement.bind(parameter++, *query.related_entity_id);
  }
  if (query.fictional_axis_id)
    statement.bind(parameter++, *query.fictional_axis_id);
  if (query.fictional_time_point_id) {
    statement.bind(parameter++, *query.fictional_time_point_id);
    statement.bind(parameter++, *query.fictional_time_point_id);
  }
  if (query.fictional_window_start_time_point_id) {
    statement.bind(parameter++, *query.fictional_window_start_time_point_id);
    statement.bind(parameter++, *query.fictional_window_end_time_point_id);
    statement.bind(parameter++, *query.fictional_window_start_time_point_id);
    statement.bind(parameter++, *query.fictional_window_end_time_point_id);
    statement.bind(parameter++, *query.fictional_window_start_time_point_id);
    statement.bind(parameter++, *query.fictional_window_end_time_point_id);
  }
  if (query.editorial_node_id)
    statement.bind(parameter++, *query.editorial_node_id);
  if (query.document_id)
    statement.bind(parameter++, *query.document_id);
  return parameter;
}

void validate_entity_filters(const EntityQuery &query) {
  validate_filter_ids(query.entity_type_ids, "Filtro de tipos de entidade");
  validate_filter_ids(query.relation_type_ids, "Filtro de tipos de relacao");
  if (query.work_id && query.work_id->empty())
    throw std::runtime_error("Filtro de Obra contem identificador vazio");
  if (query.related_entity_id && query.related_entity_id->empty())
    throw std::runtime_error("Filtro de contraparte contem identificador vazio");
  if (query.fictional_axis_id && query.fictional_axis_id->empty())
    throw std::runtime_error("Filtro de eixo ficcional contem identificador vazio");
  if (query.fictional_time_point_id && query.fictional_time_point_id->empty())
    throw std::runtime_error("Filtro de ponto ficcional contem identificador vazio");
  if (query.fictional_window_start_time_point_id &&
      query.fictional_window_start_time_point_id->empty())
    throw std::runtime_error(
        "Inicio do periodo ficcional contem identificador vazio");
  if (query.fictional_window_end_time_point_id &&
      query.fictional_window_end_time_point_id->empty())
    throw std::runtime_error("Fim do periodo ficcional contem identificador vazio");
  if (query.fictional_window_start_time_point_id.has_value() !=
      query.fictional_window_end_time_point_id.has_value())
    throw std::runtime_error(
        "Filtro de periodo ficcional exige inicio e fim");
  if (query.fictional_time_point_id &&
      query.fictional_window_start_time_point_id)
    throw std::runtime_error(
        "Filtro ficcional nao pode combinar ponto e periodo");
  if (query.editorial_node_id && query.editorial_node_id->empty())
    throw std::runtime_error(
        "Filtro de unidade editorial contem identificador vazio");
  if (query.document_id && query.document_id->empty())
    throw std::runtime_error("Filtro de Documento contem identificador vazio");
}

project::EntityType read_entity_type(SqliteStatement &statement) {
  project::EntityType value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_integer(4) != 0, statement.column_text(5),
      statement.column_text(6)};
  project::validate(value);
  return value;
}

project::NarrativeEntity read_entity(SqliteStatement &statement) {
  project::NarrativeEntity value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5)};
  project::validate(value);
  return value;
}

project::RelationType read_relation_type(SqliteStatement &statement) {
  project::RelationType value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4),
      project::relation_directionality_from_string(statement.column_text(5)),
      statement.column_integer(6) != 0, statement.column_text(7),
      statement.column_text(8)};
  project::validate(value);
  return value;
}

project::NarrativeRelation read_relation(SqliteStatement &statement) {
  project::NarrativeRelation value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5),
      statement.column_text(6),
      statement.column_is_null(7)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(7)},
      statement.column_is_null(8)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(8)},
      statement.column_is_null(9)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(9)}};
  project::validate(value);
  return value;
}

project::EntityWorkScope read_work_scope(SqliteStatement &statement) {
  project::EntityWorkScope value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5)};
  project::validate(value);
  return value;
}

project::EditorialEntityReference
read_editorial_reference(SqliteStatement &statement) {
  project::EditorialEntityReference value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5),
      statement.column_text(6), statement.column_text(7)};
  project::validate(value);
  return value;
}

void require_removed(SqliteDatabase &database, const char *message) {
  if (database.changes() != 1)
    throw std::runtime_error(message);
}

} // namespace

void SqliteNarrativeRepository::initialize(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  SchemaMigrator{}.migrate(database);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto find = database.prepare(
      "SELECT id, is_builtin FROM entity_types "
      "WHERE project_id = ? AND key = ?");
  auto insert = database.prepare(
      "INSERT INTO entity_types "
      "(id, project_id, key, name, description, is_builtin, created_at, "
      "updated_at) VALUES (?, ?, ?, ?, ?, 1, ?, ?)");
  for (const auto &builtin : project::builtin_entity_types()) {
    find.bind(1, owner);
    find.bind(2, builtin.key);
    if (find.step()) {
      if (find.column_text(0) != builtin.id || find.column_integer(1) != 1)
        throw std::runtime_error(
            "Um tipo interno de entidade divergiu do formato esperado");
      if (find.step())
        throw std::runtime_error("Tipo interno de entidade duplicado");
      find.reset();
      continue;
    }
    find.reset();
    const auto now = project::utc_now();
    insert.bind(1, builtin.id);
    insert.bind(2, owner);
    insert.bind(3, builtin.key);
    insert.bind(4, builtin.name);
    insert.bind(5, builtin.description);
    insert.bind(6, now);
    insert.bind(7, now);
    insert.run();
    insert.reset();
  }
  transaction.commit();
}

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

void SqliteNarrativeRepository::save(
    const std::filesystem::path &project_path,
    const project::EntityType &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  const bool updating = exists(database, "entity_types", value.id);
  auto statement = database.prepare(
      "INSERT INTO entity_types "
      "(id, project_id, key, name, description, is_builtin, created_at, "
      "updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET key=excluded.key, name=excluded.name, "
      "description=excluded.description, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.key);
  statement.bind(4, value.name);
  statement.bind(5, value.description);
  statement.bind(6, std::int64_t{value.is_builtin ? 1 : 0});
  statement.bind(7, value.created_at);
  statement.bind(8, value.updated_at);
  statement.run();
  record_change(database, owner,
                updating ? "update_entity_type" : "create_entity_type");
  transaction.commit();
}

void SqliteNarrativeRepository::save(
    const std::filesystem::path &project_path,
    const project::NarrativeEntity &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  const bool updating = exists(database, "entities", value.id);
  auto statement = database.prepare(
      "INSERT INTO entities "
      "(id, project_id, entity_type_id, name, summary, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
      "entity_type_id=excluded.entity_type_id, name=excluded.name, "
      "summary=excluded.summary, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.entity_type_id);
  statement.bind(4, value.name);
  statement.bind(5, value.summary);
  statement.bind(6, value.created_at);
  statement.bind(7, value.updated_at);
  statement.run();
  record_change(database, owner,
                updating ? "update_entity" : "create_entity");
  transaction.commit();
}

void SqliteNarrativeRepository::save(
    const std::filesystem::path &project_path,
    const project::RelationType &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  const bool updating = exists(database, "relation_types", value.id);
  auto statement = database.prepare(
      "INSERT INTO relation_types "
      "(id, project_id, key, name, inverse_name, description, directionality, "
      "is_builtin, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET key=excluded.key, name=excluded.name, "
      "inverse_name=excluded.inverse_name, description=excluded.description, "
      "directionality=excluded.directionality, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.key);
  statement.bind(4, value.name);
  statement.bind(5, value.inverse_name);
  statement.bind(6, value.description);
  statement.bind(7, project::to_string(value.directionality));
  statement.bind(8, std::int64_t{value.is_builtin ? 1 : 0});
  statement.bind(9, value.created_at);
  statement.bind(10, value.updated_at);
  statement.run();
  record_change(database, owner,
                updating ? "update_relation_type" : "create_relation_type");
  transaction.commit();
}

void SqliteNarrativeRepository::save(
    const std::filesystem::path &project_path,
    const project::NarrativeRelation &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  const bool updating = exists(database, "relations", value.id);
  auto statement = database.prepare(
      "INSERT INTO relations "
      "(id, project_id, relation_type_id, source_entity_id, target_entity_id, "
      "description, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET relation_type_id=excluded.relation_type_id, "
      "source_entity_id=excluded.source_entity_id, "
      "target_entity_id=excluded.target_entity_id, "
      "description=excluded.description, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.relation_type_id);
  statement.bind(4, value.source_entity_id);
  statement.bind(5, value.target_entity_id);
  statement.bind(6, value.description);
  statement.bind(7, value.created_at);
  statement.bind(8, value.updated_at);
  statement.run();
  auto context = database.prepare(
      "INSERT INTO relation_contexts "
      "(relation_id, project_id, fictional_time_point_id, location_entity_id, "
      "cause_entity_id) VALUES (?, ?, ?, ?, ?) "
      "ON CONFLICT(relation_id) DO UPDATE SET "
      "project_id=excluded.project_id, "
      "fictional_time_point_id=excluded.fictional_time_point_id, "
      "location_entity_id=excluded.location_entity_id, "
      "cause_entity_id=excluded.cause_entity_id");
  context.bind(1, value.id);
  context.bind(2, owner);
  bind_optional(context, 3, value.fictional_time_point_id);
  bind_optional(context, 4, value.location_entity_id);
  bind_optional(context, 5, value.cause_entity_id);
  context.run();
  record_change(database, owner,
                updating ? "update_relation" : "create_relation");
  transaction.commit();
}

void SqliteNarrativeRepository::save(
    const std::filesystem::path &project_path,
    const project::EntityWorkScope &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  const bool updating = exists(database, "entity_work_scopes", value.id);
  auto statement = database.prepare(
      "INSERT INTO entity_work_scopes "
      "(id, project_id, entity_id, work_id, notes, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
      "notes=excluded.notes, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.entity_id);
  statement.bind(4, value.work_id);
  statement.bind(5, value.notes);
  statement.bind(6, value.created_at);
  statement.bind(7, value.updated_at);
  statement.run();
  record_change(database, owner,
                updating ? "update_entity_work_scope"
                         : "create_entity_work_scope");
  transaction.commit();
}

void SqliteNarrativeRepository::save(
    const std::filesystem::path &project_path,
    const project::EditorialEntityReference &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  const bool updating =
      exists(database, "editorial_entity_references", value.id);
  auto statement = database.prepare(
      "INSERT INTO editorial_entity_references "
      "(id, project_id, entity_id, work_id, editorial_node_id, purpose, notes, "
      "created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET purpose=excluded.purpose, "
      "notes=excluded.notes, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.entity_id);
  statement.bind(4, value.work_id);
  statement.bind(5, value.editorial_node_id);
  statement.bind(6, value.purpose);
  statement.bind(7, value.notes);
  statement.bind(8, value.created_at);
  statement.bind(9, value.updated_at);
  statement.run();
  record_change(database, owner,
                updating ? "update_editorial_entity_reference"
                         : "create_editorial_entity_reference");
  transaction.commit();
}

void SqliteNarrativeRepository::remove_entity_type(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement = database.prepare("DELETE FROM entity_types WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  require_removed(database, "Tipo de entidade não encontrado no banco");
  record_change(database, owner, "delete_entity_type");
  transaction.commit();
}

void SqliteNarrativeRepository::remove_entity(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement = database.prepare("DELETE FROM entities WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  require_removed(database, "Entidade narrativa não encontrada no banco");
  record_change(database, owner, "delete_entity");
  transaction.commit();
}

void SqliteNarrativeRepository::remove_relation_type(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement = database.prepare("DELETE FROM relation_types WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  require_removed(database, "Tipo de relação não encontrado no banco");
  record_change(database, owner, "delete_relation_type");
  transaction.commit();
}

void SqliteNarrativeRepository::remove_relation(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement = database.prepare("DELETE FROM relations WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  require_removed(database, "Relação narrativa não encontrada no banco");
  record_change(database, owner, "delete_relation");
  transaction.commit();
}

void SqliteNarrativeRepository::remove_work_scope(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement =
      database.prepare("DELETE FROM entity_work_scopes WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  require_removed(database, "Vínculo entre entidade e obra não encontrado");
  record_change(database, owner, "delete_entity_work_scope");
  transaction.commit();
}

void SqliteNarrativeRepository::remove_editorial_reference(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement = database.prepare(
      "DELETE FROM editorial_entity_references WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  require_removed(database, "Referência editorial de entidade não encontrada");
  record_change(database, owner, "delete_editorial_entity_reference");
  transaction.commit();
}

} // namespace inde::persistence

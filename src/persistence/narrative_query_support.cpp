#include "inde/persistence/sqlite_narrative_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"
#include "inde/project/worldbuilding.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "narrative_query_support.hpp"
#include "text_entity_usage.hpp"

namespace inde::persistence::narrative_detail {

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

void append_entity_filters(std::string &sql, const EntityQuery &query) {
  if (!query.entity_type_ids.empty()) {
    sql += " AND e.entity_type_id IN";
    append_placeholders(sql, query.entity_type_ids.size());
  }
  if (!query.search.empty())
    sql += " AND (e.name LIKE ? ESCAPE '\\' OR e.summary LIKE ? ESCAPE '\\' OR EXISTS (SELECT 1 FROM entity_aliases a WHERE a.project_id=e.project_id AND a.entity_id=e.id AND a.normalized LIKE ? ESCAPE '\\'))";
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
        " AND EXISTS (SELECT 1 FROM " + text_entity_usage_sql() + " doc_reference "
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
    const auto trimmed=project::trim_world_text(query.search);
    statement.bind(parameter++, like_pattern(trimmed.empty() || query.search.size()>512 ? query.search : project::alias_key(query.search)));
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

} // namespace inde::persistence::narrative_detail

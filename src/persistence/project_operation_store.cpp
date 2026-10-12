#include "inde/persistence/project_operation_store.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"
#define SQLITE_ENABLE_SESSION 1
#include <algorithm>
#include <cstring>
#include <glib.h>
#include <map>
#include <memory>
#include <set>
#include <sqlite3.h>
#include <stdexcept>
#include <tuple>

namespace inde::persistence {
namespace {
constexpr std::size_t max_changes = 8 * 1024 * 1024;
constexpr std::size_t max_rows = 20000;
using Bytes = std::vector<std::uint8_t>;
void check(int code) {
  if (code != SQLITE_OK)
    throw std::runtime_error(
        "Não foi possível preparar/reverter a operação SQLite (" +
        std::to_string(code) + ")");
}
std::string quote(const std::string &name) {
  if (name.empty() ||
      name.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
          std::string::npos)
    throw std::runtime_error("Identificador inesperado no histórico");
  return "\"" + name + "\"";
}
bool excluded(const std::string &name) {
  return name.starts_with("sqlite_") ||
         name.starts_with("cartographic_position_index") ||
         name == "schema_migrations" || name == "projects" ||
         name == "change_log" || name == "project_operations";
}
std::string hash(std::span<const std::uint8_t> bytes) {
  auto *value = g_compute_checksum_for_data(G_CHECKSUM_SHA256, bytes.data(),
                                            bytes.size());
  if (!value)
    throw std::bad_alloc();
  std::string result(value);
  g_free(value);
  return result;
}
std::string owner(SqliteDatabase &db) {
  if (SchemaMigrator{}.current_version(db) != current_database_schema_version)
    throw std::runtime_error("Operações exigem o schema desta versão; reabra o "
                             "Projeto nesta versão");
  auto s = db.prepare("SELECT id FROM projects");
  if (!s.step())
    throw std::runtime_error("Projeto sem identidade");
  auto id = s.column_text(0);
  if (s.step())
    throw std::runtime_error("Identidade de Projeto ambígua");
  return id;
}
SqliteDatabase open(const std::filesystem::path &path) {
  const auto file = ProjectDatabaseRepository::database_path(path);
  if (!std::filesystem::is_regular_file(file))
    throw std::runtime_error("Banco do Projeto ausente");
  return SqliteDatabase(file);
}
void integrity(SqliteDatabase &db) {
  auto s = db.prepare("PRAGMA foreign_key_check");
  if (s.step())
    throw std::runtime_error(
        "A operação deixaria vínculos inválidos; nada foi confirmado");
}
Bytes session_changes(sqlite3_session *session) {
  if (sqlite3session_memory_used(session) > 128 * 1024 * 1024)
    throw std::runtime_error(
        "Operação excede o orçamento de captura; nada foi confirmado");
  int size{};
  void *data{};
  check(sqlite3session_changeset(session, &size, &data));
  std::unique_ptr<void, decltype(&sqlite3_free)> release(data, sqlite3_free);
  if (size < 0 || static_cast<std::size_t>(size) > max_changes)
    throw std::runtime_error(
        "Operação excede 8 MiB de histórico; nada foi confirmado");
  if (!size)
    return {};
  const auto *p = static_cast<const std::uint8_t *>(data);
  return {p, p + size};
}
struct Iterator {
  sqlite3_changeset_iter *p{};
  explicit Iterator(const Bytes &bytes) {
    if (bytes.empty() || bytes.size() > max_changes)
      throw std::runtime_error("Registro reversível vazio ou excessivo");
    check(sqlite3changeset_start(&p, static_cast<int>(bytes.size()),
                                 const_cast<std::uint8_t *>(bytes.data())));
  }
  ~Iterator() { sqlite3changeset_finalize(p); }
  bool next() {
    const int r = sqlite3changeset_next(p);
    if (r == SQLITE_DONE)
      return false;
    if (r != SQLITE_ROW)
      check(r);
    return true;
  }
};
struct Query {
  sqlite3_stmt *p{};
  Query(sqlite3 *db, const std::string &sql) {
    check(sqlite3_prepare_v2(db, sql.c_str(), -1, &p, nullptr));
  }
  ~Query() { sqlite3_finalize(p); }
};
// Explicit type/length/value encoding; hashes are independent of changeset
// order and of whether an effect originated in a foreign-key cascade.
std::string value(sqlite3_value *v) {
  if (!v)
    return "missing;";
  const int type = sqlite3_value_type(v);
  std::string out = std::to_string(type) + ":";
  if (type == SQLITE_NULL)
    return out + ";";
  if (type == SQLITE_INTEGER)
    return out + std::to_string(sqlite3_value_int64(v)) + ";";
  if (type == SQLITE_FLOAT) {
    const double d = sqlite3_value_double(v);
    out.append(reinterpret_cast<const char *>(&d), sizeof(d));
    return out + ";";
  }
  const int length = sqlite3_value_bytes(v);
  const auto *data =
      type == SQLITE_BLOB ? sqlite3_value_blob(v) : sqlite3_value_text(v);
  out += std::to_string(length) + ":";
  if (length)
    out.append(static_cast<const char *>(data), length);
  return out + ";";
}
std::string label(const std::string &table) {
  static const std::map<std::string, std::string> labels = {
      {"documents", "Documentos (texto e metadados)"},
      {"compositions", "Composições (ordem, blocos e camadas)"},
      {"composition_outputs", "Proveniência da saída de Composição"},
      {"document_groups", "Grupos de Documentos"},
      {"entities", "Entidades narrativas"},
      {"entity_aliases", "Nomes alternativos de entidades"},
      {"fictional_calendars", "Calendários ficcionais"},
      {"axis_calendars", "Calendários associados aos eixos"},
      {"calendar_revisions", "Revisões de sistemas temporais"},
      {"calendar_adoptions", "Adoções históricas de calendários"},
      {"temporal_references", "Datas e horários autorais"},
      {"temporal_attachments", "Vínculos autorais de datas"},
      {"temporal_reference_extents", "Precisão e intervalos temporais"},
      {"semantic_date_bindings", "Datas e horários no texto"},
      {"relations", "Relações narrativas"},
      {"entity_types", "Tipos de entidade"},
      {"relation_types", "Tipos de relação"},
      {"document_anchors", "Âncoras textuais"},
      {"document_entity_references", "Vínculos Texto–Projeto"},
      {"document_text_references", "Referências entre Documentos"},
      {"document_format_spans", "Formatação textual"},
      {"document_semantics", "Camadas semânticas e revisão"},
      {"semantic_annotations", "Marcações semânticas"},
      {"semantic_entity_bindings", "Vínculos semânticos com entidades"},
      {"editorial_nodes", "Unidades editoriais"},
      {"editorial_structures", "Estruturas editoriais"},
      {"works", "Obras"},
      {"intellectual_properties", "Universos"},
      {"entity_work_scopes", "Vínculos de entidades com Obras"},
      {"entity_presences", "Presenças de entidades"},
      {"editorial_entity_references", "Referências nas unidades editoriais"},
      {"editorial_trash_operations", "Registro de exclusão estrutural"},
      {"editorial_node_trash", "Unidades guardadas para restauração"},
      {"relation_contexts", "Contextos de relações"},
      {"structural_element_types", "Tipos de unidade editorial"},
      {"fictional_time_axes", "Eixos de tempo ficcional"},
      {"fictional_time_points", "Pontos de tempo ficcional"},
      {"event_occurrences", "Ocorrências de eventos"},
      {"event_participations", "Participações em eventos"},
      {"semantic_type_definitions", "Tipos de marcação semântica"},
      {"semantic_annotation_relations", "Relações entre marcações"},
      {"semantic_fictional_time_bindings", "Vínculos textuais com o tempo"},
      {"structure_models", "Modelos de estrutura"},
      {"structure_model_items", "Itens de modelos"},
      {"structure_model_item_links", "Vínculos de modelos"},
      {"narrative_structures", "Estruturas narrativas"},
      {"narrative_lines", "Linhas narrativas"},
      {"narrative_units", "Unidades narrativas"},
      {"narrative_unit_lines", "Unidades nas linhas narrativas"},
      {"narrative_unit_entities", "Entidades nas unidades narrativas"},
      {"narrative_unit_contexts", "Contexto de Local e fila de escrita"},
      {"narrative_realizations", "Realizações textuais das unidades"},
      {"narrative_text_targets", "Trechos e ocorrências das unidades"},
      {"narrative_text_decisions", "Decisões de reconciliação textual"},
      {"narrative_continuity_exceptions", "Exceções autorais de continuidade"},
      {"narrative_unit_presence_intents", "Presenças planejadas nas unidades"},
      {"narrative_unit_relations", "Relações em foco nas unidades"},
      {"narrative_branch_provenance", "Proveniência das alternativas narrativas"},
      {"narrative_dependency_scopes", "Autoridade declarada dos vínculos narrativos"},
      {"narrative_links", "Vínculos narrativos"},
      {"narrative_roles", "Papéis narrativos"},
      {"narrative_role_assignments", "Atribuições de papéis"},
      {"cartographic_planets", "Mundos cartográficos"},
      {"cartographic_chunks", "Áreas cartográficas"},
      {"cartographic_positions", "Posições cartográficas"},
      {"cartographic_terrain_locks", "Proteções de terreno"}};
  const auto found = labels.find(table);
  return found == labels.end() ? table : found->second;
}
struct Inspection {
  std::string fingerprint;
  std::vector<project::OperationImpact> impacts;
  std::string subject;
};
std::string short_text(const std::string &text) {
  if (!g_utf8_validate(text.data(), text.size(), nullptr))
    return "Identificação indisponível";
  if (g_utf8_strlen(text.data(), text.size()) <= 100)
    return text;
  return text.substr(0,
                     g_utf8_offset_to_pointer(text.data(), 100) - text.data()) +
         "…";
}
Inspection inspect(sqlite3 *db, const Bytes &bytes, bool preflight = false) {
  Iterator it(bytes);
  std::vector<std::string> rows;
  std::map<std::string, project::OperationImpact> counts;
  int subject_rank = 100;
  std::string subject;
  while (it.next()) {
    if (rows.size() >= max_rows)
      throw std::runtime_error("Operação excede 20000 registros");
    const char *table{};
    int columns{}, op{}, indirect{};
    check(sqlite3changeset_op(it.p, &table, &columns, &op, &indirect));
    const std::string name(table ? table : "");
    quote(name);
    if (excluded(name) || columns < 1 || columns > 128 ||
        (op != SQLITE_INSERT && op != SQLITE_UPDATE && op != SQLITE_DELETE))
      throw std::runtime_error("Tabela/operação incompatível com o histórico");
    unsigned char *pk{};
    int n{};
    check(sqlite3changeset_pk(it.p, &pk, &n));
    std::vector<std::string> column_names;
    std::vector<bool> primary;
    Query schema(db, "PRAGMA table_info(" + quote(name) + ")");
    int code;
    while ((code = sqlite3_step(schema.p)) == SQLITE_ROW) {
      column_names.emplace_back(
          reinterpret_cast<const char *>(sqlite3_column_text(schema.p, 1)));
      primary.push_back(sqlite3_column_int(schema.p, 5) > 0);
    }
    if (code != SQLITE_DONE || n != columns ||
        column_names.size() != static_cast<std::size_t>(columns))
      throw std::runtime_error("Schema incompatível com a operação gravada");
    std::string encoded = name + ":" + std::to_string(op) + ":";
    std::string where;
    std::vector<sqlite3_value *> olds(columns), news(columns);
    for (int c = 0; c < columns; ++c) {
      if (primary[c] != (pk[c] != 0))
        throw std::runtime_error("Chave incompatível com o histórico");
      if (op != SQLITE_INSERT)
        check(sqlite3changeset_old(it.p, c, &olds[c]));
      if (op != SQLITE_DELETE)
        check(sqlite3changeset_new(it.p, c, &news[c]));
      encoded += value(olds[c]) + value(news[c]);
      if (pk[c]) {
        auto *v = op == SQLITE_INSERT ? news[c] : olds[c];
        if (!v || sqlite3_value_type(v) == SQLITE_NULL)
          throw std::runtime_error("Chave nula no histórico");
        if (!where.empty())
          where += " AND ";
        where += quote(column_names[c]) + " IS ?";
      }
    }
    if (where.empty())
      throw std::runtime_error("Tabela sem chave no histórico");
    if (preflight) {
      Query current(db, "SELECT * FROM " + quote(name) + " WHERE " + where);
      int index = 1;
      for (int c = 0; c < columns; ++c)
        if (pk[c])
          check(sqlite3_bind_value(current.p, index++,
                                   op == SQLITE_INSERT ? news[c] : olds[c]));
      const int result = sqlite3_step(current.p);
      if (op == SQLITE_INSERT) {
        if (result != SQLITE_DONE)
          throw std::runtime_error(
              "A identidade a restaurar já existe; reversão bloqueada");
      } else {
        if (result != SQLITE_ROW)
          throw std::runtime_error(
              "Um registro afetado foi removido; reversão bloqueada");
        for (int c = 0; c < columns; ++c)
          if (olds[c] &&
              value(olds[c]) != value(sqlite3_column_value(current.p, c)))
            throw std::runtime_error("Um campo afetado mudou depois desta "
                                     "operação; reversão bloqueada");
        if (sqlite3_step(current.p) != SQLITE_DONE)
          throw std::runtime_error("Identidade ambígua no histórico");
      }
    }
    rows.push_back(std::move(encoded));
    auto &count = counts[name];
    count.table = name;
    count.label = label(name);
    if (op == SQLITE_UPDATE) {
      for (int c = 0; c < columns; ++c)
        if (!pk[c] && olds[c] && news[c] && value(olds[c]) != value(news[c]) &&
            std::find(count.fields.begin(), count.fields.end(),
                      column_names[c]) == count.fields.end())
          count.fields.push_back(column_names[c]);
    }
    if (count.examples.size() < 3) {
      std::string example;
      auto names = column_names;
      for (const auto *field : {"title", "name", "label", "id"}) {
        auto found = std::find(names.begin(), names.end(), field);
        if (found == names.end())
          continue;
        const auto c = std::distance(names.begin(), found);
        auto *v = news[c] ? news[c] : olds[c];
        if (v && sqlite3_value_type(v) == SQLITE_TEXT) {
          example =
              short_text({reinterpret_cast<const char *>(sqlite3_value_text(v)),
                          static_cast<std::size_t>(sqlite3_value_bytes(v))});
          if (!example.empty())
            break;
        }
      }
      if (op == SQLITE_UPDATE) {
        // Unchanged names are absent from a changeset; observe only this row's
        // identity/title, never read or display its body in the impact dialog.
        Query current(db, "SELECT * FROM " + quote(name) + " WHERE " + where);
        int index = 1;
        for (int c = 0; c < columns; ++c)
          if (pk[c])
            check(sqlite3_bind_value(current.p, index++, olds[c]));
        if (sqlite3_step(current.p) == SQLITE_ROW)
          for (const auto *field : {"title", "name", "label"}) {
            auto found = std::find(names.begin(), names.end(), field);
            if (found == names.end())
              continue;
            const auto c = std::distance(names.begin(), found);
            if (sqlite3_column_type(current.p, c) == SQLITE_TEXT) {
              example = short_text({reinterpret_cast<const char *>(
                                        sqlite3_column_text(current.p, c)),
                                    static_cast<std::size_t>(
                                        sqlite3_column_bytes(current.p, c))});
              break;
            }
          }
      }
      if (name == "relations") {
        const auto related_name = [&](const char *field, const char *table) {
          const auto f = std::find(names.begin(), names.end(), field);
          if (f == names.end())
            return std::string{};
          const auto c = std::distance(names.begin(), f);
          auto *v = news[c] ? news[c] : olds[c];
          if (!v)
            return std::string{};
          Query q(db, "SELECT name FROM " + quote(table) + " WHERE id=?");
          check(sqlite3_bind_value(q.p, 1, v));
          if (sqlite3_step(q.p) != SQLITE_ROW)
            return std::string{};
          return short_text(
              {reinterpret_cast<const char *>(sqlite3_column_text(q.p, 0)),
               static_cast<std::size_t>(sqlite3_column_bytes(q.p, 0))});
        };
        auto from = related_name("source_entity_id", "entities");
        auto to = related_name("target_entity_id", "entities");
        auto relation = related_name("relation_type_id", "relation_types");
        if (!from.empty() || !to.empty())
          example = (from.empty() ? "Entidade" : from) + " — " +
                    (relation.empty() ? "relação" : relation) + " → " +
                    (to.empty() ? "Entidade" : to);
      }
      if (!example.empty()) {
        count.examples.push_back(example);
        int rank =
            (name == "entities" || name == "relations" || name == "works" ||
             name == "intellectual_properties" || name == "document_groups" ||
             name == "editorial_structures" || name == "narrative_structures")
                ? 0
            : (name == "documents" || name == "structure_models") ? 1
                                                                  : 10;
        if (rank < subject_rank) {
          subject_rank = rank;
          subject = example;
        }
      }
    }
    if (op == SQLITE_INSERT)
      ++count.inserted;
    else if (op == SQLITE_DELETE)
      ++count.removed;
    else
      ++count.updated;
  }
  if (rows.empty())
    throw std::runtime_error("A operação não contém efeitos reversíveis");
  std::sort(rows.begin(), rows.end());
  std::string all;
  for (const auto &row : rows) {
    all += std::to_string(row.size()) + ":";
    all += row;
  }
  Inspection result{
      hash({reinterpret_cast<const std::uint8_t *>(all.data()), all.size()}),
      {},
      {}};
  for (auto &[name, count] : counts)
    result.impacts.push_back(std::move(count));
  result.subject = subject;
  return result;
}
void copy(sqlite3 *source, sqlite3 *target) {
  // Bound the private simulation, including accumulated history; never
  // truncate.
  Query size(source,
             "SELECT (SELECT page_count FROM pragma_page_count)*(SELECT "
             "page_size FROM pragma_page_size)");
  if (sqlite3_step(size.p) != SQLITE_ROW ||
      sqlite3_column_int64(size.p, 0) > 256 * 1024 * 1024)
    throw std::runtime_error("Prévia excede o orçamento de 256 MiB do Projeto");
  auto *backup = sqlite3_backup_init(target, "main", source, "main");
  if (!backup)
    throw std::runtime_error("Não foi possível criar a simulação privada");
  const int step = sqlite3_backup_step(backup, -1);
  const int finish = sqlite3_backup_finish(backup);
  if (step != SQLITE_DONE)
    check(step);
  check(finish);
}
std::string removal_label(project::RemovalKind kind) {
  switch (kind) {
  case project::RemovalKind::Document:
    return "Remover Documento";
  case project::RemovalKind::Entity:
    return "Remover entidade";
  case project::RemovalKind::Relation:
    return "Remover relação";
  case project::RemovalKind::DocumentGroup:
    return "Remover grupo de Documentos";
  case project::RemovalKind::Work:
    return "Remover Obra";
  case project::RemovalKind::IntellectualProperty:
    return "Remover universo";
  case project::RemovalKind::EditorialBranch:
    return "Remover ramificação editorial";
  case project::RemovalKind::CalendarSystem:
    return "Excluir sistema temporal";
  case project::RemovalKind::CartographicMap:
    return "Excluir mapa";
  case project::RemovalKind::EntityType:
    return "Remover tipo de entidade";
  case project::RemovalKind::RelationType:
    return "Remover tipo de relação";
  case project::RemovalKind::StructuralType:
    return "Remover tipo editorial";
  case project::RemovalKind::TimeAxis:
    return "Remover eixo temporal";
  case project::RemovalKind::TimePoint:
    return "Remover ponto temporal";
  case project::RemovalKind::TemporalReference:
    return "Remover data ou horário";
  case project::RemovalKind::StructureModel:
    return "Remover modelo estrutural";
  case project::RemovalKind::EditorialStructure:
    return "Remover estrutura editorial";
  case project::RemovalKind::NarrativeStructure:
    return "Remover estrutura narrativa";
  case project::RemovalKind::NarrativeLine:
    return "Remover linha narrativa";
  case project::RemovalKind::NarrativeUnit:
    return "Remover unidade narrativa";
  case project::RemovalKind::NarrativeRole:
    return "Remover papel narrativo";
  case project::RemovalKind::DocumentAnchor:
    return "Usos da âncora textual";
  case project::RemovalKind::TextAnnotation:
    return "Usos da marcação textual";
  case project::RemovalKind::EntityWorkScope:
    return "Remover vínculo da entidade com a Obra";
  }
  throw std::runtime_error("Tipo de remoção inválido");
}
std::string table_for(project::RemovalKind kind) {
  switch (kind) {
  case project::RemovalKind::Document:
    return "documents";
  case project::RemovalKind::Entity:
    return "entities";
  case project::RemovalKind::Relation:
    return "relations";
  case project::RemovalKind::DocumentGroup:
    return "document_groups";
  case project::RemovalKind::Work:
    return "works";
  case project::RemovalKind::IntellectualProperty:
    return "intellectual_properties";
  case project::RemovalKind::EditorialBranch:
    return "editorial_nodes";
  case project::RemovalKind::CalendarSystem:
    return "fictional_calendars";
  case project::RemovalKind::CartographicMap:
    return "cartographic_planets";
  case project::RemovalKind::EntityType:
    return "entity_types";
  case project::RemovalKind::RelationType:
    return "relation_types";
  case project::RemovalKind::StructuralType:
    return "structural_element_types";
  case project::RemovalKind::TimeAxis:
    return "fictional_time_axes";
  case project::RemovalKind::TimePoint:
    return "fictional_time_points";
  case project::RemovalKind::TemporalReference:
    return "temporal_references";
  case project::RemovalKind::StructureModel:
    return "structure_models";
  case project::RemovalKind::EditorialStructure:
    return "editorial_structures";
  case project::RemovalKind::NarrativeStructure:
    return "narrative_structures";
  case project::RemovalKind::NarrativeLine:
    return "narrative_lines";
  case project::RemovalKind::NarrativeUnit:
    return "narrative_units";
  case project::RemovalKind::NarrativeRole:
    return "narrative_roles";
  case project::RemovalKind::DocumentAnchor:
    return "document_anchors";
  case project::RemovalKind::TextAnnotation:
    return "semantic_annotations";
  case project::RemovalKind::EntityWorkScope:
    return "entity_work_scopes";
  }
  throw std::runtime_error("Tipo de remoção inválido");
}
bool uses(SqliteDatabase &db, const std::string &table,
          const std::string &column, const std::string &id) {
  auto s = db.prepare("SELECT 1 FROM " + quote(table) + " WHERE " +
                      quote(column) + "=? LIMIT 1");
  s.bind(1, id);
  return s.step();
}
using UsageRow = std::map<std::string, std::string>;
std::vector<std::string> usage_columns(SqliteDatabase &db,
                                       const std::string &table) {
  static const std::set<std::string> wanted = {"id",
                                               "name",
                                               "title",
                                               "label",
                                               "role",
                                               "purpose",
                                               "work_id",
                                               "structure_id",
                                               "axis_id",
                                               "calendar_id",
                                               "reference_id",
                                               "document_id",
                                               "role_id",
                                               "planet_id",
                                               "annotation_id",
                                               "anchor_id",
                                               "source_document_id",
                                               "target_anchor_id",
                                               "start_offset",
                                               "from_ordinal",
                                               "entity_id",
                                               "unit_id",
                                               "line_id",
                                               "relation_id",
                                               "event_entity_id",
                                               "occurrence_id",
                                               "source_entity_id",
                                               "target_entity_id",
                                               "source_unit_id",
                                               "target_unit_id",
                                               "source_id",
                                               "target_id",
                                               "model_id",
                                               "layer",
                                               "field_key",
                                               "ordinal",
                                               "kind",
                                               "type_key",
                                               "participant_entity_id",
                                               "event_occurrence_id",
                                               "editorial_node_id",
                                               "location_entity_id",
                                               "start_time_point_id",
                                               "end_time_point_id",
                                               "operation_id"};
  std::vector<std::string> result;
  auto q = db.prepare("PRAGMA table_info(" + quote(table) + ")");
  while (q.step())
    if (wanted.contains(q.column_text(1)))
      result.push_back(q.column_text(1));
  return result;
}
std::string usage_select(const std::vector<std::string> &columns) {
  std::string result;
  for (const auto &c : columns)
    result += (result.empty() ? "" : ",") + std::string("CAST(coalesce(") +
              quote(c) + ",'') AS TEXT)";
  return result;
}
UsageRow usage_row(SqliteStatement &q,
                   const std::vector<std::string> &columns) {
  UsageRow result;
  for (std::size_t i = 0; i < columns.size(); ++i)
    result[columns[i]] = q.column_text(static_cast<int>(i));
  return result;
}
UsageRow usage_record(SqliteDatabase &db, const std::string &table,
                      const std::string &id) {
  const auto columns = usage_columns(db, table);
  if (std::find(columns.begin(), columns.end(), "id") == columns.end())
    return {};
  auto q = db.prepare("SELECT " + usage_select(columns) + " FROM " +
                      quote(table) + " WHERE id=?");
  q.bind(1, id);
  return q.step() ? usage_row(q, columns) : UsageRow{};
}
std::string usage_name(const UsageRow &r) {
  for (const auto *key :
       {"title", "name", "label", "role", "purpose", "field_key", "type_key"}) {
    const auto it = r.find(key);
    if (it != r.end() && !it->second.empty())
      return g_utf8_validate(it->second.data(), it->second.size(), nullptr)
                 ? it->second
                 : "Identificação indisponível";
  }
  return "Vínculo sem rótulo";
}
project::RemovalUse usage_location(SqliteDatabase &db, const std::string &table,
                                   UsageRow r) {
  project::RemovalUse u;
  u.table = table;
  u.record_id = r["id"];
  u.name = usage_name(r);
  u.location_table = table;
  u.location_id = r["id"];
  u.guidance = "Abra este item e remova ou substitua explicitamente o vínculo. "
               "O item não será apagado pelo auxiliar.";
  auto locate = [&](const std::string &t, const std::string &id) {
    u.location_table = t;
    u.location_id = id;
    auto owner = usage_record(db, t, id);
    u.name = usage_name(owner) + " — " +
             (u.name == "Vínculo sem rótulo" ? label(table) : u.name);
    if (t == "relations")
      u.name =
          usage_name(usage_record(db, "entities", owner["source_entity_id"])) +
          " → " +
          usage_name(usage_record(db, "entities", owner["target_entity_id"])) +
          " — " + label(table);
    for (const auto *key : {"work_id", "structure_id", "axis_id"})
      if (r[key].empty())
        r[key] = owner[key];
  };
  if (table == "axis_calendars" || table == "calendar_adoptions") {
    locate("fictional_time_axes", r["axis_id"]);
    u.name += table == "axis_calendars"
                  ? " · calendário de origem"
                  : " · corte na ordem " + r["from_ordinal"];
    u.guidance = table == "axis_calendars"
                     ? "Configuração do eixo: desvincule o calendário. Pontos "
                       "permanecem em ordem relativa; datas autorais são "
                       "preservadas. Se houver sucessões, revise-as primeiro."
                     : "Configuração do eixo: revise as adoções. A remoção de "
                       "uma sucessão muda a exibição dos pontos, não suas "
                       "ordens nem as datas autorais.";
  } else if (table == "calendar_revisions") {
    locate("fictional_calendars", r["calendar_id"]);
    u.guidance =
        "Esta versão preserva sua origem. Preserve-a ou exclua primeiro a "
        "versão derivada; não é seguro apagar sua proveniência.";
  } else if (table == "temporal_attachments") {
    locate("temporal_references", r["reference_id"]);
    for (const auto &[key, t] : {std::pair{"entity_id", "entities"},
                                 {"relation_id", "relations"},
                                 {"unit_id", "narrative_units"},
                                 {"line_id", "narrative_lines"}})
      if (!r[key].empty())
        u.name += " → " + usage_name(usage_record(db, t, r[key]));
    u.guidance = "Abra Vínculos desta data e remova somente o vínculo "
                 "escolhido; a data e o item relacionado permanecem.";
  } else if (table.starts_with("semantic_") && !r["document_id"].empty()) {
    locate("documents", r["document_id"]);
    auto annotation = usage_record(db, "semantic_annotations",
                                   table == "semantic_annotation_relations"
                                       ? r["source_id"]
                                       : r["annotation_id"]);
    u.annotation_id = annotation["id"];
    auto anchor = usage_record(db, "document_anchors", annotation["anchor_id"]);
    if (!anchor["start_offset"].empty())
      u.text_offset =
          static_cast<std::size_t>(std::stoull(anchor["start_offset"]));
    u.name += " · trecho na posição " + std::to_string(u.text_offset + 1);
    u.guidance = "Abra o trecho e use suas marcações para desvincular a data, "
                 "entidade ou ponto temporal. O texto será preservado.";
  } else if (table == "narrative_realizations" || table == "narrative_text_targets" || table == "narrative_unit_presence_intents" || table == "narrative_unit_contexts" || table == "narrative_unit_relations") {
    locate("narrative_units", r["unit_id"]);
    u.guidance = "Abra o detalhe da unidade e retire somente a realização, relação em foco ou contexto de Local correspondente. Texto e fatos canônicos permanecem.";
  } else if (table == "document_text_references") {
    locate("documents", r["source_document_id"]);
    u.guidance =
        "Abra Referências textuais deste documento de origem e remova a "
        "referência escolhida; ambos os documentos serão preservados.";
  } else if (!r["document_id"].empty()) {
    locate("documents", r["document_id"]);
    auto anchor = usage_record(db, "document_anchors", r["anchor_id"]);
    if (!anchor["start_offset"].empty())
      u.text_offset =
          static_cast<std::size_t>(std::stoull(anchor["start_offset"]));
    u.guidance =
        "Abra o documento de origem e suas referências para desvincular o uso. "
        "Não apague o texto para resolver este vínculo.";
  } else if (table == "relation_contexts") {
    locate("relations", r["relation_id"]);
  } else if (table == "event_occurrences") {
    locate("entities", r["event_entity_id"]);
  } else if (table == "event_participations") {
    auto event =
        usage_record(db, "event_occurrences", r["event_occurrence_id"]);
    u.name =
        usage_name(usage_record(db, "entities", r["participant_entity_id"])) +
        " — " + u.name;
    locate("entities", event["event_entity_id"]);
    u.guidance = "Abra os participantes deste acontecimento e remova a "
                 "participação selecionada. As entidades serão preservadas.";
  } else if (table == "entity_presences" || table == "entity_work_scopes" ||
             table == "editorial_entity_references") {
    locate("entities", r["entity_id"]);
    if (table == "editorial_entity_references")
      u.name +=
          " · unidade " + usage_name(usage_record(db, "editorial_nodes",
                                                  r["editorial_node_id"]));
    if (table == "entity_presences")
      u.name +=
          " · Local " +
          usage_name(usage_record(db, "entities", r["location_entity_id"])) +
          " · " +
          usage_name(usage_record(db, "fictional_time_points",
                                  r["start_time_point_id"]));
  } else if (table == "narrative_unit_entities" ||
             table == "narrative_unit_lines") {
    locate("narrative_units", r["unit_id"]);
  } else if (table == "narrative_links") {
    locate("narrative_units", r["source_unit_id"]);
  } else if (table == "narrative_role_assignments") {
    u.location_table = "narrative_roles";
    u.location_id = r["role_id"];
    u.name = usage_name(usage_record(db, "entities", r["entity_id"])) + " — " +
             u.name;
  } else if (table == "structure_model_items" ||
             table == "structure_model_item_links") {
    locate("structure_models", r["model_id"]);
  } else if (table == "cartographic_positions") {
    u.location_table = "cartographic_positions";
    u.location_id = r["entity_id"];
    u.map_id = r["planet_id"];
    u.name = usage_name(usage_record(db, "entities", r["entity_id"])) + " — " +
             usage_name(usage_record(db, "cartographic_planets", u.map_id));
    u.guidance = "Cartografia: remova a posição deste Local no mapa indicado. "
                 "Não é necessário apagar o mapa.";
  } else if (table == "editorial_node_trash") {
    u.location_table.clear();
    u.location_id.clear();
    auto operation =
        db.prepare("SELECT id FROM project_operations WHERE "
                   "trash_operation_id=? ORDER BY sequence DESC LIMIT 1");
    operation.bind(1, r["operation_id"]);
    if (operation.step()) {
      u.location_table = "project_operations";
      u.location_id = operation.column_text(0);
    }
    u.guidance =
        u.location_id.empty()
            ? "Unidade preservada em exclusão antiga, anterior ao histórico "
              "reversível desta versão. Não há atalho seguro de restauração "
              "para este registro legado; preserve o tipo ou restaure a origem "
              "por uma cópia anterior. O auxiliar não purga a lixeira."
            : "Abra a exclusão correspondente no Histórico / restauração. "
              "Confira a reversão da ramificação; depois reclassifique a "
              "unidade restaurada. Nenhuma lixeira será purgada "
              "automaticamente.";
  }
  u.work_id = r["work_id"];
  u.structure_id = r["structure_id"];
  u.axis_id = r["axis_id"];
  if (u.location_table == "structure_models")
    u.model_layer =
        usage_record(db, "structure_models", u.location_id)["layer"];
  u.path = label(table);
  if (!u.work_id.empty())
    u.path += " · " + usage_name(usage_record(db, "works", u.work_id));
  if (!u.structure_id.empty()) {
    auto s = usage_record(db, "narrative_structures", u.structure_id);
    if (s.empty())
      s = usage_record(db, "editorial_structures", u.structure_id);
    u.path += " · " + usage_name(s);
    if (u.work_id.empty())
      u.work_id = s["work_id"];
  }
  if (!u.axis_id.empty())
    u.path +=
        " · " + usage_name(usage_record(db, "fictional_time_axes", u.axis_id));
  return u;
}
std::vector<project::RemovalUse> removal_uses(SqliteDatabase &db,
                                              project::RemovalKind kind,
                                              const std::string &id) {
  struct Edge {
    std::string table, column, target, action;
  };
  std::vector<Edge> edges;
  // Trash intentionally has no FK to live catalogues, but the structural-type
  // deletion trigger protects this retained provenance just like a live use.
  edges.push_back({"editorial_node_trash", "structural_type_id",
                   "structural_element_types", "RESTRICT"});
  if(kind==project::RemovalKind::DocumentAnchor)
    edges.push_back({"narrative_text_targets","anchor_id","document_anchors","RESTRICT"});
  auto tables = db.prepare(
      "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
  while (tables.step()) {
    const auto table = tables.column_text(0);
    if (excluded(table))
      continue;
    auto q = db.prepare("PRAGMA foreign_key_list(" + quote(table) + ")");
    while (q.step())
      if (q.column_text(4) == "id")
        edges.push_back(
            {table, q.column_text(3), q.column_text(2), q.column_text(6)});
  }
  std::vector<project::RemovalUse> result;
  if (kind == project::RemovalKind::EntityWorkScope) {
    // This incoming FK references a composite natural key, not the scope UUID.
    const auto scope = usage_record(db, "entity_work_scopes", id);
    const auto columns = usage_columns(db, "editorial_entity_references");
    auto q = db.prepare("SELECT " + usage_select(columns) +
                        " FROM editorial_entity_references WHERE entity_id=? "
                        "AND work_id=? ORDER BY id LIMIT 20001");
    q.bind(1, scope.at("entity_id"));
    q.bind(2, scope.at("work_id"));
    while (q.step()) {
      if (result.size() >= max_rows)
        throw std::runtime_error(
            "A inspeção excede 20.000 usos; a exclusão continua protegida.");
      result.push_back(usage_location(db, "editorial_entity_references",
                                      usage_row(q, columns)));
    }
  }
  std::set<std::string> visited, reported;
  std::vector<std::pair<std::string, std::string>> queue{{table_for(kind), id}};
  for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
    const auto [target, value] = queue[cursor];
    if (!visited.insert(target + ":" + value).second)
      continue;
    for (const auto &edge : edges) {
      if (edge.target != target)
        continue;
      const bool owned_revision = target == "fictional_calendars" &&
                                  edge.table == "calendar_revisions" &&
                                  edge.column == "calendar_id";
      const bool entity_use =
          target == "entities" && edge.table != "entity_aliases";
      const bool document_source =
          target == "documents" && (edge.column == "revision_of_id" ||
                                    edge.column == "target_document_id");
      const bool branch_child =
          edge.table == "editorial_nodes" && edge.column == "parent_id";
      const bool blocking =
          !owned_revision && !branch_child &&
          (entity_use || document_source ||
           (edge.action != "CASCADE" && edge.action != "SET NULL"));
      // Owned binary terrain/text metadata cannot have authorial blockers.
      if (!blocking && edge.table != "narrative_units" &&
          edge.table != "narrative_lines" && edge.table != "editorial_nodes")
        continue;
      const auto columns = usage_columns(db, edge.table);
      if (columns.empty())
        continue;
      auto q = db.prepare("SELECT " + usage_select(columns) + " FROM " +
                          quote(edge.table) + " WHERE " + quote(edge.column) +
                          "=? ORDER BY 1 LIMIT 20001");
      q.bind(1, value);
      std::size_t count{};
      while (q.step()) {
        if (++count > max_rows || result.size() + queue.size() > max_rows)
          throw std::runtime_error(
              "A inspeção de usos excede 20.000 itens. A exclusão continua "
              "protegida; reduza o escopo antes de repetir.");
        auto r = usage_row(q, columns);
        // Missing UUIDs (composite joins) remain distinct through all selected
        // fields.
        std::string key = edge.table;
        for (const auto &[k, v] : r)
          key += std::to_string(k.size()) + ":" + k + std::to_string(v.size()) +
                 ":" + v;
        if (blocking && reported.insert(key).second)
          result.push_back(usage_location(db, edge.table, r));
        if (!r["id"].empty() &&
            (!blocking || edge.table == "temporal_references"))
          queue.emplace_back(edge.table, r["id"]);
      }
    }
  }
  return result;
}
void guard_removal(SqliteDatabase &db, project::RemovalKind kind,
                   const std::string &id) {
  if (id.empty() || id.size() > 128)
    throw std::runtime_error("Identidade de remoção inválida");
  table_for(kind);
  if (kind == project::RemovalKind::DocumentAnchor ||
      kind == project::RemovalKind::TextAnnotation)
    throw std::runtime_error(
        "Esta marcação é editada na Escrita, com Desfazer edição. Resolva os "
        "usos abaixo e tente removê-la novamente pelas ferramentas do texto.");
  const auto table = table_for(kind);
  const bool builtin = kind == project::RemovalKind::EntityType ||
                       kind == project::RemovalKind::RelationType ||
                       kind == project::RemovalKind::StructuralType ||
                       kind == project::RemovalKind::StructureModel ||
                       kind == project::RemovalKind::NarrativeRole;
  const bool active = kind == project::RemovalKind::EditorialStructure ||
                      kind == project::RemovalKind::NarrativeStructure;
  if (builtin || active || kind == project::RemovalKind::TimeAxis) {
    const auto flag = builtin  ? "is_builtin"
                      : active ? "is_active"
                               : "is_default";
    auto q = db.prepare("SELECT " + quote(flag) + " FROM " + quote(table) +
                        " WHERE id=?");
    q.bind(1, id);
    if (!q.step())
      throw std::runtime_error("Item não encontrado");
    if (q.column_integer(0))
      throw std::runtime_error(
          builtin
              ? "Item interno do sistema é protegido; não pode ser removido."
          : active ? "Estrutura ativa é protegida. Ative outra estrutura antes "
                     "de remover esta."
                   : "O eixo principal é protegido e não pode ser removido.");
  }
  if (kind == project::RemovalKind::CalendarSystem) {
    std::string dependencies;
    for (const auto &[table, column, description] :
         {std::tuple{"axis_calendars", "calendar_id", "associações com eixos"},
          std::tuple{"calendar_adoptions", "calendar_id",
                     "adoções e sucessões"},
          std::tuple{"temporal_references", "calendar_id",
                     "datas e horários autorais"},
          std::tuple{"calendar_revisions", "parent_id", "revisões derivadas"}})
      if (uses(db, table, column, id))
        dependencies +=
            (dependencies.empty() ? "" : "; ") + std::string(description);
    if (!dependencies.empty())
      throw std::runtime_error(
          "Sistema temporal em uso: " + dependencies +
          ". Preserve ou desvincule esses usos antes de excluir. Nenhuma data "
          "será apagada ou convertida automaticamente; revisões derivadas "
          "devem ser preservadas ou excluídas primeiro.");
  }
  if (kind == project::RemovalKind::StructuralType &&
      uses(db, "editorial_node_trash", "structural_type_id", id))
    throw std::runtime_error(
        "Tipo estrutural preservado em unidades excluídas. Revise a exclusão "
        "no Histórico / restauração; nenhuma lixeira será purgada.");
  if (kind == project::RemovalKind::Document &&
      (uses(db, "document_text_references", "target_document_id", id) ||
       uses(db, "documents", "revision_of_id", id)))
    throw std::runtime_error(
        "Documento usado como fonte de revisão ou referência. Preserve a "
        "origem e desvincule os usos antes de removê-la.");
  if (kind == project::RemovalKind::Entity) {
    auto tables =
        db.prepare("SELECT name FROM sqlite_master WHERE type='table'");
    while (tables.step()) {
      const auto name = tables.column_text(0);
      // An alias is owned metadata of this identity, not an external use.
      // Its cascade is included in the preview and reversible changeset.
      if (excluded(name) || name == "entity_aliases")
        continue;
      auto keys = db.prepare("PRAGMA foreign_key_list(" + quote(name) + ")");
      while (keys.step())
        if (keys.column_text(2) == "entities" && keys.column_text(4) == "id" &&
            uses(db, name, keys.column_text(3), id))
          throw std::runtime_error(
              "Entidade em uso: " + label(name) +
              ". Desvincule esses usos antes de remover; nenhum vínculo será "
              "apagado silenciosamente.");
    }
  }
  if (kind == project::RemovalKind::Work &&
      (uses(db, "editorial_nodes", "work_id", id) ||
       uses(db, "entity_work_scopes", "work_id", id)))
    throw std::runtime_error("Obra com unidades ou vínculos de entidades; "
                             "remova/desvincule esses usos primeiro");
  if (kind == project::RemovalKind::IntellectualProperty &&
      uses(db, "works", "intellectual_property_id", id))
    throw std::runtime_error(
        "Universo com Obras; preserve ou desvincule as Obras primeiro");
}
void remove(SqliteDatabase &db, project::RemovalKind kind,
            const std::string &id, const std::string &trash_id = {},
            const std::string &observed_at = {}) {
  guard_removal(db, kind, id);
  if (kind == project::RemovalKind::EditorialStructure) {
    auto nodes = db.prepare("DELETE FROM editorial_nodes WHERE structure_id=?");
    nodes.bind(1, id);
    nodes.run();
  }
  if (kind == project::RemovalKind::CalendarSystem) {
    // A leaf revision owns this provenance edge, not its parent definition.
    auto revision =
        db.prepare("DELETE FROM calendar_revisions WHERE calendar_id=?");
    revision.bind(1, id);
    revision.run();
  }
  if (kind == project::RemovalKind::EditorialBranch) {
    if (trash_id.empty() || trash_id.size() > 128 || observed_at.empty() ||
        observed_at.size() > 64)
      throw std::runtime_error("Identificação da exclusão estrutural inválida");
    const std::string tree =
        "WITH RECURSIVE branch(id) AS (SELECT id FROM editorial_nodes WHERE "
        "id=? UNION SELECT n.id FROM editorial_nodes n JOIN branch b ON "
        "n.parent_id=b.id) ";
    auto count = db.prepare(tree + "SELECT count(*) FROM branch");
    count.bind(1, id);
    if (!count.step() || count.column_integer(0) == 0)
      throw std::runtime_error("Ramificação não encontrada");
    if (count.column_integer(0) > static_cast<std::int64_t>(max_rows))
      throw std::runtime_error("Ramificação excessiva");
    auto operation = db.prepare(
        "INSERT INTO editorial_trash_operations(id,created_at) VALUES(?,?)");
    operation.bind(1, trash_id);
    operation.bind(2, observed_at);
    operation.run();
    auto archive = db.prepare(
        tree +
        "INSERT INTO "
        "editorial_node_trash(operation_id,id,work_id,parent_id,type,title,"
        "subtitle,synopsis,custom_type_name,status,position,created_at,updated_"
        "at,structural_type_id,designator,structure_id) SELECT "
        "?,id,work_id,parent_id,type,title,subtitle,synopsis,custom_type_name,"
        "status,position,created_at,updated_at,structural_type_id,designator,"
        "structure_id FROM editorial_nodes WHERE id IN (SELECT id FROM "
        "branch)");
    archive.bind(1, id);
    archive.bind(2, trash_id);
    archive.run();
    auto deletion = db.prepare(
        tree +
        "DELETE FROM editorial_nodes WHERE id IN (SELECT id FROM branch)");
    deletion.bind(1, id);
    deletion.run();
    return;
  }
  auto s = db.prepare("DELETE FROM " + quote(table_for(kind)) + " WHERE id=?");
  s.bind(1, id);
  s.run();
  if (db.changes() != 1)
    throw std::runtime_error("Registro de origem não encontrado");
}
struct Stored {
  std::string label;
  Bytes bytes;
};
Stored load(SqliteDatabase &db, const std::string &id) {
  auto s = db.prepare(
      "SELECT label,changes,sha256,state,capture_project_id,is_compensation "
      "FROM project_operations WHERE project_id=? AND id=?");
  s.bind(1, owner(db));
  s.bind(2, id);
  if (!s.step())
    throw std::runtime_error("Operação não encontrada");
  if (s.column_text(3) != "applied")
    throw std::runtime_error("Esta operação já foi revertida");
  if (s.column_text(4) != owner(db))
    throw std::runtime_error("Histórico herdado de outro Projeto; a cópia não "
                             "herda autorização de reversão");
  if (s.column_integer(5))
    throw std::runtime_error("Reversão compensatória registrada; refazer "
                             "automático não faz parte deste corte");
  Stored result{s.column_text(0), s.column_blob(1)};
  if (hash(result.bytes) != s.column_text(2))
    throw std::runtime_error(
        "Hash do histórico incompatível; reversão bloqueada");
  return result;
}
Bytes inverse(sqlite3 *db, const Bytes &bytes) {
  inspect(db, bytes);
  int size{};
  void *data{};
  check(sqlite3changeset_invert(bytes.size(), bytes.data(), &size, &data));
  std::unique_ptr<void, decltype(&sqlite3_free)> release(data, sqlite3_free);
  if (size <= 0 || static_cast<std::size_t>(size) > max_changes)
    throw std::runtime_error("Inverso excessivo/inválido");
  const auto *p = static_cast<const std::uint8_t *>(data);
  return {p, p + size};
}
int local_cascade_conflict(void *, int conflict, sqlite3_changeset_iter *it) {
  // All old values were verified before apply, under the same isolated/write
  // transaction. Only an intra-apply cascade may have changed them. A final
  // exact effect comparison rejects every unplanned cascade/trigger effect.
  const char *table{};
  int columns{}, op{}, indirect{};
  if (sqlite3changeset_op(it, &table, &columns, &op, &indirect) != SQLITE_OK)
    return SQLITE_CHANGESET_ABORT;
  if (conflict == SQLITE_CHANGESET_NOTFOUND && op == SQLITE_DELETE)
    return SQLITE_CHANGESET_OMIT;
  if (conflict == SQLITE_CHANGESET_DATA &&
      (op == SQLITE_UPDATE || op == SQLITE_DELETE))
    return SQLITE_CHANGESET_REPLACE;
  return SQLITE_CHANGESET_ABORT;
}
void apply_inverse(sqlite3 *raw, SqliteDatabase &db, const Bytes &bytes) {
  const auto expected = inspect(raw, bytes, true);
  OperationCapture capture(db, "Conferência interna");
  db.execute("PRAGMA defer_foreign_keys=ON");
  Iterator pending(bytes);
  while (pending.next()) {
    const char *table{};
    int columns{}, op{}, indirect{};
    check(sqlite3changeset_op(pending.p, &table, &columns, &op, &indirect));
    const std::string name = table ? table : "";
    if (op != SQLITE_UPDATE ||
        (name != "editorial_structures" && name != "narrative_structures"))
      continue;
    auto names = db.prepare("PRAGMA table_info(" + quote(name) + ")");
    int c{};
    while (names.step()) {
      if (names.column_text(1) == "is_active") {
        sqlite3_value *v{}, *id{};
        check(sqlite3changeset_new(pending.p, c, &v));
        check(sqlite3changeset_old(pending.p, 0, &id));
        if (v && sqlite3_value_type(v) == SQLITE_INTEGER &&
            sqlite3_value_int64(v) == 0) {
          Query q(raw, "UPDATE " + quote(name) + " SET is_active=0 WHERE id=?");
          check(sqlite3_bind_value(q.p, 1, id));
          const int r = sqlite3_step(q.p);
          if (r != SQLITE_DONE)
            check(r);
        }
      }
      ++c;
    }
  }
  check(sqlite3changeset_apply(raw, bytes.size(),
                               const_cast<std::uint8_t *>(bytes.data()),
                               nullptr, local_cascade_conflict, nullptr));
  integrity(db);
  if (inspect(raw, capture.changes()).fingerprint != expected.fingerprint)
    throw std::runtime_error("Há novos usos ou efeitos não previstos. Reversão "
                             "bloqueada; nada foi confirmado.");
}
} // namespace

void validate_project_removal(SqliteDatabase &db, project::RemovalKind kind,
                              const std::string &id) {
  guard_removal(db, kind, id);
}

OperationCapture::OperationCapture(SqliteDatabase &db, std::string label,
                                   bool compensation)
    : database_(db), label_(std::move(label)), compensation_(compensation) {
  owner(db);
  if (label_.empty() || label_.size() > 512)
    throw std::runtime_error("Identificação de operação inválida");
  check(sqlite3session_create(db.database_, "main", &session_));
  try {
    auto tables = db.prepare(
        "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
    while (tables.step()) {
      auto name = tables.column_text(0);
      if (!excluded(name))
        check(sqlite3session_attach(session_, name.c_str()));
    }
  } catch (...) {
    sqlite3session_delete(session_);
    session_ = nullptr;
    throw;
  }
}
OperationCapture::~OperationCapture() { sqlite3session_delete(session_); }
Bytes OperationCapture::changes() const { return session_changes(session_); }
void OperationCapture::persist() {
  const auto bytes = changes();
  if (bytes.empty())
    return;
  const auto observed = inspect(database_.database_, bytes);
  std::string description = short_text(label_), subject = observed.subject;
  if (label_ == "Cadastro e vínculos narrativos" ||
      label_ == "Obras e universos" || label_ == "Estruturas do Projeto" ||
      label_ == "Organização editorial" || label_ == "Tempo e ocorrências") {
    for (const auto *table : {"works",
                              "intellectual_properties",
                              "entities",
                              "relations",
                              "entity_types",
                              "relation_types",
                              "structure_models",
                              "editorial_structures",
                              "narrative_structures",
                              "editorial_nodes",
                              "fictional_time_axes",
                              "fictional_time_points",
                              "event_occurrences",
                              "event_participations",
                              "narrative_units",
                              "narrative_lines",
                              "entity_work_scopes",
                              "editorial_entity_references",
                              "narrative_links",
                              "narrative_roles",
                              "narrative_role_assignments",
                              "narrative_unit_entities",
                              "narrative_unit_lines"}) {
      const auto found =
          std::find_if(observed.impacts.begin(), observed.impacts.end(),
                       [&](const auto &i) { return i.table == table; });
      if (found == observed.impacts.end())
        continue;
      const auto &impact = *found;
      const std::string verb =
          impact.removed && !impact.inserted && !impact.updated   ? "Remover"
          : impact.inserted && !impact.removed && !impact.updated ? "Criar"
                                                                  : "Atualizar";
      description = verb + " " + impact.label;
      if (!impact.examples.empty())
        subject = impact.examples.front();
      break;
    }
  }
  auto s = database_.prepare(
      "INSERT INTO "
      "project_operations(id,project_id,capture_project_id,label,created_at,"
      "changes,sha256,is_compensation,trash_operation_id) "
      "VALUES(?,?,?,?,?,?,?,?,?)");
  s.bind(1, project::new_uuid());
  const auto id = owner(database_);
  s.bind(2, id);
  s.bind(3, id);
  // Only the display label is abbreviated, never canonical effects/values.
  s.bind(4, short_text(description + (subject.empty() ? "" : " — " + subject)));
  s.bind(5, project::utc_now());
  s.bind_blob(6, bytes);
  s.bind(7, hash(bytes));
  s.bind(8, std::int64_t{compensation_ ? 1 : 0});
  std::string trash;
  Iterator changeset(bytes);
  while (changeset.next()) {
    const char *table{};
    int columns{}, op{}, indirect{};
    check(sqlite3changeset_op(changeset.p, &table, &columns, &op, &indirect));
    if (std::string(table) == "editorial_trash_operations" &&
        op == SQLITE_INSERT) {
      sqlite3_value *id{};
      check(sqlite3changeset_new(changeset.p, 1, &id));
      if (id && sqlite3_value_type(id) == SQLITE_TEXT)
        trash = reinterpret_cast<const char *>(sqlite3_value_text(id));
    }
  }
  if (trash.empty())
    s.bind_null(9);
  else
    s.bind(9, trash);
  s.run();
}
std::vector<project::ProjectOperation>
ProjectOperationStore::list(const std::filesystem::path &path,
                            std::size_t offset) const {
  if (offset > 1000000)
    throw std::runtime_error("Página do histórico excessiva");
  auto db = open(path);
  auto s = db.prepare("SELECT "
                      "id,label,created_at,state,capture_project_id,length("
                      "changes),is_compensation FROM project_operations WHERE "
                      "project_id=? ORDER BY sequence DESC LIMIT 25 OFFSET ?");
  s.bind(1, owner(db));
  s.bind(2, static_cast<std::int64_t>(offset));
  std::vector<project::ProjectOperation> result;
  while (s.step())
    result.push_back({s.column_text(0), s.column_text(1), s.column_text(2),
                      s.column_text(3), s.column_text(4),
                      static_cast<std::size_t>(s.column_integer(5)),
                      s.column_integer(6) != 0});
  return result;
}
project::OperationPreview
ProjectOperationStore::preview_removal(const std::filesystem::path &path,
                                       project::RemovalKind kind,
                                       const std::string &id) const {
  project::OperationPreview result;
  result.target_id = id;
  result.removal = kind;
  result.observed_at = project::utc_now();
  if (kind == project::RemovalKind::EditorialBranch)
    result.operation_id = project::new_uuid();
  result.description = removal_label(kind);
  try {
    auto db = open(path);
    SqliteTransaction read(db, SqliteTransaction::Mode::Deferred);
    result.project_id = owner(db);
    const auto record = usage_record(db, table_for(kind), id);
    if (record.empty())
      throw std::runtime_error("Item não encontrado; atualize a tela.");
    const auto target = kind == project::RemovalKind::Relation
                            ? std::string{}
                            : short_text(usage_name(record));
    if (!target.empty())
      result.description += " — " + target;
    result.uses = removal_uses(db, kind, id);
    SqliteDatabase simulation(":memory:");
    copy(db.database_, simulation.database_);
    SqliteTransaction tx(simulation);
    OperationCapture capture(simulation, result.description);
    remove(simulation, kind, id, result.operation_id, result.observed_at);
    integrity(simulation);
    const auto effect = inspect(simulation.database_, capture.changes());
    result.fingerprint = effect.fingerprint;
    result.impacts = effect.impacts;
    if (target.empty() && !effect.subject.empty())
      result.description += " — " + effect.subject;
    result.allowed = true;
  } catch (const std::exception &e) {
    result.blocked_reason = e.what();
    if (const auto *sql = dynamic_cast<const SqliteError *>(&e);
        sql && sql->extended_code() == SQLITE_CONSTRAINT_FOREIGNKEY)
      result.blocked_reason = "Este item possui vínculos autorais que precisam "
                              "ser resolvidos antes da exclusão.";
    if (!result.uses.empty())
      result.blocked_reason += "\nConsulte os usos abaixo. Nada foi removido; "
                               "resolva cada vínculo e atualize a prévia.";
  }
  return result;
}
project::OperationPreview
ProjectOperationStore::preview_reversal(const std::filesystem::path &path,
                                        const std::string &id) const {
  project::OperationPreview result;
  result.operation_id = id;
  result.reversing = true;
  result.description = "Reverter operação do Projeto";
  try {
    auto db = open(path);
    SqliteTransaction read(db, SqliteTransaction::Mode::Deferred);
    result.project_id = owner(db);
    const auto stored = load(db, id);
    result.description = "Reverter: " + stored.label;
    SqliteDatabase simulation(":memory:");
    copy(db.database_, simulation.database_);
    SqliteTransaction tx(simulation);
    const auto bytes = inverse(simulation.database_, stored.bytes);
    const auto effects = inspect(simulation.database_, bytes);
    apply_inverse(simulation.database_, simulation, bytes);
    result.fingerprint = effects.fingerprint;
    result.impacts = effects.impacts;
    result.allowed = true;
  } catch (const std::exception &e) {
    result.blocked_reason = e.what();
  }
  return result;
}
void ProjectOperationStore::execute(
    const std::filesystem::path &path,
    const project::OperationPreview &preview) const {
  if (!preview.allowed || preview.fingerprint.empty())
    throw std::runtime_error(
        "Prepare e confira uma prévia válida antes de confirmar");
  auto db = open(path);
  SqliteTransaction tx(db);
  if (owner(db) != preview.project_id)
    throw std::runtime_error("O Projeto mudou; prepare outra prévia");
  OperationCapture verification(db, "Conferência interna");
  if (preview.reversing) {
    const auto stored = load(db, preview.operation_id);
    tx.record_project_operation(short_text("Reverter: " + stored.label), true);
    const auto bytes = inverse(db.database_, stored.bytes);
    apply_inverse(db.database_, db, bytes);
    auto s = db.prepare(
        "UPDATE project_operations SET state='reverted',reversed_at=? WHERE "
        "id=? AND project_id=? AND state='applied'");
    s.bind(1, project::utc_now());
    s.bind(2, preview.operation_id);
    s.bind(3, preview.project_id);
    s.run();
    if (db.changes() != 1)
      throw std::runtime_error("O estado da operação mudou");
  } else {
    tx.record_project_operation(removal_label(preview.removal));
    remove(db, preview.removal, preview.target_id, preview.operation_id,
           preview.observed_at);
  }
  integrity(db);
  if (inspect(db.database_, verification.changes()).fingerprint !=
      preview.fingerprint)
    throw std::runtime_error("O impacto mudou desde a prévia; nada foi "
                             "confirmado. Prepare outra prévia.");
  tx.commit();
}
} // namespace inde::persistence

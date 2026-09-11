#include "inde/persistence/sqlite_writing_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <limits>
#include <stdexcept>

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
    throw std::runtime_error("O banco contém mais de um projeto");
  return result;
}

std::int64_t checked_limit(std::size_t value) {
  if (value == 0 || value > 500)
    throw std::runtime_error("Limite da consulta de Documentos inválido");
  return static_cast<std::int64_t>(value);
}

std::int64_t checked_offset(std::size_t value) {
  if (value >
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
    throw std::runtime_error("Deslocamento da consulta de Documentos inválido");
  return static_cast<std::int64_t>(value);
}

std::int64_t checked_size(std::size_t value, const char *label) {
  if (value >
      static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
    throw std::runtime_error(std::string(label) + " excede o limite do banco");
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

void validate_query(const DocumentQuery &query) {
  if (query.editorial_node_id && query.editorial_node_id->empty())
    throw std::runtime_error("Filtro editorial de Documentos inválido");
  if (query.entity_id && query.entity_id->empty())
    throw std::runtime_error("Filtro de entidade dos Documentos inválido");
  if (query.group_id && query.group_id->empty())
    throw std::runtime_error("Filtro de grupo dos Documentos inválido");
  if (query.perspective.size() > 256)
    throw std::runtime_error("Filtro de perspectiva dos Documentos inválido");
  static_cast<void>(checked_limit(query.limit));
  static_cast<void>(checked_offset(query.offset));
}

std::string where_clause(const DocumentQuery &query) {
  std::string sql{" WHERE d.project_id = ?"};
  if (!query.search.empty())
    sql += " AND (d.title LIKE ? ESCAPE '\\' COLLATE NOCASE OR "
           "d.content LIKE ? ESCAPE '\\' COLLATE NOCASE)";
  if (query.editorial_node_id)
    sql += " AND d.editorial_node_id = ?";
  if (query.entity_id)
    sql += " AND EXISTS (SELECT 1 FROM document_entity_references reference "
           "WHERE reference.project_id = d.project_id "
           "AND reference.document_id = d.id AND reference.entity_id = ?)";
  if (query.group_id)
    sql += " AND d.group_id = ?";
  else if (query.ungrouped_only)
    sql += " AND d.group_id IS NULL";
  if (query.purpose)
    sql += " AND d.purpose = ?";
  if (!query.perspective.empty())
    sql += " AND d.perspective LIKE ? ESCAPE '\\' COLLATE NOCASE";
  if (query.revisions_only)
    sql += " AND d.revision_of_id IS NOT NULL";
  return sql;
}

int bind_query(SqliteStatement &statement, int parameter,
               const std::string &owner, const DocumentQuery &query) {
  statement.bind(parameter++, owner);
  if (!query.search.empty()) {
    const auto pattern = like_pattern(query.search);
    statement.bind(parameter++, pattern);
    statement.bind(parameter++, pattern);
  }
  if (query.editorial_node_id)
    statement.bind(parameter++, *query.editorial_node_id);
  if (query.entity_id)
    statement.bind(parameter++, *query.entity_id);
  if (query.group_id)
    statement.bind(parameter++, *query.group_id);
  if (query.purpose)
    statement.bind(parameter++, project::to_string(*query.purpose));
  if (!query.perspective.empty())
    statement.bind(parameter++, like_pattern(query.perspective));
  return parameter;
}

project::Document read_document(SqliteStatement &statement) {
  project::Document value{
      statement.column_text(0),
      statement.column_is_null(1)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(1)},
      statement.column_text(2),
      statement.column_text(3),
      statement.column_text(4),
      statement.column_text(5),
      std::nullopt,
      {},
      {},
      {},
      std::nullopt,
      project::DocumentPurpose::MainText,
      std::nullopt,
      {},
      {}};
  if (!statement.column_is_null(6)) {
    const auto goal = statement.column_integer(6);
    if (goal <= 0)
      throw std::runtime_error("Meta de palavras inválida no banco");
    value.word_goal = static_cast<std::size_t>(goal);
  }
  value.group_id = statement.column_is_null(7)
                       ? std::nullopt
                       : std::optional<std::string>{statement.column_text(7)};
  value.purpose =
      project::document_purpose_from_string(statement.column_text(8));
  value.revision_of_id =
      statement.column_is_null(9)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(9)};
  value.revision_label = statement.column_text(10);
  value.perspective = statement.column_text(11);
  return value;
}

void load_document_layers(SqliteDatabase &database, const std::string &owner,
                          project::Document &value) {
  auto formatting = database.prepare(
      "SELECT style, start_offset, end_offset FROM document_format_spans "
      "WHERE project_id = ? AND document_id = ? "
      "ORDER BY start_offset, end_offset, style");
  formatting.bind(1, owner);
  formatting.bind(2, value.id);
  while (formatting.step()) {
    const auto start = formatting.column_integer(1);
    const auto end = formatting.column_integer(2);
    if (start < 0 || end < 0)
      throw std::runtime_error("Intervalo de formatação negativo no banco");
    value.formatting.push_back(
        {project::document_text_style_from_string(formatting.column_text(0)),
         static_cast<std::size_t>(start), static_cast<std::size_t>(end)});
  }

  auto anchors = database.prepare(
      "SELECT id, label, start_offset, end_offset FROM document_anchors "
      "WHERE project_id = ? AND document_id = ? "
      "ORDER BY start_offset, end_offset, label COLLATE NOCASE, id");
  anchors.bind(1, owner);
  anchors.bind(2, value.id);
  while (anchors.step()) {
    const auto start = anchors.column_integer(2);
    const auto end = anchors.column_integer(3);
    if (start < 0 || end < 0)
      throw std::runtime_error("Intervalo de âncora negativo no banco");
    value.anchors.push_back({anchors.column_text(0), anchors.column_text(1),
                             static_cast<std::size_t>(start),
                             static_cast<std::size_t>(end)});
  }

  auto references = database.prepare("SELECT id, entity_id, anchor_id, notes "
                                     "FROM document_entity_references "
                                     "WHERE project_id = ? AND document_id = ? "
                                     "ORDER BY entity_id, anchor_id, id");
  references.bind(1, owner);
  references.bind(2, value.id);
  while (references.step()) {
    value.entity_references.push_back(
        {references.column_text(0), references.column_text(1),
         references.column_is_null(2)
             ? std::nullopt
             : std::optional<std::string>{references.column_text(2)},
         references.column_text(3)});
  }
  project::validate(value);
}

project::DocumentSearchMatch read_match(SqliteStatement &statement,
                                        int title_column, int content_column) {
  const bool title = statement.column_integer(title_column) != 0;
  const bool content = statement.column_integer(content_column) != 0;
  if (title && content)
    return project::DocumentSearchMatch::NameAndContent;
  if (title)
    return project::DocumentSearchMatch::Name;
  if (content)
    return project::DocumentSearchMatch::Content;
  return project::DocumentSearchMatch::None;
}

void record_change(SqliteDatabase &database, const std::string &owner,
                   const std::string &command) {
  auto log = database.prepare(
      "INSERT INTO change_log(id, project_id, command_name, system_created_at) "
      "VALUES (?, ?, ?, ?)");
  log.bind(1, project::new_uuid());
  log.bind(2, owner);
  log.bind(3, command);
  log.bind(4, project::utc_now());
  log.run();
}

} // namespace

void SqliteWritingRepository::initialize(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  SchemaMigrator{}.migrate(database);
}

std::vector<project::Document>
SqliteWritingRepository::documents(const std::filesystem::path &project_path,
                                   const DocumentQuery &query) const {
  validate_query(query);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT d.id, d.editorial_node_id, d.title, d.content, d.created_at, "
      "d.updated_at, d.word_goal, d.group_id, d.purpose, d.revision_of_id, "
      "d.revision_label, d.perspective FROM documents d" +
      where_clause(query) +
      " ORDER BY d.updated_at DESC, d.title COLLATE NOCASE, d.id "
      "LIMIT ? OFFSET ?");
  auto parameter = bind_query(statement, 1, owner, query);
  statement.bind(parameter++, checked_limit(query.limit));
  statement.bind(parameter, checked_offset(query.offset));
  std::vector<project::Document> result;
  while (statement.step()) {
    auto value = read_document(statement);
    load_document_layers(database, owner, value);
    result.push_back(std::move(value));
  }
  return result;
}

std::vector<project::DocumentSummary>
SqliteWritingRepository::document_summaries(
    const std::filesystem::path &project_path,
    const DocumentQuery &query) const {
  validate_query(query);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  std::string matches = "0, 0";
  if (!query.search.empty())
    matches = "d.title LIKE ? ESCAPE '\\' COLLATE NOCASE, "
              "d.content LIKE ? ESCAPE '\\' COLLATE NOCASE";
  auto statement = database.prepare(
      "SELECT d.id, d.editorial_node_id, d.title, length(d.content), "
      "d.created_at, d.updated_at, "
      "(SELECT count(DISTINCT reference.entity_id) "
      " FROM document_entity_references reference "
      " WHERE reference.project_id = d.project_id "
      " AND reference.document_id = d.id), " +
      matches +
      ", d.group_id, d.purpose, d.revision_of_id, "
      "d.revision_label, d.perspective FROM documents d" +
      where_clause(query) +
      (query.search.empty()
           ? " ORDER BY d.updated_at DESC, d.title COLLATE NOCASE, d.id "
           : " ORDER BY 8 DESC, d.updated_at DESC, d.title COLLATE NOCASE, "
             "d.id ") +
      "LIMIT ? OFFSET ?");
  int parameter = 1;
  if (!query.search.empty()) {
    const auto pattern = like_pattern(query.search);
    statement.bind(parameter++, pattern);
    statement.bind(parameter++, pattern);
  }
  parameter = bind_query(statement, parameter, owner, query);
  statement.bind(parameter++, checked_limit(query.limit));
  statement.bind(parameter, checked_offset(query.offset));
  std::vector<project::DocumentSummary> result;
  while (statement.step()) {
    const auto characters = statement.column_integer(3);
    const auto references = statement.column_integer(6);
    if (characters < 0 || references < 0)
      throw std::runtime_error("Resumo de Documento inválido no banco");
    result.push_back(
        {statement.column_text(0),
         statement.column_is_null(1)
             ? std::nullopt
             : std::optional<std::string>{statement.column_text(1)},
         statement.column_text(2), static_cast<std::size_t>(characters),
         statement.column_text(4), statement.column_text(5),
         static_cast<std::size_t>(references), read_match(statement, 7, 8),
         statement.column_is_null(9)
             ? std::nullopt
             : std::optional<std::string>{statement.column_text(9)},
         project::document_purpose_from_string(statement.column_text(10)),
         statement.column_is_null(11)
             ? std::nullopt
             : std::optional<std::string>{statement.column_text(11)},
         statement.column_text(12), statement.column_text(13)});
  }
  return result;
}

std::size_t SqliteWritingRepository::document_count(
    const std::filesystem::path &project_path,
    const DocumentQuery &query) const {
  validate_query(query);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare("SELECT count(*) FROM documents d" +
                                    where_clause(query));
  static_cast<void>(bind_query(statement, 1, owner, query));
  if (!statement.step())
    throw std::runtime_error("Não foi possível contar os Documentos");
  return static_cast<std::size_t>(statement.column_integer(0));
}

std::optional<project::Document>
SqliteWritingRepository::document(const std::filesystem::path &project_path,
                                  const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto statement = database.prepare(
      "SELECT id, editorial_node_id, title, content, created_at, updated_at, "
      "word_goal, group_id, purpose, revision_of_id, revision_label, "
      "perspective "
      "FROM documents WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  auto result = read_document(statement);
  if (statement.step())
    throw std::runtime_error("Documento duplicado no banco");
  load_document_layers(database, owner, result);
  return result;
}

bool SqliteWritingRepository::referenced_entity_exists(
    const std::filesystem::path &project_path,
    const std::string &entity_id) const {
  auto database = open_database(project_path);
  auto statement = database.prepare(
      "SELECT count(*) FROM entities WHERE project_id = ? AND id = ?");
  statement.bind(1, project_id(database));
  statement.bind(2, entity_id);
  return statement.step() && statement.column_integer(0) == 1;
}

std::vector<project::DocumentGroup> SqliteWritingRepository::document_groups(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  auto statement =
      database.prepare("SELECT id, name, description, created_at, updated_at "
                       "FROM document_groups WHERE project_id = ? "
                       "ORDER BY name COLLATE NOCASE, id");
  statement.bind(1, project_id(database));
  std::vector<project::DocumentGroup> result;
  while (statement.step()) {
    project::DocumentGroup value{
        statement.column_text(0), statement.column_text(1),
        statement.column_text(2), statement.column_text(3),
        statement.column_text(4)};
    project::validate(value);
    result.push_back(std::move(value));
  }
  return result;
}

std::optional<project::DocumentGroup> SqliteWritingRepository::document_group(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  auto statement =
      database.prepare("SELECT id, name, description, created_at, updated_at "
                       "FROM document_groups WHERE project_id = ? AND id = ?");
  statement.bind(1, project_id(database));
  statement.bind(2, id);
  if (!statement.step())
    return std::nullopt;
  project::DocumentGroup result{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4)};
  project::validate(result);
  return result;
}

void SqliteWritingRepository::save_group(
    const std::filesystem::path &project_path,
    const project::DocumentGroup &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement = database.prepare(
      "INSERT INTO document_groups(id, project_id, name, description, "
      "created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
      "name=excluded.name, description=excluded.description, "
      "updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  statement.bind(3, value.name);
  statement.bind(4, value.description);
  statement.bind(5, value.created_at);
  statement.bind(6, value.updated_at);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Não foi possível salvar o grupo documental");
  record_change(database, owner, "save_document_group");
  transaction.commit();
}

void SqliteWritingRepository::remove_group(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement = database.prepare(
      "DELETE FROM document_groups WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Grupo documental não encontrado");
  record_change(database, owner, "delete_document_group");
  transaction.commit();
}

void SqliteWritingRepository::save(const std::filesystem::path &project_path,
                                   const project::Document &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  auto exists = database.prepare(
      "SELECT count(*) FROM documents WHERE project_id = ? AND id = ?");
  exists.bind(1, owner);
  exists.bind(2, value.id);
  if (!exists.step())
    throw std::runtime_error("Não foi possível verificar o Documento");
  const bool updating = exists.column_integer(0) == 1;

  SqliteTransaction transaction(database);
  auto statement = database.prepare(
      "INSERT INTO documents(id, project_id, editorial_node_id, title, "
      "content, created_at, updated_at, word_goal, group_id, purpose, "
      "revision_of_id, revision_label, perspective) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET "
      "editorial_node_id=excluded.editorial_node_id, title=excluded.title, "
      "content=excluded.content, updated_at=excluded.updated_at, "
      "word_goal=excluded.word_goal, group_id=excluded.group_id, "
      "purpose=excluded.purpose, revision_of_id=excluded.revision_of_id, "
      "revision_label=excluded.revision_label, "
      "perspective=excluded.perspective");
  statement.bind(1, value.id);
  statement.bind(2, owner);
  if (value.editorial_node_id)
    statement.bind(3, *value.editorial_node_id);
  else
    statement.bind_null(3);
  statement.bind(4, value.title);
  statement.bind(5, value.content);
  statement.bind(6, value.created_at);
  statement.bind(7, value.updated_at);
  if (value.word_goal)
    statement.bind(8, checked_size(*value.word_goal, "A meta de palavras"));
  else
    statement.bind_null(8);
  if (value.group_id)
    statement.bind(9, *value.group_id);
  else
    statement.bind_null(9);
  statement.bind(10, project::to_string(value.purpose));
  if (value.revision_of_id)
    statement.bind(11, *value.revision_of_id);
  else
    statement.bind_null(11);
  statement.bind(12, value.revision_label);
  statement.bind(13, value.perspective);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Não foi possível salvar o Documento");

  for (const char *table : {"document_entity_references", "document_anchors",
                            "document_format_spans"}) {
    auto clear = database.prepare("DELETE FROM " + std::string(table) +
                                  " WHERE project_id = ? AND document_id = ?");
    clear.bind(1, owner);
    clear.bind(2, value.id);
    clear.run();
  }

  auto format = database.prepare(
      "INSERT INTO document_format_spans(project_id, document_id, style, "
      "start_offset, end_offset) VALUES (?, ?, ?, ?, ?)");
  for (const auto &span : value.formatting) {
    format.bind(1, owner);
    format.bind(2, value.id);
    format.bind(3, project::to_string(span.style));
    format.bind(4, checked_size(span.start_offset, "Início da formatação"));
    format.bind(5, checked_size(span.end_offset, "Fim da formatação"));
    format.run();
    format.reset();
  }

  auto anchor = database.prepare(
      "INSERT INTO document_anchors(id, project_id, document_id, label, "
      "start_offset, end_offset) VALUES (?, ?, ?, ?, ?, ?)");
  for (const auto &value_anchor : value.anchors) {
    anchor.bind(1, value_anchor.id);
    anchor.bind(2, owner);
    anchor.bind(3, value.id);
    anchor.bind(4, value_anchor.label);
    anchor.bind(5, checked_size(value_anchor.start_offset, "Início da âncora"));
    anchor.bind(6, checked_size(value_anchor.end_offset, "Fim da âncora"));
    anchor.run();
    anchor.reset();
  }

  auto reference = database.prepare(
      "INSERT INTO document_entity_references(id, project_id, document_id, "
      "entity_id, anchor_id, notes) VALUES (?, ?, ?, ?, ?, ?)");
  for (const auto &value_reference : value.entity_references) {
    reference.bind(1, value_reference.id);
    reference.bind(2, owner);
    reference.bind(3, value.id);
    reference.bind(4, value_reference.entity_id);
    if (value_reference.anchor_id)
      reference.bind(5, *value_reference.anchor_id);
    else
      reference.bind_null(5);
    reference.bind(6, value_reference.notes);
    reference.run();
    reference.reset();
  }

  record_change(database, owner,
                updating ? "update_document" : "create_document");
  transaction.commit();
}

void SqliteWritingRepository::remove(const std::filesystem::path &project_path,
                                     const std::string &id) const {
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto statement =
      database.prepare("DELETE FROM documents WHERE project_id = ? AND id = ?");
  statement.bind(1, owner);
  statement.bind(2, id);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Documento não encontrado");
  record_change(database, owner, "delete_document");
  transaction.commit();
}

} // namespace inde::persistence

#include "inde/persistence/sqlite_structural_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::persistence {
namespace {

SqliteDatabase open_database(const std::filesystem::path &project_path) {
  const auto path = ProjectDatabaseRepository::database_path(project_path);
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("O banco de dados do projeto não foi encontrado");
  return SqliteDatabase(path);
}

project::StructuralNode read_node(SqliteStatement &statement) {
  project::StructuralNode value{
      statement.column_text(0),
      statement.column_text(1),
      statement.column_is_null(2)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(2)},
      project::structural_node_type_from_string(statement.column_text(3)),
      statement.column_text(4),
      statement.column_text(5),
      statement.column_text(6),
      statement.column_text(7),
      statement.column_text(8),
      statement.column_integer(9),
      statement.column_text(10),
      statement.column_text(11),
      statement.column_text(12),
      statement.column_text(13),
      statement.column_text(14),
      statement.column_text(15)};
  project::validate(value);
  return value;
}

void bind_node(SqliteStatement &statement,
               const project::StructuralNode &value) {
  statement.bind(1, value.id);
  statement.bind(2, value.work_id);
  if (value.parent_id)
    statement.bind(3, *value.parent_id);
  else
    statement.bind_null(3);
  statement.bind(4, project::to_string(value.type));
  statement.bind(5, value.title);
  statement.bind(6, value.subtitle);
  statement.bind(7, value.synopsis);
  statement.bind(8, value.custom_type_name);
  statement.bind(9, value.status);
  statement.bind(10, value.position);
  statement.bind(11, value.created_at);
  statement.bind(12, value.updated_at);
  statement.bind(13, value.structural_type_id);
  statement.bind(14, value.designator);
  statement.bind(15, value.structure_id);
}

SqliteStatement node_upsert(SqliteDatabase &database) {
  return database.prepare(
      "INSERT INTO editorial_nodes "
      "(id, work_id, parent_id, type, title, subtitle, synopsis, "
      "custom_type_name, status, position, created_at, updated_at, "
      "structural_type_id, designator, structure_id) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET work_id=excluded.work_id, "
      "parent_id=excluded.parent_id, type=excluded.type, title=excluded.title, "
      "subtitle=excluded.subtitle, synopsis=excluded.synopsis, "
      "custom_type_name=excluded.custom_type_name, status=excluded.status, "
      "position=excluded.position, updated_at=excluded.updated_at, "
      "structural_type_id=excluded.structural_type_id, "
      "designator=excluded.designator, structure_id=excluded.structure_id");
}

} // namespace

std::vector<project::StructuralNode> SqliteStructuralRepository::load(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  auto statement = database.prepare(
      "SELECT n.id, n.work_id, n.parent_id, n.type, n.title, n.subtitle, "
      "n.synopsis, n.custom_type_name, n.status, n.position, n.created_at, "
      "n.updated_at, n.structural_type_id, t.name, n.designator "
      ", n.structure_id "
      "FROM editorial_nodes n JOIN structural_element_types t "
      "ON t.id = n.structural_type_id ORDER BY n.position, n.id");
  std::vector<project::StructuralNode> values;
  while (statement.step())
    values.push_back(read_node(statement));
  return values;
}

std::vector<project::StructuralElementType>
SqliteStructuralRepository::load_types(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  auto statement = database.prepare(
      "SELECT id, name, is_builtin, created_at, updated_at "
      "FROM structural_element_types ORDER BY is_builtin DESC, name, id");
  std::vector<project::StructuralElementType> values;
  while (statement.step()) {
    project::StructuralElementType value{
        statement.column_text(0), statement.column_text(1),
        statement.column_integer(2) != 0, statement.column_text(3),
        statement.column_text(4)};
    project::validate(value);
    values.push_back(std::move(value));
  }
  return values;
}

void SqliteStructuralRepository::save_type(
    const std::filesystem::path &project_path,
    const project::StructuralElementType &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  const auto project_id = database.query_text("SELECT id FROM projects");
  auto statement = database.prepare(
      "INSERT INTO structural_element_types "
      "(id, project_id, name, is_builtin, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?) ON CONFLICT(id) DO UPDATE SET "
      "name=excluded.name, updated_at=excluded.updated_at");
  statement.bind(1, value.id);
  statement.bind(2, project_id);
  statement.bind(3, value.name);
  statement.bind(4, static_cast<std::int64_t>(value.is_builtin));
  statement.bind(5, value.created_at);
  statement.bind(6, value.updated_at);
  statement.run();
  auto mirror =
      database.prepare("UPDATE editorial_nodes SET custom_type_name = ? "
                       "WHERE structural_type_id = ? AND type = 'custom'");
  mirror.bind(1, value.name);
  mirror.bind(2, value.id);
  mirror.run();
  auto trash_mirror =
      database.prepare("UPDATE editorial_node_trash SET custom_type_name = ? "
                       "WHERE structural_type_id = ? AND type = 'custom'");
  trash_mirror.bind(1, value.name);
  trash_mirror.bind(2, value.id);
  trash_mirror.run();
  transaction.commit();
}

void SqliteStructuralRepository::remove_type(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto statement =
      database.prepare("DELETE FROM structural_element_types WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Tipo estrutural não encontrado no banco");
  transaction.commit();
}

void SqliteStructuralRepository::save(
    const std::filesystem::path &project_path,
    const project::StructuralNode &value) const {
  save_many(project_path, {value});
}

void SqliteStructuralRepository::save_many(
    const std::filesystem::path &project_path,
    const std::vector<project::StructuralNode> &values) const {
  for (const auto &value : values)
    project::validate(value);
  if (values.empty())
    return;
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto statement = node_upsert(database);
  for (const auto &value : values) {
    auto persisted = value;
    if (persisted.structure_id.empty()) {
      auto active = database.prepare("SELECT id FROM editorial_structures "
                                     "WHERE work_id = ? AND is_active = 1");
      active.bind(1, persisted.work_id);
      if (!active.step())
        throw std::runtime_error("A Obra não possui estrutura editorial ativa");
      persisted.structure_id = active.column_text(0);
    }
    bind_node(statement, persisted);
    statement.run();
    statement.reset();
  }
  transaction.commit();
}

void SqliteStructuralRepository::remove(
    const std::filesystem::path &project_path, const std::string &id) const {
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto statement = database.prepare("DELETE FROM editorial_nodes WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  if (database.changes() != 1)
    throw std::runtime_error("Elemento estrutural não encontrado no banco");
  transaction.commit();
}

std::string SqliteStructuralRepository::move_to_trash(
    const std::filesystem::path &project_path,
    const std::vector<std::string> &ids) const {
  if (ids.empty())
    throw std::runtime_error("Nenhum elemento estrutural foi selecionado");
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  const auto operation_id = project::new_uuid();
  auto operation = database.prepare(
      "INSERT INTO editorial_trash_operations(id, created_at) VALUES (?, ?)");
  operation.bind(1, operation_id);
  operation.bind(2, project::utc_now());
  operation.run();

  auto archive = database.prepare(
      "INSERT INTO editorial_node_trash "
      "(operation_id, id, work_id, parent_id, type, title, subtitle, synopsis, "
      "custom_type_name, status, position, created_at, updated_at, "
      "structural_type_id, designator, structure_id) "
      "SELECT ?, id, work_id, parent_id, type, title, subtitle, synopsis, "
      "custom_type_name, status, position, created_at, updated_at, "
      "structural_type_id, designator, structure_id "
      "FROM editorial_nodes WHERE id = ?");
  auto remove = database.prepare("DELETE FROM editorial_nodes WHERE id = ?");
  for (const auto &id : ids) {
    archive.bind(1, operation_id);
    archive.bind(2, id);
    archive.run();
    if (database.changes() != 1)
      throw std::runtime_error(
          "Elemento estrutural ausente durante a exclusão");
    archive.reset();

    remove.bind(1, id);
    remove.run();
    if (database.changes() != 1)
      throw std::runtime_error(
          "Não foi possível excluir o elemento estrutural");
    remove.reset();
  }
  transaction.commit();
  return operation_id;
}

bool SqliteStructuralRepository::restore_latest_trash(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  SqliteTransaction transaction(database);
  auto latest = database.prepare("SELECT id FROM editorial_trash_operations "
                                 "ORDER BY sequence DESC LIMIT 1");
  if (!latest.step()) {
    transaction.rollback();
    return false;
  }
  const auto operation_id = latest.column_text(0);

  auto restore = database.prepare(
      "INSERT INTO editorial_nodes "
      "(id, work_id, parent_id, type, title, subtitle, synopsis, "
      "custom_type_name, status, position, created_at, updated_at, "
      "structural_type_id, designator, structure_id) "
      "SELECT id, work_id, parent_id, type, title, subtitle, synopsis, "
      "custom_type_name, status, position, created_at, updated_at, "
      "structural_type_id, designator, structure_id "
      "FROM editorial_node_trash WHERE operation_id = ?");
  restore.bind(1, operation_id);
  restore.run();
  if (database.changes() == 0)
    throw std::runtime_error("A exclusão estrutural não contém elementos");

  auto remove_operation =
      database.prepare("DELETE FROM editorial_trash_operations WHERE id = ?");
  remove_operation.bind(1, operation_id);
  remove_operation.run();
  if (database.changes() != 1)
    throw std::runtime_error("Não foi possível concluir a restauração");
  transaction.commit();
  return true;
}

} // namespace inde::persistence

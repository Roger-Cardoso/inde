#include "inde/persistence/project_database_repository.hpp"

#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"

#include <stdexcept>

namespace inde::persistence {
namespace {

void verify_identity(SqliteDatabase &database,
                     const std::string &expected_project_id) {
  auto project = database.prepare("SELECT id FROM projects");
  if (!project.step() || project.column_text(0) != expected_project_id) {
    throw std::runtime_error(
        "O banco SQLite não pertence ao manifesto deste projeto");
  }
  if (project.step()) {
    throw std::runtime_error("O banco SQLite contém mais de um projeto");
  }
}

void verify_database(SqliteDatabase &database,
                     const std::string &expected_project_id) {
  if (database.query_text("PRAGMA integrity_check") != "ok") {
    throw std::runtime_error("O banco SQLite do projeto está corrompido");
  }
  auto foreign_keys = database.prepare("PRAGMA foreign_key_check");
  if (foreign_keys.step()) {
    throw std::runtime_error(
        "O banco SQLite do projeto contém referências inválidas");
  }
  verify_identity(database, expected_project_id);
}

void bind_manifest(SqliteStatement &statement,
                   const project::Manifest &manifest, int offset = 1) {
  statement.bind(offset, manifest.name);
  statement.bind(offset + 1,
                 static_cast<std::int64_t>(manifest.format_version));
  statement.bind(offset + 2, manifest.created_at);
  statement.bind(offset + 3, manifest.updated_at);
}

} // namespace

std::filesystem::path ProjectDatabaseRepository::database_path(
    const std::filesystem::path &project_path) {
  return project_path / "data" / "project.sqlite3";
}

void ProjectDatabaseRepository::prepare(const project::Project &value) const {
  const auto path = database_path(value.path());
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error(
        "O banco de dados obrigatório deste projeto INDE não foi encontrado");
  }
  SqliteDatabase database(path);
  SchemaMigrator{}.migrate(database);
  verify_database(database, value.manifest().project_id);
}

void ProjectDatabaseRepository::synchronize_manifest(
    const project::Project &value) const {
  const auto path = database_path(value.path());
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("O banco de dados do projeto não foi encontrado");
  SqliteDatabase database(path);
  SchemaMigrator{}.migrate(database);
  verify_identity(database, value.manifest().project_id);
  SqliteTransaction transaction(database);
  auto update = database.prepare(
      "UPDATE projects SET name = ?, format_version = ?, created_at = ?, "
      "updated_at = ? WHERE id = ?");
  bind_manifest(update, value.manifest());
  update.bind(5, value.manifest().project_id);
  update.run();
  if (database.changes() != 1) {
    throw std::runtime_error("Não foi possível sincronizar o projeto no banco");
  }
  transaction.commit();
}

void ProjectDatabaseRepository::replace_project_identity(
    const std::filesystem::path &project_path,
    const std::string &old_project_id,
    const project::Manifest &manifest) const {
  const auto path = database_path(project_path);
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("O banco copiado do projeto não foi encontrado");
  SqliteDatabase database(path);
  SchemaMigrator{}.migrate(database);
  verify_database(database, old_project_id);
  SqliteTransaction transaction(database);

  auto insert = database.prepare(
      "INSERT INTO projects(id, name, format_version, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?)");
  insert.bind(1, manifest.project_id);
  bind_manifest(insert, manifest, 2);
  insert.run();

  auto move_catalog = database.prepare(
      "UPDATE intellectual_properties SET project_id = ? WHERE project_id = ?");
  move_catalog.bind(1, manifest.project_id);
  move_catalog.bind(2, old_project_id);
  move_catalog.run();

  for (const char *table : {"structural_element_types",
                            "entity_types",
                            "entities",
                            "relation_types",
                            "relations",
                            "relation_contexts",
                            "fictional_time_axes",
                            "fictional_time_points",
                            "event_occurrences",
                            "event_participations",
                            "entity_presences",
                            "entity_work_scopes",
                            "editorial_entity_references",
                            "document_groups",
                            "documents",
                            "document_format_spans",
                            "document_anchors",
                            "document_entity_references",
                            "document_text_references",
                            "cartographic_planets",
                            "cartographic_positions",
                            "cartographic_terrain_locks",
                            "structure_models",
                            "structure_model_items",
                            "structure_model_item_links",
                            "editorial_structures",
                            "narrative_structures",
                            "narrative_lines",
                            "narrative_units",
                            "narrative_unit_lines",
                            "narrative_unit_entities",
                            "narrative_links",
                            "narrative_roles",
                            "narrative_role_assignments",
                            "change_log"}) {
    auto move_narrative = database.prepare("UPDATE " + std::string(table) +
                                           " SET project_id = ? "
                                           "WHERE project_id = ?");
    move_narrative.bind(1, manifest.project_id);
    move_narrative.bind(2, old_project_id);
    move_narrative.run();
  }

  auto log = database.prepare(
      "INSERT INTO change_log(id, project_id, command_name, system_created_at) "
      "VALUES (?, ?, 'save_as_project', ?)");
  log.bind(1, project::new_uuid());
  log.bind(2, manifest.project_id);
  log.bind(3, project::utc_now());
  log.run();

  auto remove_old = database.prepare("DELETE FROM projects WHERE id = ?");
  remove_old.bind(1, old_project_id);
  remove_old.run();
  if (database.changes() != 1) {
    throw std::runtime_error(
        "Não foi possível substituir a identidade do projeto");
  }
  transaction.commit();
}

} // namespace inde::persistence

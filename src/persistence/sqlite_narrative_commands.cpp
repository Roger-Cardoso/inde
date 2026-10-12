#include "inde/persistence/sqlite_narrative_repository.hpp"
#include "inde/persistence/project_operation_store.hpp"

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

void SqliteNarrativeRepository::save(
    const std::filesystem::path &project_path,
    const project::EntityType &value) const {
  project::validate(value);
  auto database = open_database(project_path);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
  validate_project_removal(database,project::RemovalKind::Entity,id);
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
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
  SqliteTransaction transaction(database, "Cadastro e vínculos narrativos");
  auto statement = database.prepare(
      "DELETE FROM editorial_entity_references WHERE id = ?");
  statement.bind(1, id);
  statement.run();
  require_removed(database, "Referência editorial de entidade não encontrada");
  record_change(database, owner, "delete_editorial_entity_reference");
  transaction.commit();
}

} // namespace inde::persistence

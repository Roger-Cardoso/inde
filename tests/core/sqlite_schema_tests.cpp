#include "inde/application/catalog_service.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/project_session.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/spelling_provider.hpp"
#include "inde/application/structural_service.hpp"
#include "inde/application/temporal_view.hpp"
#include "inde/application/timeline_projection.hpp"
#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_importer.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_catalog_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_structural_repository.hpp"
#include "inde/persistence/structural_repository.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

#include "core_test_cases.hpp"
#include "test_support.hpp"
#include "sqlite_fixture.hpp"

namespace inde::tests {

void sqlite_schema_and_transactions() {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path / "project.sqlite3";
  const std::string project_id = "10000000-0000-4000-8000-000000000001";

  inde::persistence::SqliteDatabase database(database_path);
  inde::persistence::SchemaMigrator migrator;
  assert(database.query_integer("PRAGMA foreign_keys") == 1);
  assert(migrator.current_version(database) == 0);

  migrator.migrate(database);
  assert(migrator.current_version(database) ==
         inde::persistence::current_database_schema_version);
  assert(database.query_text("PRAGMA journal_mode") != "wal");
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('projects', 'intellectual_properties', 'works', "
             "'editorial_nodes', 'schema_migrations', "
             "'editorial_trash_operations', 'editorial_node_trash', "
             "'entity_types', 'entities', 'relation_types', 'relations', "
             "'relation_contexts', "
             "'change_log', 'fictional_time_axes', 'fictional_time_points', "
             "'event_occurrences', 'event_participations', "
             "'entity_presences', 'entity_work_scopes', "
             "'editorial_entity_references', 'documents', "
             "'document_groups', "
             "'document_format_spans', 'document_anchors', "
             "'document_entity_references', "
             "'structural_element_types', 'structure_models', "
             "'structure_model_items', 'structure_model_item_links', "
             "'editorial_structures', "
             "'narrative_structures', 'narrative_lines', "
             "'narrative_units', 'narrative_unit_lines', "
             "'narrative_unit_entities', 'narrative_links', "
             "'narrative_roles', 'narrative_role_assignments')") == 38);

  // Reaplicar o migrador é uma operação idempotente.
  migrator.migrate(database);
  assert(database.query_integer("SELECT count(*) FROM schema_migrations") ==
         inde::persistence::current_database_schema_version);

  // Simula um banco v1 e prova as migrações incrementais v2-v11 sem recriar
  // as tabelas editoriais.
  downgrade_v12_to_v11(database);
  database.execute("DROP TRIGGER editorial_nodes_structural_type_insert_guard");
  database.execute("DROP TRIGGER editorial_nodes_structural_type_update_guard");
  database.execute(
      "DROP TRIGGER structural_element_types_builtin_update_guard");
  database.execute("DROP TRIGGER structural_element_types_delete_guard");
  database.execute("ALTER TABLE editorial_nodes DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_nodes DROP COLUMN structural_type_id");
  database.execute("ALTER TABLE editorial_node_trash DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_node_trash DROP COLUMN structural_type_id");
  database.execute("DROP TABLE structural_element_types");
  database.execute("DELETE FROM schema_migrations WHERE version = 11");
  database.execute("DROP TABLE relation_contexts");
  database.execute("DELETE FROM schema_migrations WHERE version = 10");
  database.execute("DELETE FROM schema_migrations WHERE version = 9");
  database.execute("DROP TABLE document_entity_references");
  database.execute("DROP TABLE document_anchors");
  database.execute("DROP TABLE document_format_spans");
  database.execute("DELETE FROM schema_migrations WHERE version = 8");
  database.execute("DROP TABLE documents");
  database.execute("DROP TABLE document_groups");
  database.execute("DELETE FROM schema_migrations WHERE version = 7");
  database.execute("DROP TABLE editorial_entity_references");
  database.execute("DROP TABLE entity_work_scopes");
  database.execute("DELETE FROM schema_migrations WHERE version = 6");
  database.execute("DROP TABLE entity_presences");
  database.execute("DELETE FROM schema_migrations WHERE version = 5");
  database.execute("DROP TABLE event_participations");
  database.execute("DROP TABLE event_occurrences");
  database.execute("DROP TABLE fictional_time_points");
  database.execute("DROP TABLE fictional_time_axes");
  database.execute("DELETE FROM schema_migrations WHERE version = 4");
  database.execute("DROP TABLE change_log");
  database.execute("DROP TABLE relations");
  database.execute("DROP TABLE relation_types");
  database.execute("DROP TABLE entities");
  database.execute("DROP TABLE entity_types");
  database.execute("DELETE FROM schema_migrations WHERE version = 3");
  database.execute("DROP TABLE editorial_node_trash");
  database.execute("DROP TABLE editorial_trash_operations");
  database.execute("DELETE FROM schema_migrations WHERE version = 2");
  assert(migrator.current_version(database) == 1);
  migrator.migrate(database);
  assert(migrator.current_version(database) ==
         inde::persistence::current_database_schema_version);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('editorial_trash_operations', "
             "'editorial_node_trash')") == 2);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('entity_types', 'entities', 'relation_types', "
             "'relations', 'relation_contexts', 'change_log')") == 6);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('fictional_time_axes', 'fictional_time_points', "
             "'event_occurrences', 'event_participations')") == 4);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'entity_presences'") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('entity_work_scopes', "
             "'editorial_entity_references')") == 2);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'documents'") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('document_format_spans', 'document_anchors', "
             "'document_entity_references')") == 3);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'structural_element_types'") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('structure_models', 'structure_model_items', "
             "'structure_model_item_links', "
             "'editorial_structures', 'narrative_structures', "
             "'narrative_lines', 'narrative_units', "
             "'narrative_unit_lines', 'narrative_unit_entities', "
             "'narrative_links', 'narrative_roles', "
             "'narrative_role_assignments')") == 12);

  {
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, project_id, "Universo Áureo 🚀");
    transaction.commit();
  }
  assert(database.query_text("SELECT name FROM projects") ==
         "Universo Áureo 🚀");

  {
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, "10000000-0000-4000-8000-000000000002",
                            "Será revertido");
    // Sem commit: o destruidor precisa executar rollback.
  }
  assert(database.query_integer("SELECT count(*) FROM projects") == 1);

  {
    auto reusable = database.prepare("SELECT name FROM projects WHERE id = ?");
    reusable.bind(1, project_id);
    assert(reusable.step());
    assert(reusable.column_text(0) == "Universo Áureo 🚀");
    assert(!reusable.step());
    reusable.reset();
    reusable.bind(1, "10000000-0000-4000-8000-000000000099");
    assert(!reusable.step());
  }

  assert(std::filesystem::is_regular_file(database_path));
}

void sqlite_database_closes_before_copy() {
  TemporaryDirectory temporary;
  const auto source = temporary.path / "source.sqlite3";
  const auto copy = temporary.path / "copy.sqlite3";
  {
    inde::persistence::SqliteDatabase database(source);
    inde::persistence::SchemaMigrator{}.migrate(database);
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, "10000000-0000-4000-8000-000000000001",
                            "Projeto copiável");
    transaction.commit();
  }

  assert(std::filesystem::copy_file(source, copy));
  inde::persistence::SqliteDatabase reopened(copy);
  assert(reopened.query_text("SELECT name FROM projects") ==
         "Projeto copiável");
}

void sqlite_v11_migrates_structural_identity_and_per_type_designators() {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path / "v10.sqlite3";
  inde::persistence::SqliteDatabase database(database_path);
  inde::persistence::SchemaMigrator migrator;
  migrator.migrate(database);

  // Reconstrói o estado imediatamente anterior ao corte para provar a
  // migração com dados, inclusive nomes personalizados que diferem apenas em
  // maiúsculas e irmãos de tipos diferentes.
  downgrade_v12_to_v11(database);
  database.execute("DROP TRIGGER editorial_nodes_structural_type_insert_guard");
  database.execute("DROP TRIGGER editorial_nodes_structural_type_update_guard");
  database.execute(
      "DROP TRIGGER structural_element_types_builtin_update_guard");
  database.execute("DROP TRIGGER structural_element_types_delete_guard");
  database.execute("ALTER TABLE editorial_nodes DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_nodes DROP COLUMN structural_type_id");
  database.execute("ALTER TABLE editorial_node_trash DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_node_trash DROP COLUMN structural_type_id");
  database.execute("DROP TABLE structural_element_types");
  database.execute("DELETE FROM schema_migrations WHERE version = 11");
  assert(migrator.current_version(database) == 10);

  database.execute(
      "INSERT INTO projects VALUES "
      "('10000000-0000-4000-8000-000000000001','Migração',2,'agora','agora');"
      "INSERT INTO intellectual_properties "
      "(id,project_id,title,created_at,updated_at) VALUES "
      "('20000000-0000-4000-8000-000000000001',"
      "'10000000-0000-4000-8000-000000000001','IP','agora','agora');"
      "INSERT INTO works "
      "(id,intellectual_property_id,title,language,status,created_at,updated_"
      "at) "
      "VALUES ('30000000-0000-4000-8000-000000000001',"
      "'20000000-0000-4000-8000-000000000001','Obra','pt-BR',"
      "'Planejamento','agora','agora');"
      "INSERT INTO editorial_nodes "
      "(id,work_id,parent_id,type,title,subtitle,synopsis,custom_type_name,"
      "status,position,created_at,updated_at) VALUES "
      "('40000000-0000-4000-8000-000000000001',"
      "'30000000-0000-4000-8000-000000000001',NULL,'custom','Intervalo A',"
      "'','','Interlúdio','Planejamento',1000,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000002',"
      "'30000000-0000-4000-8000-000000000001',NULL,'custom','Intervalo B',"
      "'','','interlúdio','Planejamento',2000,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000003',"
      "'30000000-0000-4000-8000-000000000001',"
      "'40000000-0000-4000-8000-000000000001','chapter','Primeiro',"
      "'','','','Planejamento',1000,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000004',"
      "'30000000-0000-4000-8000-000000000001',"
      "'40000000-0000-4000-8000-000000000001','scene','Cena',"
      "'','','','Planejamento',1500,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000005',"
      "'30000000-0000-4000-8000-000000000001',"
      "'40000000-0000-4000-8000-000000000001','chapter','Segundo',"
      "'','','','Planejamento',2000,'agora','agora');");

  migrator.migrate(database);
  assert(migrator.current_version(database) ==
         inde::persistence::current_database_schema_version);
  assert(database.query_integer(
             "SELECT count(*) FROM structural_element_types") == 10);
  assert(database.query_integer(
             "SELECT count(DISTINCT structural_type_id) FROM editorial_nodes "
             "WHERE type = 'custom'") == 1);
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000001'") == "1");
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000002'") == "2");
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000004'") == "1");
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000005'") == "2");
  assert(database.query_text("PRAGMA integrity_check") == "ok");
  assert(!database.prepare("PRAGMA foreign_key_check").step());

  const auto project_path = temporary.path / "Migrado.inde";
  std::filesystem::create_directories(project_path / "data");
  const auto migrated_path = project_path / "data" / "project.sqlite3";
  std::filesystem::copy_file(database_path, migrated_path);
  assert(std::filesystem::is_regular_file(migrated_path));
  const auto nodes =
      inde::persistence::SqliteStructuralRepository{}.load(project_path);
  assert(nodes.size() == 5);
  assert(inde::project::structural_node_path_label(
             nodes, "40000000-0000-4000-8000-000000000005") ==
         "Interlúdio: 1 > Capítulo: 2 — Segundo");
}

void sqlite_foreign_keys_and_editorial_parent_integrity() {
  TemporaryDirectory temporary;
  inde::persistence::SqliteDatabase database(temporary.path /
                                             "integrity.sqlite3");
  inde::persistence::SchemaMigrator{}.migrate(database);

  bool missing_project_rejected = false;
  try {
    auto insert =
        database.prepare("INSERT INTO intellectual_properties "
                         "(id, project_id, title, created_at, updated_at) "
                         "VALUES ('20000000-0000-4000-8000-000000000001', "
                         "'ausente', 'IP órfã', 'agora', 'agora')");
    insert.run();
  } catch (const inde::persistence::SqliteError &error) {
    missing_project_rejected =
        error.code() == SQLITE_CONSTRAINT &&
        error.extended_code() == SQLITE_CONSTRAINT_FOREIGNKEY;
  }
  assert(missing_project_rejected);

  const std::string project_id = "10000000-0000-4000-8000-000000000001";
  const std::string ip_id = "20000000-0000-4000-8000-000000000001";
  const std::string work_a = "30000000-0000-4000-8000-000000000001";
  const std::string work_b = "30000000-0000-4000-8000-000000000002";
  const std::string parent_id = "40000000-0000-4000-8000-000000000001";
  const std::string child_id = "40000000-0000-4000-8000-000000000002";

  {
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, project_id, "Projeto");

    auto ip =
        database.prepare("INSERT INTO intellectual_properties "
                         "(id, project_id, title, created_at, updated_at) "
                         "VALUES (?, ?, 'Universo', 'agora', 'agora')");
    ip.bind(1, ip_id);
    ip.bind(2, project_id);
    ip.run();

    auto work = database.prepare(
        "INSERT INTO works "
        "(id, intellectual_property_id, title, language, status, created_at, "
        "updated_at) VALUES (?, ?, ?, 'pt-BR', 'Planejamento', 'agora', "
        "'agora')");
    work.bind(1, work_a);
    work.bind(2, ip_id);
    work.bind(3, "Obra A");
    work.run();
    work.reset();
    work.bind(1, work_b);
    work.bind(2, ip_id);
    work.bind(3, "Obra B");
    work.run();
    transaction.commit();
  }

  bool duplicate_uuid_rejected = false;
  try {
    insert_database_project(database, project_id, "UUID repetido");
  } catch (const inde::persistence::SqliteError &) {
    duplicate_uuid_rejected = true;
  }
  assert(duplicate_uuid_rejected);

  bool cross_work_parent_rejected = false;
  try {
    inde::persistence::SqliteTransaction transaction(database);
    auto node = database.prepare(
        "INSERT INTO editorial_nodes "
        "(id, work_id, parent_id, type, title, status, position, created_at, "
        "updated_at) VALUES (?, ?, ?, 'chapter', ?, 'Planejamento', 1000, "
        "'agora', 'agora')");
    node.bind(1, parent_id);
    node.bind(2, work_a);
    node.bind_null(3);
    node.bind(4, "Pai");
    node.run();
    node.reset();
    node.bind(1, child_id);
    node.bind(2, work_b);
    node.bind(3, parent_id);
    node.bind(4, "Filho em outra obra");
    node.run();
    transaction.commit();
  } catch (const inde::persistence::SqliteError &) {
    cross_work_parent_rejected = true;
  }
  assert(cross_work_parent_rejected);
  assert(database.query_integer("SELECT count(*) FROM editorial_nodes") == 0);
}

void sqlite_rejects_future_schema() {
  TemporaryDirectory temporary;
  inde::persistence::SqliteDatabase database(temporary.path / "future.sqlite3");
  inde::persistence::SchemaMigrator migrator;
  migrator.migrate(database);
  database.execute("INSERT INTO schema_migrations(version, applied_at) "
                   "VALUES (999, 'futuro')");

  bool rejected = false;
  try {
    migrator.migrate(database);
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(migrator.current_version(database) == 999);
}

void sqlite_rolls_back_interrupted_migration() {
  TemporaryDirectory temporary;
  inde::persistence::SqliteDatabase database(temporary.path /
                                             "interrupted.sqlite3");
  // Simula um conflito durante o esquema v1, depois da criação transacional da
  // tabela de histórico de migrações.
  database.execute("CREATE TABLE projects(id INTEGER PRIMARY KEY)");

  bool failed = false;
  try {
    inde::persistence::SchemaMigrator{}.migrate(database);
  } catch (const inde::persistence::SqliteError &) {
    failed = true;
  }
  assert(failed);
  assert(inde::persistence::SchemaMigrator{}.current_version(database) == 0);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'schema_migrations'") == 0);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'intellectual_properties'") == 0);
}

} // namespace inde::tests

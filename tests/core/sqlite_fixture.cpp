#include "sqlite_fixture.hpp"
#include "../schema17_downgrade.hpp"

namespace inde::tests {

void insert_database_project(inde::persistence::SqliteDatabase &database,
                             std::string_view id, std::string_view name) {
  auto insert = database.prepare(
      "INSERT INTO projects(id, name, format_version, created_at, updated_at) "
      "VALUES (?, ?, 1, '2026-01-01T00:00:00Z', "
      "'2026-01-01T00:00:00Z')");
  insert.bind(1, id);
  insert.bind(2, name);
  insert.run();
}

void downgrade_v15_to_v14(inde::persistence::SqliteDatabase &database) {
  downgrade_schema17_for_fixture(database);
  if(database.query_integer("SELECT max(version) FROM schema_migrations")>=16)
    database.execute("DROP TABLE semantic_annotation_relations; DROP TABLE semantic_entity_bindings; DROP TABLE semantic_annotations; DROP TABLE document_semantics; DROP TABLE semantic_type_definitions; DELETE FROM schema_migrations WHERE version=16;");
  database.execute("DROP TABLE cartographic_terrain_locks;"
                   "ALTER TABLE cartographic_planets DROP COLUMN "
                   "terrain_revision;"
                   "DELETE FROM schema_migrations WHERE version=15;");
}

void downgrade_v14_to_v13(inde::persistence::SqliteDatabase &database) {
  downgrade_v15_to_v14(database);
  database.execute("DROP TRIGGER cartographic_local_type_guard;"
                   "DROP TABLE cartographic_positions;"
                   "DROP TABLE cartographic_position_index;"
                   "DROP TABLE cartographic_chunks;"
                   "DROP TABLE cartographic_planets;"
                   "DELETE FROM schema_migrations WHERE version=14;");
}

void downgrade_v12_to_v11(inde::persistence::SqliteDatabase &database) {
  downgrade_v14_to_v13(database);
  database.execute("DROP TABLE document_text_references;"
                   "DROP INDEX document_anchors_identity_idx;"
                   "DELETE FROM schema_migrations WHERE version = 13;");
  database.execute("DROP TRIGGER editorial_nodes_structure_insert_guard;"
                   "DROP TRIGGER editorial_nodes_structure_update_guard;"
                   "DROP TRIGGER editorial_nodes_structure_default;"
                   "DROP TRIGGER works_default_structures;"
                   "DROP TRIGGER structure_models_builtin_update_guard;"
                   "DROP TRIGGER structure_models_builtin_delete_guard;"
                   "DROP TRIGGER narrative_roles_builtin_update_guard;"
                   "DROP TRIGGER narrative_roles_builtin_delete_guard;"
                   "DROP TRIGGER narrative_unit_lines_structure_insert_guard;"
                   "DROP TRIGGER narrative_unit_lines_structure_update_guard;"
                   "DROP TRIGGER narrative_links_structure_insert_guard;"
                   "DROP TRIGGER narrative_links_structure_update_guard;"
                   "DROP TRIGGER narrative_role_assignments_scope_insert_guard;"
                   "DROP TRIGGER narrative_role_assignments_scope_update_guard;"
                   "DROP INDEX editorial_nodes_structure_parent_position_idx;"
                   "DROP TABLE narrative_role_assignments;"
                   "DROP TABLE narrative_roles;"
                   "DROP TABLE narrative_links;"
                   "DROP TABLE narrative_unit_entities;"
                   "DROP TABLE narrative_unit_lines;"
                   "DROP TABLE narrative_units;"
                   "DROP TABLE narrative_lines;"
                   "DROP TABLE narrative_structures;"
                   "ALTER TABLE editorial_nodes DROP COLUMN structure_id;"
                   "ALTER TABLE editorial_node_trash DROP COLUMN structure_id;"
                   "DROP TABLE editorial_structures;"
                   "DROP TABLE structure_model_item_links;"
                   "DROP TABLE structure_model_items;"
                   "DROP TABLE structure_models;"
                   "DELETE FROM schema_migrations WHERE version = 12;");
}

} // namespace inde::tests

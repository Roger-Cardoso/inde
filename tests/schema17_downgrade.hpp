#pragma once
#include "schema20_downgrade.hpp"
#include "../src/persistence/schema_sql_v16.hpp"
#include "inde/persistence/sqlite_database.hpp"
// Fixture-only inverse: refuses new authorial data instead of dropping it.
inline void
downgrade_schema18_for_fixture(inde::persistence::SqliteDatabase &db) {
  downgrade_schema21_for_fixture(db);
  // Disposable migration fixtures discard only the operational journal before
  // reconstructing an older schema; never leave a future table behind.
  if(db.query_integer("SELECT max(version) FROM schema_migrations")>=20)
    db.execute("DROP TABLE project_operations;DELETE FROM schema_migrations WHERE version=20;");
  if (db.query_integer("SELECT max(version) FROM schema_migrations") < 18)
    return;
  if (db.query_integer(
          "SELECT count(*) FROM semantic_annotations WHERE scope='document'") ||
      db.query_integer("SELECT count(*) FROM semantic_fictional_time_bindings"))
    throw std::runtime_error("Fixture inverse refuses schema18 data");
  db.execute("DROP TRIGGER semantic_fictional_point_order_guard;DROP TRIGGER "
             "semantic_fictional_time_order_insert;DROP TRIGGER "
             "semantic_fictional_time_order_update;DROP TABLE "
             "semantic_fictional_time_bindings;DROP INDEX "
             "fictional_time_point_axis_identity;"
             "CREATE TEMP TABLE old_annotations AS SELECT "
             "id,project_id,document_id,anchor_id,type_key,type_version,status,"
             "category FROM semantic_annotations;"
             "CREATE TEMP TABLE old_entities AS SELECT * FROM "
             "semantic_entity_bindings;CREATE TEMP TABLE old_relations AS "
             "SELECT * FROM semantic_annotation_relations;"
             "DROP TABLE semantic_annotation_relations;DROP TABLE "
             "semantic_entity_bindings;DROP TABLE semantic_annotations;");
  const std::string schema = inde::persistence::schema_sql::schema_version_16;
  db.execute(schema.substr(schema.find("CREATE TABLE semantic_annotations")));
  db.execute(
      "INSERT INTO semantic_annotations SELECT * FROM old_annotations;INSERT "
      "INTO semantic_entity_bindings SELECT * FROM old_entities;INSERT INTO "
      "semantic_annotation_relations SELECT * FROM old_relations;"
      "DROP TABLE old_annotations;DROP TABLE old_entities;DROP TABLE "
      "old_relations;DELETE FROM schema_migrations WHERE version>=18;");
}
inline void
downgrade_schema17_for_fixture(inde::persistence::SqliteDatabase &db) {
  downgrade_schema18_for_fixture(db);
  if (db.query_integer("SELECT max(version) FROM schema_migrations") < 17)
    return;
  db.execute(
      "ALTER TABLE documents DROP COLUMN inline_commands;"
      "ALTER TABLE document_format_spans RENAME TO document_format_spans_v17;"
      "DROP INDEX document_format_spans_project_idx;"
      "CREATE TABLE document_format_spans(project_id TEXT NOT NULL,document_id "
      "TEXT NOT NULL,"
      "style TEXT NOT NULL CHECK(style IN "
      "('bold','italic','underline','strikethrough','heading','subheading','"
      "quote')),"
      "start_offset INTEGER NOT NULL CHECK(start_offset>=0),end_offset INTEGER "
      "NOT NULL CHECK(end_offset>start_offset),"
      "PRIMARY KEY(document_id,style,start_offset,end_offset),"
      "FOREIGN KEY(document_id,project_id) REFERENCES documents(id,project_id) "
      "ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED);"
      "INSERT INTO document_format_spans SELECT "
      "project_id,document_id,style,start_offset,end_offset FROM "
      "document_format_spans_v17;"
      "DROP TABLE document_format_spans_v17;"
      "CREATE INDEX document_format_spans_project_idx ON "
      "document_format_spans(project_id,document_id,start_offset);"
      "DELETE FROM schema_migrations WHERE version=17;");
}

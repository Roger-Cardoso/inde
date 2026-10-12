#pragma once
#include "inde/persistence/sqlite_database.hpp"
inline void
downgrade_schema28_for_fixture(inde::persistence::SqliteDatabase &db) {
  if (db.query_integer("SELECT max(version) FROM schema_migrations") >= 28) {
    for (const auto *table :
         {"narrative_text_targets", "narrative_text_decisions",
          "narrative_continuity_exceptions", "narrative_unit_presence_intents"})
      if (db.query_integer("SELECT count(*) FROM " + std::string(table)))
        throw std::runtime_error(
            "Fixture refuses removal of realization/continuity data");
    db.execute("DROP TABLE narrative_text_targets;DROP TABLE "
               "narrative_text_decisions;DROP TABLE "
               "narrative_continuity_exceptions;DROP TABLE "
               "narrative_unit_presence_intents;DROP INDEX "
               "compositions_identity_owner;DELETE FROM schema_migrations "
               "WHERE version=28");
  }
}
inline void
downgrade_schema27_for_fixture(inde::persistence::SqliteDatabase &db) {
  downgrade_schema28_for_fixture(db);
  if (db.query_integer("SELECT max(version) FROM schema_migrations") < 27)
    return;
  if (db.query_integer("SELECT count(*) FROM narrative_branch_provenance") ||
      db.query_integer("SELECT count(*) FROM narrative_dependency_scopes"))
    throw std::runtime_error(
        "Fixture refuses removal of narrative alternative data");
  db.execute(
      "DROP TABLE narrative_dependency_scopes;DROP TABLE "
      "narrative_branch_provenance;DROP INDEX narrative_link_owner;DELETE FROM "
      "schema_migrations WHERE version=27");
}
inline void
downgrade_schema22_for_fixture(inde::persistence::SqliteDatabase &db) {
  downgrade_schema27_for_fixture(db);
  if (db.query_integer("SELECT max(version) FROM schema_migrations") >= 26) {
    if (db.query_integer("SELECT count(*) FROM narrative_unit_contexts") ||
        db.query_integer("SELECT count(*) FROM narrative_realizations") ||
        db.query_integer("SELECT count(*) FROM narrative_unit_relations"))
      throw std::runtime_error(
          "Fixture refuses removal of authorial narrative context");
    db.execute(
        "DROP TRIGGER narrative_context_local_type;DROP TABLE "
        "narrative_unit_relations;DROP TABLE narrative_realizations;DROP TABLE "
        "narrative_unit_contexts;DELETE FROM schema_migrations WHERE "
        "version=26");
  }
  if (db.query_integer("SELECT max(version) FROM schema_migrations") >= 25) {
    if (db.query_integer("SELECT count(*) FROM semantic_date_bindings") ||
        db.query_integer("SELECT count(*) FROM temporal_reference_extents"))
      throw std::runtime_error(
          "Fixture refuses removal of temporal context data");
    db.execute("DROP TABLE semantic_date_bindings;DROP TABLE "
               "temporal_reference_extents;"
               "ALTER TABLE temporal_attachments DROP COLUMN meaning;DELETE "
               "FROM schema_migrations WHERE version=25");
  }
  if (db.query_integer("SELECT max(version) FROM schema_migrations") >= 24) {
    if (db.query_integer("SELECT count(*) FROM temporal_attachments"))
      throw std::runtime_error("Fixture refuses removal of date attachments");
    db.execute(
        "DROP TABLE temporal_attachments;DROP INDEX "
        "temporal_references_owner;DROP INDEX relations_temporal_owner;DELETE "
        "FROM schema_migrations WHERE version=24");
  }
  if (db.query_integer("SELECT max(version) FROM schema_migrations") >= 23) {
    if (db.query_integer("SELECT count(*) FROM calendar_revisions") ||
        db.query_integer("SELECT count(*) FROM calendar_adoptions") ||
        db.query_integer("SELECT count(*) FROM temporal_references"))
      throw std::runtime_error("Fixture refuses removal of temporal data");
    db.execute("DROP TABLE temporal_references;DROP TABLE "
               "calendar_adoptions;DROP TABLE calendar_revisions;DELETE FROM "
               "schema_migrations WHERE version=23");
  }
  if (db.query_integer("SELECT max(version) FROM schema_migrations") < 22)
    return;
  if (db.query_integer("SELECT count(*) FROM entity_aliases") ||
      db.query_integer("SELECT count(*) FROM fictional_calendars") ||
      db.query_integer("SELECT count(*) FROM axis_calendars"))
    throw std::runtime_error(
        "Fixture refuses removal of authorial worldbuilding data");
  db.execute(
      "DROP TABLE axis_calendars;DROP TABLE fictional_calendars;DROP TABLE "
      "entity_aliases;DELETE FROM schema_migrations WHERE version=22;");
}

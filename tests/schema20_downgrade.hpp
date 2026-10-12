#pragma once
#include "inde/persistence/sqlite_database.hpp"
#include "schema21_downgrade.hpp"
inline void downgrade_schema21_for_fixture(inde::persistence::SqliteDatabase &db) {
  downgrade_schema22_for_fixture(db);
  if(db.query_integer("SELECT max(version) FROM schema_migrations")<21)return;
  if(db.query_integer("SELECT count(*) FROM compositions") || db.query_integer("SELECT count(*) FROM composition_outputs"))
    throw std::runtime_error("Fixture refuses removal of authorial Compositions");
  db.execute("DROP TABLE composition_outputs;DROP TABLE compositions;DELETE FROM schema_migrations WHERE version=21;");
}

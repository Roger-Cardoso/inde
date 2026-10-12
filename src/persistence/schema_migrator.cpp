#include "inde/persistence/schema_migrator.hpp"

#include "schema_sql_v01_v04.hpp"
#include "schema_sql_v05_v11.hpp"
#include "schema_sql_v12_v15.hpp"
#include "schema_sql_v16.hpp"
#include "schema_sql_v17.hpp"
#include "schema_sql_v18.hpp"
#include "schema_sql_v19.hpp"
#include "schema_sql_v20.hpp"
#include "schema_sql_v21.hpp"
#include "schema_sql_v22.hpp"
#include "schema_sql_v23.hpp"
#include "schema_sql_v24.hpp"
#include "schema_sql_v25.hpp"
#include "schema_sql_v26.hpp"
#include "schema_sql_v27.hpp"
#include "schema_sql_v28.hpp"

#include "inde/persistence/sqlite_database.hpp"

#include <stdexcept>

namespace inde::persistence {
namespace {


constexpr auto create_migration_table = R"sql(
CREATE TABLE IF NOT EXISTS schema_migrations (
  version INTEGER PRIMARY KEY CHECK (version > 0),
  applied_at TEXT NOT NULL CHECK (length(applied_at) > 0)
)
)sql";















} // namespace

void SchemaMigrator::migrate(SqliteDatabase &database) const {
  SqliteTransaction transaction(database);
  database.execute(create_migration_table);

  const int version = current_version(database);
  if (version > current_database_schema_version) {
    throw std::runtime_error(
        "O banco do projeto foi criado por uma versão mais nova do INDE");
  }

  if (version < 1) {
    apply_version_1(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{1});
    record.run();
  }

  if (version < 2) {
    apply_version_2(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{2});
    record.run();
  }

  if (version < 3) {
    apply_version_3(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{3});
    record.run();
  }

  if (version < 4) {
    apply_version_4(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{4});
    record.run();
  }

  if (version < 5) {
    apply_version_5(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{5});
    record.run();
  }

  if (version < 6) {
    apply_version_6(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{6});
    record.run();
  }

  if (version < 7) {
    apply_version_7(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{7});
    record.run();
  }

  if (version < 8) {
    apply_version_8(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{8});
    record.run();
  }

  if (version < 9) {
    apply_version_9(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{9});
    record.run();
  }

  if (version < 10) {
    apply_version_10(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{10});
    record.run();
  }

  if (version < 11) {
    apply_version_11(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{11});
    record.run();
  }

  if (version < 12) {
    apply_version_12(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (?, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.bind(1, std::int64_t{12});
    record.run();
  }

  if (version < 13) {
    apply_version_13(database);
    auto record =
        database.prepare("INSERT INTO schema_migrations(version, applied_at) "
                         "VALUES (13, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
    record.run();
  }

  if (version < 14) {
    apply_version_14(database);
    database.execute("INSERT INTO schema_migrations(version, applied_at) "
                     "VALUES (14, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
  }

  if (version < 15) {
    apply_version_15(database);
    database.execute("INSERT INTO schema_migrations(version, applied_at) "
                     "VALUES (15, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))");
  }

  if (version < 16) {
    apply_version_16(database);
    auto record=database.prepare("INSERT INTO schema_migrations(version,applied_at) VALUES(16,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
    record.run();
  }
  if (version < 17) {
    apply_version_17(database);
    database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(17,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
  }
  if (version < 18) {
    apply_version_18(database);
    database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(18,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
  }
  if (version < 19) {
    apply_version_19(database);
    database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(19,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
  }
  if (version < 20) {
    apply_version_20(database);
    database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(20,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
  }
  if (version < 21) {
    apply_version_21(database);
    database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(21,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
  }
  if (version < 22) {
    apply_version_22(database);
    database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(22,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
  }
  if (version < 23) {
    apply_version_23(database);
    database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(23,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");
  }
  if(version<24){apply_version_24(database);database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(24,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");}
  if(version<25){apply_version_25(database);database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(25,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");}
  if(version<26){apply_version_26(database);database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(26,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");}
  if(version<27){apply_version_27(database);database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(27,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");}
  if(version<28){apply_version_28(database);database.execute("INSERT INTO schema_migrations(version,applied_at) VALUES(28,strftime('%Y-%m-%dT%H:%M:%fZ','now'))");}
  transaction.commit();
}

int SchemaMigrator::current_version(SqliteDatabase &database) const {
  auto exists =
      database.prepare("SELECT count(*) FROM sqlite_master "
                       "WHERE type = 'table' AND name = 'schema_migrations'");
  if (!exists.step() || exists.column_integer(0) == 0) {
    return 0;
  }

  return static_cast<int>(database.query_integer(
      "SELECT COALESCE(MAX(version), 0) FROM schema_migrations"));
}

void SchemaMigrator::apply_version_26(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_26);
}
void SchemaMigrator::apply_version_27(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_27);
}
void SchemaMigrator::apply_version_28(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_28);
}

void SchemaMigrator::apply_version_1(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_1);
}

void SchemaMigrator::apply_version_2(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_2);
}

void SchemaMigrator::apply_version_3(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_3);
}

void SchemaMigrator::apply_version_4(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_4);
}

void SchemaMigrator::apply_version_5(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_5);
}

void SchemaMigrator::apply_version_6(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_6);
}

void SchemaMigrator::apply_version_7(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_7);
}

void SchemaMigrator::apply_version_8(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_8);
}

void SchemaMigrator::apply_version_9(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_9);
}

void SchemaMigrator::apply_version_10(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_10);
}

void SchemaMigrator::apply_version_11(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_11);
}

void SchemaMigrator::apply_version_12(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_12);
}

void SchemaMigrator::apply_version_13(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_13);
}

void SchemaMigrator::apply_version_14(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_14);
}

void SchemaMigrator::apply_version_15(SqliteDatabase &database) {
  database.execute(schema_sql::schema_version_15);
}
void SchemaMigrator::apply_version_16(SqliteDatabase &database) { database.execute(schema_sql::schema_version_16); }
void SchemaMigrator::apply_version_17(SqliteDatabase &database) { database.execute(schema_sql::schema_version_17); }
void SchemaMigrator::apply_version_18(SqliteDatabase &database) { database.execute(schema_sql::schema_version_18); }
void SchemaMigrator::apply_version_19(SqliteDatabase &database) { database.execute(schema_sql::schema_version_19); }
void SchemaMigrator::apply_version_20(SqliteDatabase &database) { database.execute(schema_sql::schema_version_20); }
void SchemaMigrator::apply_version_21(SqliteDatabase &database) { database.execute(schema_sql::schema_version_21); }
void SchemaMigrator::apply_version_22(SqliteDatabase &database) { database.execute(schema_sql::schema_version_22); }
void SchemaMigrator::apply_version_23(SqliteDatabase &database) { database.execute(schema_sql::schema_version_23); }
void SchemaMigrator::apply_version_24(SqliteDatabase &database) { database.execute(schema_sql::schema_version_24); }
void SchemaMigrator::apply_version_25(SqliteDatabase &database) { database.execute(schema_sql::schema_version_25); }

} // namespace inde::persistence

#pragma once

namespace inde::persistence {

class SqliteDatabase;

inline constexpr int current_database_schema_version = 12;

class SchemaMigrator {
public:
  void migrate(SqliteDatabase &database) const;
  [[nodiscard]] int current_version(SqliteDatabase &database) const;

private:
  static void apply_version_1(SqliteDatabase &database);
  static void apply_version_2(SqliteDatabase &database);
  static void apply_version_3(SqliteDatabase &database);
  static void apply_version_4(SqliteDatabase &database);
  static void apply_version_5(SqliteDatabase &database);
  static void apply_version_6(SqliteDatabase &database);
  static void apply_version_7(SqliteDatabase &database);
  static void apply_version_8(SqliteDatabase &database);
  static void apply_version_9(SqliteDatabase &database);
  static void apply_version_10(SqliteDatabase &database);
  static void apply_version_11(SqliteDatabase &database);
  static void apply_version_12(SqliteDatabase &database);
};

} // namespace inde::persistence

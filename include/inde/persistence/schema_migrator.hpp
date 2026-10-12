#pragma once

namespace inde::persistence {

class SqliteDatabase;

inline constexpr int current_database_schema_version = 28;

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
  static void apply_version_13(SqliteDatabase &database);
  static void apply_version_14(SqliteDatabase &database);
  static void apply_version_15(SqliteDatabase &database);
  static void apply_version_16(SqliteDatabase &database);
  static void apply_version_17(SqliteDatabase &database);
  static void apply_version_18(SqliteDatabase &database);
  static void apply_version_19(SqliteDatabase &database);
  static void apply_version_20(SqliteDatabase &database);
  static void apply_version_21(SqliteDatabase &database);
  static void apply_version_22(SqliteDatabase &database);
  static void apply_version_23(SqliteDatabase &database);
  static void apply_version_24(SqliteDatabase &database);
  static void apply_version_25(SqliteDatabase &database);
  static void apply_version_26(SqliteDatabase &database);
  static void apply_version_27(SqliteDatabase &database);
  static void apply_version_28(SqliteDatabase &database);
};

} // namespace inde::persistence

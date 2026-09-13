#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace inde::persistence {

class SqliteError final : public std::runtime_error {
public:
  SqliteError(std::string message, int code, int extended_code);

  [[nodiscard]] int code() const noexcept { return code_; }
  [[nodiscard]] int extended_code() const noexcept { return extended_code_; }

private:
  int code_;
  int extended_code_;
};

class SqliteStatement {
public:
  SqliteStatement(sqlite3 *database, std::string_view sql);
  ~SqliteStatement();

  SqliteStatement(const SqliteStatement &) = delete;
  SqliteStatement &operator=(const SqliteStatement &) = delete;
  SqliteStatement(SqliteStatement &&other) noexcept;
  SqliteStatement &operator=(SqliteStatement &&other) noexcept;

  void bind(int index, std::string_view value);
  void bind(int index, std::int64_t value);
  void bind_null(int index);
  void bind_blob(int index, std::span<const std::uint8_t> value);

  // Retorna true quando uma linha está disponível e false em SQLITE_DONE.
  bool step();
  void run();
  void reset();

  [[nodiscard]] bool column_is_null(int index) const;
  [[nodiscard]] std::int64_t column_integer(int index) const;
  [[nodiscard]] std::string column_text(int index) const;
  [[nodiscard]] std::vector<std::uint8_t> column_blob(int index) const;

private:
  void check_column(int index) const;
  [[noreturn]] void throw_error(std::string_view operation, int code) const;

  sqlite3 *database_{nullptr};
  sqlite3_stmt *statement_{nullptr};
};

class SqliteDatabase {
public:
  explicit SqliteDatabase(const std::filesystem::path &path);
  ~SqliteDatabase();

  SqliteDatabase(const SqliteDatabase &) = delete;
  SqliteDatabase &operator=(const SqliteDatabase &) = delete;
  SqliteDatabase(SqliteDatabase &&) = delete;
  SqliteDatabase &operator=(SqliteDatabase &&) = delete;

  void execute(std::string_view sql);
  [[nodiscard]] SqliteStatement prepare(std::string_view sql);
  [[nodiscard]] std::int64_t query_integer(std::string_view sql);
  [[nodiscard]] std::string query_text(std::string_view sql);
  [[nodiscard]] int changes() const noexcept;

private:
  friend class SqliteTransaction;

  [[noreturn]] void throw_error(std::string_view operation, int code) const;
  sqlite3 *database_{nullptr};
};

class SqliteTransaction {
public:
  enum class Mode { Immediate, Deferred };
  explicit SqliteTransaction(SqliteDatabase &database,
                             Mode mode = Mode::Immediate);
  ~SqliteTransaction();

  SqliteTransaction(const SqliteTransaction &) = delete;
  SqliteTransaction &operator=(const SqliteTransaction &) = delete;
  SqliteTransaction(SqliteTransaction &&) = delete;
  SqliteTransaction &operator=(SqliteTransaction &&) = delete;

  void commit();
  void rollback();
  [[nodiscard]] bool active() const noexcept { return active_; }

private:
  SqliteDatabase *database_;
  bool active_{true};
};

} // namespace inde::persistence

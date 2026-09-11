#include "inde/persistence/sqlite_database.hpp"

#include <sqlite3.h>

#include <limits>
#include <utility>

namespace inde::persistence {
namespace {

constexpr int busy_timeout_milliseconds = 2500;

std::string error_message(sqlite3 *database, std::string_view operation,
                          int code) {
  std::string message{"Falha SQLite ao "};
  message += operation;
  message += " (código ";
  message += std::to_string(code);
  message += "): ";
  message += database ? sqlite3_errmsg(database) : sqlite3_errstr(code);
  return message;
}

} // namespace

SqliteError::SqliteError(std::string message, int code, int extended_code)
    : std::runtime_error(std::move(message)), code_(code),
      extended_code_(extended_code) {}

SqliteStatement::SqliteStatement(sqlite3 *database, std::string_view sql)
    : database_(database) {
  if (sql.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::length_error("A operação SQLite excede o tamanho suportado");
  }
  const int result = sqlite3_prepare_v2(database_, sql.data(),
                                        static_cast<int>(sql.size()),
                                        &statement_, nullptr);
  if (result != SQLITE_OK) {
    if (statement_) {
      sqlite3_finalize(statement_);
      statement_ = nullptr;
    }
    throw_error("preparar uma operação", result);
  }
}

SqliteStatement::~SqliteStatement() {
  if (statement_) {
    sqlite3_finalize(statement_);
  }
}

SqliteStatement::SqliteStatement(SqliteStatement &&other) noexcept
    : database_(std::exchange(other.database_, nullptr)),
      statement_(std::exchange(other.statement_, nullptr)) {}

SqliteStatement &
SqliteStatement::operator=(SqliteStatement &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (statement_) {
    sqlite3_finalize(statement_);
  }
  database_ = std::exchange(other.database_, nullptr);
  statement_ = std::exchange(other.statement_, nullptr);
  return *this;
}

void SqliteStatement::bind(int index, std::string_view value) {
  if (value.size() >
      static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::length_error("O texto excede o tamanho suportado pelo SQLite");
  }
  const int result = sqlite3_bind_text(statement_, index, value.data(),
                                       static_cast<int>(value.size()),
                                       SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    throw_error("vincular um texto", result);
  }
}

void SqliteStatement::bind(int index, std::int64_t value) {
  const int result = sqlite3_bind_int64(statement_, index, value);
  if (result != SQLITE_OK) {
    throw_error("vincular um número inteiro", result);
  }
}

void SqliteStatement::bind_null(int index) {
  const int result = sqlite3_bind_null(statement_, index);
  if (result != SQLITE_OK) {
    throw_error("vincular um valor nulo", result);
  }
}

bool SqliteStatement::step() {
  const int result = sqlite3_step(statement_);
  if (result == SQLITE_ROW) {
    return true;
  }
  if (result == SQLITE_DONE) {
    return false;
  }
  throw_error("executar uma operação preparada", result);
}

void SqliteStatement::run() {
  if (step()) {
    throw std::logic_error(
        "Uma operação sem resultado retornou uma linha inesperada");
  }
}

void SqliteStatement::reset() {
  const int reset_result = sqlite3_reset(statement_);
  if (reset_result != SQLITE_OK) {
    throw_error("reiniciar uma operação preparada", reset_result);
  }
  const int clear_result = sqlite3_clear_bindings(statement_);
  if (clear_result != SQLITE_OK) {
    throw_error("limpar os valores de uma operação preparada", clear_result);
  }
}

bool SqliteStatement::column_is_null(int index) const {
  check_column(index);
  return sqlite3_column_type(statement_, index) == SQLITE_NULL;
}

std::int64_t SqliteStatement::column_integer(int index) const {
  check_column(index);
  if (sqlite3_column_type(statement_, index) != SQLITE_INTEGER) {
    throw std::runtime_error("A coluna SQLite consultada não é um inteiro");
  }
  return sqlite3_column_int64(statement_, index);
}

std::string SqliteStatement::column_text(int index) const {
  check_column(index);
  if (sqlite3_column_type(statement_, index) != SQLITE_TEXT) {
    throw std::runtime_error("A coluna SQLite consultada não é um texto");
  }
  const auto *text = sqlite3_column_text(statement_, index);
  const int bytes = sqlite3_column_bytes(statement_, index);
  return {reinterpret_cast<const char *>(text), static_cast<std::size_t>(bytes)};
}

void SqliteStatement::check_column(int index) const {
  if (index < 0 || index >= sqlite3_column_count(statement_)) {
    throw std::out_of_range("Índice de coluna SQLite fora dos limites");
  }
}

[[noreturn]] void SqliteStatement::throw_error(std::string_view operation,
                                                int code) const {
  throw SqliteError(error_message(database_, operation, code), code & 0xff,
                    database_ ? sqlite3_extended_errcode(database_) : code);
}

SqliteDatabase::SqliteDatabase(const std::filesystem::path &path) {
  const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                    SQLITE_OPEN_FULLMUTEX;
  const auto filename = path.string();
  const int open_result =
      sqlite3_open_v2(filename.c_str(), &database_, flags, nullptr);
  if (open_result != SQLITE_OK) {
    const auto message = error_message(database_, "abrir o banco", open_result);
    const int extended = database_ ? sqlite3_extended_errcode(database_)
                                   : open_result;
    if (database_) {
      sqlite3_close_v2(database_);
      database_ = nullptr;
    }
    throw SqliteError(message, open_result & 0xff, extended);
  }

  try {
    sqlite3_extended_result_codes(database_, 1);
    const int timeout_result =
        sqlite3_busy_timeout(database_, busy_timeout_milliseconds);
    if (timeout_result != SQLITE_OK) {
      throw_error("configurar o tempo limite do banco", timeout_result);
    }
    execute("PRAGMA foreign_keys = ON");
    if (query_integer("PRAGMA foreign_keys") != 1) {
      throw std::runtime_error(
          "O SQLite não confirmou a ativação das chaves estrangeiras");
    }
  } catch (...) {
    sqlite3_close_v2(database_);
    database_ = nullptr;
    throw;
  }
}

SqliteDatabase::~SqliteDatabase() {
  if (database_) {
    sqlite3_close_v2(database_);
  }
}

void SqliteDatabase::execute(std::string_view sql) {
  char *error = nullptr;
  const int result = sqlite3_exec(database_, std::string(sql).c_str(), nullptr,
                                  nullptr, &error);
  if (result == SQLITE_OK) {
    return;
  }

  std::string message{"Falha SQLite ao executar uma operação (código "};
  message += std::to_string(result);
  message += "): ";
  message += error ? error : sqlite3_errmsg(database_);
  sqlite3_free(error);
  throw SqliteError(std::move(message), result & 0xff,
                    sqlite3_extended_errcode(database_));
}

SqliteStatement SqliteDatabase::prepare(std::string_view sql) {
  return SqliteStatement(database_, sql);
}

std::int64_t SqliteDatabase::query_integer(std::string_view sql) {
  auto statement = prepare(sql);
  if (!statement.step()) {
    throw std::runtime_error("A consulta SQLite não retornou um resultado");
  }
  const auto value = statement.column_integer(0);
  if (statement.step()) {
    throw std::runtime_error("A consulta SQLite retornou mais de uma linha");
  }
  return value;
}

std::string SqliteDatabase::query_text(std::string_view sql) {
  auto statement = prepare(sql);
  if (!statement.step()) {
    throw std::runtime_error("A consulta SQLite não retornou um resultado");
  }
  auto value = statement.column_text(0);
  if (statement.step()) {
    throw std::runtime_error("A consulta SQLite retornou mais de uma linha");
  }
  return value;
}

int SqliteDatabase::changes() const noexcept {
  return sqlite3_changes(database_);
}

[[noreturn]] void SqliteDatabase::throw_error(std::string_view operation,
                                               int code) const {
  throw SqliteError(error_message(database_, operation, code), code & 0xff,
                    database_ ? sqlite3_extended_errcode(database_) : code);
}

SqliteTransaction::SqliteTransaction(SqliteDatabase &database)
    : database_(&database) {
  database_->execute("BEGIN IMMEDIATE");
}

SqliteTransaction::~SqliteTransaction() {
  if (!active_) {
    return;
  }
  try {
    database_->execute("ROLLBACK");
  } catch (...) {
    // Destruidores não propagam falhas; a transação será revertida no fechamento.
  }
}

void SqliteTransaction::commit() {
  if (!active_) {
    throw std::logic_error("A transação SQLite já foi encerrada");
  }
  database_->execute("COMMIT");
  active_ = false;
}

void SqliteTransaction::rollback() {
  if (!active_) {
    throw std::logic_error("A transação SQLite já foi encerrada");
  }
  database_->execute("ROLLBACK");
  active_ = false;
}

} // namespace inde::persistence

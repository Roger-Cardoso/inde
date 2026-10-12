#include "inde/persistence/sqlite_narrative_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "narrative_repository_support.hpp"

namespace inde::persistence::narrative_detail {

SqliteDatabase open_database(const std::filesystem::path &project_path) {
  const auto path = ProjectDatabaseRepository::database_path(project_path);
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("O banco de dados do projeto não foi encontrado");
  return SqliteDatabase(path);
}

std::string project_id(SqliteDatabase &database) {
  auto statement = database.prepare("SELECT id FROM projects");
  if (!statement.step())
    throw std::runtime_error("O banco não contém a identidade do projeto");
  auto result = statement.column_text(0);
  if (statement.step())
    throw std::runtime_error("O banco contém mais de uma identidade de projeto");
  return result;
}

bool exists(SqliteDatabase &database, std::string_view table,
            const std::string &id) {
  // `table` é escolhido apenas entre literais internos abaixo.
  auto statement = database.prepare("SELECT count(*) FROM " +
                                    std::string(table) + " WHERE id = ?");
  statement.bind(1, id);
  return statement.step() && statement.column_integer(0) == 1;
}

void record_change(SqliteDatabase &database, const std::string &owner,
                   std::string_view command) {
  auto statement = database.prepare(
      "INSERT INTO change_log(id, project_id, command_name, system_created_at) "
      "VALUES (?, ?, ?, ?)");
  statement.bind(1, project::new_uuid());
  statement.bind(2, owner);
  statement.bind(3, command);
  statement.bind(4, project::utc_now());
  statement.run();
}

void bind_optional(SqliteStatement &statement, int index,
                   const std::optional<std::string> &value) {
  if (value)
    statement.bind(index, *value);
  else
    statement.bind_null(index);
}

void require_removed(SqliteDatabase &database, const char *message) {
  if (database.changes() != 1)
    throw std::runtime_error(message);
}

} // namespace inde::persistence::narrative_detail

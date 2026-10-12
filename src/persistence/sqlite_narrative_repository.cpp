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
#include "narrative_query_support.hpp"
#include "narrative_row_codec.hpp"

namespace inde::persistence {

using namespace narrative_detail;

void SqliteNarrativeRepository::initialize(
    const std::filesystem::path &project_path) const {
  auto database = open_database(project_path);
  SchemaMigrator{}.migrate(database);
  const auto owner = project_id(database);
  SqliteTransaction transaction(database);
  auto find = database.prepare(
      "SELECT id, is_builtin FROM entity_types "
      "WHERE project_id = ? AND key = ?");
  auto insert = database.prepare(
      "INSERT INTO entity_types "
      "(id, project_id, key, name, description, is_builtin, created_at, "
      "updated_at) VALUES (?, ?, ?, ?, ?, 1, ?, ?)");
  for (const auto &builtin : project::builtin_entity_types()) {
    find.bind(1, owner);
    find.bind(2, builtin.key);
    if (find.step()) {
      if (find.column_text(0) != builtin.id || find.column_integer(1) != 1)
        throw std::runtime_error(
            "Um tipo interno de entidade divergiu do formato esperado");
      if (find.step())
        throw std::runtime_error("Tipo interno de entidade duplicado");
      find.reset();
      continue;
    }
    find.reset();
    const auto now = project::utc_now();
    insert.bind(1, builtin.id);
    insert.bind(2, owner);
    insert.bind(3, builtin.key);
    insert.bind(4, builtin.name);
    insert.bind(5, builtin.description);
    insert.bind(6, now);
    insert.bind(7, now);
    insert.run();
    insert.reset();
  }
  transaction.commit();
}

} // namespace inde::persistence

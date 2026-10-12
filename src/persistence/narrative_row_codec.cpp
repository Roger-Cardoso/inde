#include "inde/persistence/sqlite_narrative_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>

#include "narrative_row_codec.hpp"

namespace inde::persistence::narrative_detail {

project::EntityType read_entity_type(SqliteStatement &statement) {
  project::EntityType value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_integer(4) != 0, statement.column_text(5),
      statement.column_text(6)};
  project::validate(value);
  return value;
}

project::NarrativeEntity read_entity(SqliteStatement &statement) {
  project::NarrativeEntity value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5)};
  project::validate(value);
  return value;
}

project::RelationType read_relation_type(SqliteStatement &statement) {
  project::RelationType value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4),
      project::relation_directionality_from_string(statement.column_text(5)),
      statement.column_integer(6) != 0, statement.column_text(7),
      statement.column_text(8)};
  project::validate(value);
  return value;
}

project::NarrativeRelation read_relation(SqliteStatement &statement) {
  project::NarrativeRelation value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5),
      statement.column_text(6),
      statement.column_is_null(7)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(7)},
      statement.column_is_null(8)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(8)},
      statement.column_is_null(9)
          ? std::nullopt
          : std::optional<std::string>{statement.column_text(9)}};
  project::validate(value);
  return value;
}

project::EntityWorkScope read_work_scope(SqliteStatement &statement) {
  project::EntityWorkScope value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5)};
  project::validate(value);
  return value;
}

project::EditorialEntityReference
read_editorial_reference(SqliteStatement &statement) {
  project::EditorialEntityReference value{
      statement.column_text(0), statement.column_text(1),
      statement.column_text(2), statement.column_text(3),
      statement.column_text(4), statement.column_text(5),
      statement.column_text(6), statement.column_text(7)};
  project::validate(value);
  return value;
}

} // namespace inde::persistence::narrative_detail

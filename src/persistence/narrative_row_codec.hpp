#pragma once

#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/narrative.hpp"

namespace inde::persistence::narrative_detail {

project::EntityType read_entity_type(SqliteStatement &statement);
project::NarrativeEntity read_entity(SqliteStatement &statement);
project::RelationType read_relation_type(SqliteStatement &statement);
project::NarrativeRelation read_relation(SqliteStatement &statement);
project::EntityWorkScope read_work_scope(SqliteStatement &statement);
project::EditorialEntityReference read_editorial_reference(SqliteStatement &statement);

} // namespace inde::persistence::narrative_detail

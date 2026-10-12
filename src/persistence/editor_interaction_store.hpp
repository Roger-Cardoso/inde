#pragma once
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/editor_interaction.hpp"
namespace inde::persistence {
// Caller owns the surrounding document transaction; never commits separately.
void insert_text_entity(SqliteDatabase &, const std::string &,
                        const project::TextEntityCreation &);
} // namespace inde::persistence

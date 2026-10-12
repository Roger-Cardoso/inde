#pragma once

#include "inde/persistence/sqlite_database.hpp"
#include <string_view>

namespace inde::tests {

void insert_database_project(persistence::SqliteDatabase &database,
                             std::string_view id, std::string_view name);
void downgrade_v15_to_v14(persistence::SqliteDatabase &database);
void downgrade_v14_to_v13(persistence::SqliteDatabase &database);
void downgrade_v12_to_v11(persistence::SqliteDatabase &database);

} // namespace inde::tests

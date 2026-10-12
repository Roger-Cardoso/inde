#pragma once

#include "inde/persistence/sqlite_database.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace inde::persistence::narrative_detail {

SqliteDatabase open_database(const std::filesystem::path &project_path);
std::string project_id(SqliteDatabase &database);
bool exists(SqliteDatabase &database, std::string_view table,
            const std::string &id);
void record_change(SqliteDatabase &database, const std::string &owner,
                   std::string_view command);
void bind_optional(SqliteStatement &statement, int index,
                   const std::optional<std::string> &value);
void require_removed(SqliteDatabase &database, const char *message);

} // namespace inde::persistence::narrative_detail

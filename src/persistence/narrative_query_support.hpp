#pragma once

#include "inde/persistence/narrative_store.hpp"
#include "inde/persistence/sqlite_database.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace inde::persistence::narrative_detail {

std::int64_t checked_limit(std::size_t value, std::size_t maximum,
                           const char *label);
std::int64_t checked_offset(std::size_t value);
std::string like_pattern(const std::string &search);
std::string like_prefix_pattern(const std::string &search);
void validate_filter_ids(const std::vector<std::string> &ids,
                         const char *label);
void append_placeholders(std::string &sql, std::size_t count);
void append_entity_filters(std::string &sql, const EntityQuery &query);
int bind_entity_filters(SqliteStatement &statement, int parameter,
                        const EntityQuery &query);
void validate_entity_filters(const EntityQuery &query);

} // namespace inde::persistence::narrative_detail

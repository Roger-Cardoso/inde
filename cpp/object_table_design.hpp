#pragma once
#include "inde/persistence/json.hpp"
#include <set>
#include <string>
#include <vector>
namespace inde::objects::pilot::table_design {
using Value = persistence::json::Value;
[[nodiscard]] Value defaults();
[[nodiscard]] Value presets();
// Closed, bounded presentation data; never arbitrary CSS or authored content.
void validate(const Value &, const std::vector<std::string> &rows,
              const std::vector<std::string> &columns,
              const std::set<std::string> &cells);
} // namespace inde::objects::pilot::table_design

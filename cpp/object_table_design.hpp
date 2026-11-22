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
// Visible editing never erases the style of temporarily retired axes/cells.
[[nodiscard]] Value project(const Value &,
                            const std::vector<std::string> &all_columns,
                            const std::vector<std::string> &columns,
                            const std::vector<std::string> &rows,
                            const std::set<std::string> &cells);
[[nodiscard]] Value merge_visible(const Value &stored, const Value &visible,
                                  const std::vector<std::string> &all_columns,
                                  const std::vector<std::string> &columns,
                                  const std::vector<std::string> &rows,
                                  const std::set<std::string> &cells);
[[nodiscard]] Value with_inserted_column(const Value &, std::size_t index);
} // namespace inde::objects::pilot::table_design

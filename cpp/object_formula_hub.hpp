#pragma once
#include "inde/persistence/json.hpp"
#include <optional>
#include <string>
namespace inde::objects::formula::hub {
// Source is the sole persistent model. Free writing is never guessed into
// cards.
std::string compile(const persistence::json::Value &);
std::optional<persistence::json::Value> recognize(const std::string &);
void preserve_symbols(const std::string &old_source,
                      const persistence::json::Value &new_model);
} // namespace inde::objects::formula::hub

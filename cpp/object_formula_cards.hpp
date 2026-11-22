#pragma once
#include "inde/persistence/json.hpp"
#include <optional>
#include <string>
namespace inde::objects::formula::cards {
// A reversible authoring view over a strict subset of language 2. Source alone
// is persisted; a failed recognition never rewrites an advanced expression.
std::string compile(const persistence::json::Value &);
std::optional<persistence::json::Value> recognize(const std::string &);
} // namespace inde::objects::formula::cards

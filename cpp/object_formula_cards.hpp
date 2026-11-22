#pragma once
#include "inde/persistence/json.hpp"
#include <optional>
#include <string>
namespace inde::objects::formula::cards {
// A reversible authoring view over a strict subset of language 2. Source alone
// is persisted; a failed recognition never rewrites an advanced expression.
std::string checked_text(const persistence::json::Value &,
                         std::size_t cap = 4096);
std::string quote_text(const std::string &);
std::string emit_operand(const persistence::json::Value &,
                         const std::string &type);
persistence::json::Value read_operand(const std::string &,
                                      const std::string &type);
std::string normalize_lines(const std::string &);
std::string compile(const persistence::json::Value &);
std::optional<persistence::json::Value> recognize(const std::string &);
} // namespace inde::objects::formula::cards

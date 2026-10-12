#pragma once
#include "inde/project/writing.hpp"
namespace inde::application {
struct InlineQuery {std::size_t start{},end{};std::string token;};
[[nodiscard]] std::optional<InlineQuery> inline_query(const std::string &text,std::size_t cursor);
[[nodiscard]] std::vector<std::size_t> inline_candidates(const std::vector<project::DocumentInlineCommand> &,const InlineQuery &);
}

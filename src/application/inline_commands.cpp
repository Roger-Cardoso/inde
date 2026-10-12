#include "inde/application/inline_commands.hpp"
#include <glib.h>
#include <stdexcept>
namespace inde::application {
std::optional<InlineQuery> inline_query(const std::string &text,
                                        std::size_t cursor) {
  if (!g_utf8_validate(text.data(), text.size(), nullptr) ||
      cursor > project::utf8_character_count(text))
    throw std::runtime_error("Cursor inline inválido");
  const char *end = g_utf8_offset_to_pointer(text.data(), cursor), *start = end;
  std::size_t length{};
  while (start > text.data()) {
    auto *previous = g_utf8_find_prev_char(text.data(), start);
    if (g_unichar_isspace(g_utf8_get_char(previous)))
      break;
    if (++length > 128)
      return {};
    start = previous;
  }
  if (start == end)
    return {};
  return InlineQuery{cursor - length, cursor, std::string(start, end)};
}
std::vector<std::size_t>
inline_candidates(const std::vector<project::DocumentInlineCommand> &items,
                  const InlineQuery &q) {
  std::size_t longest{};
  for (const auto &c : items)
    if (q.token.starts_with(c.prefix))
      longest = std::max(longest, c.prefix.size());
  std::vector<std::size_t> result;
  for (std::size_t i = 0; i < items.size(); ++i) {
    const auto &c = items[i];
    const auto trigger = c.prefix + c.keyword;
    if (c.prefix.size() == longest && q.token.starts_with(c.prefix) &&
        (trigger.starts_with(q.token) || q.token == trigger))
      result.push_back(i);
  }
  return result;
}
} // namespace inde::application

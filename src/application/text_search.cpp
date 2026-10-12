#include "inde/application/text_search.hpp"

#include <algorithm>
#include <glib.h>
#include <stdexcept>

namespace inde::application {
namespace {
struct IndexedText {
  std::string folded;
  std::vector<std::size_t> boundaries;
  std::vector<gunichar> scalars;
};
IndexedText index(const std::string &text, bool sensitive, std::stop_token stop) {
  if (!g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr))
    throw std::runtime_error("A busca exige texto UTF-8 válido");
  IndexedText result;
  for (const char *cursor = text.data(), *end = cursor + text.size(); cursor < end;) {
    if (stop.stop_requested()) return {};
    result.boundaries.push_back(result.folded.size());
    result.scalars.push_back(g_utf8_get_char(cursor));
    const char *next = g_utf8_next_char(cursor);
    if (sensitive) result.folded.append(cursor, next);
    else {
      auto *folded = g_utf8_casefold(cursor, next - cursor);
      result.folded += folded;
      g_free(folded);
    }
    cursor = next;
  }
  result.boundaries.push_back(result.folded.size());
  return result;
}
bool word(gunichar value) {
  return g_unichar_isalnum(value) || g_unichar_ismark(value) || value == '_';
}
}
TextSearchResult find_text(const std::string &text, const TextSearchRequest &request,
                           std::stop_token cancellation) {
  if (request.query.empty() || cancellation.stop_requested()) return {};
  if (request.maximum_matches == 0)
    throw std::runtime_error("O limite de resultados da busca precisa ser positivo");
  const auto source = index(text, request.case_sensitive, cancellation);
  const auto query = index(request.query, request.case_sensitive, cancellation);
  if (cancellation.stop_requested()) return {};
  TextSearchResult result;
  std::size_t byte = 0;
  while ((byte = source.folded.find(query.folded, byte)) != std::string::npos) {
    if (cancellation.stop_requested()) return {};
    const auto end_byte = byte + query.folded.size();
    const auto start = std::lower_bound(source.boundaries.begin(), source.boundaries.end(), byte);
    const auto end = std::lower_bound(source.boundaries.begin(), source.boundaries.end(), end_byte);
    const bool complete = start != source.boundaries.end() && *start == byte &&
                          end != source.boundaries.end() && *end == end_byte;
    const auto a = static_cast<std::size_t>(start - source.boundaries.begin());
    const auto b = static_cast<std::size_t>(end - source.boundaries.begin());
    const bool bounded = complete && (!request.whole_word ||
        ((a == 0 || !word(source.scalars[a - 1])) &&
         (b == source.scalars.size() || !word(source.scalars[b]))));
    if (bounded) {
      if (result.matches.size() == request.maximum_matches) {
        result.truncated = true;
        break;
      }
      result.matches.push_back({a, b});
      byte = end_byte;
    } else ++byte;
  }
  return result;
}
} // namespace inde::application

#pragma once

#include <cstddef>
#include <stop_token>
#include <string>
#include <vector>

namespace inde::application {
struct TextMatch {
  std::size_t start{};
  std::size_t end{};
};
struct TextSearchRequest {
  std::string query;
  bool case_sensitive{};
  bool whole_word{};
  std::size_t maximum_matches{100000};
};
struct TextSearchResult {
  std::vector<TextMatch> matches;
  bool truncated{};
};

// Literal, non-overlapping matches. Offsets count Unicode scalars, as GTK does.
// Case folding never permits a match through half of an expanded scalar (ß).
// No regex or implicit Unicode normalization. Whole-word includes marks and _.
[[nodiscard]] TextSearchResult find_text(const std::string &text,
    const TextSearchRequest &request, std::stop_token cancellation = {});
} // namespace inde::application

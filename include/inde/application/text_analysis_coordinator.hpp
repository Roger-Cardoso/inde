#pragma once

#include "inde/project/dictionary.hpp"
#include "inde/project/writing.hpp"
#include "inde/application/text_search.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_set>

namespace inde::application {

// Plain, owned values only. GTK objects and mutable services never cross this
// boundary. One running job and one replaceable pending snapshot bound the queue.
struct TextAnalysisSnapshot {
  std::string document_id;
  std::string text;
  std::shared_ptr<const std::unordered_set<std::string>> custom_words;
  std::optional<TextSearchRequest> search{};
};

struct TextAnalysisResult {
  std::uint64_t generation{};
  std::string document_id;
  std::optional<project::DocumentTextStatistics> statistics;
  std::vector<project::TextIssue> issues;
  std::string error;
  std::optional<TextSearchResult> search{};
  std::chrono::nanoseconds elapsed{};
};

class TextAnalysisCoordinator final {
public:
  using Analyzer = std::function<TextAnalysisResult(
      const TextAnalysisSnapshot &, std::stop_token)>;
  explicit TextAnalysisCoordinator(Analyzer analyzer = {});
  ~TextAnalysisCoordinator();
  TextAnalysisCoordinator(const TextAnalysisCoordinator &) = delete;
  TextAnalysisCoordinator &operator=(const TextAnalysisCoordinator &) = delete;

  // Invalidate immediately on an edit, not only when its debounce expires.
  void invalidate();
  std::uint64_t submit(TextAnalysisSnapshot snapshot);
  [[nodiscard]] std::optional<TextAnalysisResult> take_latest();
  // Includes a completed result awaiting delivery (avoids a polling race).
  [[nodiscard]] bool busy() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace inde::application

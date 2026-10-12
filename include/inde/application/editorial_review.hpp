#pragma once
#include "inde/project/writing.hpp"
#include <stop_token>
namespace inde::application {
struct EditorialState {
  std::string state{"draft"}, reviewer, note, decided_at;
  bool outdated{};
};
struct ReviewCounts { std::size_t open_comments{}, resolved_comments{}, pending_suggestions{}; };
[[nodiscard]] ReviewCounts review_counts(const project::Document &);
[[nodiscard]] EditorialState editorial_state(const project::Document &);
[[nodiscard]] project::Document set_editorial_state(const project::Document &, const std::string &state,
                                                   const std::string &reviewer, const std::string &note);
[[nodiscard]] project::Document decide_comment(const project::Document &, const std::string &id, bool resolved);
[[nodiscard]] project::Document propose_change(const project::Document &, std::size_t start, std::size_t end,
                                              const std::string &replacement, const std::string &note);
[[nodiscard]] project::Document decide_suggestion(const project::Document &, const std::string &id, bool accept);
[[nodiscard]] std::string review_field(const project::TextAnnotation &, const std::string &key,
                                      const std::string &fallback = {});
enum class DifferenceKind { Equal, Removed, Added };
struct DifferenceLine { DifferenceKind kind; std::string text; std::size_t left_line{}, right_line{}; };
struct DocumentComparison {
  std::vector<DifferenceLine> lines;
  std::size_t removed{}, added{};
  bool coarse{}, layers_differ{};
};
// Exact LF-preserving lines; bounded LCS, no normalization or implicit merge.
[[nodiscard]] DocumentComparison compare_documents(const project::Document &, const project::Document &,
                                                   std::stop_token = {});
}

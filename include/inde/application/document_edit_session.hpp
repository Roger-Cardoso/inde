#pragma once

#include "inde/application/text_search.hpp"
#include "inde/project/writing.hpp"
#include <optional>
#include <cstdint>
#include <string>

namespace inde::application {
struct TextSelection {
  std::size_t cursor{};
  std::size_t bound{};
};
struct DocumentEditState {
  project::Document document;
  TextSelection selection;
};
struct EditCommand {
  std::size_t byte_offset{};
  std::string removed;
  std::string inserted;
};
[[nodiscard]] EditCommand text_delta(const std::string &before, const std::string &after);
struct EditTransaction {
  std::string label;
  DocumentEditState before;
  DocumentEditState after;
  std::optional<EditCommand> delta;
  std::uint64_t before_hash{};
  std::uint64_t after_hash{};
  [[nodiscard]] std::size_t retained_text_bytes() const;
};

// Pure editing state: no GTK, persistence, filesystem or implicit semantics.
class DocumentEditSession {
public:
  explicit DocumentEditSession(std::size_t text_budget = 64 * 1024 * 1024);
  void open(DocumentEditState state);
  void reset();
  void note_mutation();
  void update_selection(TextSelection selection);
  bool commit(DocumentEditState state, std::string label = "Digitação");
  void mark_saved(const project::Document &document);
  void acknowledge_saved(const project::Document &document);
  [[nodiscard]] bool dirty() const;
  [[nodiscard]] bool pending() const { return pending_; }
  [[nodiscard]] bool can_undo() const { return !undo_.empty() || pending_; }
  [[nodiscard]] bool can_redo() const { return !redo_.empty(); }
  [[nodiscard]] std::optional<DocumentEditState> undo();
  [[nodiscard]] std::optional<DocumentEditState> redo();
  [[nodiscard]] std::size_t history_text_bytes() const;
  [[nodiscard]] std::size_t undo_count() const { return undo_.size(); }
private:
  std::optional<DocumentEditState> state_;
  project::Document saved_layers_;
  std::uint64_t saved_hash_{};
  std::vector<EditTransaction> undo_, redo_;
  std::size_t budget_;
  bool pending_{};
};

// First-character formatting inheritance; anchors retain identities and use
// inclusive left/right affinity at a replacement, matching the existing marks.
// Matches must be ordered, non-overlapping and in bounds. Invalid plans fail
// before mutation; empty replacement is deletion. No truncated plans accepted.
[[nodiscard]] DocumentEditState replace_text(const DocumentEditState &before,
    const TextSearchResult &matches, const std::string &replacement);
[[nodiscard]] DocumentEditState replace_selection(const DocumentEditState &before,
    std::size_t start, std::size_t end, const std::string &replacement);
} // namespace inde::application

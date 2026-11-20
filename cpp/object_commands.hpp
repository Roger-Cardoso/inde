#pragma once
#include "object_model.hpp"
#include <variant>

namespace inde::objects {
// Anchors name an unselected child in the destination; nullopt means append.
// Selection order is always the source reading order, never caller order.
struct ReorderOccurrences {
  std::string layout_id;
  std::vector<std::string> occurrence_ids;
  std::optional<std::string> before;
};
enum class FramePolicy { Preserve, Clear, Replace };
struct FrameAssignment {
  std::string occurrence_id;
  Frame frame;
};
struct TransferOccurrences {
  std::vector<std::string> occurrence_ids; // One source layout per command.
  std::string destination_layout_id;
  std::optional<std::string> before;
  FramePolicy frames{FramePolicy::Preserve};
  std::vector<FrameAssignment> replacements{};
};
struct SetFrame { std::string occurrence_id; Frame frame; };
struct SetPaintOrder { std::string layout_id; std::vector<std::string> order; };
struct ChangeLayout {
  std::string layout_id;
  LayoutKind kind{LayoutKind::Stack};
  std::size_t columns{};
  FramePolicy frames{FramePolicy::Preserve};
  std::vector<FrameAssignment> replacements{};
};
using Command = std::variant<ReorderOccurrences, TransferOccurrences, SetFrame,
                             SetPaintOrder, ChangeLayout>;
struct CommandResult {
  bool accepted{};
  bool changed{};
  std::uint64_t revision{};
  std::vector<Diagnostic> diagnostics;
};

// In-memory only. No text/content authority, GTK, SQLite or recovery side effects.
// Invalid initial state throws invalid_argument. Allocation failures may throw;
// state and history remain unchanged. Not a cross-thread synchronization API.
class CompositionEditor {
public:
  explicit CompositionEditor(Composition initial,
      std::size_t history_budget_bytes = 32 * 1024 * 1024,
      std::size_t history_limit = 64);
  [[nodiscard]] const Composition &current() const { return state_; }
  [[nodiscard]] std::uint64_t revision() const { return revision_; }
  [[nodiscard]] bool can_undo() const { return !undo_.empty(); }
  [[nodiscard]] bool can_redo() const { return !redo_.empty(); }
  [[nodiscard]] std::size_t history_bytes() const;
  [[nodiscard]] CommandResult apply(std::uint64_t expected,
                                    const std::vector<Command> &commands);
  [[nodiscard]] CommandResult undo(std::uint64_t expected);
  [[nodiscard]] CommandResult redo(std::uint64_t expected);
private:
  Composition state_;
  std::uint64_t revision_{}; // Monotonic even across undo/redo; no ABA reuse.
  std::vector<Composition> undo_, redo_;
  std::size_t budget_, limit_;
  void trim_history();
  [[nodiscard]] CommandResult replay(std::uint64_t expected, bool forward);
};
} // namespace inde::objects

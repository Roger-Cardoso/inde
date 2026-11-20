#pragma once
#include "object_change_plan.hpp"

namespace inde::objects::planning {
// Budget counts unique retained states: canonical source bytes and logical graph
// storage. It is not RSS. Every change must retain a complete reversible pair.
struct SessionHistoryLimits {
  std::size_t bytes{64 * 1024 * 1024};
  std::size_t steps{64};
};
struct SessionResult {
  bool accepted{}, changed{};
  std::uint64_t revision{};
  std::vector<PlanDiagnostic> diagnostics;
};
class DocumentChangeSession;
class CompositionChangeSession;
class PreparedDocumentChange final {
public:
  PreparedDocumentChange(const PreparedDocumentChange &) = delete;
  PreparedDocumentChange &operator=(const PreparedDocumentChange &) = delete;
  PreparedDocumentChange(PreparedDocumentChange &&) = delete;
  PreparedDocumentChange &operator=(PreparedDocumentChange &&) = delete;
  [[nodiscard]] const DocumentChangePlan &plan() const { return *plan_; }
  [[nodiscard]] std::uint64_t revision() const { return revision_; }
  [[nodiscard]] bool consumed() const { return consumed_; }
private:
  friend class DocumentChangeSession;
  PreparedDocumentChange() = default;
  std::unique_ptr<DocumentChangePlan> plan_;
  std::string session_id_;
  std::uint64_t revision_{};
  bool consumed_{};
};
class PreparedCompositionChange final {
public:
  PreparedCompositionChange(const PreparedCompositionChange &) = delete;
  PreparedCompositionChange &operator=(const PreparedCompositionChange &) = delete;
  PreparedCompositionChange(PreparedCompositionChange &&) = delete;
  PreparedCompositionChange &operator=(PreparedCompositionChange &&) = delete;
  [[nodiscard]] const CompositionChangePlan &plan() const { return *plan_; }
  [[nodiscard]] std::uint64_t revision() const { return revision_; }
  [[nodiscard]] bool consumed() const { return consumed_; }
private:
  friend class CompositionChangeSession;
  PreparedCompositionChange() = default;
  std::unique_ptr<CompositionChangePlan> plan_;
  std::string session_id_;
  std::uint64_t revision_{};
  bool consumed_{};
};
template<class Ticket> struct PreparationResult {
  std::unique_ptr<Ticket> ticket;
  std::vector<PlanDiagnostic> diagnostics;
};
// Owns the working authority in memory. No external edits, pending typing,
// saving, rebasing, persistent revisions or recovery records in this API.
// Views from current()/graph() must be treated as invalid after mutation.
// Caller inputs must be stable during a call; sessions are not thread-safe.
class DocumentChangeSession final {
public:
  explicit DocumentChangeSession(const compat::DocumentReadProjection &,
      application::TextSelection selection = {}, SessionHistoryLimits = {});
  ~DocumentChangeSession();
  DocumentChangeSession(const DocumentChangeSession &) = delete;
  DocumentChangeSession &operator=(const DocumentChangeSession &) = delete;
  [[nodiscard]] const application::DocumentEditState &current() const;
  [[nodiscard]] const Composition &graph() const;
  [[nodiscard]] const compat::DocumentSourceStamp &origin() const;
  [[nodiscard]] std::uint64_t revision() const;
  [[nodiscard]] bool source_dirty() const;
  [[nodiscard]] bool dirty() const;
  [[nodiscard]] bool can_undo() const;
  [[nodiscard]] bool can_redo() const;
  [[nodiscard]] std::size_t history_bytes() const;
  [[nodiscard]] PreparationResult<PreparedDocumentChange> prepare(const ChangeRequest &) const;
  // Explicit application of this exact reviewed ticket, once. The provided
  // external origin must still match the ORIGINAL opening capture, not working.
  [[nodiscard]] SessionResult apply(PreparedDocumentChange &, const std::string &project_id,
      const project::Document &external_origin);
  [[nodiscard]] SessionResult undo(std::uint64_t expected, const std::string &project_id,
      const project::Document &external_origin);
  [[nodiscard]] SessionResult redo(std::uint64_t expected, const std::string &project_id,
      const project::Document &external_origin);
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  SessionResult replay(std::uint64_t, const std::string &, const project::Document &, bool);
};
class CompositionChangeSession final {
public:
  explicit CompositionChangeSession(const compat::CompositionReadProjection &, SessionHistoryLimits = {});
  ~CompositionChangeSession();
  CompositionChangeSession(const CompositionChangeSession &) = delete;
  CompositionChangeSession &operator=(const CompositionChangeSession &) = delete;
  [[nodiscard]] const project::Composition &current() const;
  [[nodiscard]] const Composition &graph() const;
  [[nodiscard]] const compat::CompositionSourceStamp &origin() const;
  [[nodiscard]] std::uint64_t revision() const;
  [[nodiscard]] bool source_dirty() const;
  [[nodiscard]] bool dirty() const;
  [[nodiscard]] bool can_undo() const;
  [[nodiscard]] bool can_redo() const;
  [[nodiscard]] std::size_t history_bytes() const;
  [[nodiscard]] PreparationResult<PreparedCompositionChange> prepare(const ChangeRequest &) const;
  [[nodiscard]] SessionResult apply(PreparedCompositionChange &, const project::StoredComposition &external_origin);
  [[nodiscard]] SessionResult undo(std::uint64_t expected, const project::StoredComposition &external_origin);
  [[nodiscard]] SessionResult redo(std::uint64_t expected, const project::StoredComposition &external_origin);
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  SessionResult replay(std::uint64_t, const project::StoredComposition &, bool);
};
} // namespace inde::objects::planning

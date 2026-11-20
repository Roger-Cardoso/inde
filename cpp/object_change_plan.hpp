#pragma once
#include "object_commands.hpp"
#include "object_document_adapter.hpp"
#include "object_composition_adapter.hpp"
#include "inde/application/document_edit_session.hpp"

namespace inde::objects::planning {
// Explicit intent; never inferred from coordinates, text or command type.
enum class ChangePurpose { VisualOnly, AuthorialReorder };
struct ChangeRequest {
  ChangePurpose purpose{ChangePurpose::VisualOnly};
  std::vector<Command> commands;
};
struct PlanDiagnostic {
  std::string code, subject_id;
  std::string reason; // Private authoring diagnostic; may include owner-engine refusal.
};
struct PlanReview {
  ChangePurpose purpose{ChangePurpose::VisualOnly};
  bool graph_changed{}, source_changed{};
};
class DocumentChangePlan;
class CompositionChangePlan;
template<class Plan> struct PlanResult {
  std::unique_ptr<Plan> plan;
  std::vector<PlanDiagnostic> diagnostics; // Refusal never returns a partial plan.
};
using DocumentPlanResult = PlanResult<DocumentChangePlan>;
using CompositionPlanResult = PlanResult<CompositionChangePlan>;
[[nodiscard]] DocumentPlanResult plan_document_change(const compat::DocumentReadProjection &,
    const std::string &current_project_id, const project::Document &current,
    const ChangeRequest &, application::TextSelection selection);
[[nodiscard]] CompositionPlanResult plan_composition_change(const compat::CompositionReadProjection &,
    const project::StoredComposition &current, const ChangeRequest &);

// Presentation-aware overloads preserve only validated presentation changes of
// the same flat capture. Authorial intent requires source-aligned reading order.
[[nodiscard]] DocumentPlanResult plan_document_change(const compat::DocumentReadProjection &,
    const std::string &, const project::Document &, const ChangeRequest &,
    application::TextSelection, const Composition &presentation);
[[nodiscard]] CompositionPlanResult plan_composition_change(const compat::CompositionReadProjection &,
    const project::StoredComposition &, const ChangeRequest &, const Composition &presentation);

// Immutable, authoring-private, independently retained preview. No save/apply,
// transaction, signature, access grant or durable undo. Recheck is in-memory.
class DocumentChangePlan final {
public:
  [[nodiscard]] const PlanReview &review() const { return review_; }
  [[nodiscard]] const compat::DocumentSourceStamp &source() const { return source_; }
  [[nodiscard]] const Composition &before_graph() const { return before_graph_; }
  [[nodiscard]] const Composition &after_graph() const { return after_graph_; }
  [[nodiscard]] const application::DocumentEditState &before() const { return before_; }
  [[nodiscard]] const application::DocumentEditState &preview() const { return after_; }
  [[nodiscard]] const std::string &preview_sha256() const { return preview_sha256_; }
  [[nodiscard]] compat::SourceCheck check_source(const std::string &project_id, const project::Document &) const;
private:
  DocumentChangePlan() = default;
  friend DocumentPlanResult plan_document_change(const compat::DocumentReadProjection &, const std::string &,
      const project::Document &, const ChangeRequest &, application::TextSelection);
  friend DocumentPlanResult plan_document_change(const compat::DocumentReadProjection &, const std::string &,
      const project::Document &, const ChangeRequest &, application::TextSelection, const Composition &);
  PlanReview review_;
  compat::DocumentSourceStamp source_;
  Composition before_graph_, after_graph_;
  application::DocumentEditState before_, after_;
  std::string preview_sha256_;
};
class CompositionChangePlan final {
public:
  [[nodiscard]] const PlanReview &review() const { return review_; }
  [[nodiscard]] const compat::CompositionSourceStamp &source() const { return source_; }
  [[nodiscard]] const Composition &before_graph() const { return before_graph_; }
  [[nodiscard]] const Composition &after_graph() const { return after_graph_; }
  [[nodiscard]] const project::Composition &before() const { return before_; }
  [[nodiscard]] const project::Composition &preview() const { return after_; }
  // A candidate hash, NOT a StoredComposition or a future persisted revision.
  [[nodiscard]] const std::string &preview_sha256() const { return preview_sha256_; }
  [[nodiscard]] const experimental::TextProjection &before_reading() const { return before_reading_; }
  [[nodiscard]] const experimental::TextProjection &preview_reading() const { return after_reading_; }
  [[nodiscard]] const std::vector<std::string> &source_order() const { return source_order_; }
  [[nodiscard]] const std::vector<std::string> &preview_order() const { return preview_order_; }
  [[nodiscard]] compat::CompositionSourceCheck check_source(const project::StoredComposition &) const;
private:
  CompositionChangePlan() = default;
  friend CompositionPlanResult plan_composition_change(const compat::CompositionReadProjection &,
      const project::StoredComposition &, const ChangeRequest &);
  friend CompositionPlanResult plan_composition_change(const compat::CompositionReadProjection &,
      const project::StoredComposition &, const ChangeRequest &, const Composition &);
  PlanReview review_;
  compat::CompositionSourceStamp source_;
  Composition before_graph_, after_graph_;
  project::Composition before_, after_;
  experimental::TextProjection before_reading_, after_reading_;
  std::vector<std::string> source_order_, preview_order_;
  std::string preview_sha256_;
};
} // namespace inde::objects::planning

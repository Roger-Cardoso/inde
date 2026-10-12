#pragma once
#include "inde/application/project_session.hpp"
#include "inde/project/composition.hpp"
#include "inde/project/project_operations.hpp"
#include "inde/project/structure_model.hpp"
#include <optional>
#include <string>
#include <vector>
namespace inde::application {
enum class BuilderOrder { Narrative, WritingQueue };
struct BuilderContext {
  std::string unit_id, location_id, location_name;
  std::optional<std::int64_t> writing_position;
};
struct BuilderRealization {
  std::string id, unit_id, document_id, title, note;
  std::string anchor_id{}, composition_id{}, occurrence_id{}, status{"current"},
      observed_sha{}, current_sha{}, review_state{"unchecked"}, provenance{},
      table{"narrative_realizations"};
  std::size_t start{}, end{};
};
struct BuilderNotice {
  std::string key, kind, unit_id, other_unit_id, title, evidence, evidence_sha,
      exception_note{};
};
struct BuilderPresence { std::string unit_id, entity_id; };
struct BuilderRelation {
  std::string id, unit_id, relation_id, title;
};
struct BuilderDate {
  std::string id, unit_id, line_id, reference_id, role, note, meaning;
};
struct BuilderScope {
  std::string link_id, scope;
};
struct BuilderDifference {
  std::string dimension, unit_id, other_unit_id, title, detail;
};
struct BuilderComparison {
  std::string source_name, target_name;
  std::vector<BuilderDifference> differences;
  bool against_recorded_origin{};
};
struct BuilderMoveImpact {
  std::vector<std::string> connected_units, warnings;
  std::size_t documents{}, dates{};
};
// Bounded observation, not a saved approval. No text is copied into the plan.
struct NarrativeBuilderSnapshot {
  std::string project_id, structure_id, revision, work_id, name, description;
  std::string provenance_json;
  std::vector<project::NarrativeUnit> units;
  std::vector<project::NarrativeLine> lines;
  std::vector<BuilderContext> contexts;
  std::vector<BuilderRealization> realizations;
  std::vector<BuilderRelation> relations;
  std::vector<project::NarrativeUnitLine> memberships;
  std::vector<project::NarrativeUnitEntity> entities;
  std::vector<project::NarrativeLink> links;
  std::vector<project::NarrativeRoleAssignment> roles;
  std::vector<BuilderDate> dates;
  std::vector<BuilderScope> scopes;
  std::vector<BuilderPresence> presences;
};
class NarrativeBuilder {
public:
  explicit NarrativeBuilder(ProjectSession &s) : session_(s) {}
  NarrativeBuilderSnapshot snapshot(const std::string &structure) const;
  project::NarrativeUnit create_unit(const NarrativeBuilderSnapshot &,
                                     project::NarrativeUnit,
                                     const std::string &line = {}) const;
  void update_unit(const NarrativeBuilderSnapshot &,
                   project::NarrativeUnit) const;
  void update_line(const NarrativeBuilderSnapshot &,
                   project::NarrativeLine) const;
  void move_unit(const NarrativeBuilderSnapshot &, const std::string &,
                 BuilderOrder, int direction) const;
  void queue_unit(const NarrativeBuilderSnapshot &, const std::string &,
                  bool include) const;
  void set_location(const NarrativeBuilderSnapshot &, const std::string &,
                    const std::string &location) const;
  void attach_document(const NarrativeBuilderSnapshot &, const std::string &,
                       const std::string &) const;
  void attach_anchor(const NarrativeBuilderSnapshot &, const std::string &unit,
                     const project::Document &expected, const std::string &anchor,
                     const std::string &replace = {}) const;
  void attach_range(const NarrativeBuilderSnapshot &, const std::string &unit,
                    const project::Document &expected, std::size_t start,
                    std::size_t end, const std::string &label) const;
  void attach_occurrence(const NarrativeBuilderSnapshot &, const std::string &unit,
                         const project::StoredComposition &expected,
                         const std::string &occurrence,
                         const std::string &replace = {}) const;
  void review_realization(const NarrativeBuilderSnapshot &, const std::string &id,
                          const std::string &note, bool divergent) const;
  void make_independent(const NarrativeBuilderSnapshot &, const std::string &id,
                        const std::string &title) const;
  std::vector<BuilderNotice> continuity(const NarrativeBuilderSnapshot &) const;
  void set_exception(const NarrativeBuilderSnapshot &, const BuilderNotice &,
                     const std::string &note) const;
  void declare_presence(const NarrativeBuilderSnapshot &, const std::string &unit,
                        const std::string &entity, bool present) const;
  std::vector<project::RemovalUse> text_uses(const std::string &document,
      const std::string &composition = {}, const std::string &occurrence = {}) const;
  std::string create_document(const NarrativeBuilderSnapshot &,
                              const std::string &, std::string title) const;
  void attach_relation(const NarrativeBuilderSnapshot &, const std::string &,
                       const std::string &) const;
  void detach(const NarrativeBuilderSnapshot &, const std::string &kind,
              const std::string &id) const;
  project::NarrativeStructure derive(const NarrativeBuilderSnapshot &,
                                     std::string name,
                                     bool derived = true) const;
  BuilderComparison compare(const std::string &source,
                            const std::string &target) const;
  BuilderComparison compare_to_origin(const std::string &target) const;
  void move_in_line(const NarrativeBuilderSnapshot &, const std::string &unit,
                    const std::string &line, int direction) const;
  void set_dependency_scope(const NarrativeBuilderSnapshot &,
                            const std::string &link,
                            const std::string &scope) const;
  BuilderMoveImpact preview_move(const NarrativeBuilderSnapshot &,
                                 const std::string &, BuilderOrder,
                                 int direction) const;

private:
  const project::Project &owner() const;
  ProjectSession &session_;
};
} // namespace inde::application

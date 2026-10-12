#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace inde::project {
enum class RemovalKind {
  Document,
  Entity,
  Relation,
  DocumentGroup,
  Work,
  IntellectualProperty,
  EditorialBranch,
  CalendarSystem,
  CartographicMap,
  EntityType,
  RelationType,
  StructuralType,
  TimeAxis,
  TimePoint,
  TemporalReference,
  StructureModel,
  EditorialStructure,
  NarrativeStructure,
  NarrativeLine,
  NarrativeUnit,
  NarrativeRole,
  DocumentAnchor,
  TextAnnotation,
  EntityWorkScope
};
// Read-only directions to an authorial use, not permission to erase it.
struct RemovalUse {
  std::string table, record_id, location_table, location_id;
  std::string name, path, guidance, work_id, structure_id, axis_id, map_id,
      model_layer, annotation_id;
  std::size_t text_offset{};
};
struct OperationImpact {
  std::string table, label;
  std::size_t inserted{}, updated{}, removed{};
  std::vector<std::string> examples, fields;
};
struct ProjectOperation {
  std::string id, label, created_at, state, capture_project_id;
  std::size_t bytes{};
  bool compensation{};
};
// A transient observation, never a saved approval or executable SQL.
struct OperationPreview {
  std::string project_id, operation_id, target_id, fingerprint, description;
  std::string observed_at;
  RemovalKind removal{RemovalKind::Document};
  bool reversing{}, allowed{};
  std::string blocked_reason;
  std::vector<OperationImpact> impacts;
  std::vector<RemovalUse> uses;
};
} // namespace inde::project

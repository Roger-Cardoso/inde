#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace inde::project {
struct Document;
enum class SemanticFieldKind { Text, Enum, Integer, Boolean, Entity };
struct SemanticFieldDefinition {
  std::string key, label;
  SemanticFieldKind kind{SemanticFieldKind::Text};
  bool required{}, multiple{};
  std::vector<std::string> values, entity_type_keys;
  bool operator==(const SemanticFieldDefinition &) const = default;
};
struct AnnotationTypeDefinition {
  std::string key, label, description;
  std::uint32_t version{1};
  bool system{}, allow_overlap{true};
  std::vector<SemanticFieldDefinition> fields;
  bool operator==(const AnnotationTypeDefinition &) const = default;
};
struct SemanticFieldValue {
  SemanticFieldKind kind{SemanticFieldKind::Text};
  std::vector<std::string> values;
  bool operator==(const SemanticFieldValue &) const = default;
};
enum class AnchorResolution { Resolved, Ambiguous, Orphaned };
enum class AnchorGravity { Left, Right };
struct SemanticAnchorEvidence {
  std::string anchor_id;
  AnchorResolution state{AnchorResolution::Resolved};
  AnchorGravity start_gravity{AnchorGravity::Left},
      end_gravity{AnchorGravity::Right};
  std::uint64_t revision{};
  std::string selected_hash, before_hash, after_hash, reason;
  bool operator==(const SemanticAnchorEvidence &) const = default;
};
enum class AnnotationScope { Interval, Document };
// A manual textual context, not an event, presence, date or elapsed duration.
// Missing end means exactly one point; two endpoints form a closed interval.
struct SemanticTemporalBinding {
  std::string id, fictional_axis_id, fictional_start_point_id;
  std::optional<std::string> fictional_end_point_id;
  std::string origin{"human"}, source, notes, created_at;
  bool operator==(const SemanticTemporalBinding &) const = default;
};
struct SemanticDateBinding {
  std::string id, reference_id, role, notes, created_at;
  bool operator==(const SemanticDateBinding &) const = default;
};
struct TextAnnotation {
  std::string id, anchor_id, type_key;
  std::uint32_t type_version{1};
  std::map<std::string, SemanticFieldValue> fields;
  std::string status{"active"}, origin{"human"}, source, notes, opaque_payload,
      created_at, updated_at;
  AnnotationScope scope{AnnotationScope::Interval};
  std::vector<SemanticTemporalBinding> fictional_time;
  std::vector<SemanticDateBinding> dates;
  bool operator==(const TextAnnotation &) const = default;
};
struct AnnotationRelation {
  std::string id, source_annotation_id, target_annotation_id;
  std::string type_key{"inde.semantic.reference"}, origin{"human"}, notes,
      created_at;
  bool operator==(const AnnotationRelation &) const = default;
};
struct DocumentSemantics {
  std::vector<SemanticAnchorEvidence> anchors;
  std::vector<TextAnnotation> annotations;
  std::vector<AnnotationRelation> relations;
  bool operator==(const DocumentSemantics &) const = default;
};
struct SemanticDiagnostic {
  std::string annotation_id, code, message;
  bool integrity{};
};
[[nodiscard]] std::string to_string(SemanticFieldKind);
[[nodiscard]] std::string to_string(AnnotationScope);
[[nodiscard]] AnnotationScope annotation_scope(const std::string &);
[[nodiscard]] SemanticFieldKind semantic_field_kind(const std::string &);
[[nodiscard]] std::string to_string(AnchorResolution);
[[nodiscard]] AnchorResolution anchor_resolution(const std::string &);
[[nodiscard]] std::string to_string(AnchorGravity);
[[nodiscard]] AnchorGravity anchor_gravity(const std::string &);
[[nodiscard]] bool same_semantics(const DocumentSemantics &,
                                  const DocumentSemantics &);
[[nodiscard]] const std::vector<AnnotationTypeDefinition> &
builtin_annotation_types();
[[nodiscard]] const AnnotationTypeDefinition *
find_annotation_type(const std::vector<AnnotationTypeDefinition> &,
                     const std::string &, std::uint32_t);
void validate(const AnnotationTypeDefinition &);
void validate_semantic_structure(const Document &);
[[nodiscard]] std::vector<SemanticDiagnostic>
semantic_diagnostics(const Document &,
                     const std::vector<AnnotationTypeDefinition> &);
void validate_semantic_key(const std::string &);
} // namespace inde::project

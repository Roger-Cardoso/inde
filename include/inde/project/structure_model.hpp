#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace inde::project {

enum class StructureLayer { Editorial, Narrative };
enum class StructureCreationKind {
  Migrated,
  Blank,
  Instantiated,
  Duplicated,
  Derived
};
enum class StructureModelItemKind {
  EditorialNode,
  NarrativeLine,
  NarrativeUnit
};
enum class NarrativeLinkKind { Precedes, Causes, Alternative, Depends };

struct StructureModel {
  std::string id;
  StructureLayer layer{StructureLayer::Editorial};
  std::string key;
  std::string name;
  std::string description;
  bool is_builtin{};
  std::string created_at;
  std::string updated_at;
};

struct StructureModelItem {
  std::string id;
  std::string model_id;
  std::optional<std::string> parent_id;
  StructureModelItemKind kind{StructureModelItemKind::EditorialNode};
  std::string type_key;
  std::string designator;
  std::string title;
  std::string summary;
  std::string purpose;
  std::string perspective;
  std::int64_t position{};
  std::string created_at;
  std::string updated_at;
};

struct StructureModelItemLink {
  std::string id;
  std::string model_id;
  std::string source_item_id;
  std::string target_item_id;
  // membership ou um valor de NarrativeLinkKind.
  std::string kind;
  std::string label;
  std::int64_t position{};
};

struct EditorialStructure {
  std::string id;
  std::string work_id;
  std::string name;
  std::string description;
  std::optional<std::string> model_id;
  std::optional<std::string> derived_from_id;
  StructureCreationKind creation_kind{StructureCreationKind::Blank};
  bool is_active{};
  std::string created_at;
  std::string updated_at;
};

struct NarrativeStructure {
  std::string id;
  std::string work_id;
  std::string name;
  std::string description;
  std::optional<std::string> model_id;
  std::optional<std::string> derived_from_id;
  StructureCreationKind creation_kind{StructureCreationKind::Blank};
  bool is_active{};
  std::string created_at;
  std::string updated_at;
};

struct NarrativeLine {
  std::string id;
  std::string structure_id;
  std::string name;
  std::string description;
  std::int64_t position{};
  std::string created_at;
  std::string updated_at;
};

struct NarrativeUnit {
  std::string id;
  std::string structure_id;
  std::string designator;
  std::string title;
  std::string summary;
  std::string purpose;
  std::string perspective;
  std::int64_t position{};
  std::string created_at;
  std::string updated_at;
};

struct NarrativeUnitLine {
  std::string unit_id;
  std::string line_id;
  std::int64_t position{};
};

struct NarrativeUnitEntity {
  std::string unit_id;
  std::string entity_id;
  std::string role;
};

struct NarrativeLink {
  std::string id;
  std::string structure_id;
  std::string source_unit_id;
  std::string target_unit_id;
  NarrativeLinkKind kind{NarrativeLinkKind::Precedes};
  std::string label;
  std::string created_at;
  std::string updated_at;
};

struct NarrativeRole {
  std::string id;
  std::string name;
  std::string description;
  bool is_builtin{};
  std::string created_at;
  std::string updated_at;
};

struct NarrativeRoleAssignment {
  std::string id;
  std::string role_id;
  std::string entity_id;
  std::string work_id;
  std::optional<std::string> structure_id;
  std::optional<std::string> unit_id;
  std::string notes;
  std::string created_at;
  std::string updated_at;
};

struct StructureActivationImpact {
  std::string current_structure_id;
  std::string target_structure_id;
  std::size_t affected_documents{};
  std::size_t target_units{};
};

[[nodiscard]] std::string to_string(StructureLayer value);
[[nodiscard]] StructureLayer
structure_layer_from_string(const std::string &value);
[[nodiscard]] std::string to_string(StructureCreationKind value);
[[nodiscard]] StructureCreationKind
structure_creation_kind_from_string(const std::string &value);
[[nodiscard]] std::string to_string(StructureModelItemKind value);
[[nodiscard]] StructureModelItemKind
structure_model_item_kind_from_string(const std::string &value);
[[nodiscard]] std::string to_string(NarrativeLinkKind value);
[[nodiscard]] NarrativeLinkKind
narrative_link_kind_from_string(const std::string &value);

void validate(const StructureModel &value);
void validate(const StructureModelItem &value);
void validate(const StructureModelItemLink &value);
void validate(const EditorialStructure &value);
void validate(const NarrativeStructure &value);
void validate(const NarrativeLine &value);
void validate(const NarrativeUnit &value);
void validate(const NarrativeUnitLine &value);
void validate(const NarrativeUnitEntity &value);
void validate(const NarrativeLink &value);
void validate(const NarrativeRole &value);
void validate(const NarrativeRoleAssignment &value);

} // namespace inde::project

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace inde::project {

enum class RelationDirectionality { Directed, Symmetric };

struct EntityType {
  std::string id;
  std::string key;
  std::string name;
  std::string description;
  bool is_builtin{};
  std::string created_at;
  std::string updated_at;
};

struct NarrativeEntity {
  std::string id;
  std::string entity_type_id;
  std::string name;
  std::string summary;
  std::string created_at;
  std::string updated_at;
};

struct RelationType {
  std::string id;
  std::string key;
  std::string name;
  std::string inverse_name;
  std::string description;
  RelationDirectionality directionality{RelationDirectionality::Directed};
  bool is_builtin{};
  std::string created_at;
  std::string updated_at;
};

struct NarrativeRelation {
  std::string id;
  std::string relation_type_id;
  std::string source_entity_id;
  std::string target_entity_id;
  std::string description;
  std::string created_at;
  std::string updated_at;
  // Qualificadores opcionais da relação. Eles descrevem este vínculo sem
  // transformá-lo em ocorrência, presença ou outra entidade narrativa.
  std::optional<std::string> fictional_time_point_id;
  std::optional<std::string> location_entity_id;
  std::optional<std::string> cause_entity_id;
};

struct EntityWorkScope {
  std::string id;
  std::string entity_id;
  std::string work_id;
  std::string notes;
  std::string created_at;
  std::string updated_at;
};

struct EditorialEntityReference {
  std::string id;
  std::string entity_id;
  std::string work_id;
  std::string editorial_node_id;
  std::string purpose;
  std::string notes;
  std::string created_at;
  std::string updated_at;
};

struct ChangeLogEntry {
  std::int64_t sequence{};
  std::string id;
  std::string command_name;
  std::string system_created_at;
};

struct BuiltinEntityType {
  const char *id;
  const char *key;
  const char *name;
  const char *description;
};

[[nodiscard]] const std::vector<BuiltinEntityType> &builtin_entity_types();
[[nodiscard]] std::string to_string(RelationDirectionality value);
[[nodiscard]] RelationDirectionality
relation_directionality_from_string(const std::string &value);

void validate_key(const std::string &value);
void validate(const EntityType &value);
void validate(const NarrativeEntity &value);
void validate(const RelationType &value);
void validate(const NarrativeRelation &value);
void validate(const EntityWorkScope &value);
void validate(const EditorialEntityReference &value);

} // namespace inde::project

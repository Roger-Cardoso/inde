#pragma once
#include "inde/project/narrative.hpp"
#include "inde/project/writing.hpp"
namespace inde::project {
// An insert-only plan. The type snapshot is checked in the same transaction.
struct TextEntityCreation {
  NarrativeEntity entity;
  EntityType type;
  bool create_type{};
};
void validate(const TextEntityCreation &, const Document &);
struct TextRelationCreation {
  NarrativeEntity source, target;
  RelationType type;
  NarrativeRelation relation;
  bool create_type{};
};
} // namespace inde::project

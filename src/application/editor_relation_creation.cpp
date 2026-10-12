#include "inde/application/narrative_service.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::application {
project::NarrativeRelation
NarrativeService::create_relation_from_text(project::TextRelationCreation p) {
  project::validate(p.source);
  project::validate(p.target);
  project::validate(p.type);
  project::validate(p.relation);
  if (p.source.id != p.relation.source_entity_id ||
      p.target.id != p.relation.target_entity_id ||
      p.type.id != p.relation.relation_type_id ||
      (p.create_type && p.type.is_builtin) ||
      p.relation.fictional_time_point_id || p.relation.location_entity_id ||
      p.relation.cause_entity_id)
    throw std::runtime_error("Plano de relação textual incompatível");
  if (p.type.directionality == project::RelationDirectionality::Symmetric &&
      p.relation.source_entity_id > p.relation.target_entity_id)
    std::swap(p.relation.source_entity_id, p.relation.target_entity_id);
  store_.create_relation_from_text(require_project().path(), p);
  return p.relation;
}
} // namespace inde::application

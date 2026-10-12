#include "inde/application/writing_service.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::application {
project::Document WritingService::create_date_from_text(
    const project::Document &input, const project::Document &expected,
    const project::TextDateCreation &plan) {
  const auto &active = require_project();
  if (input.id != expected.id)
    throw std::runtime_error("O Documento mudou");
  project::validate(plan.calendar, plan.reference);
  validate_placement(input.editorial_node_id);
  validate_organization(input);
  validate_entity_references(input);
  auto value = input;
  value.created_at = expected.created_at;
  value.updated_at = project::utc_now();
  store_.save_text_date(active.path(), value, expected,
                        active.manifest().project_id, plan);
  return value;
}
project::Document WritingService::create_entity_from_text(
    const project::Document &input, const project::Document &expected,
    const project::TextEntityCreation &plan) {
  const auto &active = require_project();
  project::validate(plan, input);
  if (input.id != expected.id)
    throw std::runtime_error("O Documento mudou");
  // This operation adds a binding; it cannot silently remove protected anchors.
  for (const auto &a : expected.anchors)
    if (std::none_of(input.anchors.begin(), input.anchors.end(),
                     [&](const auto &v) { return v.id == a.id; }))
      throw std::runtime_error(
          "A criação textual não pode remover âncoras existentes");
  validate_placement(input.editorial_node_id);
  validate_organization(input);
  for (const auto &r : input.entity_references)
    if (r.entity_id != plan.entity.id &&
        !store_.referenced_entity_exists(active.path(), r.entity_id))
      throw std::runtime_error("Uma entidade do Documento foi removida");
  auto value = input;
  value.created_at = expected.created_at;
  value.updated_at = project::utc_now();
  store_.save_text_entity(active.path(), value, expected,
                          active.manifest().project_id, plan);
  return value;
}
} // namespace inde::application

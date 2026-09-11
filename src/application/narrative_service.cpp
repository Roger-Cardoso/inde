#include "inde/application/narrative_service.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace inde::application {

const project::Project &NarrativeService::require_project() const {
  const auto *value = session_.current();
  if (!value)
    throw std::runtime_error("Nenhum projeto está aberto");
  return *value;
}

std::vector<project::EntityType> NarrativeService::entity_types() const {
  return store_.entity_types(require_project().path());
}

std::vector<project::NarrativeEntity>
NarrativeService::entities(const persistence::EntityQuery &query) const {
  return store_.entities(require_project().path(), query);
}

std::size_t
NarrativeService::entity_count(const persistence::EntityQuery &query) const {
  return store_.entity_count(require_project().path(), query);
}

std::optional<project::NarrativeEntity>
NarrativeService::entity(const std::string &id) const {
  return store_.entity(require_project().path(), id);
}

std::vector<project::RelationType> NarrativeService::relation_types() const {
  return store_.relation_types(require_project().path());
}

std::vector<project::NarrativeRelation>
NarrativeService::relations(const persistence::RelationQuery &query) const {
  return store_.relations(require_project().path(), query);
}

std::vector<project::EntityWorkScope> NarrativeService::work_scopes(
    const persistence::EntityWorkScopeQuery &query) const {
  return store_.work_scopes(require_project().path(), query);
}

std::vector<project::EditorialEntityReference>
NarrativeService::editorial_references(
    const persistence::EditorialReferenceQuery &query) const {
  return store_.editorial_references(require_project().path(), query);
}

std::vector<persistence::EntityFacetCount>
NarrativeService::editorial_reference_entity_type_facets(
    const persistence::EditorialReferenceQuery &query) const {
  return store_.editorial_reference_entity_type_facets(require_project().path(),
                                                        query);
}

std::vector<project::ChangeLogEntry>
NarrativeService::change_log(std::size_t limit) const {
  return store_.change_log(require_project().path(), limit);
}

project::EntityType
NarrativeService::create_entity_type(std::string key, std::string name,
                                     std::string description) {
  const auto &active = require_project();
  const auto now = project::utc_now();
  project::EntityType value{project::new_uuid(),
                            std::move(key),
                            std::move(name),
                            std::move(description),
                            false,
                            now,
                            now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::EntityType
NarrativeService::update_entity_type(const project::EntityType &input) {
  const auto &active = require_project();
  const auto current = store_.entity_type(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Tipo de entidade não encontrado");
  if (current->is_builtin)
    throw std::runtime_error(
        "Tipos internos de entidade não podem ser alterados");
  auto value = input;
  value.key = current->key;
  value.is_builtin = false;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void NarrativeService::delete_entity_type(const std::string &id) {
  const auto &active = require_project();
  const auto current = store_.entity_type(active.path(), id);
  if (!current)
    throw std::runtime_error("Tipo de entidade não encontrado");
  if (current->is_builtin)
    throw std::runtime_error(
        "Tipos internos de entidade não podem ser removidos");
  persistence::EntityQuery query;
  query.entity_type_ids = {id};
  query.limit = 1;
  if (!store_.entities(active.path(), query).empty())
    throw std::runtime_error(
        "Remova ou reclassifique as entidades deste tipo primeiro");
  store_.remove_entity_type(active.path(), id);
}

project::NarrativeEntity
NarrativeService::create_entity(std::string entity_type_id, std::string name,
                                std::string summary) {
  const auto &active = require_project();
  if (!store_.entity_type(active.path(), entity_type_id))
    throw std::runtime_error("O tipo de entidade escolhido não existe");
  const auto now = project::utc_now();
  project::NarrativeEntity value{project::new_uuid(),
                                 std::move(entity_type_id),
                                 std::move(name),
                                 std::move(summary),
                                 now,
                                 now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::NarrativeEntity
NarrativeService::update_entity(const project::NarrativeEntity &input) {
  const auto &active = require_project();
  const auto current = store_.entity(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Entidade narrativa não encontrada");
  if (!store_.entity_type(active.path(), input.entity_type_id))
    throw std::runtime_error("O tipo de entidade escolhido não existe");
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void NarrativeService::delete_entity(const std::string &id) {
  const auto &active = require_project();
  if (!store_.entity(active.path(), id))
    throw std::runtime_error("Entidade narrativa não encontrada");
  persistence::RelationQuery query;
  query.entity_id = id;
  query.limit = 1;
  if (!store_.relations(active.path(), query).empty())
    throw std::runtime_error("Remova primeiro as relações desta entidade");
  persistence::EntityWorkScopeQuery scope_query;
  scope_query.entity_id = id;
  scope_query.limit = 1;
  if (!store_.work_scopes(active.path(), scope_query).empty())
    throw std::runtime_error(
        "Remova primeiro os vínculos desta entidade com Obras");
  store_.remove_entity(active.path(), id);
}

project::RelationType NarrativeService::create_relation_type(
    std::string key, std::string name, std::string inverse_name,
    project::RelationDirectionality directionality, std::string description) {
  const auto &active = require_project();
  const auto now = project::utc_now();
  project::RelationType value{project::new_uuid(),
                              std::move(key),
                              std::move(name),
                              std::move(inverse_name),
                              std::move(description),
                              directionality,
                              false,
                              now,
                              now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::RelationType
NarrativeService::update_relation_type(const project::RelationType &input) {
  const auto &active = require_project();
  const auto current = store_.relation_type(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Tipo de relação não encontrado");
  if (current->is_builtin)
    throw std::runtime_error(
        "Tipos internos de relação não podem ser alterados");
  auto value = input;
  value.key = current->key;
  value.directionality = current->directionality;
  value.is_builtin = false;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void NarrativeService::delete_relation_type(const std::string &id) {
  const auto &active = require_project();
  const auto current = store_.relation_type(active.path(), id);
  if (!current)
    throw std::runtime_error("Tipo de relação não encontrado");
  if (current->is_builtin)
    throw std::runtime_error(
        "Tipos internos de relação não podem ser removidos");
  store_.remove_relation_type(active.path(), id);
}

project::NarrativeRelation NarrativeService::canonicalize_relation(
    project::NarrativeRelation value) const {
  const auto &active = require_project();
  const auto type = store_.relation_type(active.path(), value.relation_type_id);
  if (!type)
    throw std::runtime_error("O tipo de relação escolhido não existe");
  if (!store_.entity(active.path(), value.source_entity_id) ||
      !store_.entity(active.path(), value.target_entity_id))
    throw std::runtime_error("A origem ou o destino da relação não existe");
  if (type->directionality == project::RelationDirectionality::Symmetric &&
      value.source_entity_id > value.target_entity_id)
    std::swap(value.source_entity_id, value.target_entity_id);
  validate_relation_qualifiers(value);
  project::validate(value);
  return value;
}

void NarrativeService::validate_relation_qualifiers(
    const project::NarrativeRelation &value) const {
  const auto &active = require_project();
  if (value.fictional_time_point_id &&
      !planning_store_.time_point(active.path(), *value.fictional_time_point_id))
    throw std::runtime_error(
        "O ponto ficcional informado para a relação não existe");
  if (value.location_entity_id) {
    const auto location =
        store_.entity(active.path(), *value.location_entity_id);
    if (!location)
      throw std::runtime_error("O Local informado para a relação não existe");
    const auto location_type = std::find_if(
        project::builtin_entity_types().begin(),
        project::builtin_entity_types().end(), [](const auto &type) {
          return std::string_view{type.key} == "location";
        });
    if (location_type == project::builtin_entity_types().end() ||
        location->entity_type_id != location_type->id)
      throw std::runtime_error(
          "O qualificador Onde de uma relação exige uma entidade Local");
  }
  if (value.cause_entity_id &&
      !store_.entity(active.path(), *value.cause_entity_id))
    throw std::runtime_error(
        "A entidade informada como causa da relação não existe");
}

project::NarrativeRelation NarrativeService::create_relation(
    std::string relation_type_id, std::string source_entity_id,
    std::string target_entity_id, std::string description,
    std::optional<std::string> fictional_time_point_id,
    std::optional<std::string> location_entity_id,
    std::optional<std::string> cause_entity_id) {
  const auto now = project::utc_now();
  project::NarrativeRelation value{project::new_uuid(),
                                   std::move(relation_type_id),
                                   std::move(source_entity_id),
                                   std::move(target_entity_id),
                                   std::move(description),
                                   now,
                                   now,
                                   std::move(fictional_time_point_id),
                                   std::move(location_entity_id),
                                   std::move(cause_entity_id)};
  value = canonicalize_relation(std::move(value));
  store_.save(require_project().path(), value);
  return value;
}

project::NarrativeRelation
NarrativeService::update_relation(const project::NarrativeRelation &input) {
  const auto &active = require_project();
  const auto current = store_.relation(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Relação narrativa não encontrada");
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  value = canonicalize_relation(std::move(value));
  store_.save(active.path(), value);
  return value;
}

void NarrativeService::delete_relation(const std::string &id) {
  const auto &active = require_project();
  if (!store_.relation(active.path(), id))
    throw std::runtime_error("Relação narrativa não encontrada");
  store_.remove_relation(active.path(), id);
}

project::EntityWorkScope NarrativeService::add_entity_to_work(
    std::string entity_id, std::string work_id, std::string notes) {
  const auto &active = require_project();
  if (!store_.entity(active.path(), entity_id))
    throw std::runtime_error("Entidade narrativa não encontrada");
  if (std::none_of(session_.catalog().works.begin(),
                   session_.catalog().works.end(),
                   [&](const auto &work) { return work.id == work_id; }))
    throw std::runtime_error("Obra não encontrada");
  const auto now = project::utc_now();
  project::EntityWorkScope value{project::new_uuid(), std::move(entity_id),
                                 std::move(work_id), std::move(notes), now,
                                 now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::EntityWorkScope NarrativeService::update_work_scope(
    const project::EntityWorkScope &input) {
  const auto &active = require_project();
  const auto current = store_.work_scope(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Vínculo entre entidade e obra não encontrado");
  auto value = input;
  value.entity_id = current->entity_id;
  value.work_id = current->work_id;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void NarrativeService::remove_entity_from_work(const std::string &id) {
  store_.remove_work_scope(require_project().path(), id);
}

project::EditorialEntityReference NarrativeService::add_editorial_reference(
    std::string entity_id, std::string editorial_node_id,
    std::string purpose, std::string notes) {
  const auto &active = require_project();
  if (!store_.entity(active.path(), entity_id))
    throw std::runtime_error("Entidade narrativa não encontrada");
  const auto node = std::find_if(
      session_.structural_nodes().begin(), session_.structural_nodes().end(),
      [&](const auto &value) { return value.id == editorial_node_id; });
  if (node == session_.structural_nodes().end())
    throw std::runtime_error("Unidade editorial não encontrada");
  persistence::EntityWorkScopeQuery query;
  query.entity_id = entity_id;
  query.work_id = node->work_id;
  query.limit = 1;
  if (store_.work_scopes(active.path(), query).empty())
    throw std::runtime_error(
        "Vincule primeiro a entidade à Obra desta unidade editorial");
  const auto now = project::utc_now();
  project::EditorialEntityReference value{
      project::new_uuid(), std::move(entity_id), node->work_id,
      std::move(editorial_node_id), std::move(purpose), std::move(notes), now,
      now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::EditorialEntityReference
NarrativeService::update_editorial_reference(
    const project::EditorialEntityReference &input) {
  const auto &active = require_project();
  const auto current = store_.editorial_reference(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Referência editorial de entidade não encontrada");
  auto value = input;
  value.entity_id = current->entity_id;
  value.work_id = current->work_id;
  value.editorial_node_id = current->editorial_node_id;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void NarrativeService::remove_editorial_reference(const std::string &id) {
  store_.remove_editorial_reference(require_project().path(), id);
}

} // namespace inde::application

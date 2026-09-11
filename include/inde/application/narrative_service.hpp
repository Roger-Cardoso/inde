#pragma once

#include "inde/application/project_session.hpp"
#include "inde/persistence/narrative_store.hpp"
#include "inde/persistence/planning_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace inde::application {

class NarrativeService {
public:
  NarrativeService(ProjectSession &session, persistence::NarrativeStore &store,
                   persistence::PlanningStore &planning_store)
      : session_(session), store_(store), planning_store_(planning_store) {}

  [[nodiscard]] std::vector<project::EntityType> entity_types() const;
  [[nodiscard]] std::vector<project::NarrativeEntity>
  entities(const persistence::EntityQuery &query = {}) const;
  [[nodiscard]] std::size_t
  entity_count(const persistence::EntityQuery &query = {}) const;
  [[nodiscard]] std::optional<project::NarrativeEntity>
  entity(const std::string &id) const;
  [[nodiscard]] std::vector<project::RelationType> relation_types() const;
  [[nodiscard]] std::vector<project::NarrativeRelation>
  relations(const persistence::RelationQuery &query = {}) const;
  [[nodiscard]] std::vector<project::EntityWorkScope>
  work_scopes(const persistence::EntityWorkScopeQuery &query = {}) const;
  [[nodiscard]] std::vector<project::EditorialEntityReference>
  editorial_references(
      const persistence::EditorialReferenceQuery &query = {}) const;
  [[nodiscard]] std::vector<persistence::EntityFacetCount>
  editorial_reference_entity_type_facets(
      const persistence::EditorialReferenceQuery &query = {}) const;
  [[nodiscard]] std::vector<project::ChangeLogEntry>
  change_log(std::size_t limit = 100) const;

  [[nodiscard]] project::EntityType
  create_entity_type(std::string key, std::string name,
                     std::string description = {});
  [[nodiscard]] project::EntityType
  update_entity_type(const project::EntityType &input);
  void delete_entity_type(const std::string &id);

  [[nodiscard]] project::NarrativeEntity
  create_entity(std::string entity_type_id, std::string name,
                std::string summary = {});
  [[nodiscard]] project::NarrativeEntity
  update_entity(const project::NarrativeEntity &input);
  void delete_entity(const std::string &id);

  [[nodiscard]] project::RelationType
  create_relation_type(std::string key, std::string name,
                       std::string inverse_name,
                       project::RelationDirectionality directionality,
                       std::string description = {});
  [[nodiscard]] project::RelationType
  update_relation_type(const project::RelationType &input);
  void delete_relation_type(const std::string &id);

  [[nodiscard]] project::NarrativeRelation
  create_relation(std::string relation_type_id, std::string source_entity_id,
                  std::string target_entity_id, std::string description = {},
                  std::optional<std::string> fictional_time_point_id = {},
                  std::optional<std::string> location_entity_id = {},
                  std::optional<std::string> cause_entity_id = {});
  [[nodiscard]] project::NarrativeRelation
  update_relation(const project::NarrativeRelation &input);
  void delete_relation(const std::string &id);

  [[nodiscard]] project::EntityWorkScope
  add_entity_to_work(std::string entity_id, std::string work_id,
                     std::string notes = {});
  [[nodiscard]] project::EntityWorkScope
  update_work_scope(const project::EntityWorkScope &input);
  void remove_entity_from_work(const std::string &id);

  [[nodiscard]] project::EditorialEntityReference
  add_editorial_reference(std::string entity_id,
                          std::string editorial_node_id,
                          std::string purpose, std::string notes = {});
  [[nodiscard]] project::EditorialEntityReference update_editorial_reference(
      const project::EditorialEntityReference &input);
  void remove_editorial_reference(const std::string &id);

private:
  [[nodiscard]] const project::Project &require_project() const;
  [[nodiscard]] project::NarrativeRelation
  canonicalize_relation(project::NarrativeRelation value) const;
  void validate_relation_qualifiers(
      const project::NarrativeRelation &value) const;

  ProjectSession &session_;
  persistence::NarrativeStore &store_;
  persistence::PlanningStore &planning_store_;
};

} // namespace inde::application

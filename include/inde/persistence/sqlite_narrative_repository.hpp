#pragma once

#include "inde/persistence/narrative_store.hpp"

namespace inde::persistence {

class SqliteNarrativeRepository final : public NarrativeStore {
public:
  void initialize(const std::filesystem::path &project_path) const override;
  [[nodiscard]] std::vector<project::EntityType>
  entity_types(const std::filesystem::path &project_path) const override;
  [[nodiscard]] std::optional<project::EntityType>
  entity_type(const std::filesystem::path &project_path,
              const std::string &id) const override;
  [[nodiscard]] std::vector<project::NarrativeEntity>
  entities(const std::filesystem::path &project_path,
           const EntityQuery &query) const override;
  [[nodiscard]] std::size_t
  entity_count(const std::filesystem::path &project_path,
               const EntityQuery &query) const override;
  [[nodiscard]] std::vector<EntityFacetCount>
  entity_type_facets(const std::filesystem::path &project_path,
                     const EntityQuery &query) const override;
  [[nodiscard]] std::vector<EntityFacetCount>
  relation_type_facets(const std::filesystem::path &project_path,
                       const EntityQuery &query) const override;
  [[nodiscard]] std::optional<project::NarrativeEntity>
  entity(const std::filesystem::path &project_path,
         const std::string &id) const override;
  [[nodiscard]] std::vector<project::RelationType>
  relation_types(const std::filesystem::path &project_path) const override;
  [[nodiscard]] std::optional<project::RelationType>
  relation_type(const std::filesystem::path &project_path,
                const std::string &id) const override;
  [[nodiscard]] std::vector<project::NarrativeRelation>
  relations(const std::filesystem::path &project_path,
            const RelationQuery &query) const override;
  [[nodiscard]] std::optional<project::NarrativeRelation>
  relation(const std::filesystem::path &project_path,
           const std::string &id) const override;
  [[nodiscard]] std::vector<project::EntityWorkScope>
  work_scopes(const std::filesystem::path &project_path,
              const EntityWorkScopeQuery &query) const override;
  [[nodiscard]] std::optional<project::EntityWorkScope>
  work_scope(const std::filesystem::path &project_path,
             const std::string &id) const override;
  [[nodiscard]] std::vector<project::EditorialEntityReference>
  editorial_references(const std::filesystem::path &project_path,
                       const EditorialReferenceQuery &query) const override;
  [[nodiscard]] std::vector<EntityFacetCount>
  editorial_reference_entity_type_facets(
      const std::filesystem::path &project_path,
      const EditorialReferenceQuery &query) const override;
  [[nodiscard]] std::optional<project::EditorialEntityReference>
  editorial_reference(const std::filesystem::path &project_path,
                       const std::string &id) const override;
  [[nodiscard]] std::vector<project::ChangeLogEntry>
  change_log(const std::filesystem::path &project_path,
             std::size_t limit) const override;

  void save(const std::filesystem::path &project_path,
            const project::EntityType &value) const override;
  void save(const std::filesystem::path &project_path,
            const project::NarrativeEntity &value) const override;
  void save(const std::filesystem::path &project_path,
            const project::RelationType &value) const override;
  void save(const std::filesystem::path &project_path,
            const project::NarrativeRelation &value) const override;
  void save(const std::filesystem::path &project_path,
            const project::EntityWorkScope &value) const override;
  void save(const std::filesystem::path &project_path,
            const project::EditorialEntityReference &value) const override;
  void remove_entity_type(const std::filesystem::path &project_path,
                          const std::string &id) const override;
  void remove_entity(const std::filesystem::path &project_path,
                     const std::string &id) const override;
  void remove_relation_type(const std::filesystem::path &project_path,
                            const std::string &id) const override;
  void remove_relation(const std::filesystem::path &project_path,
                       const std::string &id) const override;
  void remove_work_scope(const std::filesystem::path &project_path,
                         const std::string &id) const override;
  void remove_editorial_reference(const std::filesystem::path &project_path,
                                  const std::string &id) const override;
};

} // namespace inde::persistence

#pragma once

#include "inde/project/narrative.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace inde::persistence {

struct EntityQuery {
  std::string search;
  std::optional<std::string> work_id;
  std::vector<std::string> entity_type_ids;
  std::vector<std::string> relation_type_ids;
  std::optional<std::string> related_entity_id;
  std::optional<std::string> fictional_axis_id;
  std::optional<std::string> fictional_time_point_id;
  std::optional<std::string> fictional_window_start_time_point_id;
  std::optional<std::string> fictional_window_end_time_point_id;
  std::optional<std::string> editorial_node_id;
  bool require_document_reference{};
  std::optional<std::string> document_id;
  std::size_t limit{100};
  std::size_t offset{};
};

// Contagem agregada devolvida pelo armazenamento para compor facetas sem
// carregar todas as entidades do projeto na interface.
struct EntityFacetCount {
  std::string id;
  std::size_t count{};
};

struct RelationQuery {
  std::string search;
  std::optional<std::string> entity_id;
  std::optional<std::string> relation_type_id;
  std::optional<std::string> fictional_time_point_id;
  std::optional<std::string> location_entity_id;
  std::optional<std::string> cause_entity_id;
  std::size_t limit{200};
  std::size_t offset{};
};

struct EntityWorkScopeQuery {
  std::optional<std::string> entity_id;
  std::optional<std::string> work_id;
  std::size_t limit{100};
  std::size_t offset{};
};

struct EditorialReferenceQuery {
  std::optional<std::string> entity_id;
  std::optional<std::string> work_id;
  std::optional<std::string> editorial_node_id;
  std::vector<std::string> entity_type_ids;
  std::size_t limit{100};
  std::size_t offset{};
};

class NarrativeStore {
public:
  virtual ~NarrativeStore() = default;

  virtual void initialize(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::vector<project::EntityType>
  entity_types(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::optional<project::EntityType>
  entity_type(const std::filesystem::path &project_path,
              const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeEntity>
  entities(const std::filesystem::path &project_path,
           const EntityQuery &query) const = 0;
  [[nodiscard]] virtual std::size_t
  entity_count(const std::filesystem::path &project_path,
               const EntityQuery &query) const = 0;
  [[nodiscard]] virtual std::vector<EntityFacetCount>
  entity_type_facets(const std::filesystem::path &project_path,
                     const EntityQuery &query) const = 0;
  [[nodiscard]] virtual std::vector<EntityFacetCount>
  relation_type_facets(const std::filesystem::path &project_path,
                       const EntityQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::NarrativeEntity>
  entity(const std::filesystem::path &project_path,
         const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::RelationType>
  relation_types(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::optional<project::RelationType>
  relation_type(const std::filesystem::path &project_path,
                const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeRelation>
  relations(const std::filesystem::path &project_path,
            const RelationQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::NarrativeRelation>
  relation(const std::filesystem::path &project_path,
           const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::EntityWorkScope>
  work_scopes(const std::filesystem::path &project_path,
              const EntityWorkScopeQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::EntityWorkScope>
  work_scope(const std::filesystem::path &project_path,
             const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::EditorialEntityReference>
  editorial_references(const std::filesystem::path &project_path,
                       const EditorialReferenceQuery &query) const = 0;
  // Facetas de tipos que realmente aparecem nas unidades editoriais do
  // contexto consultado. A UI usa a contagem como descoberta, sem varrer
  // entidades ou interpretar SQLite.
  [[nodiscard]] virtual std::vector<EntityFacetCount>
  editorial_reference_entity_type_facets(
      const std::filesystem::path &project_path,
      const EditorialReferenceQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::EditorialEntityReference>
  editorial_reference(const std::filesystem::path &project_path,
                       const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::ChangeLogEntry>
  change_log(const std::filesystem::path &project_path,
             std::size_t limit) const = 0;

  virtual void save(const std::filesystem::path &project_path,
                    const project::EntityType &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeEntity &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::RelationType &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeRelation &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::EntityWorkScope &value) const = 0;
  virtual void save(
      const std::filesystem::path &project_path,
      const project::EditorialEntityReference &value) const = 0;
  virtual void remove_entity_type(const std::filesystem::path &project_path,
                                  const std::string &id) const = 0;
  virtual void remove_entity(const std::filesystem::path &project_path,
                             const std::string &id) const = 0;
  virtual void remove_relation_type(const std::filesystem::path &project_path,
                                    const std::string &id) const = 0;
  virtual void remove_relation(const std::filesystem::path &project_path,
                               const std::string &id) const = 0;
  virtual void remove_work_scope(const std::filesystem::path &project_path,
                                 const std::string &id) const = 0;
  virtual void
  remove_editorial_reference(const std::filesystem::path &project_path,
                             const std::string &id) const = 0;
};

} // namespace inde::persistence

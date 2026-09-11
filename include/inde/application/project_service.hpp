#pragma once

#include "inde/application/catalog_service.hpp"
#include "inde/application/narrative_service.hpp"
#include "inde/application/planning_service.hpp"
#include "inde/application/project_session.hpp"
#include "inde/application/structural_service.hpp"
#include "inde/application/structure_model_service.hpp"
#include "inde/application/writing_service.hpp"
#include "inde/persistence/project_database_importer.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/persistence/sqlite_catalog_repository.hpp"
#include "inde/persistence/sqlite_narrative_repository.hpp"
#include "inde/persistence/sqlite_planning_repository.hpp"
#include "inde/persistence/sqlite_structural_repository.hpp"
#include "inde/persistence/sqlite_structure_model_repository.hpp"
#include "inde/persistence/sqlite_writing_repository.hpp"

#include <utility>
#include <vector>

namespace inde::application {

class ProjectService {
public:
  ProjectService();

  const project::Project &create(const std::filesystem::path &path,
                                 const std::string &name);
  const project::Project &open(const std::filesystem::path &path);
  void save();
  const project::Project &save_as(const std::filesystem::path &path);
  void close() noexcept;

  [[nodiscard]] const project::Project *current() const noexcept;
  [[nodiscard]] std::vector<std::filesystem::path> recent_projects() const;
  [[nodiscard]] const project::Catalog &catalog() const noexcept;
  [[nodiscard]] NarrativeService &narrative() noexcept {
    return narrative_service_;
  }
  [[nodiscard]] const NarrativeService &narrative() const noexcept {
    return narrative_service_;
  }
  [[nodiscard]] PlanningService &planning() noexcept {
    return planning_service_;
  }
  [[nodiscard]] const PlanningService &planning() const noexcept {
    return planning_service_;
  }
  [[nodiscard]] StructureModelService &structures() noexcept {
    return structure_model_service_;
  }
  [[nodiscard]] const StructureModelService &structures() const noexcept {
    return structure_model_service_;
  }
  [[nodiscard]] WritingService &writing() noexcept { return writing_service_; }
  [[nodiscard]] const WritingService &writing() const noexcept {
    return writing_service_;
  }
  // Estado de navegação descartável compartilhado por Planejamento e Gráficos.
  [[nodiscard]] const PlanningContext &planning_context() const noexcept {
    return planning_context_;
  }
  void set_planning_context(PlanningContext context) {
    planning_context_ = std::move(context);
  }
  void clear_planning_context() noexcept { planning_context_ = {}; }
  const project::IntellectualProperty &
  create_intellectual_property(std::string title, std::string subtitle,
                               std::string description,
                               std::string cover_path = {});
  void update_intellectual_property(const project::IntellectualProperty &value);
  void delete_intellectual_property(const std::string &id);
  const project::Work &create_work(std::string intellectual_property_id,
                                   std::string title, std::string subtitle,
                                   std::string synopsis, std::string language,
                                   std::string status);
  void update_work(const project::Work &value);
  void delete_work(const std::string &id);
  [[nodiscard]] std::vector<project::StructuralNode>
  structural_nodes_for_work(const std::string &work_id) const;
  [[nodiscard]] std::vector<project::StructuralElementType>
  structural_element_types() const;
  [[nodiscard]] project::StructuralElementType
  create_structural_element_type(std::string name);
  [[nodiscard]] project::StructuralElementType
  update_structural_element_type(const project::StructuralElementType &input);
  void delete_structural_element_type(const std::string &id);
  const project::StructuralNode &
  create_structural_node(std::string work_id,
                         std::optional<std::string> parent_id,
                         std::string structural_type_id, std::string designator,
                         std::string title);
  const project::StructuralNode &
  create_structural_node(std::string work_id,
                         std::optional<std::string> parent_id,
                         project::StructuralNodeType type, std::string title,
                         std::string custom_type_name = {});
  void update_structural_node(const project::StructuralNode &value);
  void move_structural_node(const std::string &id,
                            std::optional<std::string> new_parent_id);
  void move_structural_node_up(const std::string &id);
  void move_structural_node_down(const std::string &id);
  void delete_structural_branch(const std::string &id);
  std::string duplicate_structural_branch(const std::string &id);
  void create_structural_template(const std::string &work_id,
                                  const std::string &template_name);
  bool restore_last_structural_deletion();

private:
  persistence::ProjectRepository repository_;
  persistence::ProjectDatabaseRepository database_repository_;
  persistence::ProjectDatabaseImporter database_importer_;
  persistence::RecentProjectsStore recents_;
  persistence::SqliteCatalogRepository catalog_repository_;
  persistence::SqliteStructuralRepository structural_repository_;
  persistence::SqliteStructureModelRepository structure_model_repository_;
  persistence::SqliteNarrativeRepository narrative_repository_;
  persistence::SqlitePlanningRepository planning_repository_;
  persistence::SqliteWritingRepository writing_repository_;
  ProjectSession session_;
  CatalogService catalog_service_{session_, catalog_repository_};
  StructuralService structural_service_{session_, structural_repository_,
                                        structure_model_repository_};
  StructureModelService structure_model_service_{
      session_, structure_model_repository_, structural_repository_};
  NarrativeService narrative_service_{session_, narrative_repository_,
                                      planning_repository_};
  PlanningService planning_service_{session_, planning_repository_,
                                    narrative_repository_};
  WritingService writing_service_{session_, writing_repository_};
  PlanningContext planning_context_;
};

} // namespace inde::application

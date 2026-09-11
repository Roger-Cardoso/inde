#pragma once

#include "inde/project/structural_node.hpp"
#include "inde/project/structure_model.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace inde::persistence {

class StructureModelStore {
public:
  virtual ~StructureModelStore() = default;
  virtual void initialize(const std::filesystem::path &project_path) const = 0;

  [[nodiscard]] virtual std::vector<project::StructureModel>
  models(const std::filesystem::path &project_path,
         project::StructureLayer layer) const = 0;
  [[nodiscard]] virtual std::vector<project::StructureModelItem>
  model_items(const std::filesystem::path &project_path,
              const std::string &model_id) const = 0;
  [[nodiscard]] virtual std::vector<project::StructureModelItemLink>
  model_item_links(const std::filesystem::path &project_path,
                   const std::string &model_id) const = 0;
  virtual void save_model_bundle(
      const std::filesystem::path &project_path,
      const project::StructureModel &model,
      const std::vector<project::StructureModelItem> &items,
      const std::vector<project::StructureModelItemLink> &links) const = 0;
  virtual void remove_model(const std::filesystem::path &project_path,
                            const std::string &id) const = 0;

  [[nodiscard]] virtual std::vector<project::EditorialStructure>
  editorial_structures(const std::filesystem::path &project_path,
                       const std::string &work_id) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeStructure>
  narrative_structures(const std::filesystem::path &project_path,
                       const std::string &work_id) const = 0;
  virtual void save_editorial_structure_bundle(
      const std::filesystem::path &project_path,
      const project::EditorialStructure &structure,
      const std::vector<project::StructuralNode> &nodes) const = 0;
  virtual void save_narrative_structure_bundle(
      const std::filesystem::path &project_path,
      const project::NarrativeStructure &structure,
      const std::vector<project::NarrativeLine> &lines,
      const std::vector<project::NarrativeUnit> &units,
      const std::vector<project::NarrativeUnitLine> &memberships,
      const std::vector<project::NarrativeUnitEntity> &entities,
      const std::vector<project::NarrativeLink> &links) const = 0;
  virtual void activate_editorial(const std::filesystem::path &project_path,
                                  const std::string &work_id,
                                  const std::string &id) const = 0;
  virtual void activate_narrative(const std::filesystem::path &project_path,
                                  const std::string &work_id,
                                  const std::string &id) const = 0;
  [[nodiscard]] virtual std::size_t
  document_count(const std::filesystem::path &project_path,
                 const std::string &editorial_structure_id) const = 0;
  virtual void
  remove_editorial_structure(const std::filesystem::path &project_path,
                             const std::string &id) const = 0;
  virtual void
  remove_narrative_structure(const std::filesystem::path &project_path,
                             const std::string &id) const = 0;

  [[nodiscard]] virtual std::vector<project::NarrativeLine>
  lines(const std::filesystem::path &project_path,
        const std::string &structure_id) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeUnit>
  units(const std::filesystem::path &project_path,
        const std::string &structure_id) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeUnitLine>
  unit_lines(const std::filesystem::path &project_path,
             const std::string &structure_id) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeUnitEntity>
  unit_entities(const std::filesystem::path &project_path,
                const std::string &structure_id) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeLink>
  links(const std::filesystem::path &project_path,
        const std::string &structure_id) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeLine &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeUnit &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeUnitLine &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeUnitEntity &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeLink &value) const = 0;
  virtual void remove_line(const std::filesystem::path &project_path,
                           const std::string &id) const = 0;
  virtual void remove_unit(const std::filesystem::path &project_path,
                           const std::string &id) const = 0;
  virtual void remove_unit_line(const std::filesystem::path &project_path,
                                const std::string &unit_id,
                                const std::string &line_id) const = 0;
  virtual void remove_unit_entity(const std::filesystem::path &project_path,
                                  const std::string &unit_id,
                                  const std::string &entity_id,
                                  const std::string &role) const = 0;
  virtual void remove_link(const std::filesystem::path &project_path,
                           const std::string &id) const = 0;

  [[nodiscard]] virtual std::vector<project::NarrativeRole>
  roles(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::vector<project::NarrativeRoleAssignment>
  role_assignments(const std::filesystem::path &project_path,
                   const std::optional<std::string> &work_id) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeRole &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::NarrativeRoleAssignment &value) const = 0;
  virtual void remove_role(const std::filesystem::path &project_path,
                           const std::string &id) const = 0;
  virtual void remove_role_assignment(const std::filesystem::path &project_path,
                                      const std::string &id) const = 0;
};

} // namespace inde::persistence

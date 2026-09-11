#pragma once

#include "inde/application/project_session.hpp"
#include "inde/persistence/structural_store.hpp"
#include "inde/persistence/structure_model_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace inde::application {

class StructureModelService {
public:
  StructureModelService(ProjectSession &session,
                        persistence::StructureModelStore &store,
                        persistence::StructuralStore &structural_store)
      : session_(session), store_(store), structural_store_(structural_store) {}

  [[nodiscard]] std::vector<project::StructureModel>
  models(project::StructureLayer layer) const;
  void delete_model(const std::string &id);
  [[nodiscard]] std::vector<project::EditorialStructure>
  editorial_structures(const std::string &work_id) const;
  [[nodiscard]] std::vector<project::NarrativeStructure>
  narrative_structures(const std::string &work_id) const;
  [[nodiscard]] std::optional<project::EditorialStructure>
  active_editorial(const std::string &work_id) const;
  [[nodiscard]] std::size_t
  editorial_node_count(const std::string &structure_id) const;
  [[nodiscard]] std::optional<project::NarrativeStructure>
  active_narrative(const std::string &work_id) const;

  [[nodiscard]] project::EditorialStructure
  create_editorial(const std::string &work_id, std::string name,
                   std::string description = {});
  [[nodiscard]] project::EditorialStructure
  instantiate_editorial(const std::string &work_id, const std::string &model_id,
                        std::string name);
  [[nodiscard]] project::EditorialStructure
  duplicate_editorial(const std::string &source_id, std::string name,
                      bool derived);
  [[nodiscard]] project::StructureModel
  capture_editorial_model(const std::string &structure_id, std::string name,
                          std::string description);
  [[nodiscard]] project::StructureActivationImpact
  editorial_activation_impact(const std::string &target_id) const;
  void activate_editorial(const std::string &target_id);
  void delete_editorial(const std::string &id);

  [[nodiscard]] project::NarrativeStructure
  create_narrative(const std::string &work_id, std::string name,
                   std::string description = {});
  [[nodiscard]] project::NarrativeStructure
  instantiate_narrative(const std::string &work_id, const std::string &model_id,
                        std::string name);
  [[nodiscard]] project::NarrativeStructure
  duplicate_narrative(const std::string &source_id, std::string name,
                      bool derived);
  [[nodiscard]] project::StructureModel
  capture_narrative_model(const std::string &structure_id, std::string name,
                          std::string description);
  [[nodiscard]] project::StructureActivationImpact
  narrative_activation_impact(const std::string &target_id) const;
  void activate_narrative(const std::string &target_id);
  void delete_narrative(const std::string &id);

  [[nodiscard]] std::vector<project::NarrativeLine>
  lines(const std::string &structure_id) const;
  [[nodiscard]] std::vector<project::NarrativeUnit>
  units(const std::string &structure_id) const;
  [[nodiscard]] std::vector<project::NarrativeUnitLine>
  unit_lines(const std::string &structure_id) const;
  [[nodiscard]] std::vector<project::NarrativeUnitEntity>
  unit_entities(const std::string &structure_id) const;
  [[nodiscard]] std::vector<project::NarrativeLink>
  links(const std::string &structure_id) const;
  [[nodiscard]] project::NarrativeLine
  create_line(const std::string &structure_id, std::string name,
              std::string description);
  [[nodiscard]] project::NarrativeUnit
  create_unit(const std::string &structure_id, std::string designator,
              std::string title, std::string summary, std::string purpose,
              std::string perspective);
  void update_line(project::NarrativeLine value);
  void update_unit(project::NarrativeUnit value);
  void delete_line(const std::string &id);
  void delete_unit(const std::string &id);
  void add_unit_to_line(const std::string &unit_id, const std::string &line_id);
  void remove_unit_from_line(const std::string &unit_id,
                             const std::string &line_id);
  void add_entity_to_unit(const std::string &unit_id,
                          const std::string &entity_id, std::string role);
  void remove_entity_from_unit(const std::string &unit_id,
                               const std::string &entity_id,
                               const std::string &role);
  [[nodiscard]] project::NarrativeLink
  create_link(const std::string &structure_id,
              const std::string &source_unit_id,
              const std::string &target_unit_id,
              project::NarrativeLinkKind kind, std::string label);
  void delete_link(const std::string &id);

  [[nodiscard]] std::vector<project::NarrativeRole> roles() const;
  [[nodiscard]] std::vector<project::NarrativeRoleAssignment>
  role_assignments(const std::optional<std::string> &work_id) const;
  [[nodiscard]] project::NarrativeRole create_role(std::string name,
                                                   std::string description);
  void update_role(project::NarrativeRole value);
  void delete_role(const std::string &id);
  [[nodiscard]] project::NarrativeRoleAssignment
  assign_role(const std::string &role_id, const std::string &entity_id,
              const std::string &work_id,
              std::optional<std::string> structure_id = std::nullopt,
              std::optional<std::string> unit_id = std::nullopt,
              std::string notes = {});
  void remove_role_assignment(const std::string &id);

private:
  [[nodiscard]] const project::Project &require_project() const;
  void require_work(const std::string &work_id) const;
  void reload_structural_nodes();

  ProjectSession &session_;
  persistence::StructureModelStore &store_;
  persistence::StructuralStore &structural_store_;
};

} // namespace inde::application

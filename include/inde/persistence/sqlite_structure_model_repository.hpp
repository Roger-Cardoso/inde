#pragma once

#include "inde/persistence/structure_model_store.hpp"

namespace inde::persistence {

class SqliteStructureModelRepository final : public StructureModelStore {
public:
  void initialize(const std::filesystem::path &) const override;
  [[nodiscard]] std::vector<project::StructureModel>
  models(const std::filesystem::path &, project::StructureLayer) const override;
  [[nodiscard]] std::vector<project::StructureModelItem>
  model_items(const std::filesystem::path &,
              const std::string &) const override;
  [[nodiscard]] std::vector<project::StructureModelItemLink>
  model_item_links(const std::filesystem::path &,
                   const std::string &) const override;
  void save_model_bundle(
      const std::filesystem::path &, const project::StructureModel &,
      const std::vector<project::StructureModelItem> &,
      const std::vector<project::StructureModelItemLink> &) const override;
  void remove_model(const std::filesystem::path &,
                    const std::string &) const override;
  [[nodiscard]] std::vector<project::EditorialStructure>
  editorial_structures(const std::filesystem::path &,
                       const std::string &) const override;
  [[nodiscard]] std::vector<project::NarrativeStructure>
  narrative_structures(const std::filesystem::path &,
                       const std::string &) const override;
  void save_editorial_structure_bundle(
      const std::filesystem::path &, const project::EditorialStructure &,
      const std::vector<project::StructuralNode> &) const override;
  void save_narrative_structure_bundle(
      const std::filesystem::path &, const project::NarrativeStructure &,
      const std::vector<project::NarrativeLine> &,
      const std::vector<project::NarrativeUnit> &,
      const std::vector<project::NarrativeUnitLine> &,
      const std::vector<project::NarrativeUnitEntity> &,
      const std::vector<project::NarrativeLink> &) const override;
  void activate_editorial(const std::filesystem::path &, const std::string &,
                          const std::string &) const override;
  void activate_narrative(const std::filesystem::path &, const std::string &,
                          const std::string &) const override;
  [[nodiscard]] std::size_t document_count(const std::filesystem::path &,
                                           const std::string &) const override;
  void remove_editorial_structure(const std::filesystem::path &,
                                  const std::string &) const override;
  void remove_narrative_structure(const std::filesystem::path &,
                                  const std::string &) const override;
  [[nodiscard]] std::vector<project::NarrativeLine>
  lines(const std::filesystem::path &, const std::string &) const override;
  [[nodiscard]] std::vector<project::NarrativeUnit>
  units(const std::filesystem::path &, const std::string &) const override;
  [[nodiscard]] std::vector<project::NarrativeUnitLine>
  unit_lines(const std::filesystem::path &, const std::string &) const override;
  [[nodiscard]] std::vector<project::NarrativeUnitEntity>
  unit_entities(const std::filesystem::path &,
                const std::string &) const override;
  [[nodiscard]] std::vector<project::NarrativeLink>
  links(const std::filesystem::path &, const std::string &) const override;
  void save(const std::filesystem::path &,
            const project::NarrativeLine &) const override;
  void save(const std::filesystem::path &,
            const project::NarrativeUnit &) const override;
  void save(const std::filesystem::path &,
            const project::NarrativeUnitLine &) const override;
  void save(const std::filesystem::path &,
            const project::NarrativeUnitEntity &) const override;
  void save(const std::filesystem::path &,
            const project::NarrativeLink &) const override;
  void remove_line(const std::filesystem::path &,
                   const std::string &) const override;
  void remove_unit(const std::filesystem::path &,
                   const std::string &) const override;
  void remove_unit_line(const std::filesystem::path &, const std::string &,
                        const std::string &) const override;
  void remove_unit_entity(const std::filesystem::path &, const std::string &,
                          const std::string &,
                          const std::string &) const override;
  void remove_link(const std::filesystem::path &,
                   const std::string &) const override;
  [[nodiscard]] std::vector<project::NarrativeRole>
  roles(const std::filesystem::path &) const override;
  [[nodiscard]] std::vector<project::NarrativeRoleAssignment>
  role_assignments(const std::filesystem::path &,
                   const std::optional<std::string> &) const override;
  void save(const std::filesystem::path &,
            const project::NarrativeRole &) const override;
  void save(const std::filesystem::path &,
            const project::NarrativeRoleAssignment &) const override;
  void remove_role(const std::filesystem::path &,
                   const std::string &) const override;
  void remove_role_assignment(const std::filesystem::path &,
                              const std::string &) const override;
};

} // namespace inde::persistence

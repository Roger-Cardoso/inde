#pragma once

#include "inde/application/project_session.hpp"
#include "inde/persistence/structural_store.hpp"
#include "inde/persistence/structure_model_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace inde::application {

class StructuralService {
public:
  StructuralService(ProjectSession &session,
                    persistence::StructuralStore &repository,
                    persistence::StructureModelStore &structure_models)
      : session_(session), repository_(repository),
        structure_models_(&structure_models) {}
  StructuralService(ProjectSession &session,
                    persistence::StructuralStore &repository)
      : session_(session), repository_(repository) {}

  [[nodiscard]] std::vector<project::StructuralNode>
  nodes_for_work(const std::string &work_id) const;
  [[nodiscard]] std::vector<project::StructuralElementType>
  element_types() const;
  [[nodiscard]] project::StructuralElementType
  create_element_type(std::string name);
  [[nodiscard]] project::StructuralElementType
  update_element_type(const project::StructuralElementType &input);
  void delete_element_type(const std::string &id);
  const project::StructuralNode &
  create_node(std::string work_id, std::optional<std::string> parent_id,
              std::string structural_type_id, std::string designator,
              std::string title);
  const project::StructuralNode &
  create_node(std::string work_id, std::optional<std::string> parent_id,
              project::StructuralNodeType type, std::string title,
              std::string custom_type_name = {});
  void update_node(const project::StructuralNode &value);
  void move_node(const std::string &id,
                 std::optional<std::string> new_parent_id);
  void move_node_up(const std::string &id);
  void move_node_down(const std::string &id);
  void delete_branch(const std::string &id);
  std::string duplicate_branch(const std::string &id);
  void create_template(const std::string &work_id,
                       const std::string &template_name);
  bool restore_last_deletion();

private:
  [[nodiscard]] project::Project &require_project();
  [[nodiscard]] const project::Project &require_project() const;
  [[nodiscard]] project::StructuralElementType
  require_element_type(const std::string &id) const;

  ProjectSession &session_;
  persistence::StructuralStore &repository_;
  persistence::StructureModelStore *structure_models_{};
};

} // namespace inde::application

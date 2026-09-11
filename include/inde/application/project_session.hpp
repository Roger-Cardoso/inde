#pragma once

#include "inde/project/catalog.hpp"
#include "inde/project/project.hpp"
#include "inde/project/structural_node.hpp"

#include <optional>
#include <vector>

namespace inde::application {

class ProjectSession {
public:
  void begin(project::Project project, project::Catalog catalog,
             std::vector<project::StructuralNode> structural_nodes);
  void clear() noexcept;

  [[nodiscard]] project::Project *current() noexcept;
  [[nodiscard]] const project::Project *current() const noexcept;
  [[nodiscard]] project::Catalog &catalog() noexcept { return catalog_; }
  [[nodiscard]] const project::Catalog &catalog() const noexcept {
    return catalog_;
  }
  [[nodiscard]] std::vector<project::StructuralNode> &
  structural_nodes() noexcept {
    return structural_nodes_;
  }
  [[nodiscard]] const std::vector<project::StructuralNode> &
  structural_nodes() const noexcept {
    return structural_nodes_;
  }

private:
  std::optional<project::Project> current_;
  project::Catalog catalog_;
  std::vector<project::StructuralNode> structural_nodes_;
};

} // namespace inde::application

#include "inde/application/project_session.hpp"

namespace inde::application {

void ProjectSession::begin(
    project::Project project, project::Catalog catalog,
    std::vector<project::StructuralNode> structural_nodes) {
  current_ = std::move(project);
  catalog_ = std::move(catalog);
  structural_nodes_ = std::move(structural_nodes);
}

void ProjectSession::clear() noexcept {
  current_.reset();
  catalog_ = {};
  structural_nodes_.clear();
}

project::Project *ProjectSession::current() noexcept {
  return current_ ? &*current_ : nullptr;
}

const project::Project *ProjectSession::current() const noexcept {
  return current_ ? &*current_ : nullptr;
}

} // namespace inde::application

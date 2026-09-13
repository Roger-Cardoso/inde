#include "inde/application/project_service.hpp"

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace inde::application {

ProjectService::ProjectService() = default;

const project::Project &
ProjectService::create(const std::filesystem::path &path,
                       const std::string &name) {
  auto created = repository_.create(path, name);
  try {
    static_cast<void>(database_importer_.create_from_json(created.path()));
    database_repository_.prepare(created);
    database_repository_.synchronize_manifest(created);
    structure_model_repository_.initialize(created.path());
    narrative_repository_.initialize(created.path());
    planning_repository_.initialize(created.path());
    writing_repository_.initialize(created.path());
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove_all(created.path(), ignored);
    throw;
  }
  session_.begin(std::move(created), {}, {});
  clear_planning_context();
  recents_.touch(session_.current()->path());
  return *session_.current();
}

const project::Project &
ProjectService::open(const std::filesystem::path &path) {
  auto loaded_project = repository_.open(path);
  const bool legacy = loaded_project.manifest().format_version == 1;
  if (legacy) {
    // Para v1, inclusive quando já existe um banco de uma importação manual,
    // o importador compara integralmente as duas representações antes do corte.
    static_cast<void>(
        database_importer_.create_from_json(loaded_project.path()));
  }
  database_repository_.prepare(loaded_project);
  if (legacy) {
    loaded_project.manifest().format_version = project::current_format_version;
    repository_.save(loaded_project);
  }
  database_repository_.synchronize_manifest(loaded_project);
  structure_model_repository_.initialize(loaded_project.path());

  auto catalog = catalog_repository_.load(loaded_project.path());
  auto structural_nodes = structural_repository_.load(loaded_project.path());
  std::vector<std::string> work_ids;
  for (const auto &work : catalog.works)
    work_ids.push_back(work.id);
  project::StructuralTree(structural_nodes).validate_all(work_ids);
  narrative_repository_.initialize(loaded_project.path());
  planning_repository_.initialize(loaded_project.path());
  writing_repository_.initialize(loaded_project.path());
  session_.begin(std::move(loaded_project), std::move(catalog),
                 std::move(structural_nodes));
  clear_planning_context();
  recents_.touch(session_.current()->path());
  return *session_.current();
}

void ProjectService::save() {
  if (!session_.current())
    throw std::runtime_error("Nenhum projeto está aberto");
  repository_.save(*session_.current());
  database_repository_.synchronize_manifest(*session_.current());
  recents_.touch(session_.current()->path());
}

const project::Project &
ProjectService::save_as(const std::filesystem::path &path) {
  if (!session_.current())
    throw std::runtime_error("Nenhum projeto está aberto");
  const auto old_project_id = session_.current()->manifest().project_id;
  auto project = repository_.save_as(*session_.current(), path);
  try {
    database_repository_.replace_project_identity(
        project.path(), old_project_id, project.manifest());
    database_repository_.prepare(project);
    structure_model_repository_.initialize(project.path());
    narrative_repository_.initialize(project.path());
    planning_repository_.initialize(project.path());
    writing_repository_.initialize(project.path());
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove_all(project.path(), ignored);
    throw;
  }
  auto catalog = std::move(session_.catalog());
  auto structural_nodes = std::move(session_.structural_nodes());
  session_.begin(std::move(project), std::move(catalog),
                 std::move(structural_nodes));
  recents_.touch(session_.current()->path());
  return *session_.current();
}

void ProjectService::close() noexcept {
  cartography_service_.clear_cache();
  session_.clear();
  clear_planning_context();
}

const project::Project *ProjectService::current() const noexcept {
  return session_.current();
}

std::vector<std::filesystem::path> ProjectService::recent_projects() const {
  return recents_.load();
}

const project::Catalog &ProjectService::catalog() const noexcept {
  return session_.catalog();
}

const project::IntellectualProperty &
ProjectService::create_intellectual_property(std::string title,
                                             std::string subtitle,
                                             std::string description,
                                             std::string cover_path) {
  return catalog_service_.create_intellectual_property(
      std::move(title), std::move(subtitle), std::move(description),
      std::move(cover_path));
}

void ProjectService::update_intellectual_property(
    const project::IntellectualProperty &value) {
  catalog_service_.update_intellectual_property(value);
}

void ProjectService::delete_intellectual_property(const std::string &id) {
  catalog_service_.delete_intellectual_property(id);
}

const project::Work &
ProjectService::create_work(std::string intellectual_property_id,
                            std::string title, std::string subtitle,
                            std::string synopsis, std::string language,
                            std::string status) {
  return catalog_service_.create_work(std::move(intellectual_property_id),
                                      std::move(title), std::move(subtitle),
                                      std::move(synopsis), std::move(language),
                                      std::move(status));
}

void ProjectService::update_work(const project::Work &value) {
  catalog_service_.update_work(value);
}

void ProjectService::delete_work(const std::string &id) {
  persistence::EntityWorkScopeQuery query;
  query.work_id = id;
  query.limit = 1;
  if (!narrative_service_.work_scopes(query).empty())
    throw std::runtime_error(
        "Remova primeiro os vínculos de entidades com esta Obra");
  catalog_service_.delete_work(id);
}

std::vector<project::StructuralNode>
ProjectService::structural_nodes_for_work(const std::string &work_id) const {
  return structural_service_.nodes_for_work(work_id);
}

std::vector<project::StructuralElementType>
ProjectService::structural_element_types() const {
  return structural_service_.element_types();
}

project::StructuralElementType
ProjectService::create_structural_element_type(std::string name) {
  return structural_service_.create_element_type(std::move(name));
}

project::StructuralElementType ProjectService::update_structural_element_type(
    const project::StructuralElementType &input) {
  return structural_service_.update_element_type(input);
}

void ProjectService::delete_structural_element_type(const std::string &id) {
  structural_service_.delete_element_type(id);
}

const project::StructuralNode &ProjectService::create_structural_node(
    std::string work_id, std::optional<std::string> parent_id,
    std::string structural_type_id, std::string designator, std::string title) {
  return structural_service_.create_node(
      std::move(work_id), std::move(parent_id), std::move(structural_type_id),
      std::move(designator), std::move(title));
}

const project::StructuralNode &ProjectService::create_structural_node(
    std::string work_id, std::optional<std::string> parent_id,
    project::StructuralNodeType type, std::string title,
    std::string custom_type_name) {
  return structural_service_.create_node(
      std::move(work_id), std::move(parent_id), type, std::move(title),
      std::move(custom_type_name));
}

void ProjectService::update_structural_node(
    const project::StructuralNode &value) {
  structural_service_.update_node(value);
}

void ProjectService::move_structural_node(
    const std::string &id, std::optional<std::string> new_parent_id) {
  structural_service_.move_node(id, std::move(new_parent_id));
}

void ProjectService::move_structural_node_up(const std::string &id) {
  structural_service_.move_node_up(id);
}

void ProjectService::move_structural_node_down(const std::string &id) {
  structural_service_.move_node_down(id);
}

void ProjectService::delete_structural_branch(const std::string &id) {
  auto ids =
      project::StructuralTree(session_.structural_nodes()).descendants_of(id);
  ids.push_back(id);
  for (const auto &node_id : ids) {
    persistence::EditorialReferenceQuery query;
    query.editorial_node_id = node_id;
    query.limit = 1;
    if (!narrative_service_.editorial_references(query).empty())
      throw std::runtime_error(
          "Remova primeiro as referências de entidades desta ramificação");
  }
  structural_service_.delete_branch(id);
}

std::string ProjectService::duplicate_structural_branch(const std::string &id) {
  return structural_service_.duplicate_branch(id);
}

void ProjectService::create_structural_template(
    const std::string &work_id, const std::string &template_name) {
  structural_service_.create_template(work_id, template_name);
}

bool ProjectService::restore_last_structural_deletion() {
  return structural_service_.restore_last_deletion();
}

} // namespace inde::application

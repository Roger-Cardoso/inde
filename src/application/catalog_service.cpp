#include "inde/application/catalog_service.hpp"

#include <algorithm>
#include <stdexcept>

namespace inde::application {

project::Project &CatalogService::require_project() {
  auto *project = session_.current();
  if (!project)
    throw std::runtime_error("Nenhum projeto está aberto");
  return *project;
}

const project::IntellectualProperty &
CatalogService::create_intellectual_property(std::string title,
                                             std::string subtitle,
                                             std::string description,
                                             std::string cover_path) {
  static_cast<void>(require_project());
  const auto now = project::utc_now();
  project::IntellectualProperty value{project::new_uuid(),
                                      std::move(title),
                                      std::move(subtitle),
                                      std::move(description),
                                      std::move(cover_path),
                                      now,
                                      now};
  repository_.save(session_.current()->path(), value);
  session_.catalog().intellectual_properties.push_back(std::move(value));
  return session_.catalog().intellectual_properties.back();
}

void CatalogService::update_intellectual_property(
    const project::IntellectualProperty &input) {
  static_cast<void>(require_project());
  auto found =
      std::find_if(session_.catalog().intellectual_properties.begin(),
                   session_.catalog().intellectual_properties.end(),
                   [&](const auto &value) { return value.id == input.id; });
  if (found == session_.catalog().intellectual_properties.end())
    throw std::runtime_error("Propriedade intelectual não encontrada");
  auto value = input;
  value.updated_at = project::utc_now();
  repository_.save(session_.current()->path(), value);
  *found = std::move(value);
}

void CatalogService::delete_intellectual_property(const std::string &id) {
  static_cast<void>(require_project());
  const bool has_works = std::any_of(
      session_.catalog().works.begin(), session_.catalog().works.end(),
      [&](const auto &w) { return w.intellectual_property_id == id; });
  if (has_works)
    throw std::runtime_error(
        "Remova primeiro as obras vinculadas a esta propriedade intelectual");
  repository_.remove_intellectual_property(session_.current()->path(), id);
  std::erase_if(session_.catalog().intellectual_properties,
                [&](const auto &value) { return value.id == id; });
}

const project::Work &
CatalogService::create_work(std::string ip_id, std::string title,
                            std::string subtitle, std::string synopsis,
                            std::string language, std::string status) {
  static_cast<void>(require_project());
  const bool exists =
      std::any_of(session_.catalog().intellectual_properties.begin(),
                  session_.catalog().intellectual_properties.end(),
                  [&](const auto &ip) { return ip.id == ip_id; });
  if (!exists)
    throw std::runtime_error("A propriedade intelectual escolhida não existe");
  const auto now = project::utc_now();
  project::Work value{project::new_uuid(),
                      std::move(ip_id),
                      std::move(title),
                      std::move(subtitle),
                      std::move(synopsis),
                      std::move(language),
                      std::move(status),
                      now,
                      now};
  repository_.save(session_.current()->path(), value);
  session_.catalog().works.push_back(std::move(value));
  return session_.catalog().works.back();
}

void CatalogService::update_work(const project::Work &input) {
  static_cast<void>(require_project());
  auto found = std::find_if(
      session_.catalog().works.begin(), session_.catalog().works.end(),
      [&](const auto &value) { return value.id == input.id; });
  if (found == session_.catalog().works.end())
    throw std::runtime_error("Obra não encontrada");
  auto value = input;
  value.updated_at = project::utc_now();
  repository_.save(session_.current()->path(), value);
  *found = std::move(value);
}

void CatalogService::delete_work(const std::string &id) {
  static_cast<void>(require_project());
  if (std::any_of(session_.structural_nodes().begin(),
                  session_.structural_nodes().end(),
                  [&](const auto &node) { return node.work_id == id; }))
    throw std::runtime_error(
        "Remova primeiro a estrutura editorial vinculada a esta obra");
  repository_.remove_work(session_.current()->path(), id);
  std::erase_if(session_.catalog().works,
                [&](const auto &value) { return value.id == id; });
}

} // namespace inde::application

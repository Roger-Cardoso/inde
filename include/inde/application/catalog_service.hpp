#pragma once

#include "inde/application/project_session.hpp"
#include "inde/persistence/catalog_store.hpp"

#include <string>

namespace inde::application {

class CatalogService {
public:
  CatalogService(ProjectSession &session,
                 persistence::CatalogStore &repository)
      : session_(session), repository_(repository) {}

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

private:
  [[nodiscard]] project::Project &require_project();

  ProjectSession &session_;
  persistence::CatalogStore &repository_;
};

} // namespace inde::application

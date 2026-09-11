#pragma once

#include "inde/project/catalog.hpp"

#include <filesystem>
#include <string>

namespace inde::persistence {

class CatalogStore {
public:
  virtual ~CatalogStore() = default;

  [[nodiscard]] virtual project::Catalog
  load(const std::filesystem::path &project_path) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::IntellectualProperty &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::Work &value) const = 0;
  virtual void remove_intellectual_property(
      const std::filesystem::path &project_path, const std::string &id) const = 0;
  virtual void remove_work(const std::filesystem::path &project_path,
                           const std::string &id) const = 0;
};

} // namespace inde::persistence

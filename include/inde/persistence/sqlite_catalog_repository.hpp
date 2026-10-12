#pragma once

#include "inde/persistence/catalog_store.hpp"
#include <optional>

namespace inde::persistence {

class SqliteCatalogRepository final : public CatalogStore {
public:
  [[nodiscard]] project::Catalog
  load(const std::filesystem::path &project_path) const override;
  void save(const std::filesystem::path &project_path,
            const project::IntellectualProperty &value) const override;
  void save(const std::filesystem::path &project_path,
            const project::Work &value) const override;
  void create_work_context(const std::filesystem::path &project_path,
                           const project::Work &work,
                           const std::optional<project::IntellectualProperty> &new_universe) const;
  void remove_intellectual_property(
      const std::filesystem::path &project_path,
      const std::string &id) const override;
  void remove_work(const std::filesystem::path &project_path,
                   const std::string &id) const override;
};

} // namespace inde::persistence

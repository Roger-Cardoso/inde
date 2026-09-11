#pragma once

#include "inde/project/project.hpp"

#include <filesystem>
#include <string>

namespace inde::persistence {

class ProjectDatabaseRepository {
public:
  [[nodiscard]] static std::filesystem::path
  database_path(const std::filesystem::path &project_path);

  void prepare(const project::Project &value) const;
  void synchronize_manifest(const project::Project &value) const;
  void replace_project_identity(const std::filesystem::path &project_path,
                                const std::string &old_project_id,
                                const project::Manifest &manifest) const;
};

} // namespace inde::persistence

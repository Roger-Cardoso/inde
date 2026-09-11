#pragma once

#include <cstddef>
#include <filesystem>

namespace inde::persistence {

struct ProjectDatabaseImportResult {
  std::filesystem::path database_path;
  std::size_t intellectual_property_count{};
  std::size_t work_count{};
  std::size_t editorial_node_count{};
  bool created{};
};

class ProjectDatabaseImporter {
public:
  // Cria data/project.sqlite3 a partir dos JSON existentes. A operação não
  // altera nem remove a fonte e não participa da sessão ativa.
  [[nodiscard]] ProjectDatabaseImportResult
  create_from_json(const std::filesystem::path &project_path) const;
};

} // namespace inde::persistence


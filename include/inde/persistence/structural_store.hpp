#pragma once

#include "inde/project/structural_node.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace inde::persistence {

class StructuralStore {
public:
  virtual ~StructuralStore() = default;

  [[nodiscard]] virtual std::vector<project::StructuralNode>
  load(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::vector<project::StructuralElementType>
  load_types(const std::filesystem::path &project_path) const = 0;
  virtual void save_type(
      const std::filesystem::path &project_path,
      const project::StructuralElementType &value) const = 0;
  virtual void remove_type(const std::filesystem::path &project_path,
                           const std::string &id) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::StructuralNode &value) const = 0;
  virtual void save_many(
      const std::filesystem::path &project_path,
      const std::vector<project::StructuralNode> &values) const = 0;
  virtual void remove(const std::filesystem::path &project_path,
                      const std::string &id) const = 0;
  virtual std::string move_to_trash(
      const std::filesystem::path &project_path,
      const std::vector<std::string> &ids) const = 0;
  virtual bool
  restore_latest_trash(const std::filesystem::path &project_path) const = 0;
};

} // namespace inde::persistence

#pragma once

#include "inde/project/project.hpp"

#include <filesystem>
#include <string>

namespace inde::persistence {

class ProjectRepository {
public:
    [[nodiscard]] project::Project create(const std::filesystem::path& path,
                                          const std::string& name) const;
    [[nodiscard]] project::Project open(const std::filesystem::path& path) const;
    [[nodiscard]] project::Project save_as(const project::Project& source,
                                           const std::filesystem::path& path) const;
    void save(project::Project& project) const;

private:
    [[nodiscard]] static std::filesystem::path normalized_path(std::filesystem::path path);
    static void write_manifest_atomic(const std::filesystem::path& path,
                                      const project::Manifest& manifest);
};

} // namespace inde::persistence

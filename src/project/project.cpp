#include "inde/project/project.hpp"

#include <utility>

namespace inde::project {

Project::Project(std::filesystem::path path, Manifest manifest)
    : path_(std::move(path)), manifest_(std::move(manifest)) {}

const std::filesystem::path& Project::path() const noexcept { return path_; }
const Manifest& Project::manifest() const noexcept { return manifest_; }
Manifest& Project::manifest() noexcept { return manifest_; }

} // namespace inde::project


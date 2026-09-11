#pragma once

#include "inde/project/manifest.hpp"

#include <filesystem>

namespace inde::project {

class Project {
public:
    Project(std::filesystem::path path, Manifest manifest);

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const Manifest& manifest() const noexcept;
    [[nodiscard]] Manifest& manifest() noexcept;

private:
    std::filesystem::path path_;
    Manifest manifest_;
};

} // namespace inde::project


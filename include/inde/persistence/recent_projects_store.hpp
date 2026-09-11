#pragma once

#include <filesystem>
#include <vector>

namespace inde::persistence {

class RecentProjectsStore {
public:
    explicit RecentProjectsStore(std::filesystem::path storage_path = default_path());

    // Recent projects are a convenience cache. A damaged or unwritable cache
    // must never make project creation, opening, saving, or closing fail.
    [[nodiscard]] std::vector<std::filesystem::path> load() const noexcept;
    void touch(const std::filesystem::path& project_path) const noexcept;
    [[nodiscard]] static std::filesystem::path default_path();

private:
    std::filesystem::path storage_path_;
};

} // namespace inde::persistence

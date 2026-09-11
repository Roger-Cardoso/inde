#include "inde/persistence/recent_projects_store.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

namespace inde::persistence {
namespace {
constexpr std::size_t maximum_recents = 10;
}

RecentProjectsStore::RecentProjectsStore(std::filesystem::path storage_path)
    : storage_path_(std::move(storage_path)) {}

std::vector<std::filesystem::path> RecentProjectsStore::load() const noexcept {
    std::vector<std::filesystem::path> projects;
    try {
        std::ifstream input(storage_path_);
        std::string line;
        while (std::getline(input, line) && projects.size() < maximum_recents) {
            std::error_code error;
            if (!line.empty() && std::filesystem::is_directory(line, error) &&
                !error)
                projects.emplace_back(line);
        }
    } catch (...) {
        // An auxiliary cache cannot prevent the application from starting.
    }
    return projects;
}

void RecentProjectsStore::touch(
    const std::filesystem::path& project_path) const noexcept {
    try {
        auto projects = load();
        const auto normalized =
            std::filesystem::absolute(project_path).lexically_normal();
        projects.erase(std::remove(projects.begin(), projects.end(), normalized),
                       projects.end());
        projects.insert(projects.begin(), normalized);
        if (projects.size() > maximum_recents) projects.resize(maximum_recents);

        std::error_code error;
        std::filesystem::create_directories(storage_path_.parent_path(), error);
        if (error) return;
        const auto temporary = storage_path_.string() + ".tmp";
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) return;
        for (const auto& path : projects) output << path.string() << '\n';
        output.close();
        if (!output) {
            std::filesystem::remove(temporary, error);
            return;
        }
        std::filesystem::rename(temporary, storage_path_, error);
        if (error) std::filesystem::remove(temporary, error);
    } catch (...) {
        // Opening the project is authoritative; remembering it is best effort.
    }
}

std::filesystem::path RecentProjectsStore::default_path() {
    if (const char* config = std::getenv("XDG_CONFIG_HOME")) {
        return std::filesystem::path(config) / "inde" / "recent-projects";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".config" / "inde" / "recent-projects";
    }
    return std::filesystem::temp_directory_path() / "inde-recent-projects";
}

} // namespace inde::persistence

#pragma once

#include <string>

namespace inde::project {

// v1: catálogo e estrutura em arquivos JSON.
// v2: project.sqlite3 é a fonte operacional obrigatória.
inline constexpr int current_format_version = 2;

struct Manifest {
    std::string format{"INDE"};
    int format_version{current_format_version};
    std::string project_id;
    std::string name;
    std::string created_at;
    std::string updated_at;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Manifest from_json(const std::string& json);
    void validate() const;
};

[[nodiscard]] std::string new_uuid();
[[nodiscard]] std::string utc_now();

} // namespace inde::project

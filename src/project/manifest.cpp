#include "inde/project/manifest.hpp"
#include "inde/persistence/json.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <uuid/uuid.h>

namespace inde::project {
std::string Manifest::to_json() const {
    validate();
    persistence::json::Value::Object object{
        {"format", format},
        {"format_version", static_cast<std::int64_t>(format_version)},
        {"project_id", project_id},
        {"name", name},
        {"created_at", created_at},
        {"updated_at", updated_at},
    };
    return persistence::json::serialize(object);
}

Manifest Manifest::from_json(const std::string& source) {
    const auto root = persistence::json::parse(source);
    Manifest manifest{
        .format = root.at("format").as_string(),
        .format_version = static_cast<int>(root.at("format_version").as_integer()),
        .project_id = root.at("project_id").as_string(),
        .name = root.at("name").as_string(),
        .created_at = root.at("created_at").as_string(),
        .updated_at = root.at("updated_at").as_string(),
    };
    manifest.validate();
    return manifest;
}

void Manifest::validate() const {
    if (format != "INDE") throw std::runtime_error("O arquivo não é um projeto INDE");
    if (format_version < 1) throw std::runtime_error("Versão de formato INDE inválida");
    if (format_version > current_format_version) {
        throw std::runtime_error("Projeto criado por uma versão mais nova do INDE");
    }
    if (project_id.empty() || name.empty() || created_at.empty() || updated_at.empty()) {
        throw std::runtime_error("Manifesto INDE incompleto");
    }
    uuid_t parsed{};
    if (uuid_parse(project_id.c_str(), parsed) != 0) {
        throw std::runtime_error("UUID de projeto inválido");
    }
}

std::string new_uuid() {
    uuid_t id{};
    uuid_generate_random(id);
    std::array<char, 37> text{};
    uuid_unparse_lower(id, text.data());
    return text.data();
}

std::string utc_now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&time, &utc);
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

} // namespace inde::project

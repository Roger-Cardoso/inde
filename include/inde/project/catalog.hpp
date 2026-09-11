#pragma once
#include <string>
#include <vector>

namespace inde::project {
inline constexpr int current_catalog_version = 1;

struct IntellectualProperty {
    std::string id, title, subtitle, description, cover_path, created_at, updated_at;
};

struct Work {
    std::string id, intellectual_property_id, title, subtitle, synopsis;
    std::string language{"pt-BR"};
    std::string status{"Planejamento"};
    std::string created_at, updated_at;
};

struct Catalog {
    std::vector<IntellectualProperty> intellectual_properties;
    std::vector<Work> works;
};

void validate(const IntellectualProperty& value);
void validate(const Work& value);
} // namespace inde::project

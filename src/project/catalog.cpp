#include "inde/project/catalog.hpp"
#include <stdexcept>
#include <uuid/uuid.h>

namespace inde::project {
namespace {
void valid_id(const std::string& id, const char* label) {
    uuid_t parsed{};
    if (uuid_parse(id.c_str(), parsed) != 0) throw std::runtime_error(std::string(label) + " possui UUID inválido");
}
}
void validate(const IntellectualProperty& value) {
    valid_id(value.id, "A propriedade intelectual");
    if (value.title.empty()) throw std::runtime_error("O título da propriedade intelectual é obrigatório");
    if (value.created_at.empty() || value.updated_at.empty()) throw std::runtime_error("Datas da propriedade intelectual ausentes");
}
void validate(const Work& value) {
    valid_id(value.id, "A obra");
    valid_id(value.intellectual_property_id, "A propriedade intelectual da obra");
    if (value.title.empty()) throw std::runtime_error("O título da obra é obrigatório");
    if (value.language.empty() || value.status.empty()) throw std::runtime_error("Idioma e status da obra são obrigatórios");
    if (value.created_at.empty() || value.updated_at.empty()) throw std::runtime_error("Datas da obra ausentes");
}
} // namespace inde::project

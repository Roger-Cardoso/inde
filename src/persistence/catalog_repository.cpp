#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/json.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace inde::persistence {
namespace {

std::string read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Não foi possível ler " + path.string());
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

void atomic_write(const std::filesystem::path& target, const std::string& contents) {
    std::filesystem::create_directories(target.parent_path());
    auto temporary = target;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Não foi possível gravar " + target.string());
        output << contents;
        output.flush();
        if (!output) throw std::runtime_error("Falha ao gravar " + target.string());
    }
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Falha ao concluir gravação: " + error.message());
    }
}

void require_schema(const json::Value& root, std::string_view schema) {
    if (root.at("schema").as_string() != schema) throw std::runtime_error("Registro de catálogo inválido");
    if (root.at("version").as_integer() != 1) throw std::runtime_error("Versão de catálogo não suportada");
}

std::string serialize(const project::IntellectualProperty& value) {
    project::validate(value);
    return json::serialize(json::Value::Object{
        {"schema", "inde.intellectual-property"}, {"version", 1}, {"id", value.id},
        {"title", value.title}, {"subtitle", value.subtitle}, {"description", value.description},
        {"cover_path", value.cover_path}, {"created_at", value.created_at}, {"updated_at", value.updated_at},
    });
}

std::string serialize(const project::Work& value) {
    project::validate(value);
    return json::serialize(json::Value::Object{
        {"schema", "inde.work"}, {"version", 1}, {"id", value.id},
        {"intellectual_property_id", value.intellectual_property_id}, {"title", value.title},
        {"subtitle", value.subtitle}, {"synopsis", value.synopsis}, {"language", value.language},
        {"status", value.status}, {"created_at", value.created_at}, {"updated_at", value.updated_at},
    });
}

project::IntellectualProperty parse_ip(const std::string& source) {
    const auto root = json::parse(source);
    require_schema(root, "inde.intellectual-property");
    project::IntellectualProperty value{
        root.at("id").as_string(), root.at("title").as_string(), root.at("subtitle").as_string(),
        root.at("description").as_string(), root.at("cover_path").as_string(),
        root.at("created_at").as_string(), root.at("updated_at").as_string(),
    };
    project::validate(value);
    return value;
}

project::Work parse_work(const std::string& source) {
    const auto root = json::parse(source);
    require_schema(root, "inde.work");
    project::Work value{
        root.at("id").as_string(), root.at("intellectual_property_id").as_string(),
        root.at("title").as_string(), root.at("subtitle").as_string(), root.at("synopsis").as_string(),
        root.at("language").as_string(), root.at("status").as_string(),
        root.at("created_at").as_string(), root.at("updated_at").as_string(),
    };
    project::validate(value);
    return value;
}

template<class Parser, class Vector>
void load_directory(const std::filesystem::path& directory, Parser parser, Vector& values) {
    if (!std::filesystem::exists(directory)) return;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        try {
            values.push_back(parser(read(entry.path())));
        } catch (const std::exception& error) {
            throw std::runtime_error("Falha ao carregar " + entry.path().string() + ": " + error.what());
        }
    }
}

} // namespace

project::Catalog CatalogRepository::load(const std::filesystem::path& path) const {
    project::Catalog catalog;
    load_directory(path / "data" / "intellectual-properties", parse_ip, catalog.intellectual_properties);
    load_directory(path / "data" / "works", parse_work, catalog.works);
    return catalog;
}

void CatalogRepository::save(const std::filesystem::path& path, const project::IntellectualProperty& value) const {
    atomic_write(path / "data" / "intellectual-properties" / (value.id + ".json"), serialize(value));
}

void CatalogRepository::save(const std::filesystem::path& path, const project::Work& value) const {
    atomic_write(path / "data" / "works" / (value.id + ".json"), serialize(value));
}

void CatalogRepository::remove_intellectual_property(const std::filesystem::path& path, const std::string& id) const {
    std::filesystem::remove(path / "data" / "intellectual-properties" / (id + ".json"));
}

void CatalogRepository::remove_work(const std::filesystem::path& path, const std::string& id) const {
    std::filesystem::remove(path / "data" / "works" / (id + ".json"));
}

} // namespace inde::persistence


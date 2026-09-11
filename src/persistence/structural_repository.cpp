#include "inde/persistence/structural_repository.hpp"
#include "inde/persistence/json.hpp"
#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
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
        throw std::runtime_error("Falha ao concluir gravação estrutural: " + error.message());
    }
}

std::string serialize(const project::StructuralNode& value) {
    project::validate(value);
    json::Value parent = value.parent_id ? json::Value(*value.parent_id) : json::Value(nullptr);
    return json::serialize(json::Value::Object{
        {"schema", "inde.structural-node"}, {"version", project::current_structural_node_version},
        {"id", value.id}, {"work_id", value.work_id}, {"parent_id", std::move(parent)},
        {"type", project::to_string(value.type)}, {"title", value.title}, {"subtitle", value.subtitle},
        {"synopsis", value.synopsis}, {"custom_type_name", value.custom_type_name},
        {"status", value.status}, {"position", value.position},
        {"created_at", value.created_at}, {"updated_at", value.updated_at},
    });
}

std::int64_t parse_position(const json::Value& value) {
    const auto& storage = value.storage();
    if (const auto* integer = std::get_if<std::int64_t>(&storage)) {
        if (*integer >= 0 && *integer < project::structural_position_step) {
            // As primeiras versões gravavam a ordem em unidades decimais:
            // 1 significava 1.000 e o modelo atual representa isso como 1000.
            return *integer * project::structural_position_step;
        }
        return *integer;
    }

    const auto* decimal = std::get_if<double>(&storage);
    if (!decimal || !std::isfinite(*decimal) || *decimal < 0.0) {
        throw std::runtime_error("Posição estrutural inválida");
    }
    const long double scaled =
        static_cast<long double>(*decimal) * project::structural_position_step;
    if (scaled > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        throw std::runtime_error("Posição estrutural fora do limite suportado");
    }
    const auto rounded = static_cast<std::int64_t>(std::llround(scaled));
    if (std::fabs(scaled - static_cast<long double>(rounded)) > 0.000001L) {
        throw std::runtime_error(
            "Posição estrutural legada possui precisão não suportada");
    }
    return rounded;
}

project::StructuralNode parse(const std::string& source) {
    const auto root = json::parse(source);
    if (root.at("schema").as_string() != "inde.structural-node") {
        throw std::runtime_error("Registro estrutural inválido");
    }
    const auto version = root.at("version").as_integer();
    if (version < 1 || version > project::current_structural_node_version) {
        throw std::runtime_error("Versão estrutural não suportada");
    }
    const auto& parent = root.at("parent_id");
    project::StructuralNode value{
        root.at("id").as_string(),
        root.at("work_id").as_string(),
        parent.is_null() ? std::nullopt : std::optional<std::string>{parent.as_string()},
        project::structural_node_type_from_string(root.at("type").as_string()),
        root.at("title").as_string(),
        root.at("subtitle").as_string(),
        root.at("synopsis").as_string(),
        root.at("custom_type_name").as_string(),
        root.at("status").as_string(),
        parse_position(root.at("position")),
        root.at("created_at").as_string(),
        root.at("updated_at").as_string(),
        {},
        {},
        {},
        {},
    };
    project::validate(value);
    return value;
}

} // namespace

std::vector<project::StructuralNode> StructuralRepository::load(const std::filesystem::path& path) const {
    std::vector<project::StructuralNode> values;
    const auto directory = path / "data" / "structural-nodes";
    if (!std::filesystem::exists(directory)) return values;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        try {
            values.push_back(parse(read(entry.path())));
        } catch (const std::exception& error) {
            throw std::runtime_error("Falha ao carregar " + entry.path().string() + ": " + error.what());
        }
    }
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.position < b.position; });
    return values;
}

std::vector<project::StructuralElementType>
StructuralRepository::load_types(const std::filesystem::path&) const {
    std::vector<project::StructuralElementType> result;
    const auto now = project::utc_now();
    for (const auto& type : project::builtin_structural_element_types())
        result.push_back({type.id, type.name, true, now, now});
    return result;
}

void StructuralRepository::save_type(
    const std::filesystem::path&,
    const project::StructuralElementType&) const {
    throw std::runtime_error(
        "Tipos estruturais gerenciáveis exigem o formato SQLite atual");
}

void StructuralRepository::remove_type(const std::filesystem::path&,
                                       const std::string&) const {
    throw std::runtime_error(
        "Tipos estruturais gerenciáveis exigem o formato SQLite atual");
}

void StructuralRepository::save(const std::filesystem::path& path, const project::StructuralNode& value) const {
    atomic_write(path / "data" / "structural-nodes" / (value.id + ".json"), serialize(value));
}

void StructuralRepository::save_many(
    const std::filesystem::path& path,
    const std::vector<project::StructuralNode>& values) const {
    // Mantém o adaptador legado funcional para testes e importação. O caminho
    // operacional SQLite fornece atomicidade real para esta operação.
    for (const auto& value : values) save(path, value);
}

void StructuralRepository::remove(const std::filesystem::path& path, const std::string& id) const {
    std::error_code error;
    const bool removed = std::filesystem::remove(path / "data" / "structural-nodes" / (id + ".json"), error);
    if (error || !removed) throw std::runtime_error("Não foi possível remover o elemento estrutural");
}

std::string StructuralRepository::move_to_trash(
    const std::filesystem::path& path,
    const std::vector<std::string>& ids
) const {
    const auto operation = project::new_uuid();
    const auto trash = path / "data" / ".trash" / operation;
    std::filesystem::create_directories(trash);
    std::vector<std::string> moved;
    try {
        for (const auto& id : ids) {
            const auto source = path / "data" / "structural-nodes" / (id + ".json");
            const auto target = trash / (id + ".json");
            std::filesystem::rename(source, target);
            moved.push_back(id);
        }
    } catch (...) {
        for (const auto& id : moved) {
            std::error_code ignored;
            std::filesystem::rename(trash / (id + ".json"), path / "data" / "structural-nodes" / (id + ".json"), ignored);
        }
        std::error_code ignored;
        std::filesystem::remove_all(trash, ignored);
        throw;
    }
    return operation;
}

bool StructuralRepository::restore_latest_trash(const std::filesystem::path& path) const {
    const auto root = path / "data" / ".trash";
    if (!std::filesystem::exists(root)) return false;
    std::optional<std::filesystem::directory_entry> latest;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_directory() && (!latest || entry.last_write_time() > latest->last_write_time())) latest = entry;
    }
    if (!latest) return false;

    const auto destination = path / "data" / "structural-nodes";
    std::filesystem::create_directories(destination);
    std::vector<std::filesystem::path> moved;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(latest->path())) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
            const auto target = destination / entry.path().filename();
            std::filesystem::rename(entry.path(), target);
            moved.push_back(target);
        }
    } catch (...) {
        for (const auto& target : moved) {
            std::error_code ignored;
            std::filesystem::rename(target, latest->path() / target.filename(), ignored);
        }
        throw;
    }
    std::filesystem::remove(latest->path());
    return true;
}

} // namespace inde::persistence

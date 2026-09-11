#include "inde/persistence/project_repository.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace inde::persistence {

project::Project ProjectRepository::create(const std::filesystem::path& requested_path,
                                           const std::string& name) const {
    if (name.empty()) throw std::runtime_error("O nome do projeto não pode ficar vazio");
    const auto path = normalized_path(requested_path);
    if (std::filesystem::exists(path)) {
        throw std::runtime_error("Já existe um arquivo ou projeto nesse local");
    }

    std::filesystem::create_directories(path / "assets");
    std::filesystem::create_directories(path / "data");
    std::filesystem::create_directories(path / "documents");

    const auto now = project::utc_now();
    project::Manifest manifest{
        .format = "INDE",
        .format_version = project::current_format_version,
        .project_id = project::new_uuid(),
        .name = name,
        .created_at = now,
        .updated_at = now,
    };

    try {
        write_manifest_atomic(path, manifest);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
        throw;
    }
    return {path, std::move(manifest)};
}

project::Project ProjectRepository::open(const std::filesystem::path& requested_path) const {
    const auto path = normalized_path(requested_path);
    if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error("O projeto .inde não foi encontrado ou não é um contêiner válido");
    }
    std::ifstream input(path / "manifest.json", std::ios::binary);
    if (!input) throw std::runtime_error("Não foi possível abrir o manifesto do projeto");
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) throw std::runtime_error("Falha ao ler o manifesto do projeto");
    return {path, project::Manifest::from_json(contents.str())};
}

project::Project ProjectRepository::save_as(const project::Project& source,
                                            const std::filesystem::path& requested_path) const {
    const auto target = normalized_path(requested_path);
    if (std::filesystem::exists(target)) {
        throw std::runtime_error("Já existe um arquivo ou projeto nesse local");
    }
    if (!std::filesystem::is_directory(source.path())) {
        throw std::runtime_error("O projeto de origem não está disponível");
    }

    try {
        std::filesystem::copy(source.path(), target, std::filesystem::copy_options::recursive);
        auto manifest = source.manifest();
        const auto now = project::utc_now();
        manifest.project_id = project::new_uuid();
        manifest.created_at = now;
        manifest.updated_at = now;
        write_manifest_atomic(target, manifest);
        return {target, std::move(manifest)};
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(target, ignored);
        throw;
    }
}

void ProjectRepository::save(project::Project& current_project) const {
    current_project.manifest().updated_at = project::utc_now();
    write_manifest_atomic(current_project.path(), current_project.manifest());
}

std::filesystem::path ProjectRepository::normalized_path(std::filesystem::path path) {
    if (path.empty()) throw std::runtime_error("Escolha um local para o projeto");
    if (path.extension() != ".inde") path += ".inde";
    return std::filesystem::absolute(path).lexically_normal();
}

void ProjectRepository::write_manifest_atomic(const std::filesystem::path& path,
                                               const project::Manifest& manifest) {
    const auto target = path / "manifest.json";
    const auto temporary = path / "manifest.json.tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Não foi possível gravar o manifesto do projeto");
        output << manifest.to_json();
        output.flush();
        if (!output) throw std::runtime_error("Falha ao gravar o manifesto do projeto");
    }
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Não foi possível concluir a gravação do projeto: " + error.message());
    }
}

} // namespace inde::persistence

#include "inde/persistence/project_database_importer.hpp"

#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/structural_repository.hpp"
#include "inde/project/catalog.hpp"
#include "inde/project/structural_node.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace inde::persistence {
namespace {

struct SourceProject {
  project::Project project;
  project::Catalog catalog;
  std::vector<project::StructuralNode> nodes;
  std::vector<project::StructuralElementType> structural_types;
};

SourceProject load_and_validate_source(const std::filesystem::path &path) {
  ProjectRepository projects;
  CatalogRepository catalogs;
  StructuralRepository structures;

  auto loaded_project = projects.open(path);
  auto catalog = catalogs.load(loaded_project.path());
  auto nodes = structures.load(loaded_project.path());

  std::vector<project::StructuralElementType> structural_types;
  std::unordered_map<std::string, std::string> custom_type_ids;
  const auto type_stamp = loaded_project.manifest().created_at;
  for (const auto &type : project::builtin_structural_element_types())
    structural_types.push_back(
        {type.id, type.name, true, type_stamp, type_stamp});
  for (auto &node : nodes) {
    if (node.type == project::StructuralNodeType::Custom) {
      const auto [entry, inserted] = custom_type_ids.emplace(
          node.custom_type_name, project::new_uuid());
      if (inserted)
        structural_types.push_back({entry->second, node.custom_type_name, false,
                                    type_stamp, type_stamp});
      node.structural_type_id = entry->second;
      node.structural_type_name = node.custom_type_name;
    } else {
      node.structural_type_id =
          project::builtin_structural_type_id(node.type);
      node.structural_type_name = project::display_name(node.type);
    }
  }
  std::unordered_map<std::string, std::vector<project::StructuralNode *>>
      siblings;
  for (auto &node : nodes)
    siblings[node.work_id + "\n" + node.parent_id.value_or("") + "\n" +
             node.structural_type_id]
        .push_back(&node);
  for (auto &[group, values] : siblings) {
    static_cast<void>(group);
    std::sort(values.begin(), values.end(), [](const auto *left,
                                               const auto *right) {
      if (left->position != right->position)
        return left->position < right->position;
      return left->id < right->id;
    });
    for (std::size_t index = 0; index < values.size(); ++index)
      values[index]->designator = std::to_string(index + 1);
  }

  std::unordered_set<std::string> intellectual_property_ids;
  for (const auto &value : catalog.intellectual_properties) {
    project::validate(value);
    if (!intellectual_property_ids.insert(value.id).second) {
      throw std::runtime_error("UUID de propriedade intelectual duplicado");
    }
  }

  std::unordered_set<std::string> work_ids;
  std::vector<std::string> work_ids_for_tree;
  work_ids_for_tree.reserve(catalog.works.size());
  for (const auto &value : catalog.works) {
    project::validate(value);
    if (!work_ids.insert(value.id).second) {
      throw std::runtime_error("UUID de obra duplicado");
    }
    if (!intellectual_property_ids.contains(value.intellectual_property_id)) {
      throw std::runtime_error(
          "Obra ligada a uma propriedade intelectual inexistente");
    }
    work_ids_for_tree.push_back(value.id);
  }
  project::StructuralTree(nodes).validate_all(work_ids_for_tree);

  return {std::move(loaded_project), std::move(catalog), std::move(nodes),
          std::move(structural_types)};
}

void insert_source(SqliteDatabase &database, const SourceProject &source) {
  SqliteTransaction transaction(database);

  auto project_statement = database.prepare(
      "INSERT INTO projects(id, name, format_version, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?)");
  const auto &manifest = source.project.manifest();
  project_statement.bind(1, manifest.project_id);
  project_statement.bind(2, manifest.name);
  project_statement.bind(3,
                         static_cast<std::int64_t>(manifest.format_version));
  project_statement.bind(4, manifest.created_at);
  project_statement.bind(5, manifest.updated_at);
  project_statement.run();

  auto structural_type_statement = database.prepare(
      "INSERT INTO structural_element_types "
      "(id, project_id, name, is_builtin, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?, ?)");
  for (const auto &value : source.structural_types) {
    structural_type_statement.bind(1, value.id);
    structural_type_statement.bind(2, manifest.project_id);
    structural_type_statement.bind(3, value.name);
    structural_type_statement.bind(
        4, static_cast<std::int64_t>(value.is_builtin));
    structural_type_statement.bind(5, value.created_at);
    structural_type_statement.bind(6, value.updated_at);
    structural_type_statement.run();
    structural_type_statement.reset();
  }

  auto intellectual_property_statement = database.prepare(
      "INSERT INTO intellectual_properties "
      "(id, project_id, title, subtitle, description, cover_path, created_at, "
      "updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
  for (const auto &value : source.catalog.intellectual_properties) {
    intellectual_property_statement.bind(1, value.id);
    intellectual_property_statement.bind(2, manifest.project_id);
    intellectual_property_statement.bind(3, value.title);
    intellectual_property_statement.bind(4, value.subtitle);
    intellectual_property_statement.bind(5, value.description);
    intellectual_property_statement.bind(6, value.cover_path);
    intellectual_property_statement.bind(7, value.created_at);
    intellectual_property_statement.bind(8, value.updated_at);
    intellectual_property_statement.run();
    intellectual_property_statement.reset();
  }

  auto work_statement = database.prepare(
      "INSERT INTO works "
      "(id, intellectual_property_id, title, subtitle, synopsis, language, "
      "status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
  for (const auto &value : source.catalog.works) {
    work_statement.bind(1, value.id);
    work_statement.bind(2, value.intellectual_property_id);
    work_statement.bind(3, value.title);
    work_statement.bind(4, value.subtitle);
    work_statement.bind(5, value.synopsis);
    work_statement.bind(6, value.language);
    work_statement.bind(7, value.status);
    work_statement.bind(8, value.created_at);
    work_statement.bind(9, value.updated_at);
    work_statement.run();
    work_statement.reset();
  }

  auto node_statement = database.prepare(
      "INSERT INTO editorial_nodes "
      "(id, work_id, parent_id, type, title, subtitle, synopsis, "
      "custom_type_name, status, position, created_at, updated_at, "
      "structural_type_id, designator) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
  for (const auto &value : source.nodes) {
    node_statement.bind(1, value.id);
    node_statement.bind(2, value.work_id);
    if (value.parent_id) {
      node_statement.bind(3, *value.parent_id);
    } else {
      node_statement.bind_null(3);
    }
    node_statement.bind(4, project::to_string(value.type));
    node_statement.bind(5, value.title);
    node_statement.bind(6, value.subtitle);
    node_statement.bind(7, value.synopsis);
    node_statement.bind(8, value.custom_type_name);
    node_statement.bind(9, value.status);
    node_statement.bind(10, value.position);
    node_statement.bind(11, value.created_at);
    node_statement.bind(12, value.updated_at);
    node_statement.bind(13, value.structural_type_id);
    node_statement.bind(14, value.designator);
    node_statement.run();
    node_statement.reset();
  }

  transaction.commit();
}

void validate_intellectual_properties(SqliteDatabase &database,
                                      const SourceProject &source) {
  std::unordered_map<std::string, const project::IntellectualProperty *> expected;
  for (const auto &value : source.catalog.intellectual_properties) {
    expected.emplace(value.id, &value);
  }

  std::size_t count = 0;
  auto statement = database.prepare(
      "SELECT id, project_id, title, subtitle, description, cover_path, "
      "created_at, updated_at FROM intellectual_properties");
  while (statement.step()) {
    ++count;
    const auto found = expected.find(statement.column_text(0));
    if (found == expected.end()) {
      throw std::runtime_error(
          "Propriedade intelectual inesperada no banco importado");
    }
    const auto &value = *found->second;
    if (statement.column_text(1) != source.project.manifest().project_id ||
        statement.column_text(2) != value.title ||
        statement.column_text(3) != value.subtitle ||
        statement.column_text(4) != value.description ||
        statement.column_text(5) != value.cover_path ||
        statement.column_text(6) != value.created_at ||
        statement.column_text(7) != value.updated_at) {
      throw std::runtime_error(
          "Uma propriedade intelectual divergiu durante a importação");
    }
  }
  if (count != expected.size()) {
    throw std::runtime_error(
        "A quantidade de propriedades intelectuais divergiu na importação");
  }
}

void validate_works(SqliteDatabase &database, const SourceProject &source) {
  std::unordered_map<std::string, const project::Work *> expected;
  for (const auto &value : source.catalog.works) {
    expected.emplace(value.id, &value);
  }

  std::size_t count = 0;
  auto statement = database.prepare(
      "SELECT id, intellectual_property_id, title, subtitle, synopsis, "
      "language, status, created_at, updated_at FROM works");
  while (statement.step()) {
    ++count;
    const auto found = expected.find(statement.column_text(0));
    if (found == expected.end()) {
      throw std::runtime_error("Obra inesperada no banco importado");
    }
    const auto &value = *found->second;
    if (statement.column_text(1) != value.intellectual_property_id ||
        statement.column_text(2) != value.title ||
        statement.column_text(3) != value.subtitle ||
        statement.column_text(4) != value.synopsis ||
        statement.column_text(5) != value.language ||
        statement.column_text(6) != value.status ||
        statement.column_text(7) != value.created_at ||
        statement.column_text(8) != value.updated_at) {
      throw std::runtime_error("Uma obra divergiu durante a importação");
    }
  }
  if (count != expected.size()) {
    throw std::runtime_error("A quantidade de obras divergiu na importação");
  }
}

void validate_editorial_nodes(SqliteDatabase &database,
                              const SourceProject &source) {
  std::unordered_map<std::string, const project::StructuralNode *> expected;
  for (const auto &value : source.nodes) {
    expected.emplace(value.id, &value);
  }

  std::size_t count = 0;
  std::vector<project::StructuralNode> database_nodes;
  database_nodes.reserve(source.nodes.size());
  auto statement = database.prepare(
      "SELECT n.id, n.work_id, n.parent_id, n.type, n.title, n.subtitle, "
      "n.synopsis, n.custom_type_name, n.status, n.position, n.created_at, "
      "n.updated_at, n.structural_type_id, t.name, n.designator "
      "FROM editorial_nodes n JOIN structural_element_types t "
      "ON t.id = n.structural_type_id");
  while (statement.step()) {
    ++count;
    const auto id = statement.column_text(0);
    const auto found = expected.find(id);
    if (found == expected.end()) {
      throw std::runtime_error(
          "Elemento editorial inesperado no banco importado");
    }
    const auto parent = statement.column_is_null(2)
                            ? std::nullopt
                            : std::optional<std::string>{statement.column_text(2)};
    project::StructuralNode loaded{
        id,
        statement.column_text(1),
        parent,
        project::structural_node_type_from_string(statement.column_text(3)),
        statement.column_text(4),
        statement.column_text(5),
        statement.column_text(6),
        statement.column_text(7),
        statement.column_text(8),
        statement.column_integer(9),
        statement.column_text(10),
        statement.column_text(11),
        statement.column_text(12),
        statement.column_text(13),
        statement.column_text(14),
        {},
    };
    const auto &value = *found->second;
    if (loaded.work_id != value.work_id || loaded.parent_id != value.parent_id ||
        loaded.type != value.type || loaded.title != value.title ||
        loaded.subtitle != value.subtitle || loaded.synopsis != value.synopsis ||
        loaded.custom_type_name != value.custom_type_name ||
        loaded.status != value.status || loaded.position != value.position ||
        loaded.created_at != value.created_at ||
        loaded.updated_at != value.updated_at ||
        loaded.structural_type_id != value.structural_type_id ||
        loaded.structural_type_name != value.structural_type_name ||
        loaded.designator != value.designator) {
      throw std::runtime_error(
          "Um elemento editorial divergiu durante a importação");
    }
    database_nodes.push_back(std::move(loaded));
  }
  if (count != expected.size()) {
    throw std::runtime_error(
        "A quantidade de elementos editoriais divergiu na importação");
  }

  std::vector<std::string> work_ids;
  work_ids.reserve(source.catalog.works.size());
  for (const auto &work : source.catalog.works) {
    work_ids.push_back(work.id);
  }
  project::StructuralTree(database_nodes).validate_all(work_ids);
}

void validate_database(SqliteDatabase &database, const SourceProject &source) {
  SchemaMigrator migrator;
  if (migrator.current_version(database) != current_database_schema_version) {
    throw std::runtime_error("Versão inesperada no banco importado");
  }
  if (database.query_text("PRAGMA integrity_check") != "ok") {
    throw std::runtime_error("A verificação de integridade do SQLite falhou");
  }
  auto foreign_keys = database.prepare("PRAGMA foreign_key_check");
  if (foreign_keys.step()) {
    throw std::runtime_error("O banco importado contém uma referência inválida");
  }

  auto project_statement = database.prepare(
      "SELECT id, name, format_version, created_at, updated_at FROM projects");
  if (!project_statement.step()) {
    throw std::runtime_error("O projeto não foi encontrado no banco importado");
  }
  const auto &manifest = source.project.manifest();
  if (project_statement.column_text(0) != manifest.project_id ||
      project_statement.column_text(1) != manifest.name ||
      project_statement.column_integer(2) != manifest.format_version ||
      project_statement.column_text(3) != manifest.created_at ||
      project_statement.column_text(4) != manifest.updated_at) {
    throw std::runtime_error("O manifesto divergiu durante a importação");
  }
  if (project_statement.step()) {
    throw std::runtime_error("O banco importado contém mais de um projeto");
  }

  validate_intellectual_properties(database, source);
  validate_works(database, source);
  validate_editorial_nodes(database, source);
}

ProjectDatabaseImportResult make_result(const SourceProject &source,
                                        const std::filesystem::path &path,
                                        bool created) {
  return {path,
          source.catalog.intellectual_properties.size(),
          source.catalog.works.size(),
          source.nodes.size(),
          created};
}

void promote_without_overwrite(const std::filesystem::path &temporary,
                               const std::filesystem::path &target) {
  std::error_code error;
  std::filesystem::create_hard_link(temporary, target, error);
  if (error) {
    throw std::runtime_error(
        "Não foi possível promover o banco importado sem sobrescrever dados: " +
        error.message());
  }
  std::filesystem::remove(temporary, error);
  // Se a remoção do nome temporário falhar, ambos apontam para o mesmo banco
  // íntegro. A existência do temporário pode ser limpa em manutenção futura.
}

} // namespace

ProjectDatabaseImportResult ProjectDatabaseImporter::create_from_json(
    const std::filesystem::path &project_path) const {
  const auto source = load_and_validate_source(project_path);
  const auto data_path = source.project.path() / "data";
  const auto target = data_path / "project.sqlite3";
  const auto temporary = data_path / "project.sqlite3.tmp";

  if (std::filesystem::exists(target)) {
    SqliteDatabase database(target);
    SchemaMigrator{}.migrate(database);
    validate_database(database, source);
    return make_result(source, target, false);
  }

  if (std::filesystem::exists(temporary)) {
    {
      SqliteDatabase database(temporary);
      SchemaMigrator{}.migrate(database);
      validate_database(database, source);
    }
    promote_without_overwrite(temporary, target);
    return make_result(source, target, true);
  }

  bool owns_temporary = false;
  try {
    {
      SqliteDatabase database(temporary);
      owns_temporary = true;
      SchemaMigrator{}.migrate(database);
      insert_source(database, source);
    }
    {
      SqliteDatabase reopened(temporary);
      validate_database(reopened, source);
    }
    promote_without_overwrite(temporary, target);
    owns_temporary = false;
    return make_result(source, target, true);
  } catch (...) {
    if (owns_temporary) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
    }
    throw;
  }
}

} // namespace inde::persistence

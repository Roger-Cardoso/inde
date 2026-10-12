#include "inde/application/catalog_service.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/project_session.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/spelling_provider.hpp"
#include "inde/application/structural_service.hpp"
#include "inde/application/temporal_view.hpp"
#include "inde/application/timeline_projection.hpp"
#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_importer.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_catalog_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_structural_repository.hpp"
#include "inde/persistence/structural_repository.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

#include "core_test_cases.hpp"
#include "test_support.hpp"

namespace inde::tests {

void sqlite_imports_json_project_safely() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository catalogs;
  inde::persistence::StructuralRepository structures;
  const auto project =
      projects.create(temporary.path / "Importacao", "Projeto de importação");
  const auto project_path = project.path();
  const auto now = inde::project::utc_now();

  inde::project::IntellectualProperty ip{
      inde::project::new_uuid(),
      "Universo Áureo 🚀",
      "Crônicas",
      "Descrição com acentos e\nquebra de linha",
      "capas/capa.png",
      now,
      now};
  catalogs.save(project_path, ip);
  inde::project::Work work{inde::project::new_uuid(),
                           ip.id,
                           "Obra importada",
                           "Subtítulo",
                           "Sinopse preservada",
                           "pt-BR",
                           "Planejamento",
                           now,
                           now};
  catalogs.save(project_path, work);

  inde::project::StructuralNode parent{
      inde::project::new_uuid(),
      work.id,
      std::nullopt,
      inde::project::StructuralNodeType::Chapter,
      "Capítulo",
      "Subtítulo",
      "Sinopse do capítulo",
      "",
      "Planejamento",
      2000,
      now,
      now};
  inde::project::StructuralNode child{inde::project::new_uuid(),
                                      work.id,
                                      parent.id,
                                      inde::project::StructuralNodeType::Scene,
                                      "Cena",
                                      "",
                                      "Sinopse da cena",
                                      "",
                                      "Planejamento",
                                      1000,
                                      now,
                                      now};
  // O filho é salvo com posição menor para ser importado antes do pai. A FK
  // diferível precisa permitir essa ordem e validar tudo no commit.
  structures.save(project_path, parent);
  structures.save(project_path, child);

  const auto manifest_before = read_file(project_path / "manifest.json");
  const auto ip_path =
      project_path / "data" / "intellectual-properties" / (ip.id + ".json");
  const auto work_path = project_path / "data" / "works" / (work.id + ".json");
  const auto parent_path =
      project_path / "data" / "structural-nodes" / (parent.id + ".json");
  const auto child_path =
      project_path / "data" / "structural-nodes" / (child.id + ".json");
  const auto ip_before = read_file(ip_path);
  const auto work_before = read_file(work_path);
  const auto parent_before = read_file(parent_path);
  const auto child_before = read_file(child_path);

  inde::persistence::ProjectDatabaseImporter importer;
  const auto imported = importer.create_from_json(project_path);
  assert(imported.created);
  assert(imported.database_path == project_path / "data" / "project.sqlite3");
  assert(imported.intellectual_property_count == 1);
  assert(imported.work_count == 1);
  assert(imported.editorial_node_count == 2);
  assert(std::filesystem::is_regular_file(imported.database_path));
  assert(
      !std::filesystem::exists(project_path / "data" / "project.sqlite3.tmp"));

  // A fonte precisa permanecer byte a byte idêntica.
  assert(read_file(project_path / "manifest.json") == manifest_before);
  assert(read_file(ip_path) == ip_before);
  assert(read_file(work_path) == work_before);
  assert(read_file(parent_path) == parent_before);
  assert(read_file(child_path) == child_before);

  {
    inde::persistence::SqliteDatabase database(imported.database_path);
    assert(database.query_text("SELECT title FROM intellectual_properties") ==
           ip.title);
    assert(database.query_text("SELECT synopsis FROM works") == work.synopsis);
    assert(database.query_integer("SELECT count(*) FROM editorial_nodes") == 2);
    auto loaded_child = database.prepare(
        "SELECT parent_id, position FROM editorial_nodes WHERE id = ?");
    loaded_child.bind(1, child.id);
    assert(loaded_child.step());
    assert(loaded_child.column_text(0) == parent.id);
    assert(loaded_child.column_integer(1) == child.position);
  }

  const auto repeated = importer.create_from_json(project_path);
  assert(!repeated.created);
  assert(repeated.database_path == imported.database_path);

  // Simula uma interrupção depois da validação e antes da promoção.
  const auto pending = project_path / "data" / "project.sqlite3.tmp";
  std::filesystem::rename(imported.database_path, pending);
  const auto resumed = importer.create_from_json(project_path);
  assert(resumed.created);
  assert(std::filesystem::is_regular_file(resumed.database_path));
  assert(!std::filesystem::exists(pending));

  // Um banco já promovido mas divergente nunca deve ser sobrescrito.
  ip.title = "Título alterado apenas no JSON";
  catalogs.save(project_path, ip);
  bool stale_database_rejected = false;
  try {
    static_cast<void>(importer.create_from_json(project_path));
  } catch (const std::exception &) {
    stale_database_rejected = true;
  }
  assert(stale_database_rejected);
  inde::persistence::SqliteDatabase unchanged(resumed.database_path);
  assert(unchanged.query_text("SELECT title FROM intellectual_properties") ==
         "Universo Áureo 🚀");
}

void sqlite_import_rejects_invalid_json_without_artifacts() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  const auto project =
      projects.create(temporary.path / "Corrompido", "Projeto corrompido");
  const auto bad_work = project.path() / "data" / "works" / "bad.json";
  std::filesystem::create_directories(bad_work.parent_path());
  std::ofstream(bad_work) << "{ arquivo incompleto";

  bool rejected = false;
  try {
    static_cast<void>(
        inde::persistence::ProjectDatabaseImporter{}.create_from_json(
            project.path()));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(read_file(bad_work) == "{ arquivo incompleto");
  assert(!std::filesystem::exists(project.path() / "data" / "project.sqlite3"));
  assert(!std::filesystem::exists(project.path() / "data" /
                                  "project.sqlite3.tmp"));
}

void sqlite_imports_empty_json_project() {
  TemporaryDirectory temporary;
  const auto project = inde::persistence::ProjectRepository{}.create(
      temporary.path / "Vazio", "Projeto vazio");
  const auto result =
      inde::persistence::ProjectDatabaseImporter{}.create_from_json(
          project.path());
  assert(result.created);
  assert(result.intellectual_property_count == 0);
  assert(result.work_count == 0);
  assert(result.editorial_node_count == 0);
  inde::persistence::SqliteDatabase database(result.database_path);
  assert(database.query_integer("SELECT count(*) FROM projects") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM intellectual_properties") == 0);
  assert(database.query_integer("SELECT count(*) FROM works") == 0);
  assert(database.query_integer("SELECT count(*) FROM editorial_nodes") == 0);
}

void sqlite_import_rejects_orphan_work_without_artifacts() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository catalogs;
  const auto project =
      projects.create(temporary.path / "Orfao", "Projeto com obra órfã");
  const auto now = inde::project::utc_now();
  inde::project::Work orphan{inde::project::new_uuid(),
                             inde::project::new_uuid(),
                             "Obra órfã",
                             "",
                             "",
                             "pt-BR",
                             "Planejamento",
                             now,
                             now};
  catalogs.save(project.path(), orphan);

  bool rejected = false;
  try {
    static_cast<void>(
        inde::persistence::ProjectDatabaseImporter{}.create_from_json(
            project.path()));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(std::filesystem::is_regular_file(project.path() / "data" / "works" /
                                          (orphan.id + ".json")));
  assert(!std::filesystem::exists(project.path() / "data" / "project.sqlite3"));
  assert(!std::filesystem::exists(project.path() / "data" /
                                  "project.sqlite3.tmp"));
}

} // namespace inde::tests

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

void project_round_trip() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository repository;
  auto created =
      repository.create(temporary.path / "Romance", "Romance de teste");

  assert(created.path().extension() == ".inde");
  assert(std::filesystem::is_directory(created.path() / "assets"));
  assert(std::filesystem::is_directory(created.path() / "data"));
  assert(std::filesystem::is_directory(created.path() / "documents"));
  assert(std::filesystem::is_regular_file(created.path() / "manifest.json"));

  const auto opened = repository.open(created.path());
  assert(opened.manifest().format == "INDE");
  assert(opened.manifest().format_version ==
         inde::project::current_format_version);
  assert(opened.manifest().name == "Romance de teste");
  assert(opened.manifest().project_id == created.manifest().project_id);

  std::ofstream(created.path() / "documents" / "nota.txt")
      << "conteúdo preservado";
  const auto copy =
      repository.save_as(created, temporary.path / "Romance - cópia");
  assert(copy.path().extension() == ".inde");
  assert(copy.manifest().project_id != created.manifest().project_id);
  assert(copy.manifest().name == created.manifest().name);
  assert(
      std::filesystem::is_regular_file(copy.path() / "documents" / "nota.txt"));
  assert(repository.open(copy.path()).manifest().project_id ==
         copy.manifest().project_id);
}

void rejects_future_format() {
  TemporaryDirectory temporary;
  const auto project_path = temporary.path / "Futuro.inde";
  std::filesystem::create_directories(project_path);
  std::ofstream manifest(project_path / "manifest.json");
  manifest
      << R"({"format":"INDE","format_version":999,"project_id":"550e8400-e29b-41d4-a716-446655440000","name":"Futuro","created_at":"2026-01-01T00:00:00Z","updated_at":"2026-01-01T00:00:00Z"})";
  manifest.close();

  inde::persistence::ProjectRepository repository;
  bool rejected = false;
  try {
    static_cast<void>(repository.open(project_path));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
}

void strict_json_codec() {
  using inde::persistence::json::parse;
  using inde::persistence::json::serialize;

  const auto decoded = parse(
      R"({"texto":"Ação\n\u00e7\ud83d\ude80","ativo":true,"vazio":null,"itens":[1,2]})");
  assert(decoded.at("texto").as_string() == "Ação\nç🚀");
  assert(decoded.at("ativo").as_boolean());
  assert(decoded.at("vazio").is_null());
  assert(decoded.at("itens").as_array().size() == 2);
  const auto round_trip = parse(serialize(decoded));
  assert(round_trip.at("texto").as_string() == decoded.at("texto").as_string());

  bool duplicate_rejected = false;
  try {
    static_cast<void>(parse("{\n  \"id\": 1,\n  \"id\": 2\n}"));
  } catch (const inde::persistence::json::Error &error) {
    duplicate_rejected = error.line() == 3 && error.column() > 1;
  }
  assert(duplicate_rejected);

  bool trailing_rejected = false;
  try {
    static_cast<void>(parse("{} conteúdo indevido"));
  } catch (const inde::persistence::json::Error &) {
    trailing_rejected = true;
  }
  assert(trailing_rejected);

  bool wrong_type_rejected = false;
  try {
    static_cast<void>(parse(R"({"version":"1"})").at("version").as_integer());
  } catch (const std::exception &) {
    wrong_type_rejected = true;
  }
  assert(wrong_type_rejected);
}

void recents_are_deduplicated() {
  TemporaryDirectory temporary;
  const auto first = temporary.path / "A.inde";
  const auto second = temporary.path / "B.inde";
  std::filesystem::create_directories(first);
  std::filesystem::create_directories(second);
  inde::persistence::RecentProjectsStore store(temporary.path / "config" /
                                               "recents");
  store.touch(first);
  store.touch(second);
  store.touch(first);
  const auto recents = store.load();
  assert(recents.size() == 2);
  assert(recents[0] == first);
  assert(recents[1] == second);
}

void recents_failure_does_not_block_project_lifecycle() {
  TemporaryDirectory temporary;
  const auto blocked_config = temporary.path / "config-is-a-file";
  std::ofstream(blocked_config) << "not a directory";
  setenv("XDG_CONFIG_HOME", blocked_config.c_str(), 1);

  inde::application::ProjectService service;
  const auto project_path = temporary.path / "Projeto.inde";
  const auto project_id =
      service.create(project_path, "Projeto").manifest().project_id;
  assert(service.current());
  assert(service.current()->manifest().project_id == project_id);
  service.save();
  service.close();
  assert(!service.current());
  service.open(project_path);
  assert(service.current());
  assert(service.current()->manifest().project_id == project_id);
}

void failed_open_preserves_active_session() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto active_path =
      service.create(temporary.path / "Ativo", "Projeto ativo").path();
  const auto active_id = service.current()->manifest().project_id;

  const auto invalid_path = temporary.path / "Invalido.inde";
  std::filesystem::create_directories(invalid_path / "data" / "works");
  std::ofstream(invalid_path / "manifest.json")
      << R"({"format":"INDE","format_version":1,"project_id":"550e8400-e29b-41d4-a716-446655440000","name":"Inválido","created_at":"2026-01-01T00:00:00Z","updated_at":"2026-01-01T00:00:00Z"})";
  std::ofstream(invalid_path / "data" / "works" / "corrompido.json")
      << "{ arquivo incompleto";

  bool rejected = false;
  try {
    static_cast<void>(service.open(invalid_path));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(service.current());
  assert(service.current()->path() == active_path);
  assert(service.current()->manifest().project_id == active_id);
}

void specialized_services_share_a_guarded_session() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository catalog_repository;
  inde::persistence::StructuralRepository structural_repository;
  inde::application::ProjectSession session;
  inde::application::CatalogService catalog(session, catalog_repository);
  inde::application::StructuralService structure(session,
                                                 structural_repository);

  bool catalog_guarded = false;
  try {
    static_cast<void>(
        catalog.create_intellectual_property("Sem sessão", "", ""));
  } catch (const std::exception &) {
    catalog_guarded = true;
  }
  assert(catalog_guarded);

  bool structure_guarded = false;
  try {
    static_cast<void>(structure.nodes_for_work(inde::project::new_uuid()));
  } catch (const std::exception &) {
    structure_guarded = true;
  }
  assert(structure_guarded);

  auto project =
      projects.create(temporary.path / "Servicos", "Serviços especializados");
  const auto project_path = project.path();
  session.begin(std::move(project), {}, {});
  const auto ip_id =
      catalog.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      catalog.create_work(ip_id, "Obra", "", "", "pt-BR", "Planejamento").id;
  const auto chapter_id =
      structure
          .create_node(work_id, std::nullopt,
                       inde::project::StructuralNodeType::Chapter, "Capítulo 1")
          .id;
  structure.create_node(work_id, chapter_id,
                        inde::project::StructuralNodeType::Scene, "Cena 1");

  assert(session.catalog().intellectual_properties.size() == 1);
  assert(session.catalog().works.size() == 1);
  assert(structure.nodes_for_work(work_id).size() == 2);
  assert(catalog_repository.load(project_path).works.size() == 1);
  assert(structural_repository.load(project_path).size() == 2);

  session.clear();
  assert(!session.current());
  assert(session.catalog().works.empty());
  assert(session.structural_nodes().empty());
}

void project_service_uses_sqlite_as_the_only_operational_store() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto source_path =
      service.create(temporary.path / "Operacional", "Projeto operacional")
          .path();
  const auto source_id = service.current()->manifest().project_id;
  assert(service.current()->manifest().format_version ==
         inde::project::current_format_version);
  assert(std::filesystem::is_regular_file(source_path / "data" /
                                          "project.sqlite3"));

  const auto ip_id =
      service.create_intellectual_property("Universo SQLite", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Obra SQLite", "", "", "pt-BR", "Planejamento")
          .id;
  service.create_structural_template(work_id, "parts-and-chapters");
  assert(service.structural_nodes_for_work(work_id).size() == 18);

  // Após o corte, o catálogo e a estrutura não mantêm uma segunda fonte viva.
  assert(!std::filesystem::exists(source_path / "data" /
                                  "intellectual-properties"));
  assert(!std::filesystem::exists(source_path / "data" / "works"));
  assert(!std::filesystem::exists(source_path / "data" / "structural-nodes"));

  inde::application::ProjectService reopened;
  reopened.open(source_path);
  assert(reopened.catalog().intellectual_properties.size() == 1);
  assert(reopened.catalog().works.size() == 1);
  assert(reopened.structural_nodes_for_work(work_id).size() == 18);

  const auto copy = reopened.save_as(temporary.path / "Copia operacional");
  assert(copy.manifest().project_id != source_id);
  inde::persistence::SqliteDatabase copied_database(copy.path() / "data" /
                                                    "project.sqlite3");
  assert(copied_database.query_text("SELECT id FROM projects") ==
         copy.manifest().project_id);
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM intellectual_properties") ==
         copy.manifest().project_id);
  inde::persistence::SqliteDatabase source_database(source_path / "data" /
                                                    "project.sqlite3");
  assert(source_database.query_text("SELECT id FROM projects") == source_id);
}

void legacy_project_is_upgraded_once_and_v2_requires_its_database() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository legacy_catalog;
  auto legacy = projects.create(temporary.path / "Legado", "Projeto legado");
  legacy.manifest().format_version = 1;
  projects.save(legacy);
  const auto now = inde::project::utc_now();
  inde::project::IntellectualProperty ip{
      inde::project::new_uuid(), "IP legada", "", "", "", now, now};
  legacy_catalog.save(legacy.path(), ip);

  inde::application::ProjectService service;
  service.open(legacy.path());
  assert(service.current()->manifest().format_version ==
         inde::project::current_format_version);
  assert(service.catalog().intellectual_properties.size() == 1);
  assert(inde::project::Manifest::from_json(
             read_file(legacy.path() / "manifest.json"))
             .format_version == inde::project::current_format_version);

  const auto database_path = legacy.path() / "data" / "project.sqlite3";
  assert(std::filesystem::remove(database_path));
  bool missing_database_rejected = false;
  try {
    inde::application::ProjectService{}.open(legacy.path());
  } catch (const std::exception &) {
    missing_database_rejected = true;
  }
  assert(missing_database_rejected);
  // O JSON legado permanece como material de compatibilidade, mas não é
  // reutilizado silenciosamente depois que o manifesto marca o corte.
  assert(std::filesystem::is_regular_file(
      legacy.path() / "data" / "intellectual-properties" / (ip.id + ".json")));
  assert(!std::filesystem::exists(database_path));
}

} // namespace inde::tests

#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"

#include <algorithm>
#include <chrono>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>

namespace {

using Clock = std::chrono::steady_clock;

template <class Operation> double milliseconds(Operation operation) {
  const auto start = Clock::now();
  operation();
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}

struct TemporaryDirectory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("inde-narrative-benchmark-" + inde::project::new_uuid());
  TemporaryDirectory() { std::filesystem::create_directories(path); }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};

} // namespace

int main() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService creator;
  std::filesystem::path project_path;
  const auto create_ms = milliseconds([&] {
    project_path =
        creator.create(temporary.path / "Benchmark", "Benchmark narrativo")
            .path();
  });
  std::string character_id;
  for (const auto &type : creator.narrative().entity_types())
    if (type.key == "character")
      character_id = type.id;
  if (character_id.empty())
    throw std::runtime_error("Tipo interno character ausente");
  const auto ip = creator.create_intellectual_property(
      "Universo do benchmark", "", "Contexto de medição");
  const auto work = creator.create_work(ip.id, "Obra do benchmark", "", "",
                                        "pt-BR", "Planejamento");
  const auto related = creator.narrative().create_relation_type(
      "benchmark-link", "Ligação de benchmark", "Ligado por benchmark",
      inde::project::RelationDirectionality::Directed);
  creator.close();

  inde::persistence::SqliteDatabase database(project_path / "data" /
                                               "project.sqlite3");
  const auto owner = database.query_text("SELECT id FROM projects");
  const auto now = inde::project::utc_now();
  const auto fixture_ms = milliseconds([&] {
    inde::persistence::SqliteTransaction transaction(database);
    auto insert = database.prepare(
        "INSERT INTO entities "
        "(id, project_id, entity_type_id, name, summary, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    auto insert_scope = database.prepare(
        "INSERT INTO entity_work_scopes "
        "(id, project_id, entity_id, work_id, notes, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, '', ?, ?)");
    auto insert_relation = database.prepare(
        "INSERT INTO relations "
        "(id, project_id, relation_type_id, source_entity_id, "
        "target_entity_id, description, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, '', ?, ?)");
    for (int index = 0; index < 10000; ++index) {
      char id[37]{};
      std::snprintf(id, sizeof(id), "80000000-0000-4000-8000-%012x", index);
      insert.bind(1, id);
      insert.bind(2, owner);
      insert.bind(3, character_id);
      insert.bind(4, "Personagem " + std::to_string(index));
      insert.bind(5, "Resumo narrativo " + std::to_string(index));
      insert.bind(6, now);
      insert.bind(7, now);
      insert.run();
      insert.reset();
      if (index % 2 == 0) {
        char scope_id[37]{};
        std::snprintf(scope_id, sizeof(scope_id),
                      "82000000-0000-4000-8000-%012x", index);
        insert_scope.bind(1, scope_id);
        insert_scope.bind(2, owner);
        insert_scope.bind(3, id);
        insert_scope.bind(4, work.id);
        insert_scope.bind(5, now);
        insert_scope.bind(6, now);
        insert_scope.run();
        insert_scope.reset();
      }
      if (index > 0) {
        char relation_id[37]{};
        std::snprintf(relation_id, sizeof(relation_id),
                      "81000000-0000-4000-8000-%012x", index);
        insert_relation.bind(1, relation_id);
        insert_relation.bind(2, owner);
        insert_relation.bind(3, related.id);
        insert_relation.bind(4, "80000000-0000-4000-8000-000000000000");
        insert_relation.bind(5, id);
        insert_relation.bind(6, now);
        insert_relation.bind(7, now);
        insert_relation.run();
        insert_relation.reset();
      }
    }
    transaction.commit();
  });

  inde::application::ProjectService service;
  const auto open_ms =
      milliseconds([&] { static_cast<void>(service.open(project_path)); });
  std::size_t page_count{};
  const auto page_ms = milliseconds(
      [&] { page_count = service.narrative().entities().size(); });
  inde::persistence::EntityQuery search;
  search.search = "9999";
  std::size_t search_count{};
  const auto search_ms = milliseconds(
      [&] { search_count = service.narrative().entities(search).size(); });
  // Simula a fonte do IncrementalSelector: a cada alteração do texto ela pede
  // só uma página pequena, sem preencher o universo na UI.
  constexpr std::array<const char *, 6> incremental_terms{
      "P", "Pe", "Per", "Pers", "Personagem", "9999"};
  constexpr int incremental_rounds = 20;
  double incremental_total_ms{};
  double incremental_max_ms{};
  std::size_t incremental_results{};
  for (int round = 0; round < incremental_rounds; ++round) {
    for (const auto *term : incremental_terms) {
      inde::persistence::EntityQuery incremental;
      incremental.search = term;
      incremental.limit = 50;
      std::size_t result_count{};
      const auto elapsed = milliseconds([&] {
        result_count = service.narrative().entities(incremental).size();
      });
      incremental_total_ms += elapsed;
      incremental_max_ms = std::max(incremental_max_ms, elapsed);
      incremental_results += result_count;
    }
  }
  const auto incremental_samples =
      static_cast<double>(incremental_rounds * incremental_terms.size());
  inde::application::PlanningContext context;
  context.work_id = work.id;
  context.entity_type_ids = {character_id};
  context.relation_type_ids = {related.id};
  context.related_entity_id =
      "80000000-0000-4000-8000-000000000000";
  std::size_t contextual_count{};
  const auto contextual_ms = milliseconds([&] {
    contextual_count =
        service.planning().explore_entities(context).matching_count;
  });
  const auto write_ms = milliseconds([&] {
    static_cast<void>(service.narrative().create_entity(
        character_id, "Personagem criada pelo benchmark", ""));
  });

  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  std::cout << "entities=10000 page_count=" << page_count
            << " search_count=" << search_count
            << " contextual_count=" << contextual_count << '\n'
            << "create_project_ms=" << create_ms << '\n'
            << "fixture_transaction_ms=" << fixture_ms << '\n'
            << "open_project_ms=" << open_ms << '\n'
            << "indexed_page_ms=" << page_ms << '\n'
            << "text_search_ms=" << search_ms << '\n'
            << "incremental_search_mean_ms="
            << incremental_total_ms / incremental_samples << '\n'
            << "incremental_search_max_ms=" << incremental_max_ms << '\n'
            << "incremental_search_results=" << incremental_results << '\n'
            << "contextual_page_and_count_ms=" << contextual_ms << '\n'
            << "transactional_write_ms=" << write_ms << '\n'
            << "max_rss_kib=" << usage.ru_maxrss << '\n';
}

#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"

#include <algorithm>
#include <chrono>
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
      ("inde-writing-benchmark-" + inde::project::new_uuid());
  TemporaryDirectory() { std::filesystem::create_directories(path); }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
};

} // namespace

int main() {
  constexpr int document_count = 200;
  constexpr int analysis_iterations = 20;
  constexpr int query_iterations = 40;
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);

  std::string long_text;
  const std::string paragraph =
      "A escritora observa a cidade, compara as pistas e reescreve a cena "
      "sem perder a voz das personagens. Cada paragrafo preserva contexto, "
      "ritmo e intencao para uma revisao posterior.\n";
  while (long_text.size() < 1024 * 1024)
    long_text += paragraph;

  inde::project::DocumentTextStatistics statistics;
  const auto analysis_total_ms = milliseconds([&] {
    for (int index = 0; index < analysis_iterations; ++index)
      statistics = inde::project::analyze_document_text(long_text);
  });

  inde::application::ProjectService creator;
  std::filesystem::path project_path;
  const auto create_project_ms = milliseconds([&] {
    project_path =
        creator.create(temporary.path / "Benchmark", "Benchmark de Escrita")
            .path();
  });
  const auto entity_types = creator.narrative().entity_types();
  const auto character_type = std::find_if(
      entity_types.begin(), entity_types.end(),
      [](const auto &type) { return type.key == "character"; });
  if (character_type == entity_types.end())
    throw std::runtime_error("Tipo interno character ausente");
  const auto character = creator.narrative().create_entity(
      character_type->id, "Aurora", "Entidade usada no benchmark textual");
  creator.close();

  std::string fixture_content;
  while (fixture_content.size() < 16 * 1024)
    fixture_content += paragraph;
  const auto fixture_ms = milliseconds([&] {
    inde::persistence::SqliteDatabase database(project_path / "data" /
                                                "project.sqlite3");
    const auto owner = database.query_text("SELECT id FROM projects");
    const auto now = inde::project::utc_now();
    inde::persistence::SqliteTransaction transaction(database);
    auto insert = database.prepare(
        "INSERT INTO documents(id, project_id, editorial_node_id, title, "
        "content, created_at, updated_at, word_goal) "
        "VALUES (?, ?, NULL, ?, ?, ?, ?, NULL)");
    for (int index = 0; index < document_count; ++index) {
      insert.bind(1, inde::project::new_uuid());
      insert.bind(2, owner);
      insert.bind(3, index == 0 ? "agulha documental"
                                : "Documento " + std::to_string(index));
      insert.bind(4, fixture_content +
                         (index == document_count - 1
                              ? " agulha documental"
                              : std::string{}));
      insert.bind(5, now);
      insert.bind(6, now);
      insert.run();
      insert.reset();
    }
    transaction.commit();
  });

  inde::application::ProjectService service;
  const auto open_project_ms =
      milliseconds([&] { static_cast<void>(service.open(project_path)); });

  inde::persistence::DocumentQuery search;
  search.search = "agulha documental";
  std::vector<inde::project::DocumentSummary> search_results;
  const auto search_total_ms = milliseconds([&] {
    for (int index = 0; index < query_iterations; ++index)
      search_results = service.writing().document_summaries(search);
  });
  if (search_results.size() != 2 ||
      search_results.front().search_match !=
          inde::project::DocumentSearchMatch::Name)
    throw std::runtime_error("A busca textual perdeu ordenação ou precisão");

  auto layered = service.writing().create_document("Documento em camadas");
  layered.content = long_text;
  layered.word_goal = 120000;
  for (std::size_t index = 0; index < 100; ++index) {
    layered.formatting.push_back(
        {index % 2 == 0 ? inde::project::DocumentTextStyle::Bold
                        : inde::project::DocumentTextStyle::Italic,
         index * 20, index * 20 + 10});
  }
  for (std::size_t index = 0; index < 50; ++index) {
    const auto anchor_id = inde::project::new_uuid();
    layered.anchors.push_back({anchor_id, "Ancora " + std::to_string(index),
                               index * 1000, index * 1000 + 100});
    layered.entity_references.push_back(
        {inde::project::new_uuid(), character.id, anchor_id,
         "Referencia explicita de benchmark"});
  }
  const auto layered_write_ms = milliseconds(
      [&] { layered = service.writing().update_document(layered); });

  std::optional<inde::project::Document> loaded;
  const auto layered_read_total_ms = milliseconds([&] {
    for (int index = 0; index < query_iterations; ++index)
      loaded = service.writing().document(layered.id);
  });
  if (!loaded || loaded->formatting.size() != 100 ||
      loaded->anchors.size() != 50 ||
      loaded->entity_references.size() != 50)
    throw std::runtime_error("As camadas do Documento não foram reconstruídas");

  const auto copy_path = temporary.path / "Benchmark - copia";
  std::filesystem::path copied_project_path;
  const auto save_as_ms = milliseconds([&] {
    copied_project_path = service.save_as(copy_path).path();
  });
  inde::persistence::SqliteDatabase copy_database(copied_project_path / "data" /
                                                   "project.sqlite3");
  if (copy_database.query_text("PRAGMA integrity_check") != "ok")
    throw std::runtime_error("A cópia do benchmark ficou inconsistente");

  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  std::cout << "documents=" << document_count + 1
            << " text_bytes=" << long_text.size()
            << " words=" << statistics.words
            << " search_results=" << search_results.size() << '\n'
            << "create_project_ms=" << create_project_ms << '\n'
            << "fixture_transaction_ms=" << fixture_ms << '\n'
            << "open_project_ms=" << open_project_ms << '\n'
            << "analysis_iterations=" << analysis_iterations << '\n'
            << "analysis_mean_ms="
            << analysis_total_ms / analysis_iterations << '\n'
            << "search_iterations=" << query_iterations << '\n'
            << "search_mean_ms=" << search_total_ms / query_iterations << '\n'
            << "layered_write_ms=" << layered_write_ms << '\n'
            << "layered_read_mean_ms="
            << layered_read_total_ms / query_iterations << '\n'
            << "save_as_ms=" << save_as_ms << '\n'
            << "max_rss_kib=" << usage.ru_maxrss << '\n';
}

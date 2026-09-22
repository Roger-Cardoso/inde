#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_linguistic_catalog.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

struct TemporaryDirectory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("inde-linguistic-tests-" +
       std::to_string(std::hash<std::string>{}(
           std::filesystem::current_path().string() +
           std::to_string(
               std::chrono::steady_clock::now().time_since_epoch().count()))));
  TemporaryDirectory() { std::filesystem::create_directories(path); }
  ~TemporaryDirectory() { std::filesystem::remove_all(path); }
};

std::string bytes(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void create_lexical_catalog(const std::filesystem::path &path,
                            std::string contract = "0.5.0") {
  inde::persistence::SqliteDatabase database(path);
  database.execute(R"SQL(
CREATE TABLE query_catalog_metadata (
  singleton INTEGER PRIMARY KEY,
  contract_version TEXT NOT NULL,
  service_version TEXT NOT NULL,
  status TEXT NOT NULL,
  read_only_runtime INTEGER NOT NULL,
  network_required INTEGER NOT NULL
);
CREATE TABLE lexical_query_entry_data (
  form_id TEXT NOT NULL,
  lexeme_id TEXT NOT NULL,
  sense_id TEXT NOT NULL,
  written TEXT NOT NULL,
  normalized TEXT NOT NULL,
  ptbr_pos TEXT NOT NULL,
  upos TEXT NOT NULL,
  source_pos TEXT NOT NULL,
  definition_state TEXT NOT NULL,
  definition_text TEXT,
  sense_review_status TEXT NOT NULL
);
CREATE VIEW lexical_query_entry AS SELECT * FROM lexical_query_entry_data;
INSERT INTO lexical_query_entry_data VALUES
  ('FORM-1', 'LEX-1', 'SENSE-1', 'banco', 'banco', 'substantivo', 'NOUN', 'n',
   'known', 'um assento longo para mais de uma pessoa', 'accepted'),
  ('FORM-1', 'LEX-1', 'SENSE-2', 'banco', 'banco', 'substantivo', 'NOUN', 'n',
   'not_provided', NULL, 'accepted');
)SQL");
  auto insert = database.prepare(
      "INSERT INTO query_catalog_metadata VALUES (1, ?, 'l06-query-1', "
      "'ready', 1, 0)");
  insert.bind(1, contract);
  insert.run();
}

void create_construction_catalog(const std::filesystem::path &path,
                                 int automatic_bindings = 0) {
  inde::persistence::SqliteDatabase database(path);
  database.execute(R"SQL(
CREATE TABLE promotion_build (
  contract_version TEXT NOT NULL,
  pipeline_version TEXT NOT NULL,
  status TEXT NOT NULL,
  human_review_status TEXT NOT NULL,
  origin_kind TEXT NOT NULL,
  epistemic_status_preserved TEXT NOT NULL,
  distribution_scope TEXT NOT NULL,
  automatic_bindings INTEGER NOT NULL
);
CREATE TABLE construction (
  id TEXT, name TEXT, construction_type TEXT, description TEXT,
  epistemic_status TEXT, review_status TEXT
);
CREATE TABLE frame (
  id TEXT, name TEXT, frame_type TEXT, definition TEXT,
  epistemic_status TEXT, review_status TEXT
);
CREATE TABLE syntactic_frame (
  id TEXT, name TEXT, frame_type TEXT, epistemic_status TEXT, review_status TEXT
);
CREATE TABLE mwe (
  id TEXT, canonical_form TEXT, mwe_type TEXT, contiguity TEXT,
  epistemic_status TEXT, review_status TEXT
);
INSERT INTO construction VALUES
  ('CONS-1', 'clivada com ser e que', 'cleft', 'Destaca um constituinte.',
   'derived', 'accepted');
INSERT INTO frame VALUES
  ('FRAME-1', 'focalização', 'information_structure',
   'Um conteúdo é destacado.', 'derived', 'accepted');
INSERT INTO syntactic_frame VALUES
  ('SFRAME-1', 'quadro sintático da clivada', 'constructional',
   'derived', 'accepted');
INSERT INTO mwe VALUES
  ('MWE-1', 'de repente', 'adverbial', 'contiguous', 'derived', 'accepted');
)SQL");
  auto insert = database.prepare(
      "INSERT INTO promotion_build VALUES ('0.6.0', 'l08-promotion-1', "
      "'promoted', 'accepted', 'synthetic_fixture', 'derived', "
      "'internal_productive', ?)");
  insert.bind(1, static_cast<std::int64_t>(automatic_bindings));
  insert.run();
}

void accepted_catalogs_are_queried_without_writes_or_selection() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  const auto lexical_before = bytes(lexical);
  const auto constructions_before = bytes(constructions);

  inde::persistence::SqliteLinguisticCatalog catalog({lexical, constructions});
  const auto status = catalog.status();
  assert(status.lexical_available);
  assert(status.constructions_available);

  const auto query = catalog.query_lexicon(" banco ");
  assert(query.query == "banco");
  assert(query.total == 2);
  assert(query.candidates.size() == 2);
  assert(query.score_semantics == "none");
  assert(!query.semantic_selection_performed);
  assert(!query.automatic_binding_created);
  assert(query.candidates.front().definition.has_value());
  assert(!query.candidates.back().definition.has_value());

  const auto entities = catalog.linguistic_entities();
  assert(entities.size() == 4);
  for (const auto &entity : entities) {
    assert(entity.epistemic_status == "derived");
    assert(entity.review_status == "accepted");
  }
  const auto filtered = catalog.linguistic_entities("ser e que");
  assert(filtered.size() == 1);
  assert(filtered.front().id == "CONS-1");

  const auto injected = catalog.query_lexicon("' OR 1=1 --");
  assert(injected.total == 0);
  assert(injected.candidates.empty());
  assert(bytes(lexical) == lexical_before);
  assert(bytes(constructions) == constructions_before);
}

void unavailable_or_incompatible_catalogs_fail_closed() {
  TemporaryDirectory temporary;
  const auto missing = temporary.path / "missing.sqlite";
  inde::persistence::SqliteLinguisticCatalog absent({missing, missing});
  const auto absent_status = absent.status();
  assert(!absent_status.lexical_available);
  assert(!absent_status.constructions_available);
  bool query_rejected = false;
  try {
    static_cast<void>(absent.query_lexicon("banco"));
  } catch (const std::exception &) {
    query_rejected = true;
  }
  assert(query_rejected);

  const auto lexical = temporary.path / "wrong-l06.sqlite";
  const auto constructions = temporary.path / "wrong-l08.sqlite";
  create_lexical_catalog(lexical, "9.9.9");
  create_construction_catalog(constructions, 1);
  inde::persistence::SqliteLinguisticCatalog incompatible(
      {lexical, constructions});
  const auto incompatible_status = incompatible.status();
  assert(!incompatible_status.lexical_available);
  assert(!incompatible_status.constructions_available);
}

void limits_and_empty_queries_are_rejected() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  inde::persistence::SqliteLinguisticCatalog catalog({lexical, constructions});
  for (const auto limit : {std::size_t{0}, std::size_t{51}}) {
    bool rejected = false;
    try {
      static_cast<void>(catalog.query_lexicon("banco", limit));
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected);
  }
  bool empty_rejected = false;
  try {
    static_cast<void>(catalog.query_lexicon("   "));
  } catch (const std::invalid_argument &) {
    empty_rejected = true;
  }
  assert(empty_rejected);
}

void authorized_catalogs_smoke_when_configured() {
  const auto *lexical = std::getenv("INDE_L06_CATALOG");
  const auto *constructions = std::getenv("INDE_L08_CATALOG");
  if (!lexical || !*lexical || !constructions || !*constructions)
    return;
  inde::persistence::SqliteLinguisticCatalog catalog(
      {{lexical}, {constructions}});
  const auto status = catalog.status();
  assert(status.lexical_available);
  assert(status.constructions_available);
  const auto banco = catalog.query_lexicon("banco");
  assert(banco.total == 7);
  assert(banco.candidates.size() == 7);
  assert(!banco.semantic_selection_performed);
  assert(!banco.automatic_binding_created);
  const auto entities = catalog.linguistic_entities();
  assert(entities.size() == 7);
}

} // namespace

int main() {
  accepted_catalogs_are_queried_without_writes_or_selection();
  unavailable_or_incompatible_catalogs_fail_closed();
  limits_and_empty_queries_are_rejected();
  authorized_catalogs_smoke_when_configured();
  return 0;
}

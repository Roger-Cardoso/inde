#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_linguistic_catalog.hpp"

#include <algorithm>
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
   'not_provided', NULL, 'accepted'),
  ('FORM-2', 'LEX-2', 'SENSE-3', 'Árvore', 'árvore', 'substantivo', 'NOUN', 'n',
   'known', 'planta lenhosa de grande porte', 'accepted');
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

void create_quarantine_catalog(const std::filesystem::path &path,
                               int promoted = 0) {
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
INSERT INTO query_catalog_metadata VALUES
  (1, '0.5.0', 'l06-query-1', 'quarantine_ready', 1, 0);
CREATE TABLE quarantine_build (
  singleton INTEGER PRIMARY KEY,
  contract_version TEXT NOT NULL,
  pipeline_version TEXT NOT NULL,
  source_id TEXT NOT NULL,
  status TEXT NOT NULL,
  human_decision_status TEXT NOT NULL,
  record_review_status TEXT NOT NULL,
  distribution_scope TEXT NOT NULL,
  promoted INTEGER NOT NULL,
  isolated INTEGER NOT NULL,
  network_used INTEGER NOT NULL,
  automatic_bindings INTEGER NOT NULL
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
  sense_review_status TEXT NOT NULL,
  source_record_id TEXT NOT NULL,
  source_synset_id TEXT NOT NULL,
  reference_overlap INTEGER NOT NULL,
  criteria_version TEXT NOT NULL
);
CREATE VIEW lexical_query_entry AS SELECT * FROM lexical_query_entry_data;
INSERT INTO lexical_query_entry_data VALUES
  ('Q-FORM-1', 'Q-LEX-1', 'Q-SENSE-1', 'Ávido', 'ávido', 'adjetivo', 'ADJ',
   'adjectivo', 'known', 'que deseja intensamente algo.', 'needs_review',
   'OntoPT:fixture#form:1', 'OntoPT:fixture', 1,
   'ontopt-selective-quarantine-1'),
  ('Q-FORM-2', 'Q-LEX-2', 'Q-SENSE-2', 'coragem', 'coragem', 'substantivo',
   'NOUN', 'nome', 'known', 'soltura de ventre.', 'needs_review',
   'OntoPT:fixture#form:2', 'OntoPT:fixture-2', 0,
   'ontopt-selective-quarantine-1');
CREATE TABLE editorial_build (
  singleton INTEGER PRIMARY KEY,
  contract_version TEXT NOT NULL,
  pipeline_version TEXT NOT NULL,
  status TEXT NOT NULL,
  policy_decision TEXT NOT NULL,
  content_decision TEXT NOT NULL,
  distribution_scope TEXT NOT NULL,
  original_definitions_preserved INTEGER NOT NULL,
  promoted INTEGER NOT NULL,
  isolated INTEGER NOT NULL,
  network_used INTEGER NOT NULL,
  automatic_bindings INTEGER NOT NULL,
  total_assessed INTEGER NOT NULL,
  source_useful_candidates INTEGER NOT NULL,
  editorial_draft_required INTEGER NOT NULL,
  editorial_drafts INTEGER NOT NULL,
  abstentions INTEGER NOT NULL,
  human_accepted INTEGER NOT NULL
);
INSERT INTO editorial_build VALUES
  (1, '1.1.0', 'inde-definition-editorial-review-1',
   'editorial_review_ready', 'accepted', 'drafts_pending_human',
   'internal_quarantine', 1, 0, 1, 0, 0, 960, 667, 293, 32, 8, 0);
CREATE TABLE definition_assessment (
  source_record_id TEXT PRIMARY KEY,
  flags_json TEXT NOT NULL,
  autonomy INTEGER NOT NULL,
  non_circularity INTEGER NOT NULL,
  conceptual_intro INTEGER NOT NULL,
  clarity INTEGER NOT NULL,
  precision INTEGER NOT NULL,
  context INTEGER NOT NULL,
  score_total INTEGER NOT NULL,
  mandatory_failure INTEGER NOT NULL,
  gate TEXT NOT NULL
);
INSERT INTO definition_assessment VALUES
  ('OntoPT:fixture#form:1', '["too_terse"]', 1, 2, 1, 1, 1, 1, 7, 0,
   'editorial_draft_required'),
  ('OntoPT:fixture#form:2', '["too_terse"]', 0, 2, 0, 1, 0, 1, 4, 1,
   'editorial_draft_required');
CREATE TABLE editorial_definition (
  source_record_id TEXT PRIMARY KEY,
  editorial_definition TEXT,
  status TEXT,
  rationale TEXT
);
INSERT INTO editorial_definition VALUES
  ('OntoPT:fixture#form:1',
   'Que deseja ou procura algo com grande intensidade.',
   'editorial_draft', 'Explica o estado sem depender de vocabulário técnico.'),
  ('OntoPT:fixture#form:2', NULL, 'abstained_semantic_conflict',
   'A forma não corresponde com segurança à acepção da fonte.');
CREATE TABLE editorial_decision_build (
  singleton INTEGER PRIMARY KEY,
  contract_version TEXT, pipeline_version TEXT, status TEXT,
  content_decision TEXT, abstention_guidance TEXT, distribution_scope TEXT,
  original_definitions_preserved INTEGER, promoted INTEGER, isolated INTEGER,
  network_used INTEGER, automatic_bindings INTEGER,
  accepted_editorial_definitions INTEGER, deferred_abstentions INTEGER,
  pending_unreviewed_records INTEGER, fallback_original_records INTEGER,
  pending_relation_validations INTEGER
);
INSERT INTO editorial_decision_build VALUES
  (1, '1.2.0', 'inde-definition-human-review-1',
   'partial_human_review_recorded', '32_accepted_8_deferred',
   'sense_definition_relations_fallback', 'internal_quarantine',
   1, 0, 1, 0, 0, 32, 8, 920, 8, 8);
CREATE TABLE editorial_human_decision (
  source_record_id TEXT PRIMARY KEY, decision TEXT, decision_basis TEXT
);
INSERT INTO editorial_human_decision VALUES
  ('OntoPT:fixture#form:1', 'accepted', 'Aceite humano registrado.'),
  ('OntoPT:fixture#form:2', 'deferred_not_approved',
   'Resolver primeiro a acepção e a definição útil.');
CREATE TABLE abstention_resolution_queue (
  source_record_id TEXT PRIMARY KEY, relation_validation_status TEXT,
  fallback_mode TEXT, motive TEXT
);
INSERT INTO abstention_resolution_queue VALUES
  ('OntoPT:fixture#form:2', 'pending',
   'preserve_original_with_visible_motive',
   'A forma não corresponde com segurança à acepção da fonte.');
CREATE VIEW lexical_editorial_query_entry AS
  SELECT q.*, a.flags_json AS definition_quality_flags_json,
         a.score_total AS definition_score_total,
         a.gate AS definition_quality_gate,
         e.editorial_definition AS editorial_definition_text,
         e.status AS editorial_status,
         e.rationale AS editorial_rationale,
         COALESCE(h.decision, 'pending_human') AS editorial_review_status,
         h.decision_basis AS human_decision_basis,
         r.relation_validation_status,
         r.fallback_mode,
         r.motive AS fallback_motive
  FROM lexical_query_entry AS q
  JOIN definition_assessment AS a USING(source_record_id)
  LEFT JOIN editorial_definition AS e USING(source_record_id)
  LEFT JOIN editorial_human_decision AS h USING(source_record_id)
  LEFT JOIN abstention_resolution_queue AS r USING(source_record_id);
)SQL");
  auto insert = database.prepare(
      "INSERT INTO quarantine_build VALUES (1, '1.0.0', "
      "'ontopt-selective-quarantine-1', 'SRC-CAND-ONTOPT', "
      "'quarantine_ready', 'accepted_for_isolated_testing', "
      "'criteria_passed_needs_review', 'internal_quarantine', ?, 1, 0, 0)");
  insert.bind(1, static_cast<std::int64_t>(promoted));
  insert.run();
}

void add_decomposition_contract(const std::filesystem::path &path,
                                int promoted = 0) {
  inde::persistence::SqliteDatabase database(path);
  database.execute(R"SQL(
CREATE TABLE decomposition_build (
  singleton INTEGER PRIMARY KEY,
  contract_version TEXT NOT NULL,
  pipeline_version TEXT NOT NULL,
  status TEXT NOT NULL,
  isolated INTEGER NOT NULL,
  promoted INTEGER NOT NULL,
  parent_database_sha256 TEXT NOT NULL,
  reference_database_sha256 TEXT NOT NULL
);
CREATE TABLE synset_fragment_candidate_v22 (
  id INTEGER PRIMARY KEY,
  binding_status TEXT NOT NULL
);
INSERT INTO synset_fragment_candidate_v22 VALUES
  (1, 'bound_to_form'),
  (2, 'binding_unresolved');
CREATE TABLE decomposed_sense_candidate_v22 (id INTEGER PRIMARY KEY);
INSERT INTO decomposed_sense_candidate_v22 VALUES (1);
CREATE TABLE orthography_normalization_decision (
  id INTEGER PRIMARY KEY,
  scope TEXT NOT NULL,
  decision TEXT NOT NULL
);
INSERT INTO orthography_normalization_decision VALUES
  (1, 'integral_form', 'review_hyphen_unresolved'),
  (2, 'fragment_form', 'space_separator_normalized');
UPDATE editorial_definition
SET status='policy_approved_decomposition_v22',
    rationale='Fragmento autônomo vinculado ao mesmo synset e à forma.'
WHERE source_record_id='OntoPT:fixture#form:1';
UPDATE editorial_human_decision
SET decision='needs_human_confirmation',
    decision_basis='Política de decomposição aprovada; item ainda não revisto.'
WHERE source_record_id='OntoPT:fixture#form:1';
)SQL");
  auto insert = database.prepare(
      "INSERT INTO decomposition_build VALUES "
      "(1, '2.2.0', 'ontopt-priority-sense-decomposition-1', "
      "'priority_decomposition_complete_quarantine_retained', 1, ?, "
      "'ca02f1bdad0dec9e2777ce59fddfc79d1acee6dd798638fbfe0e0fce1c017854', "
      "'5107eef9fb94a30b094afeaca14cc09a7c84b28f953338068bb13982f047ac04')");
  insert.bind(1, static_cast<std::int64_t>(promoted));
  insert.run();
}

void create_definition_evidence_review(const std::filesystem::path &path,
                                       int promoted = 0) {
  inde::persistence::SqliteDatabase database(path);
  database.execute(R"SQL(
CREATE TABLE review_overlay_build (
  singleton INTEGER PRIMARY KEY, contract_version TEXT,
  pipeline_version TEXT, parent_database_sha256 TEXT, status TEXT,
  isolated INTEGER, promoted INTEGER, external_sources_collected INTEGER,
  definition_generation_executed INTEGER, source_contract_changed INTEGER
);
CREATE TABLE sense_cluster_v23 (cluster_id TEXT PRIMARY KEY);
INSERT INTO sense_cluster_v23 VALUES ('SC23-FIXTURE');
CREATE TABLE definition_artifact_v23 (definition_id TEXT PRIMARY KEY);
INSERT INTO definition_artifact_v23 VALUES ('DEF23-FIXTURE');
CREATE TABLE query_annotation_v23 (
  source_record_id TEXT, sense_id TEXT, sense_cluster_id TEXT,
  freeze_status TEXT, generation_status TEXT,
  authorized_evidence_count INTEGER, authorized_evidence_summary TEXT,
  definition_evidence_map TEXT, qa_gate TEXT, qa_issue_codes TEXT,
  blocking_issue_count INTEGER, review_issue_count INTEGER
);
INSERT INTO query_annotation_v23 VALUES
  ('OntoPT:fixture#form:1', 'Q-SENSE-1', 'SC23-FIXTURE',
   'frozen_source_occurrence_provisional', 'blocked_unsupported_claim', 1,
   'EV23-FIXTURE: que deseja intensamente algo.',
   '[1] Que deseja ou procura algo com grande intensidade. -> SEM EVIDÊNCIA AUTORIZADA',
   'blocked_incomplete_evidence_map', 'LEX-DEF-002', 0, 1);
CREATE TABLE definition_qa_issue_v23 (code TEXT);
INSERT INTO definition_qa_issue_v23 VALUES ('LEX-DEF-002');
)SQL");
  auto insert = database.prepare(
      "INSERT INTO review_overlay_build VALUES "
      "(1,'2.3.0','inde-sense-evidence-isolation-1',"
      "'9e75e67119e108ef209508bd62764b317589a9e8ab6fb08f3b9ec7863ca33c2c',"
      "'evidence_mapping_and_qa_ready_human_review_required',1,?,0,0,0)");
  insert.bind(1, static_cast<std::int64_t>(promoted));
  insert.run();
}

void create_atomic_definition_evidence_review(
    const std::filesystem::path &path, int promoted = 0) {
  inde::persistence::SqliteDatabase database(path);
  database.execute(R"SQL(
CREATE TABLE review_overlay_build (
  singleton INTEGER PRIMARY KEY, contract_version TEXT,
  pipeline_version TEXT, parent_database_sha256 TEXT,
  predecessor_overlay_sha256 TEXT, status TEXT,
  isolated INTEGER, promoted INTEGER, external_sources_collected INTEGER,
  definition_generation_executed INTEGER, source_contract_changed INTEGER,
  built_at TEXT
);
CREATE TABLE sense_cluster_v24 (cluster_id TEXT PRIMARY KEY);
INSERT INTO sense_cluster_v24 VALUES ('SC24-FIXTURE');
CREATE TABLE definition_artifact_v24 (definition_id TEXT PRIMARY KEY);
INSERT INTO definition_artifact_v24 VALUES ('DEF24-FIXTURE');
CREATE TABLE evidence_atom_v24 (atom_id TEXT PRIMARY KEY);
INSERT INTO evidence_atom_v24 VALUES ('EA24-FIXTURE-1');
INSERT INTO evidence_atom_v24 VALUES ('EA24-FIXTURE-2');
INSERT INTO evidence_atom_v24 VALUES ('EA24-FIXTURE-3');
CREATE TABLE surface_form_group_v24 (
  surface_form_group_id TEXT PRIMARY KEY, upos_variant_count INTEGER
);
INSERT INTO surface_form_group_v24 VALUES ('SFG24-FIXTURE', 2);
CREATE TABLE cluster_alignment_family_v24 (family_id TEXT PRIMARY KEY);
CREATE TABLE query_annotation_v24 (
  source_record_id TEXT, sense_id TEXT, sense_cluster_id TEXT,
  lexical_form_key TEXT, surface_form_group_id TEXT,
  upos_variant_count INTEGER, upos_variants TEXT,
  freeze_status TEXT, generation_status TEXT,
  authorized_evidence_count INTEGER, authorized_evidence_summary TEXT,
  definition_evidence_map TEXT, atomic_evidence_summary TEXT,
  candidate_atom_count INTEGER, authorized_atom_count INTEGER,
  alignment_family_id TEXT, qa_gate TEXT, qa_issue_codes TEXT,
  blocking_issue_count INTEGER, review_issue_count INTEGER
);
INSERT INTO query_annotation_v24 VALUES
  ('OntoPT:fixture#form:1', 'Q-SENSE-1', 'SC24-FIXTURE',
   'LFK24-FIXTURE', 'SFG24-FIXTURE', 2, 'ADJ,NOUN',
   'frozen_source_occurrence_provisional',
   'blocked_intra_evidence_sense_bundle', 1,
   'EV24-FIXTURE: três unidades documentais.',
   '[1] Unidade A. -> EV24-FIXTURE',
   '[1] EA24-FIXTURE-1 | definition_candidate | independent_sense_candidate_unreviewed | Unidade A. / [2] EA24-FIXTURE-2 | definition_candidate | independent_sense_candidate_unreviewed | Unidade B. / [3] EA24-FIXTURE-3 | definition_candidate | independent_sense_candidate_unreviewed | Unidade C.',
   3, 0, NULL, 'blocked_atomic_evidence_review', 'LEX-DEF-005', 1, 0);
CREATE TABLE definition_qa_issue_v24 (code TEXT);
INSERT INTO definition_qa_issue_v24 VALUES ('LEX-DEF-005');
)SQL");
  auto insert = database.prepare(
      "INSERT INTO review_overlay_build VALUES "
      "(1,'2.4.0','inde-atomic-evidence-alignment-2',"
      "'9e75e67119e108ef209508bd62764b317589a9e8ab6fb08f3b9ec7863ca33c2c',"
      "'edd40c858c81b1ef309b9dcf9d5da8c68e8f32668617a7612ff5fed0806fc270',"
      "'atomic_evidence_and_family_alignment_ready_human_review_required',"
      "1,?,0,0,0,'2026-09-29T00:00:00-03:00')");
  insert.bind(1, static_cast<std::int64_t>(promoted));
  insert.run();
}

void create_definition_promotion(const std::filesystem::path &path,
                                 int automatic_bindings = 0) {
  inde::persistence::SqliteDatabase database(path);
  database.execute(R"SQL(
CREATE TABLE promotion_build_v25 (
  singleton INTEGER PRIMARY KEY, contract_version TEXT,
  pipeline_version TEXT, authorization_id TEXT,
  overlay_v24_sha256 TEXT, quarantine_v22_sha256 TEXT,
  accepted_l06_sha256 TEXT, status TEXT, isolated INTEGER,
  promoted INTEGER, source_databases_modified INTEGER,
  external_sources_collected INTEGER, automatic_bindings_created INTEGER,
  quality_threshold INTEGER
);
CREATE TABLE candidate_group_v25 (
  candidate_id TEXT PRIMARY KEY, quality_tier TEXT
);
INSERT INTO candidate_group_v25 VALUES
  ('CAND25-FIXTURE', 'promotion_quality_human_reviewed');
CREATE TABLE candidate_source_v25 (candidate_id TEXT);
INSERT INTO candidate_source_v25 VALUES ('CAND25-FIXTURE');
CREATE TABLE collision_group_v25 (
  collision_id TEXT PRIMARY KEY, blocking INTEGER
);
CREATE TABLE collision_member_v25 (
  collision_id TEXT, candidate_id TEXT
);
CREATE TABLE accepted_overlap_v25 (
  overlap_id TEXT PRIMARY KEY, candidate_id TEXT, blocking INTEGER
);
CREATE TABLE promoted_sense_v25 (
  promotion_id TEXT, candidate_id TEXT, form_id TEXT, lexeme_id TEXT,
  sense_id TEXT, written TEXT, normalized TEXT, language TEXT,
  ptbr_pos TEXT, upos TEXT, source_pos TEXT, definition_state TEXT,
  definition_text TEXT, epistemic_status TEXT, review_status TEXT,
  promotion_basis TEXT, provenance_count INTEGER, quality_score INTEGER,
  collision_status TEXT, authorization_id TEXT
);
INSERT INTO promoted_sense_v25 VALUES
  ('PROM25-FIXTURE', 'CAND25-FIXTURE', 'FORM25-FIXTURE', 'LEX25-FIXTURE',
   'SENSE25-FIXTURE', 'abade', 'abade', 'pt', 'substantivo', 'NOUN',
   'Onto.PT-v0.6/NOUN', 'known', 'coadjutor de pároco.',
   'source_asserted_selected', 'human_batch_approved_best_candidate',
   'collision_safe_candidate_selection_v2.5', 1, 90,
   'unique_within_lexical_form',
   'HUMAN-AUTH-DEFINITION-CANDIDATE-PROMOTION-2026-09-29');
CREATE VIEW promoted_lexical_query_entry_v25 AS
SELECT * FROM promoted_sense_v25;
)SQL");
  auto insert = database.prepare(
      "INSERT INTO promotion_build_v25 VALUES "
      "(1,'2.5.0','inde-definition-candidate-promotion-1',"
      "'HUMAN-AUTH-DEFINITION-CANDIDATE-PROMOTION-2026-09-29',"
      "'9204300814a9a0c7c3c51de39fbc71ccd6028053070754df667336bbfd281cc9',"
      "'9e75e67119e108ef209508bd62764b317589a9e8ab6fb08f3b9ec7863ca33c2c',"
      "'5107eef9fb94a30b094afeaca14cc09a7c84b28f953338068bb13982f047ac04',"
      "'promoted_collision_safe_batch_ready',1,1,0,0,?,95)");
  insert.bind(1, static_cast<std::int64_t>(automatic_bindings));
  insert.run();
}

void accepted_catalogs_are_queried_without_writes_or_selection() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  const auto quarantine = temporary.path / "quarantine.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  create_quarantine_catalog(quarantine);
  const auto lexical_before = bytes(lexical);
  const auto constructions_before = bytes(constructions);
  const auto quarantine_before = bytes(quarantine);

  inde::persistence::SqliteLinguisticCatalog catalog(
      {lexical, constructions, quarantine, {}, {}});
  const auto status = catalog.status();
  assert(status.lexical_available);
  assert(status.constructions_available);
  assert(status.quarantine_available);

  const auto query = catalog.query_lexicon(" banco ");
  assert(query.query == "banco");
  assert(query.total == 2);
  assert(query.candidates.size() == 2);
  assert(query.score_semantics == "none");
  assert(!query.semantic_selection_performed);
  assert(!query.automatic_binding_created);
  assert(query.candidates.front().definition.has_value());
  assert(!query.candidates.back().definition.has_value());

  const auto accented = catalog.query_lexicon("ÁRVORE");
  assert(accented.total == 1);
  assert(accented.candidates.front().written == "Árvore");
  const auto decomposed = catalog.query_lexicon("A\u0301RVORE");
  assert(decomposed.total == 1);
  assert(decomposed.candidates.front().sense_id == "SENSE-3");

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
  const auto isolated = catalog.query_quarantine("ÁVIDO");
  assert(isolated.catalog_scope == "quarantine");
  assert(isolated.total == 1);
  assert(isolated.candidates.size() == 1);
  assert(isolated.candidates.front().definition.has_value());
  assert(*isolated.candidates.front().definition ==
         "Que deseja ou procura algo com grande intensidade.");
  assert(isolated.candidates.front().original_definition.has_value());
  assert(*isolated.candidates.front().original_definition ==
         "que deseja intensamente algo.");
  assert(isolated.candidates.front().definition_is_editorial);
  assert(isolated.candidates.front().editorial_status == "editorial_draft");
  assert(isolated.candidates.front().editorial_review_status == "accepted");
  assert(isolated.candidates.front().human_decision_basis ==
         "Aceite humano registrado.");
  assert(isolated.candidates.front().definition_quality_gate ==
         "editorial_draft_required");
  assert(isolated.candidates.front().definition_quality_flags ==
         "[\"too_terse\"]");
  assert(isolated.candidates.front().definition_quality_score == 7);
  assert(isolated.candidates.front().source_record_id ==
         "OntoPT:fixture#form:1");
  assert(isolated.candidates.front().source_synset_id == "OntoPT:fixture");
  assert(isolated.candidates.front().reference_overlap);
  assert(isolated.candidates.front().selection_criteria ==
         "ontopt-selective-quarantine-1");
  assert(isolated.candidates.front().catalog_scope == "quarantine");
  assert(!isolated.semantic_selection_performed);
  assert(!isolated.automatic_binding_created);

  const auto deferred = catalog.query_quarantine("coragem");
  assert(deferred.total == 1);
  assert(deferred.candidates.size() == 1);
  assert(deferred.candidates.front().definition.has_value());
  assert(*deferred.candidates.front().definition == "soltura de ventre.");
  assert(!deferred.candidates.front().definition_is_editorial);
  assert(deferred.candidates.front().editorial_status ==
         "abstained_semantic_conflict");
  assert(deferred.candidates.front().editorial_review_status ==
         "deferred_not_approved");
  assert(deferred.candidates.front().relation_validation_status == "pending");
  assert(deferred.candidates.front().fallback_mode ==
         "preserve_original_with_visible_motive");
  assert(deferred.candidates.front().fallback_motive ==
         "A forma não corresponde com segurança à acepção da fonte.");
  assert(bytes(lexical) == lexical_before);
  assert(bytes(constructions) == constructions_before);
  assert(bytes(quarantine) == quarantine_before);
}

void unavailable_or_incompatible_catalogs_fail_closed() {
  TemporaryDirectory temporary;
  const auto missing = temporary.path / "missing.sqlite";
  inde::persistence::SqliteLinguisticCatalog absent(
      {missing, missing, missing, {}, {}});
  const auto absent_status = absent.status();
  assert(!absent_status.lexical_available);
  assert(!absent_status.constructions_available);
  assert(!absent_status.quarantine_available);
  bool query_rejected = false;
  try {
    static_cast<void>(absent.query_lexicon("banco"));
  } catch (const std::exception &) {
    query_rejected = true;
  }
  assert(query_rejected);

  const auto lexical = temporary.path / "wrong-l06.sqlite";
  const auto constructions = temporary.path / "wrong-l08.sqlite";
  const auto quarantine = temporary.path / "wrong-quarantine.sqlite";
  create_lexical_catalog(lexical, "9.9.9");
  create_construction_catalog(constructions, 1);
  create_quarantine_catalog(quarantine, 1);
  inde::persistence::SqliteLinguisticCatalog incompatible(
      {lexical, constructions, quarantine, {}, {}});
  const auto incompatible_status = incompatible.status();
  assert(!incompatible_status.lexical_available);
  assert(!incompatible_status.constructions_available);
  assert(!incompatible_status.quarantine_available);

  const auto wrong_decomposition =
      temporary.path / "wrong-decomposition.sqlite";
  create_quarantine_catalog(wrong_decomposition);
  add_decomposition_contract(wrong_decomposition, 1);
  inde::persistence::SqliteLinguisticCatalog incompatible_decomposition(
      {lexical, constructions, wrong_decomposition, {}, {}});
  assert(!incompatible_decomposition.status().quarantine_available);
}

void decomposition_v22_is_identified_and_remains_isolated() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  const auto quarantine = temporary.path / "quarantine-v22.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  create_quarantine_catalog(quarantine);
  add_decomposition_contract(quarantine);
  const auto quarantine_before = bytes(quarantine);

  inde::persistence::SqliteLinguisticCatalog catalog(
      {lexical, constructions, quarantine, {}, {}});
  const auto status = catalog.status();
  assert(status.quarantine_available);
  assert(status.quarantine_contract_version == "2.2.0");
  assert(status.quarantine_fragment_candidates == 2);
  assert(status.quarantine_bound_senses == 1);
  assert(status.quarantine_unbound_fragments == 1);
  assert(status.quarantine_pending_hyphen_records == 1);
  assert(status.quarantine_message.find("Onto.PT v2.2") !=
         std::string::npos);

  const auto query = catalog.query_quarantine("ávido");
  assert(query.total == 1);
  assert(query.candidates.front().definition_is_editorial);
  assert(query.candidates.front().editorial_status ==
         "policy_approved_decomposition_v22");
  assert(query.candidates.front().editorial_review_status ==
         "needs_human_confirmation");
  assert(bytes(quarantine) == quarantine_before);
}

void definition_evidence_v23_is_read_only_and_enriches_results() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  const auto quarantine = temporary.path / "quarantine-v22.sqlite";
  const auto review = temporary.path / "definition-review-v23.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  create_quarantine_catalog(quarantine);
  add_decomposition_contract(quarantine);
  create_definition_evidence_review(review);
  const auto quarantine_before = bytes(quarantine);
  const auto review_before = bytes(review);

  inde::persistence::SqliteLinguisticCatalog catalog(
      {lexical, constructions, quarantine, review, {}});
  const auto status = catalog.status();
  assert(status.quarantine_available);
  assert(status.definition_evidence_review_available);
  assert(status.definition_evidence_review_clusters == 1);
  assert(status.definition_evidence_review_incomplete_maps == 1);
  assert(status.definition_evidence_review_message.find("Overlay v2.3") !=
         std::string::npos);

  const auto result = catalog.query_quarantine("ávido");
  assert(result.total == 1);
  const auto &candidate = result.candidates.front();
  assert(candidate.evidence_review_available);
  assert(candidate.sense_cluster_id == "SC23-FIXTURE");
  assert(candidate.cluster_freeze_status ==
         "frozen_source_occurrence_provisional");
  assert(candidate.generation_status == "blocked_unsupported_claim");
  assert(candidate.authorized_evidence_count == 1);
  assert(candidate.authorized_evidence_summary.find("EV23-FIXTURE") !=
         std::string::npos);
  assert(candidate.definition_evidence_map.find("SEM EVIDÊNCIA") !=
         std::string::npos);
  assert(candidate.definition_qa_gate == "blocked_incomplete_evidence_map");
  assert(candidate.definition_qa_issue_codes == "LEX-DEF-002");
  assert(candidate.definition_qa_blocking_issues == 0);
  assert(candidate.definition_qa_review_issues == 1);
  assert(bytes(quarantine) == quarantine_before);
  assert(bytes(review) == review_before);

  const auto invalid_review = temporary.path / "invalid-review-v23.sqlite";
  create_definition_evidence_review(invalid_review, 1);
  inde::persistence::SqliteLinguisticCatalog invalid(
      {lexical, constructions, quarantine, invalid_review, {}});
  assert(!invalid.status().definition_evidence_review_available);
}

void definition_evidence_v24_exposes_atoms_classes_and_gates() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  const auto quarantine = temporary.path / "quarantine-v22.sqlite";
  const auto review = temporary.path / "definition-review-v24.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  create_quarantine_catalog(quarantine);
  add_decomposition_contract(quarantine);
  create_atomic_definition_evidence_review(review);
  const auto review_before = bytes(review);

  inde::persistence::SqliteLinguisticCatalog catalog(
      {lexical, constructions, quarantine, review, {}});
  const auto status = catalog.status();
  assert(status.definition_evidence_review_available);
  assert(status.definition_evidence_review_clusters == 1);
  assert(status.definition_evidence_review_blocked_atomicity == 1);
  assert(status.definition_evidence_review_multi_upos_forms == 1);
  assert(status.definition_evidence_review_message.find("Overlay v2.4") !=
         std::string::npos);

  const auto result = catalog.query_quarantine("ávido");
  assert(result.total == 1);
  const auto &candidate = result.candidates.front();
  assert(candidate.evidence_review_available);
  assert(candidate.evidence_review_contract_version == "2.4.0");
  assert(candidate.sense_cluster_id == "SC24-FIXTURE");
  assert(candidate.lexical_form_key == "LFK24-FIXTURE");
  assert(candidate.surface_form_group_id == "SFG24-FIXTURE");
  assert(candidate.upos_variant_count == 2);
  assert(candidate.upos_variants == "ADJ,NOUN");
  assert(candidate.candidate_evidence_atom_count == 3);
  assert(candidate.authorized_evidence_atom_count == 0);
  assert(candidate.atomic_evidence_summary.find("EA24-FIXTURE-3") !=
         std::string::npos);
  assert(candidate.definition_qa_gate == "blocked_atomic_evidence_review");
  assert(candidate.definition_qa_issue_codes == "LEX-DEF-005");
  assert(bytes(review) == review_before);

  const auto invalid_review = temporary.path / "invalid-review-v24.sqlite";
  create_atomic_definition_evidence_review(invalid_review, 1);
  inde::persistence::SqliteLinguisticCatalog invalid(
      {lexical, constructions, quarantine, invalid_review, {}});
  assert(!invalid.status().definition_evidence_review_available);
}

void definition_promotion_v25_is_read_only_and_collision_safe() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  const auto quarantine = temporary.path / "quarantine.sqlite";
  const auto promotion = temporary.path / "promotion-v25.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  create_quarantine_catalog(quarantine);
  create_definition_promotion(promotion);
  const auto promotion_before = bytes(promotion);

  inde::persistence::SqliteLinguisticCatalog catalog(
      {lexical, constructions, quarantine, {}, promotion});
  const auto status = catalog.status();
  assert(status.definition_promotion_available);
  assert(status.definition_promoted_senses == 1);
  assert(status.definition_promoted_human_reviewed == 1);
  assert(status.definition_promotion_message.find("Promoção v2.5") !=
         std::string::npos);

  const auto result = catalog.query_lexicon("ABADE");
  assert(result.total == 1);
  assert(result.candidates.size() == 1);
  const auto &candidate = result.candidates.front();
  assert(candidate.definition_promoted);
  assert(candidate.definition_promotion_contract_version == "2.5.0");
  assert(candidate.definition_promotion_id == "PROM25-FIXTURE");
  assert(candidate.definition_promotion_provenance_count == 1);
  assert(candidate.definition_promotion_quality_score == 90);
  assert(candidate.definition_promotion_collision_status ==
         "unique_within_lexical_form");
  assert(candidate.review_status == "human_batch_approved_best_candidate");
  assert(candidate.definition.has_value());
  assert(*candidate.definition == "coadjutor de pároco.");
  assert(!result.semantic_selection_performed);
  assert(!result.automatic_binding_created);
  assert(bytes(promotion) == promotion_before);

  const auto invalid_path = temporary.path / "invalid-promotion-v25.sqlite";
  create_definition_promotion(invalid_path, 1);
  inde::persistence::SqliteLinguisticCatalog invalid(
      {lexical, constructions, quarantine, {}, invalid_path});
  assert(!invalid.status().definition_promotion_available);
  assert(invalid.query_lexicon("abade").total == 0);
}

void limits_and_empty_queries_are_rejected() {
  TemporaryDirectory temporary;
  const auto lexical = temporary.path / "l06.sqlite";
  const auto constructions = temporary.path / "l08.sqlite";
  const auto quarantine = temporary.path / "quarantine.sqlite";
  create_lexical_catalog(lexical);
  create_construction_catalog(constructions);
  create_quarantine_catalog(quarantine);
  inde::persistence::SqliteLinguisticCatalog catalog(
      {lexical, constructions, quarantine, {}, {}});
  for (const auto limit : {std::size_t{0}, std::size_t{51}}) {
    bool rejected = false;
    try {
      static_cast<void>(catalog.query_lexicon("banco", limit));
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
      static_cast<void>(catalog.query_quarantine("ávido", limit));
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
  const auto *quarantine = std::getenv("INDE_QUARANTINE_CATALOG");
  const auto *definition_review =
      std::getenv("INDE_DEFINITION_EVIDENCE_REVIEW");
  const auto *definition_promotion =
      std::getenv("INDE_DEFINITION_PROMOTION");
  if (!lexical || !*lexical || !constructions || !*constructions)
    return;
  inde::persistence::SqliteLinguisticCatalog catalog(
      {{lexical}, {constructions},
       quarantine && *quarantine ? std::filesystem::path{quarantine}
                                 : std::filesystem::path{},
       definition_review && *definition_review
           ? std::filesystem::path{definition_review}
           : std::filesystem::path{},
       definition_promotion && *definition_promotion
           ? std::filesystem::path{definition_promotion}
           : std::filesystem::path{}});
  const auto status = catalog.status();
  assert(status.lexical_available);
  assert(status.constructions_available);
  if (quarantine && *quarantine)
    assert(status.quarantine_available);
  if (definition_review && *definition_review) {
    assert(status.definition_evidence_review_available);
    assert(status.definition_evidence_review_clusters == 130057);
  }
  if (definition_promotion && *definition_promotion) {
    assert(status.definition_promotion_available);
    assert(status.definition_promoted_senses == 86);
    const auto abade = catalog.query_lexicon("abade");
    assert(std::count_if(abade.candidates.begin(), abade.candidates.end(),
                         [](const auto &candidate) {
                           return candidate.definition_promoted;
                         }) == 3);
  }
  const auto banco = catalog.query_lexicon("BANCO");
  assert(banco.total > 0);
  assert(banco.candidates.size() == banco.total);
  for (const auto &candidate : banco.candidates) {
    assert(candidate.definition_state == "known");
    assert(candidate.definition.has_value());
    assert(!candidate.definition->empty());
  }
  const auto ocupacao = catalog.query_lexicon("OCUPAÇÃO");
  assert(ocupacao.total > 0);
  assert(ocupacao.candidates.size() == ocupacao.total);
  for (const auto &candidate : ocupacao.candidates) {
    assert(candidate.definition_state == "known");
    assert(candidate.definition.has_value());
    assert(!candidate.definition->empty());
  }
  assert(!banco.semantic_selection_performed);
  assert(!banco.automatic_binding_created);
  const auto entities = catalog.linguistic_entities();
  assert(entities.size() == 7);
  if (quarantine && *quarantine) {
    const auto isolated = catalog.query_quarantine("a-par");
    assert(isolated.total > 0);
    assert(!isolated.candidates.empty());
    for (const auto &candidate : isolated.candidates) {
      assert(candidate.definition_state == "known");
      assert(candidate.definition.has_value());
      assert(!candidate.definition->empty());
      assert(candidate.review_status == "needs_review");
      assert(candidate.catalog_scope == "quarantine");
      assert(!candidate.source_record_id.empty());
      assert(!candidate.source_synset_id.empty());
    }
    assert(isolated.candidates.front().definition_is_editorial);
    assert(isolated.candidates.front().original_definition.has_value());
    assert(isolated.candidates.front().editorial_status == "editorial_draft");
    assert(!isolated.semantic_selection_performed);
    assert(!isolated.automatic_binding_created);
    if (definition_review && *definition_review) {
      assert(isolated.candidates.front().evidence_review_available);
      assert(!isolated.candidates.front().sense_cluster_id.empty());
      assert(isolated.candidates.front().authorized_evidence_count > 0);
      assert(!isolated.candidates.front().definition_evidence_map.empty());
      if (isolated.candidates.front().evidence_review_contract_version ==
          "2.4.0") {
        assert(!isolated.candidates.front().lexical_form_key.empty());
        assert(!isolated.candidates.front().surface_form_group_id.empty());
        assert(isolated.candidates.front().upos_variant_count > 0);
        assert(!isolated.candidates.front().atomic_evidence_summary.empty());
      }
    }
    if (status.quarantine_contract_version == "2.2.0") {
      assert(status.quarantine_fragment_candidates > 0);
      assert(status.quarantine_bound_senses > 0);
      const auto entusiasta = catalog.query_quarantine("entusiasta");
      assert(entusiasta.total > 0);
      const auto decomposed = std::find_if(
          entusiasta.candidates.begin(), entusiasta.candidates.end(),
          [](const auto &candidate) {
            return candidate.editorial_status ==
                   "policy_approved_decomposition_v22";
          });
      assert(decomposed != entusiasta.candidates.end());
      assert(decomposed->editorial_review_status ==
             "needs_human_confirmation");
    }
  }
}

} // namespace

int main() {
  accepted_catalogs_are_queried_without_writes_or_selection();
  unavailable_or_incompatible_catalogs_fail_closed();
  decomposition_v22_is_identified_and_remains_isolated();
  definition_evidence_v23_is_read_only_and_enriches_results();
  definition_evidence_v24_exposes_atoms_classes_and_gates();
  definition_promotion_v25_is_read_only_and_collision_safe();
  limits_and_empty_queries_are_rejected();
  authorized_catalogs_smoke_when_configured();
  return 0;
}

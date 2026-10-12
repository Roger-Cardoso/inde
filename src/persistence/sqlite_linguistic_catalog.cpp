#include "inde/persistence/sqlite_linguistic_catalog.hpp"

#include <glib.h>
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace inde::persistence {
namespace {

struct DatabaseCloser {
  void operator()(sqlite3 *database) const noexcept {
    if (database)
      sqlite3_close_v2(database);
  }
};

struct StatementCloser {
  void operator()(sqlite3_stmt *statement) const noexcept {
    if (statement)
      sqlite3_finalize(statement);
  }
};

using Database = std::unique_ptr<sqlite3, DatabaseCloser>;
using Statement = std::unique_ptr<sqlite3_stmt, StatementCloser>;

struct GlibStringDeleter {
  void operator()(gchar *value) const noexcept { g_free(value); }
};

using GlibString = std::unique_ptr<gchar, GlibStringDeleter>;

std::string sqlite_message(sqlite3 *database, std::string_view action) {
  std::string message{"Não foi possível "};
  message += action;
  message += ": ";
  message += database ? sqlite3_errmsg(database) : "erro SQLite desconhecido";
  return message;
}

Database open_read_only(const std::filesystem::path &path) {
  if (path.empty())
    throw std::runtime_error("O caminho do catálogo não foi configurado");
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Catálogo linguístico não encontrado em " +
                             path.string());
  sqlite3 *raw{};
  const auto filename = path.string();
  const int result =
      sqlite3_open_v2(filename.c_str(), &raw,
                      SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
  Database database{raw};
  if (result != SQLITE_OK)
    throw std::runtime_error(sqlite_message(raw, "abrir o catálogo"));
  if (sqlite3_db_readonly(raw, "main") != 1)
    throw std::runtime_error(
        "O catálogo linguístico não abriu em modo somente leitura");
  char *error{};
  if (sqlite3_exec(raw, "PRAGMA query_only = ON", nullptr, nullptr, &error) !=
      SQLITE_OK) {
    const std::string detail = error ? error : "erro desconhecido";
    sqlite3_free(error);
    throw std::runtime_error(
        "Não foi possível tornar o catálogo somente leitura: " + detail);
  }
  return database;
}

Statement prepare(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *raw{};
  if (sqlite3_prepare_v2(database, sql.data(), static_cast<int>(sql.size()),
                         &raw, nullptr) != SQLITE_OK)
    throw std::runtime_error(sqlite_message(database, "preparar a consulta"));
  return Statement{raw};
}

void bind_text(sqlite3 *database, sqlite3_stmt *statement, int index,
               const std::string &value) {
  if (sqlite3_bind_text(statement, index, value.data(),
                        static_cast<int>(value.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK)
    throw std::runtime_error(
        sqlite_message(database, "vincular o texto da consulta"));
}

void bind_integer(sqlite3 *database, sqlite3_stmt *statement, int index,
                  std::size_t value) {
  if (sqlite3_bind_int64(statement, index, static_cast<sqlite3_int64>(value)) !=
      SQLITE_OK)
    throw std::runtime_error(
        sqlite_message(database, "vincular o limite da consulta"));
}

std::string column_text(sqlite3_stmt *statement, int column) {
  if (sqlite3_column_type(statement, column) == SQLITE_NULL)
    return {};
  const auto *value = sqlite3_column_text(statement, column);
  const auto size = sqlite3_column_bytes(statement, column);
  return {reinterpret_cast<const char *>(value),
          static_cast<std::size_t>(size)};
}

std::string trimmed(std::string value) {
  const auto whitespace = [](unsigned char character) {
    return std::isspace(character) != 0;
  };
  value.erase(value.begin(),
              std::find_if_not(value.begin(), value.end(), whitespace));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(),
              value.end());
  return value;
}

std::string normalized_lookup_key(const std::string &value) {
  if (!g_utf8_validate(value.data(), static_cast<gssize>(value.size()),
                       nullptr))
    throw std::invalid_argument("A consulta lexical não contém UTF-8 válido");
  GlibString normalized{g_utf8_normalize(
      value.data(), static_cast<gssize>(value.size()), G_NORMALIZE_NFC)};
  if (!normalized)
    throw std::invalid_argument("Não foi possível normalizar a consulta");
  GlibString folded{g_utf8_casefold(normalized.get(), -1)};
  if (!folded)
    throw std::invalid_argument(
        "Não foi possível normalizar maiúsculas e minúsculas da consulta");
  return folded.get();
}

std::size_t checked_limit(std::size_t limit) {
  if (limit == 0 || limit > 50)
    throw std::invalid_argument("O limite da consulta deve ficar entre 1 e 50");
  return limit;
}

std::filesystem::path default_data_root() {
  if (const auto *xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
    return std::filesystem::path{xdg} / "inde" / "linguistic";
  if (const auto *home = std::getenv("HOME"); home && *home)
    return std::filesystem::path{home} / ".local" / "share" / "inde" /
           "linguistic";
  return {};
}

void require_lexical_contract(sqlite3 *database) {
  auto statement = prepare(
      database,
      "SELECT contract_version, service_version, status, read_only_runtime, "
      "network_required FROM query_catalog_metadata WHERE singleton = 1");
  if (sqlite3_step(statement.get()) != SQLITE_ROW ||
      column_text(statement.get(), 0) != "0.5.0" ||
      column_text(statement.get(), 1) != "l06-query-1" ||
      column_text(statement.get(), 2) != "ready" ||
      sqlite3_column_int(statement.get(), 3) != 1 ||
      sqlite3_column_int(statement.get(), 4) != 0)
    throw std::runtime_error(
        "O catálogo lexical não corresponde ao contrato L-06 aceito");
}

void require_construction_contract(sqlite3 *database) {
  auto statement = prepare(
      database, "SELECT contract_version, pipeline_version, status, "
                "human_review_status, origin_kind, epistemic_status_preserved, "
                "distribution_scope, automatic_bindings FROM promotion_build");
  if (sqlite3_step(statement.get()) != SQLITE_ROW ||
      column_text(statement.get(), 0) != "0.6.0" ||
      column_text(statement.get(), 1) != "l08-promotion-1" ||
      column_text(statement.get(), 2) != "promoted" ||
      column_text(statement.get(), 3) != "accepted" ||
      column_text(statement.get(), 4) != "synthetic_fixture" ||
      column_text(statement.get(), 5) != "derived" ||
      column_text(statement.get(), 6) != "internal_productive" ||
      sqlite3_column_int(statement.get(), 7) != 0)
    throw std::runtime_error(
        "O catálogo construcional não corresponde à promoção L-08 aceita");
}

struct QuarantineCatalogInfo {
  std::string contract_version{"1.2.0"};
  std::size_t fragment_candidates{};
  std::size_t bound_senses{};
  std::size_t unbound_fragments{};
  std::size_t pending_hyphen_records{};
};

struct DefinitionEvidenceReviewInfo {
  std::string contract_version;
  std::size_t clusters{};
  std::size_t blocked_alignment{};
  std::size_t blocked_atomicity{};
  std::size_t blocked_families{};
  std::size_t incomplete_maps{};
  std::size_t cross_checks{};
  std::size_t multi_upos_forms{};
};

struct DefinitionPromotionInfo {
  std::string contract_version;
  std::size_t promoted_senses{};
  std::size_t exact_collapses{};
  std::size_t human_reviewed{};
};

bool table_exists(sqlite3 *database, std::string_view name) {
  auto statement = prepare(
      database,
      "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?1");
  const std::string table_name{name};
  bind_text(database, statement.get(), 1, table_name);
  if (sqlite3_step(statement.get()) != SQLITE_ROW)
    throw std::runtime_error(
        sqlite_message(database, "inspecionar o contrato da quarentena"));
  return sqlite3_column_int(statement.get(), 0) == 1;
}

QuarantineCatalogInfo require_quarantine_contract(sqlite3 *database) {
  auto query_metadata = prepare(
      database,
      "SELECT contract_version, service_version, status, read_only_runtime, "
      "network_required FROM query_catalog_metadata WHERE singleton = 1");
  if (sqlite3_step(query_metadata.get()) != SQLITE_ROW ||
      column_text(query_metadata.get(), 0) != "0.5.0" ||
      column_text(query_metadata.get(), 1) != "l06-query-1" ||
      column_text(query_metadata.get(), 2) != "quarantine_ready" ||
      sqlite3_column_int(query_metadata.get(), 3) != 1 ||
      sqlite3_column_int(query_metadata.get(), 4) != 0)
    throw std::runtime_error(
        "O serviço lexical não está marcado como quarentena somente leitura");
  auto statement = prepare(
      database,
      "SELECT contract_version, pipeline_version, source_id, status, "
      "human_decision_status, record_review_status, distribution_scope, "
      "promoted, isolated, network_used, automatic_bindings "
      "FROM quarantine_build WHERE singleton = 1");
  if (sqlite3_step(statement.get()) != SQLITE_ROW ||
      column_text(statement.get(), 0) != "1.0.0" ||
      column_text(statement.get(), 1) != "ontopt-selective-quarantine-1" ||
      column_text(statement.get(), 2) != "SRC-CAND-ONTOPT" ||
      column_text(statement.get(), 3) != "quarantine_ready" ||
      column_text(statement.get(), 4) != "accepted_for_isolated_testing" ||
      column_text(statement.get(), 5) != "criteria_passed_needs_review" ||
      column_text(statement.get(), 6) != "internal_quarantine" ||
      sqlite3_column_int(statement.get(), 7) != 0 ||
      sqlite3_column_int(statement.get(), 8) != 1 ||
      sqlite3_column_int(statement.get(), 9) != 0 ||
      sqlite3_column_int(statement.get(), 10) != 0)
    throw std::runtime_error(
        "O catálogo Onto.PT não corresponde ao contrato de quarentena v1.0");

  auto editorial = prepare(
      database,
      "SELECT contract_version, pipeline_version, status, policy_decision, "
      "content_decision, distribution_scope, original_definitions_preserved, "
      "promoted, isolated, network_used, automatic_bindings, total_assessed, "
      "source_useful_candidates, editorial_draft_required, editorial_drafts, "
      "abstentions, human_accepted FROM editorial_build WHERE singleton = 1");
  if (sqlite3_step(editorial.get()) != SQLITE_ROW ||
      column_text(editorial.get(), 0) != "1.1.0" ||
      column_text(editorial.get(), 1) != "inde-definition-editorial-review-1" ||
      column_text(editorial.get(), 2) != "editorial_review_ready" ||
      column_text(editorial.get(), 3) != "accepted" ||
      column_text(editorial.get(), 4) != "drafts_pending_human" ||
      column_text(editorial.get(), 5) != "internal_quarantine" ||
      sqlite3_column_int(editorial.get(), 6) != 1 ||
      sqlite3_column_int(editorial.get(), 7) != 0 ||
      sqlite3_column_int(editorial.get(), 8) != 1 ||
      sqlite3_column_int(editorial.get(), 9) != 0 ||
      sqlite3_column_int(editorial.get(), 10) != 0 ||
      sqlite3_column_int(editorial.get(), 11) != 960 ||
      sqlite3_column_int(editorial.get(), 12) != 667 ||
      sqlite3_column_int(editorial.get(), 13) != 293 ||
      sqlite3_column_int(editorial.get(), 14) != 32 ||
      sqlite3_column_int(editorial.get(), 15) != 8 ||
      sqlite3_column_int(editorial.get(), 16) != 0)
    throw std::runtime_error(
        "A camada editorial não corresponde ao contrato de revisão v1.1");

  auto human_review = prepare(
      database,
      "SELECT COUNT(*) FROM editorial_decision_build WHERE singleton=1 "
      "AND contract_version='1.2.0' "
      "AND pipeline_version='inde-definition-human-review-1' "
      "AND status='partial_human_review_recorded' "
      "AND content_decision='32_accepted_8_deferred' "
      "AND abstention_guidance='sense_definition_relations_fallback' "
      "AND distribution_scope='internal_quarantine' "
      "AND original_definitions_preserved=1 AND promoted=0 AND isolated=1 "
      "AND network_used=0 AND automatic_bindings=0 "
      "AND accepted_editorial_definitions=32 AND deferred_abstentions=8 "
      "AND pending_unreviewed_records=920 AND fallback_original_records=8 "
      "AND pending_relation_validations=8");
  if (sqlite3_step(human_review.get()) != SQLITE_ROW ||
      sqlite3_column_int(human_review.get(), 0) != 1)
    throw std::runtime_error(
        "A camada editorial não corresponde à decisão humana v1.2");

  QuarantineCatalogInfo result;
  if (!table_exists(database, "decomposition_build"))
    return result;

  auto decomposition = prepare(
      database,
      "SELECT contract_version, pipeline_version, status, isolated, promoted, "
      "parent_database_sha256, reference_database_sha256 "
      "FROM decomposition_build WHERE singleton=1");
  if (sqlite3_step(decomposition.get()) != SQLITE_ROW ||
      column_text(decomposition.get(), 0) != "2.2.0" ||
      column_text(decomposition.get(), 1) !=
          "ontopt-priority-sense-decomposition-1" ||
      column_text(decomposition.get(), 2) !=
          "priority_decomposition_complete_quarantine_retained" ||
      sqlite3_column_int(decomposition.get(), 3) != 1 ||
      sqlite3_column_int(decomposition.get(), 4) != 0 ||
      column_text(decomposition.get(), 5) !=
          "ca02f1bdad0dec9e2777ce59fddfc79d1acee6dd798638fbfe0e0fce1c017854" ||
      column_text(decomposition.get(), 6) !=
          "5107eef9fb94a30b094afeaca14cc09a7c84b28f953338068bb13982f047ac04")
    throw std::runtime_error(
        "A camada de decomposição não corresponde ao contrato de quarentena "
        "v2.2");

  auto counts = prepare(
      database,
      "SELECT "
      "(SELECT COUNT(*) FROM synset_fragment_candidate_v22), "
      "(SELECT COUNT(*) FROM decomposed_sense_candidate_v22), "
      "(SELECT COUNT(*) FROM synset_fragment_candidate_v22 "
      " WHERE binding_status='binding_unresolved'), "
      "(SELECT COUNT(*) FROM orthography_normalization_decision "
      " WHERE scope='integral_form' "
      " AND decision='review_hyphen_unresolved')");
  if (sqlite3_step(counts.get()) != SQLITE_ROW)
    throw std::runtime_error(
        sqlite_message(database, "contar os registros da quarentena v2.2"));
  result.contract_version = column_text(decomposition.get(), 0);
  result.fragment_candidates =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 0));
  result.bound_senses =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 1));
  result.unbound_fragments =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 2));
  result.pending_hyphen_records =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 3));
  if (result.fragment_candidates == 0 || result.bound_senses == 0 ||
      result.unbound_fragments > result.fragment_candidates)
    throw std::runtime_error(
        "A quarentena v2.2 não contém a decomposição mensurável esperada");
  return result;
}

DefinitionEvidenceReviewInfo
require_definition_evidence_review_contract(sqlite3 *database) {
  if (!table_exists(database, "review_overlay_build"))
    throw std::runtime_error("O overlay de evidência não contém metadados");
  auto build = prepare(
      database,
      "SELECT contract_version,pipeline_version,parent_database_sha256,status,"
      "isolated,promoted,external_sources_collected,"
      "definition_generation_executed,source_contract_changed "
      "FROM review_overlay_build WHERE singleton=1");
  if (sqlite3_step(build.get()) != SQLITE_ROW ||
      column_text(build.get(), 2) !=
          "9e75e67119e108ef209508bd62764b317589a9e8ab6fb08f3b9ec7863ca33c2c" ||
      sqlite3_column_int(build.get(), 4) != 1 ||
      sqlite3_column_int(build.get(), 5) != 0 ||
      sqlite3_column_int(build.get(), 6) != 0 ||
      sqlite3_column_int(build.get(), 7) != 0 ||
      sqlite3_column_int(build.get(), 8) != 0)
    throw std::runtime_error("O overlay não permanece isolado e imutável");

  DefinitionEvidenceReviewInfo result;
  result.contract_version = column_text(build.get(), 0);
  Statement counts;
  if (result.contract_version == "2.4.0") {
    if (column_text(build.get(), 1) != "inde-atomic-evidence-alignment-2" ||
        column_text(build.get(), 3) !=
            "atomic_evidence_and_family_alignment_ready_human_review_required" ||
        !table_exists(database, "sense_cluster_v24") ||
        !table_exists(database, "definition_artifact_v24") ||
        !table_exists(database, "evidence_atom_v24") ||
        !table_exists(database, "surface_form_group_v24") ||
        !table_exists(database, "cluster_alignment_family_v24") ||
        !table_exists(database, "query_annotation_v24") ||
        !table_exists(database, "definition_qa_issue_v24"))
      throw std::runtime_error(
          "O overlay não corresponde ao contrato atômico v2.4");
    auto predecessor = prepare(
        database,
        "SELECT predecessor_overlay_sha256 FROM review_overlay_build "
        "WHERE singleton=1");
    if (sqlite3_step(predecessor.get()) != SQLITE_ROW ||
        column_text(predecessor.get(), 0) !=
            "edd40c858c81b1ef309b9dcf9d5da8c68e8f32668617a7612ff5fed0806fc270")
      throw std::runtime_error("O overlay v2.4 não preserva a linhagem v2.3");
    counts = prepare(
        database,
        "SELECT (SELECT COUNT(*) FROM sense_cluster_v24),"
        "(SELECT COUNT(*) FROM definition_artifact_v24),"
        "(SELECT COUNT(*) FROM query_annotation_v24),"
        "(SELECT COUNT(*) FROM query_annotation_v24 WHERE "
        "qa_gate='blocked_return_to_sense_alignment'),"
        "(SELECT COUNT(*) FROM query_annotation_v24 WHERE "
        "qa_gate='blocked_atomic_evidence_review'),"
        "(SELECT COUNT(*) FROM query_annotation_v24 WHERE "
        "qa_gate='blocked_family_alignment'),"
        "(SELECT COUNT(*) FROM query_annotation_v24 WHERE "
        "qa_gate='blocked_incomplete_evidence_map'),"
        "(SELECT COUNT(*) FROM query_annotation_v24 WHERE "
        "qa_gate='human_cross_definition_review'),"
        "(SELECT COUNT(*) FROM surface_form_group_v24 WHERE upos_variant_count>1),"
        "(SELECT COUNT(*) FROM definition_qa_issue_v24 WHERE code NOT IN "
        "('LEX-DEF-001','LEX-DEF-002','LEX-DEF-003','LEX-DEF-004',"
        "'LEX-DEF-005','LEX-DEF-006'))");
  } else if (result.contract_version == "2.3.0") {
    if (column_text(build.get(), 1) != "inde-sense-evidence-isolation-1" ||
        column_text(build.get(), 3) !=
            "evidence_mapping_and_qa_ready_human_review_required" ||
        !table_exists(database, "sense_cluster_v23") ||
        !table_exists(database, "definition_artifact_v23") ||
        !table_exists(database, "query_annotation_v23") ||
        !table_exists(database, "definition_qa_issue_v23"))
      throw std::runtime_error(
          "O overlay não corresponde ao contrato de evidência v2.3");
    counts = prepare(
        database,
        "SELECT (SELECT COUNT(*) FROM sense_cluster_v23),"
        "(SELECT COUNT(*) FROM definition_artifact_v23),"
        "(SELECT COUNT(*) FROM query_annotation_v23),"
        "(SELECT COUNT(*) FROM query_annotation_v23 WHERE "
        "qa_gate='blocked_return_to_sense_alignment'),0,0,"
        "(SELECT COUNT(*) FROM query_annotation_v23 WHERE "
        "qa_gate='blocked_incomplete_evidence_map'),"
        "(SELECT COUNT(*) FROM query_annotation_v23 WHERE "
        "qa_gate='human_cross_definition_review'),0,"
        "(SELECT COUNT(*) FROM definition_qa_issue_v23 WHERE code NOT IN "
        "('LEX-DEF-001','LEX-DEF-002','LEX-DEF-003','LEX-DEF-004'))");
  } else {
    throw std::runtime_error("Versão desconhecida do overlay de evidência");
  }
  if (sqlite3_step(counts.get()) != SQLITE_ROW)
    throw std::runtime_error(
        sqlite_message(database, "validar o overlay de evidência"));
  const auto clusters = sqlite3_column_int64(counts.get(), 0);
  if (clusters <= 0 || sqlite3_column_int64(counts.get(), 1) != clusters ||
      sqlite3_column_int64(counts.get(), 2) != clusters ||
      sqlite3_column_int64(counts.get(), 9) != 0)
    throw std::runtime_error(
        "O overlay não cobre definições, clusters e anotações de modo "
        "consistente");
  result.clusters = static_cast<std::size_t>(clusters);
  result.blocked_alignment =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 3));
  result.blocked_atomicity =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 4));
  result.blocked_families =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 5));
  result.incomplete_maps =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 6));
  result.cross_checks =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 7));
  result.multi_upos_forms =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 8));
  return result;
}

DefinitionPromotionInfo require_definition_promotion_contract(
    sqlite3 *database) {
  if (!table_exists(database, "promotion_build_v25") ||
      !table_exists(database, "candidate_group_v25") ||
      !table_exists(database, "candidate_source_v25") ||
      !table_exists(database, "collision_group_v25") ||
      !table_exists(database, "collision_member_v25") ||
      !table_exists(database, "accepted_overlap_v25") ||
      !table_exists(database, "promoted_sense_v25"))
    throw std::runtime_error(
        "O catálogo de promoção não contém as tabelas v2.5 obrigatórias");
  auto build = prepare(
      database,
      "SELECT contract_version,pipeline_version,authorization_id,"
      "overlay_v24_sha256,quarantine_v22_sha256,accepted_l06_sha256,status,"
      "isolated,promoted,source_databases_modified,external_sources_collected,"
      "automatic_bindings_created,quality_threshold "
      "FROM promotion_build_v25 WHERE singleton=1");
  if (sqlite3_step(build.get()) != SQLITE_ROW ||
      column_text(build.get(), 0) != "2.5.0" ||
      column_text(build.get(), 1) !=
          "inde-definition-candidate-promotion-1" ||
      column_text(build.get(), 2) !=
          "HUMAN-AUTH-DEFINITION-CANDIDATE-PROMOTION-2026-09-29" ||
      column_text(build.get(), 3) !=
          "9204300814a9a0c7c3c51de39fbc71ccd6028053070754df667336bbfd281cc9" ||
      column_text(build.get(), 4) !=
          "9e75e67119e108ef209508bd62764b317589a9e8ab6fb08f3b9ec7863ca33c2c" ||
      column_text(build.get(), 5) !=
          "5107eef9fb94a30b094afeaca14cc09a7c84b28f953338068bb13982f047ac04" ||
      column_text(build.get(), 6) !=
          "promoted_collision_safe_batch_ready" ||
      sqlite3_column_int(build.get(), 7) != 1 ||
      sqlite3_column_int(build.get(), 8) != 1 ||
      sqlite3_column_int(build.get(), 9) != 0 ||
      sqlite3_column_int(build.get(), 10) != 0 ||
      sqlite3_column_int(build.get(), 11) != 0 ||
      sqlite3_column_int(build.get(), 12) != 95)
    throw std::runtime_error(
        "O catálogo de promoção não corresponde ao contrato v2.5 aceito");

  auto counts = prepare(
      database,
      "SELECT (SELECT COUNT(*) FROM promoted_sense_v25),"
      "(SELECT COUNT(*) FROM promoted_sense_v25 "
      " WHERE collision_status='exact_duplicates_collapsed'),"
      "(SELECT COUNT(*) FROM promoted_sense_v25 p "
      " JOIN candidate_group_v25 c ON c.candidate_id=p.candidate_id "
      " WHERE c.quality_tier='promotion_quality_human_reviewed'),"
      "(SELECT COUNT(*) FROM promoted_sense_v25 p "
      " JOIN collision_member_v25 m ON m.candidate_id=p.candidate_id "
      " JOIN collision_group_v25 c ON c.collision_id=m.collision_id "
      " WHERE c.blocking=1),"
      "(SELECT COUNT(*) FROM promoted_sense_v25 p "
      " JOIN accepted_overlap_v25 a ON a.candidate_id=p.candidate_id "
      " WHERE a.blocking=1)");
  if (sqlite3_step(counts.get()) != SQLITE_ROW ||
      sqlite3_column_int64(counts.get(), 0) <= 0 ||
      sqlite3_column_int64(counts.get(), 3) != 0 ||
      sqlite3_column_int64(counts.get(), 4) != 0)
    throw std::runtime_error(
        "O catálogo v2.5 contém promoção vazia ou colisão bloqueante");
  DefinitionPromotionInfo result;
  result.contract_version = column_text(build.get(), 0);
  result.promoted_senses =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 0));
  result.exact_collapses =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 1));
  result.human_reviewed =
      static_cast<std::size_t>(sqlite3_column_int64(counts.get(), 2));
  return result;
}

} // namespace

LinguisticCatalogPaths LinguisticCatalogPaths::from_environment() {
  const auto root = default_data_root();
  const auto *lexical = std::getenv("INDE_L06_CATALOG");
  const auto *constructions = std::getenv("INDE_L08_CATALOG");
  const auto *quarantine = std::getenv("INDE_QUARANTINE_CATALOG");
  const auto *definition_review =
      std::getenv("INDE_DEFINITION_EVIDENCE_REVIEW");
  const auto *definition_promotion =
      std::getenv("INDE_DEFINITION_PROMOTION");
  auto default_quarantine =
      root / "quarantine" /
      "dictionary-ontopt-v0.6-decomposition-quarantine-v2.2.sqlite";
  if (!std::filesystem::is_regular_file(default_quarantine))
    default_quarantine =
        root / "quarantine" /
        "dictionary-ontopt-v0.6-editorial-quarantine-v1.2.sqlite";
  auto default_review =
      root / "quarantine" / "definition-evidence-review-v2.4.sqlite";
  if (!std::filesystem::is_regular_file(default_review))
    default_review =
        root / "quarantine" / "definition-evidence-review-v2.3.sqlite";
  const auto default_promotion =
      root / "definition-candidate-promotion-v2.5.sqlite";
  return {
      lexical && *lexical ? std::filesystem::path{lexical}
                          : root / "dictionary-l06-query.sqlite",
      constructions && *constructions ? std::filesystem::path{constructions}
                                      : root / "dictionary-l08-promoted.sqlite",
      quarantine && *quarantine
          ? std::filesystem::path{quarantine}
          : default_quarantine,
      definition_review && *definition_review
          ? std::filesystem::path{definition_review}
          : default_review,
      definition_promotion && *definition_promotion
          ? std::filesystem::path{definition_promotion}
          : default_promotion,
  };
}

project::LinguisticCatalogStatus SqliteLinguisticCatalog::status() const {
  project::LinguisticCatalogStatus result;
  try {
    auto database = open_read_only(paths_.lexical);
    require_lexical_contract(database.get());
    result.lexical_available = true;
    result.lexical_message =
        "Catálogo lexical L-06 disponível em modo somente leitura.";
  } catch (const std::exception &error) {
    result.lexical_message = error.what();
  }
  try {
    auto database = open_read_only(paths_.constructions);
    require_construction_contract(database.get());
    result.constructions_available = true;
    result.constructions_message =
        "Catálogo construcional L-08 disponível em modo somente leitura.";
  } catch (const std::exception &error) {
    result.constructions_message = error.what();
  }
  try {
    auto database = open_read_only(paths_.quarantine);
    const auto quarantine = require_quarantine_contract(database.get());
    result.quarantine_available = true;
    result.quarantine_contract_version = quarantine.contract_version;
    result.quarantine_fragment_candidates = quarantine.fragment_candidates;
    result.quarantine_bound_senses = quarantine.bound_senses;
    result.quarantine_unbound_fragments = quarantine.unbound_fragments;
    result.quarantine_pending_hyphen_records =
        quarantine.pending_hyphen_records;
    if (quarantine.contract_version == "2.2.0") {
      result.quarantine_message =
          "Quarentena Onto.PT v2.2 disponível em modo somente leitura; " +
          std::to_string(quarantine.bound_senses) +
          " acepções decompostas vinculadas, " +
          std::to_string(quarantine.unbound_fragments) +
          " fragmentos sem vínculo e " +
          std::to_string(quarantine.pending_hyphen_records) +
          " formas hifenizadas ainda pendentes.";
    } else {
      result.quarantine_message =
          "Quarentena editorial Onto.PT v1.2 disponível em modo somente "
          "leitura; 32 definições editoriais aceitas e 8 abstenções "
          "deferidas.";
    }
  } catch (const std::exception &error) {
    result.quarantine_message = error.what();
  }
  try {
    auto database = open_read_only(paths_.definition_review);
    const auto review =
        require_definition_evidence_review_contract(database.get());
    result.definition_evidence_review_available = true;
    result.definition_evidence_review_clusters = review.clusters;
    result.definition_evidence_review_blocked_alignment =
        review.blocked_alignment;
    result.definition_evidence_review_blocked_atomicity =
        review.blocked_atomicity;
    result.definition_evidence_review_blocked_families =
        review.blocked_families;
    result.definition_evidence_review_incomplete_maps =
        review.incomplete_maps;
    result.definition_evidence_review_cross_checks = review.cross_checks;
    result.definition_evidence_review_multi_upos_forms =
        review.multi_upos_forms;
    if (review.contract_version == "2.4.0") {
      result.definition_evidence_review_message =
          "Overlay v2.4 disponível: " + std::to_string(review.clusters) +
          " clusters; " + std::to_string(review.blocked_families) +
          " em famílias simétricas, " +
          std::to_string(review.blocked_atomicity) +
          " aguardam classificação atômica, " +
          std::to_string(review.multi_upos_forms) +
          " grafias possuem mais de uma classe e " +
          std::to_string(review.incomplete_maps) +
          " exigem completar evidências.";
    } else {
      result.definition_evidence_review_message =
          "Overlay v2.3 disponível: " + std::to_string(review.clusters) +
          " clusters com evidência isolada; " +
          std::to_string(review.blocked_alignment) +
          " retornam ao alinhamento, " +
          std::to_string(review.incomplete_maps) +
          " exigem completar evidências e " +
          std::to_string(review.cross_checks) +
          " aguardam revisão cruzada de redundância.";
    }
    if (result.quarantine_available)
      result.quarantine_message += " " +
                                   result.definition_evidence_review_message;
  } catch (const std::exception &error) {
    result.definition_evidence_review_message = error.what();
  }
  try {
    auto database = open_read_only(paths_.definition_promotion);
    const auto promotion = require_definition_promotion_contract(database.get());
    result.definition_promotion_available = true;
    result.definition_promoted_senses = promotion.promoted_senses;
    result.definition_promoted_exact_collapses = promotion.exact_collapses;
    result.definition_promoted_human_reviewed = promotion.human_reviewed;
    result.definition_promotion_message =
        "Promoção v2.5 disponível em modo somente leitura: " +
        std::to_string(promotion.promoted_senses) +
        " acepções; " + std::to_string(promotion.exact_collapses) +
        " colapsam duplicatas exatas e " +
        std::to_string(promotion.human_reviewed) +
        " decorrem dos casos humanos localizados. Nenhum binding automático.";
  } catch (const std::exception &error) {
    result.definition_promotion_message = error.what();
  }
  return result;
}

project::LexicalQueryResult
SqliteLinguisticCatalog::query_lexicon(const std::string &input,
                                       std::size_t limit) const {
  const auto query = trimmed(input);
  if (query.empty())
    throw std::invalid_argument(
        "Informe uma palavra ou expressão para consultar");
  if (query.size() > 256)
    throw std::invalid_argument("A consulta lexical excede 256 bytes");
  const auto lookup_key = normalized_lookup_key(query);
  limit = checked_limit(limit);
  auto database = open_read_only(paths_.lexical);
  require_lexical_contract(database.get());
  Database promotion_database;
  try {
    promotion_database = open_read_only(paths_.definition_promotion);
    static_cast<void>(
        require_definition_promotion_contract(promotion_database.get()));
  } catch (const std::exception &) {
    promotion_database.reset();
  }

  project::LexicalQueryResult result;
  result.query = query;
  {
    auto count = prepare(
        database.get(),
        "SELECT COUNT(*) FROM lexical_query_entry "
        "WHERE normalized = ?1 OR written = ?2");
    bind_text(database.get(), count.get(), 1, lookup_key);
    bind_text(database.get(), count.get(), 2, query);
    if (sqlite3_step(count.get()) != SQLITE_ROW)
      throw std::runtime_error(
          sqlite_message(database.get(), "contar resultados lexicais"));
    result.total =
        static_cast<std::size_t>(sqlite3_column_int64(count.get(), 0));
  }
  if (promotion_database) {
    auto count = prepare(
        promotion_database.get(),
        "SELECT COUNT(*) FROM promoted_lexical_query_entry_v25 "
        "WHERE normalized=?1 OR written=?2");
    bind_text(promotion_database.get(), count.get(), 1, lookup_key);
    bind_text(promotion_database.get(), count.get(), 2, query);
    if (sqlite3_step(count.get()) != SQLITE_ROW)
      throw std::runtime_error(sqlite_message(
          promotion_database.get(), "contar definições promovidas"));
    result.total +=
        static_cast<std::size_t>(sqlite3_column_int64(count.get(), 0));
  }
  auto statement = prepare(
      database.get(),
      "SELECT form_id, lexeme_id, sense_id, written, ptbr_pos, upos, "
      "source_pos, definition_state, definition_text, sense_review_status "
      "FROM lexical_query_entry "
      "WHERE normalized = ?1 OR written = ?2 "
      "ORDER BY normalized, lexeme_id, sense_id LIMIT ?3");
  bind_text(database.get(), statement.get(), 1, lookup_key);
  bind_text(database.get(), statement.get(), 2, query);
  bind_integer(database.get(), statement.get(), 3, limit);
  int step{};
  while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
    project::LexicalSenseCandidate candidate;
    candidate.form_id = column_text(statement.get(), 0);
    candidate.lexeme_id = column_text(statement.get(), 1);
    candidate.sense_id = column_text(statement.get(), 2);
    candidate.written = column_text(statement.get(), 3);
    candidate.ptbr_pos = column_text(statement.get(), 4);
    candidate.upos = column_text(statement.get(), 5);
    candidate.source_pos = column_text(statement.get(), 6);
    candidate.definition_state = column_text(statement.get(), 7);
    if (sqlite3_column_type(statement.get(), 8) != SQLITE_NULL)
      candidate.definition = column_text(statement.get(), 8);
    candidate.review_status = column_text(statement.get(), 9);
    result.candidates.push_back(std::move(candidate));
  }
  if (step != SQLITE_DONE)
    throw std::runtime_error(
        sqlite_message(database.get(), "consultar o léxico"));
  if (promotion_database && result.candidates.size() < limit) {
    auto promoted = prepare(
        promotion_database.get(),
        "SELECT promotion_id,form_id,lexeme_id,sense_id,written,ptbr_pos,"
        "upos,source_pos,definition_state,definition_text,review_status,"
        "provenance_count,quality_score,collision_status,promotion_basis,"
        "authorization_id FROM promoted_lexical_query_entry_v25 "
        "WHERE normalized=?1 OR written=?2 "
        "ORDER BY normalized,lexeme_id,sense_id LIMIT ?3");
    bind_text(promotion_database.get(), promoted.get(), 1, lookup_key);
    bind_text(promotion_database.get(), promoted.get(), 2, query);
    bind_integer(promotion_database.get(), promoted.get(), 3,
                 limit - result.candidates.size());
    while ((step = sqlite3_step(promoted.get())) == SQLITE_ROW) {
      project::LexicalSenseCandidate candidate;
      candidate.definition_promoted = true;
      candidate.definition_promotion_contract_version = "2.5.0";
      candidate.definition_promotion_id = column_text(promoted.get(), 0);
      candidate.form_id = column_text(promoted.get(), 1);
      candidate.lexeme_id = column_text(promoted.get(), 2);
      candidate.sense_id = column_text(promoted.get(), 3);
      candidate.written = column_text(promoted.get(), 4);
      candidate.ptbr_pos = column_text(promoted.get(), 5);
      candidate.upos = column_text(promoted.get(), 6);
      candidate.source_pos = column_text(promoted.get(), 7);
      candidate.definition_state = column_text(promoted.get(), 8);
      candidate.definition = column_text(promoted.get(), 9);
      candidate.review_status = column_text(promoted.get(), 10);
      candidate.definition_promotion_provenance_count =
          static_cast<std::size_t>(sqlite3_column_int64(promoted.get(), 11));
      candidate.definition_promotion_quality_score =
          sqlite3_column_int(promoted.get(), 12);
      candidate.definition_promotion_collision_status =
          column_text(promoted.get(), 13);
      candidate.definition_promotion_basis = column_text(promoted.get(), 14);
      candidate.definition_promotion_authorization_id =
          column_text(promoted.get(), 15);
      candidate.selection_criteria =
          "inde-definition-candidate-promotion-1";
      result.candidates.push_back(std::move(candidate));
    }
    if (step != SQLITE_DONE)
      throw std::runtime_error(sqlite_message(
          promotion_database.get(), "consultar definições promovidas"));
  }
  return result;
}

project::LexicalQueryResult
SqliteLinguisticCatalog::query_quarantine(const std::string &input,
                                          std::size_t limit) const {
  const auto query = trimmed(input);
  if (query.empty())
    throw std::invalid_argument(
        "Informe uma palavra ou expressão para consultar");
  if (query.size() > 256)
    throw std::invalid_argument("A consulta lexical excede 256 bytes");
  const auto lookup_key = normalized_lookup_key(query);
  limit = checked_limit(limit);
  auto database = open_read_only(paths_.quarantine);
  require_quarantine_contract(database.get());
  Database review_database;
  Statement review_statement;
  bool review_v24{};
  try {
    review_database = open_read_only(paths_.definition_review);
    const auto review =
        require_definition_evidence_review_contract(review_database.get());
    review_v24 = review.contract_version == "2.4.0";
    review_statement = prepare(
        review_database.get(),
        review_v24
            ? "SELECT sense_cluster_id,lexical_form_key,"
              "surface_form_group_id,upos_variant_count,upos_variants,"
              "freeze_status,generation_status,authorized_evidence_count,"
              "authorized_evidence_summary,definition_evidence_map,"
              "atomic_evidence_summary,candidate_atom_count,"
              "authorized_atom_count,alignment_family_id,qa_gate,"
              "qa_issue_codes,blocking_issue_count,review_issue_count "
              "FROM query_annotation_v24 WHERE source_record_id=?1 AND sense_id=?2"
            : "SELECT sense_cluster_id,freeze_status,generation_status,"
              "authorized_evidence_count,authorized_evidence_summary,"
              "definition_evidence_map,qa_gate,qa_issue_codes,"
              "blocking_issue_count,review_issue_count "
              "FROM query_annotation_v23 WHERE source_record_id=?1 AND sense_id=?2");
  } catch (const std::exception &) {
    review_statement.reset();
    review_database.reset();
  }

  project::LexicalQueryResult result;
  result.query = query;
  result.catalog_scope = "quarantine";
  {
    auto count = prepare(
        database.get(),
        "SELECT COUNT(*) FROM lexical_editorial_query_entry "
        "WHERE normalized = ?1 OR written = ?2");
    bind_text(database.get(), count.get(), 1, lookup_key);
    bind_text(database.get(), count.get(), 2, query);
    if (sqlite3_step(count.get()) != SQLITE_ROW)
      throw std::runtime_error(
          sqlite_message(database.get(), "contar resultados da quarentena"));
    result.total =
        static_cast<std::size_t>(sqlite3_column_int64(count.get(), 0));
  }
  auto statement = prepare(
      database.get(),
      "SELECT form_id, lexeme_id, sense_id, written, ptbr_pos, upos, "
      "source_pos, definition_state, definition_text, sense_review_status, "
      "source_record_id, source_synset_id, reference_overlap, "
      "criteria_version, definition_quality_flags_json, "
      "definition_score_total, definition_quality_gate, "
      "editorial_definition_text, editorial_status, editorial_rationale, "
      "editorial_review_status, human_decision_basis, "
      "relation_validation_status, fallback_mode, fallback_motive "
      "FROM lexical_editorial_query_entry "
      "WHERE normalized = ?1 OR written = ?2 "
      "ORDER BY normalized, lexeme_id, sense_id LIMIT ?3");
  bind_text(database.get(), statement.get(), 1, lookup_key);
  bind_text(database.get(), statement.get(), 2, query);
  bind_integer(database.get(), statement.get(), 3, limit);
  int step{};
  while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
    project::LexicalSenseCandidate candidate;
    candidate.form_id = column_text(statement.get(), 0);
    candidate.lexeme_id = column_text(statement.get(), 1);
    candidate.sense_id = column_text(statement.get(), 2);
    candidate.written = column_text(statement.get(), 3);
    candidate.ptbr_pos = column_text(statement.get(), 4);
    candidate.upos = column_text(statement.get(), 5);
    candidate.source_pos = column_text(statement.get(), 6);
    candidate.definition_state = column_text(statement.get(), 7);
    if (sqlite3_column_type(statement.get(), 8) != SQLITE_NULL) {
      candidate.original_definition = column_text(statement.get(), 8);
      candidate.definition = candidate.original_definition;
    }
    candidate.review_status = column_text(statement.get(), 9);
    candidate.source_record_id = column_text(statement.get(), 10);
    candidate.source_synset_id = column_text(statement.get(), 11);
    candidate.reference_overlap = sqlite3_column_int(statement.get(), 12) != 0;
    candidate.selection_criteria = column_text(statement.get(), 13);
    candidate.definition_quality_flags = column_text(statement.get(), 14);
    candidate.definition_quality_score = sqlite3_column_int(statement.get(), 15);
    candidate.definition_quality_gate = column_text(statement.get(), 16);
    if (sqlite3_column_type(statement.get(), 17) != SQLITE_NULL) {
      candidate.definition = column_text(statement.get(), 17);
      candidate.definition_is_editorial = true;
    }
    candidate.editorial_status = column_text(statement.get(), 18);
    candidate.editorial_rationale = column_text(statement.get(), 19);
    candidate.editorial_review_status = column_text(statement.get(), 20);
    candidate.human_decision_basis = column_text(statement.get(), 21);
    candidate.relation_validation_status = column_text(statement.get(), 22);
    candidate.fallback_mode = column_text(statement.get(), 23);
    candidate.fallback_motive = column_text(statement.get(), 24);
    candidate.catalog_scope = "quarantine";
    if (review_statement) {
      sqlite3_reset(review_statement.get());
      sqlite3_clear_bindings(review_statement.get());
      bind_text(review_database.get(), review_statement.get(), 1,
                candidate.source_record_id);
      bind_text(review_database.get(), review_statement.get(), 2,
                candidate.sense_id);
      if (sqlite3_step(review_statement.get()) == SQLITE_ROW) {
        candidate.evidence_review_available = true;
        candidate.evidence_review_contract_version =
            review_v24 ? "2.4.0" : "2.3.0";
        candidate.sense_cluster_id = column_text(review_statement.get(), 0);
        if (review_v24) {
          candidate.lexical_form_key = column_text(review_statement.get(), 1);
          candidate.surface_form_group_id =
              column_text(review_statement.get(), 2);
          candidate.upos_variant_count = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 3));
          candidate.upos_variants = column_text(review_statement.get(), 4);
          candidate.cluster_freeze_status =
              column_text(review_statement.get(), 5);
          candidate.generation_status = column_text(review_statement.get(), 6);
          candidate.authorized_evidence_count = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 7));
          candidate.authorized_evidence_summary =
              column_text(review_statement.get(), 8);
          candidate.definition_evidence_map =
              column_text(review_statement.get(), 9);
          candidate.atomic_evidence_summary =
              column_text(review_statement.get(), 10);
          candidate.candidate_evidence_atom_count = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 11));
          candidate.authorized_evidence_atom_count = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 12));
          if (sqlite3_column_type(review_statement.get(), 13) != SQLITE_NULL)
            candidate.alignment_family_id =
                column_text(review_statement.get(), 13);
          candidate.definition_qa_gate =
              column_text(review_statement.get(), 14);
          candidate.definition_qa_issue_codes =
              column_text(review_statement.get(), 15);
          candidate.definition_qa_blocking_issues = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 16));
          candidate.definition_qa_review_issues = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 17));
        } else {
          candidate.cluster_freeze_status =
              column_text(review_statement.get(), 1);
          candidate.generation_status = column_text(review_statement.get(), 2);
          candidate.authorized_evidence_count = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 3));
          candidate.authorized_evidence_summary =
              column_text(review_statement.get(), 4);
          candidate.definition_evidence_map =
              column_text(review_statement.get(), 5);
          candidate.definition_qa_gate =
              column_text(review_statement.get(), 6);
          candidate.definition_qa_issue_codes =
              column_text(review_statement.get(), 7);
          candidate.definition_qa_blocking_issues = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 8));
          candidate.definition_qa_review_issues = static_cast<std::size_t>(
              sqlite3_column_int64(review_statement.get(), 9));
        }
      }
    }
    result.candidates.push_back(std::move(candidate));
  }
  if (step != SQLITE_DONE)
    throw std::runtime_error(
        sqlite_message(database.get(), "consultar a quarentena lexical"));
  return result;
}

std::vector<project::LinguisticEntitySummary>
SqliteLinguisticCatalog::linguistic_entities(const std::string &input,
                                             std::size_t limit) const {
  const auto search = trimmed(input);
  if (search.size() > 256)
    throw std::invalid_argument("A pesquisa de construções excede 256 bytes");
  limit = checked_limit(limit);
  auto database = open_read_only(paths_.constructions);
  require_construction_contract(database.get());
  auto statement = prepare(
      database.get(),
      "WITH entities(kind, id, name, subtype, description, epistemic_status, "
      "review_status) AS ("
      "SELECT 0, id, name, construction_type, description, epistemic_status, "
      "review_status FROM construction UNION ALL "
      "SELECT 1, id, name, frame_type, definition, epistemic_status, "
      "review_status FROM frame UNION ALL "
      "SELECT 2, id, name, frame_type, 'Estrutura sintática explícita.', "
      "epistemic_status, review_status FROM syntactic_frame UNION ALL "
      "SELECT 3, id, canonical_form, mwe_type, "
      "'Expressão multipalavra com contiguidade ' || contiguity || '.', "
      "epistemic_status, review_status FROM mwe) "
      "SELECT kind, id, name, subtype, description, epistemic_status, "
      "review_status FROM entities "
      "WHERE ?1 = '' OR instr(lower(name), lower(?1)) > 0 "
      "ORDER BY kind, name, id LIMIT ?2");
  bind_text(database.get(), statement.get(), 1, search);
  bind_integer(database.get(), statement.get(), 2, limit);
  std::vector<project::LinguisticEntitySummary> result;
  int step{};
  while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
    project::LinguisticEntitySummary entity;
    switch (sqlite3_column_int(statement.get(), 0)) {
    case 0:
      entity.kind = project::LinguisticEntityKind::Construction;
      break;
    case 1:
      entity.kind = project::LinguisticEntityKind::Frame;
      break;
    case 2:
      entity.kind = project::LinguisticEntityKind::SyntacticFrame;
      break;
    case 3:
      entity.kind = project::LinguisticEntityKind::MultiwordExpression;
      break;
    default:
      throw std::runtime_error("Tipo linguístico inesperado no catálogo L-08");
    }
    entity.id = column_text(statement.get(), 1);
    entity.name = column_text(statement.get(), 2);
    entity.subtype = column_text(statement.get(), 3);
    entity.description = column_text(statement.get(), 4);
    entity.epistemic_status = column_text(statement.get(), 5);
    entity.review_status = column_text(statement.get(), 6);
    result.push_back(std::move(entity));
  }
  if (step != SQLITE_DONE)
    throw std::runtime_error(
        sqlite_message(database.get(), "consultar construções"));
  return result;
}

} // namespace inde::persistence

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace inde::project {

enum class LinguisticEntityKind {
  Construction,
  Frame,
  SyntacticFrame,
  MultiwordExpression,
};

struct LexicalSenseCandidate {
  std::string form_id;
  std::string lexeme_id;
  std::string sense_id;
  std::string written;
  std::string ptbr_pos;
  std::string upos;
  std::string source_pos;
  std::optional<std::string> definition;
  std::optional<std::string> original_definition;
  std::string definition_state;
  std::string review_status;
  std::string source_record_id;
  std::string source_synset_id;
  bool reference_overlap{};
  std::string selection_criteria;
  bool definition_is_editorial{};
  std::string editorial_status;
  std::string editorial_rationale;
  std::string editorial_review_status;
  std::string human_decision_basis;
  std::string relation_validation_status;
  std::string fallback_mode;
  std::string fallback_motive;
  std::string definition_quality_gate;
  std::string definition_quality_flags;
  int definition_quality_score{-1};
  std::string catalog_scope{"accepted"};
  bool evidence_review_available{};
  std::string evidence_review_contract_version;
  std::string sense_cluster_id;
  std::string lexical_form_key;
  std::string surface_form_group_id;
  std::size_t upos_variant_count{};
  std::string upos_variants;
  std::string cluster_freeze_status;
  std::string generation_status;
  std::size_t authorized_evidence_count{};
  std::string authorized_evidence_summary;
  std::string definition_evidence_map;
  std::string atomic_evidence_summary;
  std::size_t candidate_evidence_atom_count{};
  std::size_t authorized_evidence_atom_count{};
  std::string alignment_family_id;
  std::string definition_qa_gate;
  std::string definition_qa_issue_codes;
  std::size_t definition_qa_blocking_issues{};
  std::size_t definition_qa_review_issues{};
  bool definition_promoted{};
  std::string definition_promotion_contract_version;
  std::string definition_promotion_id;
  std::size_t definition_promotion_provenance_count{};
  int definition_promotion_quality_score{-1};
  std::string definition_promotion_collision_status;
  std::string definition_promotion_basis;
  std::string definition_promotion_authorization_id;
};

struct LexicalQueryResult {
  std::string query;
  std::vector<LexicalSenseCandidate> candidates;
  std::size_t total{};
  std::string score_semantics{"none"};
  std::string catalog_scope{"accepted"};
  bool semantic_selection_performed{};
  bool automatic_binding_created{};
};

struct LinguisticEntitySummary {
  LinguisticEntityKind kind{LinguisticEntityKind::Construction};
  std::string id;
  std::string name;
  std::string subtype;
  std::string description;
  std::string epistemic_status;
  std::string review_status;
};

struct LinguisticCatalogStatus {
  bool lexical_available{};
  bool constructions_available{};
  bool quarantine_available{};
  std::string quarantine_contract_version;
  std::size_t quarantine_fragment_candidates{};
  std::size_t quarantine_bound_senses{};
  std::size_t quarantine_unbound_fragments{};
  std::size_t quarantine_pending_hyphen_records{};
  bool definition_evidence_review_available{};
  std::size_t definition_evidence_review_clusters{};
  std::size_t definition_evidence_review_blocked_alignment{};
  std::size_t definition_evidence_review_blocked_atomicity{};
  std::size_t definition_evidence_review_blocked_families{};
  std::size_t definition_evidence_review_incomplete_maps{};
  std::size_t definition_evidence_review_cross_checks{};
  std::size_t definition_evidence_review_multi_upos_forms{};
  bool definition_promotion_available{};
  std::size_t definition_promoted_senses{};
  std::size_t definition_promoted_exact_collapses{};
  std::size_t definition_promoted_human_reviewed{};
  std::string lexical_message;
  std::string constructions_message;
  std::string quarantine_message;
  std::string definition_evidence_review_message;
  std::string definition_promotion_message;
};

[[nodiscard]] std::string display_name(LinguisticEntityKind kind);

} // namespace inde::project

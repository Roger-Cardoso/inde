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
  std::string definition_state;
  std::string review_status;
};

struct LexicalQueryResult {
  std::string query;
  std::vector<LexicalSenseCandidate> candidates;
  std::size_t total{};
  std::string score_semantics{"none"};
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
  std::string lexical_message;
  std::string constructions_message;
};

[[nodiscard]] std::string display_name(LinguisticEntityKind kind);

} // namespace inde::project

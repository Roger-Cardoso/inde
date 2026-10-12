#pragma once
#include "inde/application/document_edit_session.hpp"
#include "inde/project/narrative.hpp"
#include <set>
namespace inde::application {
struct TextRange {
  std::size_t start{}, end{};
};
[[nodiscard]] std::string selected_text(const project::Document &, TextRange);
[[nodiscard]] project::Document link_text_entity(project::Document, TextRange,
                                                 const std::string &entity_id,
                                                 const std::string &label,
                                                 std::string notes = {});
[[nodiscard]] project::Document materialize_entity(project::Document, TextRange,
                                                   const std::string &entity_id,
                                                   const std::string &purpose,
                                                   std::string note = {});
[[nodiscard]] std::set<std::string> entities_in_text(project::Document,
                                                     TextRange);
struct ProjectInlineQuery {
  std::string search;
  std::optional<std::string> type_key;
};
[[nodiscard]] std::optional<ProjectInlineQuery>
project_inline_query(const std::string &token);
struct DistributionRow {
  std::string key, label;
  std::size_t markings{}, characters{}, unresolved{}, incomplete{}, global{};
};
struct SemanticDistribution {
  TextRange scope;
  std::size_t characters{}, covered{}, overlapping{},
      native_entity_occurrences{}, annotation_relations{};
  std::set<std::string> entity_ids;
  std::vector<DistributionRow> rows;
};
[[nodiscard]] SemanticDistribution
semantic_distribution(project::Document,
                      const std::vector<project::AnnotationTypeDefinition> &,
                      TextRange);
} // namespace inde::application

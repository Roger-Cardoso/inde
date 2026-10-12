#pragma once
#include "inde/project/writing.hpp"
#include <filesystem>
namespace inde::persistence {
class SqliteDatabase;
struct SemanticUseQuery {
  std::optional<std::string> document_id, type_key, entity_id, category,
      target_annotation_id;
  std::optional<std::string> work_id;
  std::optional<std::string> annotation_id, exclude_document_id;
  bool include_archived{};
  std::size_t limit{50}, offset{};
  std::optional<project::AnnotationScope> annotation_scope;
  std::optional<std::string> fictional_axis_id, fictional_point_id;
};
struct SemanticUse {
  std::string annotation_id, document_id, document_title, anchor_id, type_key,
      status, category;
  std::size_t start{}, end{};
  project::AnnotationScope scope{project::AnnotationScope::Interval};
};
struct SemanticUsePage {
  std::vector<SemanticUse> rows;
  std::size_t total{};
};
enum class SemanticRelationDirection { Incoming, Outgoing };
struct SemanticRelationQuery {
  std::string annotation_id;
  SemanticRelationDirection direction{SemanticRelationDirection::Incoming};
  std::optional<std::string> exclude_source_document_id;
  std::size_t limit{24}, offset{};
};
struct SemanticRelationUse {
  std::string relation_id, type_key;
  SemanticUse source, target;
};
struct SemanticRelationPage {
  std::vector<SemanticRelationUse> rows;
  std::size_t total{};
};
class SemanticStore {
public:
  void validate(const std::filesystem::path &, const project::Document &) const;
  std::vector<project::AnnotationTypeDefinition>
  types(const std::filesystem::path &) const;
  void add_type(const std::filesystem::path &,
                const project::AnnotationTypeDefinition &) const;
  SemanticUsePage uses(const std::filesystem::path &,
                       const SemanticUseQuery &) const;
  SemanticRelationPage relations(const std::filesystem::path &,
                                 const SemanticRelationQuery &) const;
};
void load_semantic_layers(SqliteDatabase &, const std::string &owner,
                          project::Document &);
void save_semantic_layers(SqliteDatabase &, const std::string &owner,
                          const project::Document &);
} // namespace inde::persistence

#pragma once
#include <string>
namespace inde::persistence {
// Both projections read their own canonical payload; no duplicated binding.
inline std::string text_entity_usage_sql() {
  return "(SELECT project_id,document_id,entity_id FROM "
         "document_entity_references "
         "UNION SELECT b.project_id,b.document_id,b.entity_id FROM "
         "semantic_entity_bindings b "
         "JOIN semantic_annotations a ON a.project_id=b.project_id AND "
         "a.document_id=b.document_id "
         "AND a.id=b.annotation_id WHERE a.status!='archived')";
}
} // namespace inde::persistence

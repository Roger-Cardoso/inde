#pragma once

#include "inde/application/project_session.hpp"
#include "inde/persistence/writing_store.hpp"

namespace inde::application {

class WritingService {
public:
  WritingService(ProjectSession &session, persistence::WritingStore &store)
      : session_(session), store_(store) {}

  [[nodiscard]] std::vector<project::Document>
  documents(const persistence::DocumentQuery &query = {}) const;
  [[nodiscard]] std::vector<project::DocumentSummary>
  document_summaries(const persistence::DocumentQuery &query = {}) const;
  [[nodiscard]] std::size_t
  document_count(const persistence::DocumentQuery &query = {}) const;
  [[nodiscard]] std::optional<project::Document>
  document(const std::string &id) const;
  [[nodiscard]] std::vector<project::DocumentGroup> document_groups() const;
  [[nodiscard]] std::optional<project::DocumentGroup>
  document_group(const std::string &id) const;
  [[nodiscard]] project::DocumentGroup
  create_document_group(std::string name, std::string description = {});
  [[nodiscard]] project::DocumentGroup
  update_document_group(const project::DocumentGroup &input);
  void delete_document_group(const std::string &id);
  [[nodiscard]] project::Document
  create_document(std::string title,
                  std::optional<std::string> editorial_node_id = std::nullopt);
  [[nodiscard]] project::Document
  update_document(const project::Document &input);
  void delete_document(const std::string &id);

  [[nodiscard]] persistence::WritingStore::TextReferencePage text_references(
      const persistence::WritingStore::TextReferenceQuery &query) const;
  [[nodiscard]] project::DocumentTextReference
  add_text_reference(const std::string &source_document_id,
                     const std::string &target_document_id,
                     std::optional<std::string> target_anchor_id = std::nullopt,
                     std::string notes = {});
  void remove_text_reference(const std::string &source_document_id,
                             const std::string &id);

private:
  [[nodiscard]] const project::Project &require_project() const;
  void
  validate_placement(const std::optional<std::string> &editorial_node_id) const;
  void validate_entity_references(const project::Document &value) const;
  void validate_organization(const project::Document &value) const;

  ProjectSession &session_;
  persistence::WritingStore &store_;
};

} // namespace inde::application

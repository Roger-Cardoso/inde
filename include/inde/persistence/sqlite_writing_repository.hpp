#pragma once

#include "inde/persistence/writing_store.hpp"

namespace inde::persistence {

class SqliteWritingRepository final : public WritingStore {
public:
  [[nodiscard]] TextReferencePage
  text_references(const std::filesystem::path &project_path,
                  const TextReferenceQuery &query) const override;
  void add_text_reference(
      const std::filesystem::path &project_path,
      const project::DocumentTextReference &value) const override;
  void remove_text_reference(const std::filesystem::path &project_path,
                             const std::string &source_document_id,
                             const std::string &id) const override;
  void initialize(const std::filesystem::path &project_path) const override;
  [[nodiscard]] std::vector<project::Document>
  documents(const std::filesystem::path &project_path,
            const DocumentQuery &query = {}) const override;
  [[nodiscard]] std::vector<project::DocumentSummary>
  document_summaries(const std::filesystem::path &project_path,
                     const DocumentQuery &query = {}) const override;
  [[nodiscard]] std::size_t
  document_count(const std::filesystem::path &project_path,
                 const DocumentQuery &query = {}) const override;
  [[nodiscard]] std::optional<project::Document>
  document(const std::filesystem::path &project_path,
           const std::string &id) const override;
  [[nodiscard]] bool
  referenced_entity_exists(const std::filesystem::path &project_path,
                           const std::string &entity_id) const override;
  [[nodiscard]] std::vector<project::DocumentGroup>
  document_groups(const std::filesystem::path &project_path) const override;
  [[nodiscard]] std::optional<project::DocumentGroup>
  document_group(const std::filesystem::path &project_path,
                 const std::string &id) const override;
  void save_group(const std::filesystem::path &project_path,
                  const project::DocumentGroup &value) const override;
  void remove_group(const std::filesystem::path &project_path,
                    const std::string &id) const override;
  void save(const std::filesystem::path &project_path,
            const project::Document &value) const override;
  void remove(const std::filesystem::path &project_path,
              const std::string &id) const override;
};

} // namespace inde::persistence

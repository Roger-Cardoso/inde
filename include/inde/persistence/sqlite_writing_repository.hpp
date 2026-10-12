#pragma once

#include "inde/persistence/writing_store.hpp"
#include "inde/persistence/composition_store.hpp"
#include <functional>

namespace inde::persistence {

class SqliteWritingRepository final : public WritingStore {
public:
  // Context operations share the exact writing transaction, never two commits.
  static std::optional<project::Document> load_in_transaction(
      SqliteDatabase &, const std::string &owner, const std::string &id);
  void save_with_context(const std::filesystem::path &, const project::Document &,
      const project::Document *expected, const std::string &owner,
      const std::string &label, const std::function<void(SqliteDatabase &)> &) const;
  void save_composition_output(const std::filesystem::path &, const project::Document &,
                               const CompositionOutputGuard &) const;
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
  void save_if_unchanged(const std::filesystem::path &project_path,
      const project::Document &value, const project::Document &expected,
      const std::string &expected_project_id) const override;
  void remove(const std::filesystem::path &project_path,
              const std::string &id) const override;
private:
  void save_text_date(const std::filesystem::path &, const project::Document &,
      const project::Document &, const std::string &, const project::TextDateCreation &) const override;
  void save_text_entity(const std::filesystem::path &,const project::Document &,
      const project::Document &,const std::string &,const project::TextEntityCreation &) const override;
  void save_checked(const std::filesystem::path &, const project::Document &,
                    const project::Document *, const std::string &,
                    const project::TextEntityCreation * = nullptr,
                    const CompositionOutputGuard * = nullptr,
                    const project::TextDateCreation * = nullptr,
                    const std::string &label = {},
                    const std::function<void(SqliteDatabase &)> &context = {}) const;
};

} // namespace inde::persistence

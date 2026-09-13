#pragma once

#include "inde/project/writing.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace inde::persistence {

struct DocumentQuery {
  std::string search;
  std::optional<std::string> editorial_node_id;
  std::optional<std::string> entity_id;
  std::optional<std::string> group_id;
  std::optional<project::DocumentPurpose> purpose;
  std::string perspective;
  bool revisions_only{};
  bool ungrouped_only{};
  std::size_t limit{100};
  std::size_t offset{};
};

class WritingStore {
public:
  virtual ~WritingStore() = default;

  struct TextReferenceQuery {
    std::string document_id;
    bool incoming{};
    std::optional<std::string> target_anchor_id;
    std::size_t limit{50};
    std::size_t offset{};
  };
  struct TextReferencePage {
    std::vector<project::DocumentTextReferenceSummary> items;
    std::size_t total{};
  };
  [[nodiscard]] virtual TextReferencePage
  text_references(const std::filesystem::path &project_path,
                  const TextReferenceQuery &query) const = 0;
  virtual void
  add_text_reference(const std::filesystem::path &project_path,
                     const project::DocumentTextReference &value) const = 0;
  virtual void remove_text_reference(const std::filesystem::path &project_path,
                                     const std::string &source_document_id,
                                     const std::string &id) const = 0;

  virtual void initialize(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::vector<project::Document>
  documents(const std::filesystem::path &project_path,
            const DocumentQuery &query = {}) const = 0;
  [[nodiscard]] virtual std::vector<project::DocumentSummary>
  document_summaries(const std::filesystem::path &project_path,
                     const DocumentQuery &query = {}) const = 0;
  [[nodiscard]] virtual std::size_t
  document_count(const std::filesystem::path &project_path,
                 const DocumentQuery &query = {}) const = 0;
  [[nodiscard]] virtual std::optional<project::Document>
  document(const std::filesystem::path &project_path,
           const std::string &id) const = 0;
  [[nodiscard]] virtual bool
  referenced_entity_exists(const std::filesystem::path &project_path,
                           const std::string &entity_id) const = 0;
  [[nodiscard]] virtual std::vector<project::DocumentGroup>
  document_groups(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::optional<project::DocumentGroup>
  document_group(const std::filesystem::path &project_path,
                 const std::string &id) const = 0;
  virtual void save_group(const std::filesystem::path &project_path,
                          const project::DocumentGroup &value) const = 0;
  virtual void remove_group(const std::filesystem::path &project_path,
                            const std::string &id) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::Document &value) const = 0;
  virtual void remove(const std::filesystem::path &project_path,
                      const std::string &id) const = 0;
};

} // namespace inde::persistence

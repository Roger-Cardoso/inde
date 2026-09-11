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

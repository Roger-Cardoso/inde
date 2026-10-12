#pragma once
#include "inde/persistence/sqlite_writing_repository.hpp"
namespace inde::experimental {
// Prepared-copy facade: reads sources; explicit new-snapshot publishing.
class LabSource {
public:
  explicit LabSource(std::filesystem::path copy);
  std::vector<project::DocumentSummary> list(std::size_t offset) const;
  project::Document load(const std::string &id) const;
  std::size_t outgoing_references(const std::string &) const;
  project::Document create_snapshot(const project::Document &) const;
private:
  std::filesystem::path copy_;
  std::string manifest_at_open_;
  persistence::SqliteWritingRepository reader_;
};
}

#pragma once
#include "inde/project/project_operations.hpp"
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
struct sqlite3_session;
namespace inde::persistence {
class SqliteDatabase;
void validate_project_removal(SqliteDatabase &, project::RemovalKind,
                              const std::string &);
// Exists only inside an opted-in canonical transaction, not each text autosave.
class OperationCapture {
public:
  OperationCapture(SqliteDatabase &, std::string label,
                   bool compensation = false);
  ~OperationCapture();
  OperationCapture(const OperationCapture &) = delete;
  OperationCapture &operator=(const OperationCapture &) = delete;
  void persist();
  [[nodiscard]] std::vector<std::uint8_t> changes() const;

private:
  SqliteDatabase &database_;
  sqlite3_session *session_{};
  std::string label_;
  bool compensation_{};
};
class ProjectOperationStore {
public:
  [[nodiscard]] std::vector<project::ProjectOperation>
  list(const std::filesystem::path &, std::size_t offset = 0) const;
  [[nodiscard]] project::OperationPreview
  preview_removal(const std::filesystem::path &, project::RemovalKind,
                  const std::string &) const;
  [[nodiscard]] project::OperationPreview
  preview_reversal(const std::filesystem::path &, const std::string &) const;
  void execute(const std::filesystem::path &,
               const project::OperationPreview &) const;
};
} // namespace inde::persistence

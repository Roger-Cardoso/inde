#pragma once
#include "inde/project/document_recovery.hpp"
#include <memory>
#include <optional>
namespace inde::persistence {
// Local advisory lease, kept until the editing context is drained/released.
class RecoveryLease final {
public:
  RecoveryLease(const project::Project &, const std::string &session,
                bool create = true);
  ~RecoveryLease();
  RecoveryLease(const RecoveryLease &) = delete;
  RecoveryLease &operator=(const RecoveryLease &) = delete;

private:
  int descriptor_{-1};
};
class DocumentRecoveryStore final {
public:
  [[nodiscard]] static std::filesystem::path
  database_path(const project::Project &);
  [[nodiscard]] static bool inactive(const project::Project &,
                                     const std::string &session);
  void checkpoint(const project::Project &, project::RecoveryRecord) const;
  void acknowledge(const project::Project &, const std::string &session,
                   const project::Document &, std::uint64_t revision) const;
  [[nodiscard]] project::RecoveryPage
  pending(const project::Project &,
          const std::optional<std::string> &document = {}) const;
  [[nodiscard]] std::optional<project::RecoveryRecord>
  load(const project::Project &, const std::string &session) const;
  void resolve(const project::Project &, const std::string &session,
               const std::string &checksum,
               const std::string &resolution) const;
  [[nodiscard]] project::RecoveryRetentionPage retention(const project::Project &,
                                                        unsigned keep_days = 30) const;
  [[nodiscard]] std::filesystem::path export_record(const project::Project &,
      const std::string &session, const std::string &checksum) const;
  [[nodiscard]] static project::RecoveryRecord read_export(const std::filesystem::path &);
  // All-or-nothing removal of explicitly selected, inactive, closed records.
  // Exact versioned exports must already exist and validate for every record.
  std::size_t purge_exported(const project::Project &,
      const std::vector<project::RecoveryRetentionEntry> &, unsigned keep_days = 30) const;
};
} // namespace inde::persistence

#pragma once
#include "inde/application/writing_service.hpp"
#include "inde/persistence/document_recovery_store.hpp"
namespace inde::application {
struct RecoveryPreview {
  project::RecoveryRecord record;
  bool original_unchanged{}, already_persisted{};
  std::string explanation;
};
class DocumentRecoveryService final {
public:
  explicit DocumentRecoveryService(WritingService &w) : writing_(w) {}
  [[nodiscard]] RecoveryPreview inspect(const project::Project &,
                                        const std::string &session) const;
  [[nodiscard]] project::RecoveryState restore(const project::Project &,
                                               const std::string &session,
                                               const std::string &checksum);
  [[nodiscard]] project::Document create_copy(const project::Project &,
                                              const std::string &session,
                                              const std::string &checksum);
  void discard(const project::Project &, const std::string &session,
               const std::string &checksum);

private:
  project::RecoveryRecord checked(const project::Project &,
                                  const std::string &session,
                                  const std::string &checksum) const;
  WritingService &writing_;
  persistence::DocumentRecoveryStore store_;
};
} // namespace inde::application

#pragma once
#include "inde/persistence/document_recovery_store.hpp"
#include <chrono>
#include <functional>
namespace inde::application {
struct RecoverySnapshot {
  project::Project owner;
  project::RecoveryRecord record;
};
struct RecoveryResult {
  std::string session_id;
  std::uint64_t revision{};
  std::string error;
  std::chrono::nanoseconds elapsed{};
};
class DocumentRecoveryCoordinator final {
public:
  using Writer = std::function<void(const RecoverySnapshot &)>;
  explicit DocumentRecoveryCoordinator(Writer = {});
  ~DocumentRecoveryCoordinator();
  void begin(const project::Project &, const std::string &session);
  void submit(RecoverySnapshot);
  [[nodiscard]] std::optional<RecoveryResult> take_latest();
  [[nodiscard]] bool busy() const;
  void drain();
  void clear();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace inde::application

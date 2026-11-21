#pragma once
#include "object_change_session.hpp"
#include <filesystem>
#include <functional>
#include <stdexcept>

namespace inde::objects::planning::recoverylab {
enum class SourceKind { Document, Composition };
struct Scope {
  std::string project_id, source_id;
  SourceKind kind{SourceKind::Document};
  bool operator==(const Scope &) const = default;
};
enum class IoPoint {
  Write,
  FileSync,
  Publish,
  DirectorySync,
  Remove,
  RetentionSync
};
enum class PublishStage {
  BeforeCreate,
  Created,
  Written,
  FileSynced,
  Published,
  DirectorySynced
};
struct StoreOptions {
  std::size_t max_files{64};
  std::size_t max_bytes{256 * 1024 * 1024};
  // Laboratory seams: 0 executes the syscall; nonzero injects that errno.
  std::function<int(IoPoint)> inject_errno;
  std::function<void(PublishStage)> observe_stage;
};
class StoreError : public std::runtime_error {
public:
  StoreError(std::string code, std::string message);
  const std::string code;
};
struct Generation {
  std::uint64_t sequence{};
  std::string sha256;
  std::size_t file_bytes{};
  bool valid{};
  std::string diagnostic;
};
struct Inventory {
  std::vector<Generation>
      generations; // Ascending; invalid records are included.
  std::vector<std::string> incomplete; // Never recovered or removed implicitly.
  std::size_t file_bytes{};
};
struct PublishResult {
  bool visible{}, durable{};
  std::uint64_t sequence{};
  PublishStage stage{PublishStage::BeforeCreate};
  std::string diagnostic;
};
class CheckpointStore;
class RetentionPlan final {
public:
  RetentionPlan(const RetentionPlan &) = delete;
  RetentionPlan &operator=(const RetentionPlan &) = delete;
  [[nodiscard]] const std::vector<Generation> &remove() const {
    return remove_;
  }
  [[nodiscard]] bool consumed() const { return consumed_; }

private:
  RetentionPlan() = default;
  friend class CheckpointStore;
  std::string lease_id_, inventory_sha256_;
  std::vector<Generation> remove_;
  bool consumed_{};
};
struct RetentionResult {
  bool complete{};
  std::vector<std::uint64_t> removed;
  bool durability_unknown{};
  std::string diagnostic;
};
// Linux laboratory store, never opened implicitly by INDE or inside an author's
// project. create_new requires an absent dedicated directory and existing
// parent. Scope/marker/ownership/permissions and an exclusive advisory lease
// are required also for reads. No defaults pointing to project recovery, no
// SQLite/GTK hookup.
class CheckpointStore final {
public:
  static std::unique_ptr<CheckpointStore>
  create_new(const std::filesystem::path &, Scope, StoreOptions = {});
  static std::unique_ptr<CheckpointStore>
  open_existing(const std::filesystem::path &, Scope, StoreOptions = {});
  ~CheckpointStore();
  CheckpointStore(const CheckpointStore &) = delete;
  CheckpointStore &operator=(const CheckpointStore &) = delete;
  [[nodiscard]] const std::string &id() const;
  [[nodiscard]] const Scope &scope() const;
  [[nodiscard]] Inventory inventory() const;
  [[nodiscard]] SessionCheckpoint load(std::uint64_t sequence) const;
  // No automatic fallback if newest is corrupt. Choose an older ID explicitly.
  [[nodiscard]] SessionCheckpoint latest() const;
  [[nodiscard]] PublishResult publish(const SessionCheckpoint &);
  [[nodiscard]] std::unique_ptr<RetentionPlan>
  plan_retention(std::size_t keep_latest = 2) const;
  // Each deletion is directory-synced; failures report partial/unknown outcome.
  // A stale plan cannot delete records. At least the newest two remain
  // untouched.
  [[nodiscard]] RetentionResult apply_retention(RetentionPlan &);

private:
  CheckpointStore();
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace inde::objects::planning::recoverylab

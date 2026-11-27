#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace inde::persistence::json {
class Value;
}

namespace inde::project_file {
class OperationControl;
using Bytes = std::vector<std::uint8_t>;
struct Error : std::runtime_error {
  std::string code;
  Error(std::string code, std::string message);
};
struct Limits {
  std::size_t entries = 20000, states = 10000, versions = 100000;
  std::size_t history_entries = 1000000, references = 400000;
  std::size_t payload_bytes = 1024 * 1024, blob_bytes = 64 * 1024 * 1024;
  std::uint64_t store_bytes = 512ULL * 1024 * 1024;
};
struct Record {
  std::string key, kind, payload{"{}"}, blob;
  std::vector<std::string> references;
  std::int64_t version{};
};
struct State {
  std::string project_id, store_id, hash;
  std::int64_t sequence{};
  std::map<std::string, Record, std::less<>> records;
};
struct Edit {
  std::string key;
  // Zero means that the identity must be absent in the current state.
  std::int64_t expected_version{};
  std::optional<Record>
      value; // Absence removes the current use, not its history.
};
struct Blob {
  std::string hash;
  Bytes bytes;
};
struct Revision {
  std::int64_t sequence{};
  std::optional<std::int64_t> parent, restored_from;
  std::string hash, label, created_at;
};
enum class CommitStage { BeforeCommit, AfterCommit };
struct Options {
  Limits limits;
  // Fault/observation seam for tests; never read from a project file.
  std::function<void(CommitStage)> observe;
};
std::string sha256(const Bytes &);
std::string encode_json(const persistence::json::Value &);
persistence::json::Value parse_json(const std::string &,
                                    std::size_t limit = 1024 * 1024);
std::string canonical_json(const std::string &,
                           std::size_t limit = 1024 * 1024);

// New format, independent of the canonical schema 37. Never converts a folder
// implicitly. Every write checks an expected complete state and is one
// transaction.
class Store {
public:
  static Store create(const std::filesystem::path &,
                      const std::string &project_id, Options = {});
  static Store open(const std::filesystem::path &, Options = {},
                    OperationControl * = nullptr);
  Store(Store &&) noexcept;
  Store &operator=(Store &&) noexcept;
  ~Store();
  Store(const Store &) = delete;
  Store &operator=(const Store &) = delete;
  State head() const;
  State state(std::int64_t sequence) const;
  std::vector<Revision> history(OperationControl * = nullptr) const;
  Bytes blob(const std::string &hash) const;
  State apply(const State &expected, const std::vector<Edit> &,
              const std::vector<Blob> &, const std::string &label,
              OperationControl * = nullptr);
  State restore(const State &expected, std::int64_t sequence,
                const std::string &label, OperationControl * = nullptr);
  void verify(OperationControl * = nullptr) const;
  void copy_to(const std::filesystem::path &new_file,
               OperationControl * = nullptr) const;

private:
  struct Impl;
  explicit Store(std::unique_ptr<Impl>);
  std::unique_ptr<Impl> impl_;
};
} // namespace inde::project_file

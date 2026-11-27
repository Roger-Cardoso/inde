#pragma once
#include "project_file.hpp"
namespace inde::project_file {
enum class RecoveryChoice { Ask, Working, Saved };
struct RecoveryRequired : Error {
  std::filesystem::path working;
  bool file_advanced{};
  bool resumable{true};
  RecoveryRequired(std::filesystem::path, bool, bool = true);
};
// Persistent native workspace. Destruction never declares an unflushed editor
// clean. The caller must flush every writer before save/finish.
class FileSession {
public:
  static std::unique_ptr<FileSession>
  open(const std::filesystem::path &file,
       const std::filesystem::path &state_root,
       RecoveryChoice = RecoveryChoice::Ask, OperationControl * = nullptr);
  ~FileSession();
  const std::filesystem::path &file() const;
  const std::filesystem::path &working() const;
  const State &baseline() const;
  bool needs_reopen() const;
  State save(const std::string &label, OperationControl * = nullptr);
  void finish(OperationControl * = nullptr);
  std::vector<Revision> history(OperationControl * = nullptr) const;
  State restore(std::int64_t sequence, OperationControl * = nullptr);
  void copy_to(const std::filesystem::path &,
               OperationControl * = nullptr) const;
  static std::filesystem::path default_root();

private:
  struct Impl;
  explicit FileSession(std::unique_ptr<Impl>);
  std::unique_ptr<Impl> impl_;
};
// Compare a directory without publishing any state or changing the source.
bool directory_matches(Store &, const State &, const std::filesystem::path &,
                       const std::filesystem::path &file,
                       OperationControl * = nullptr);
} // namespace inde::project_file

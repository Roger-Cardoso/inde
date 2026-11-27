#pragma once
#include "project_file_operation.hpp"
#include <cstdint>
#include <exception>
#include <memory>
namespace inde::project_file {
class FileSession;
struct ConfirmationResult {
  bool cancelled{};
  std::int64_t sequence{};
  std::exception_ptr error;
};
// The session must outlive this object and remain exclusively assigned to its
// worker until ready(). No GTK objects or ProjectService cross this boundary.
class ConfirmationTask {
public:
  using Work = std::function<std::int64_t(OperationControl &)>;
  // Work owns its value captures; borrowed sessions remain exclusively reserved
  // by the owner until ready(). The completion handler runs on the owner
  // thread.
  explicit ConfirmationTask(Work, OperationControl::Observer = {});
  explicit ConfirmationTask(FileSession &, OperationControl::Observer = {});
  ~ConfirmationTask();
  bool cancel() noexcept;
  bool cancellable() const noexcept;
  OperationPhase phase() const noexcept;
  bool ready() const noexcept;
  ConfirmationResult result();
  void wait();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
// GTK supplies immutable work after its existing native writers have delivered
// their receipts. Waiting/cancellation never calls back into the UI thread.
class Preparation {
public:
  Preparation();
  ~Preparation();
  Preparation(const Preparation &) = delete;
  Preparation &operator=(const Preparation &) = delete;
  void provide(ConfirmationTask::Work);
  void fail(std::exception_ptr);
  std::int64_t run(OperationControl &);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace inde::project_file

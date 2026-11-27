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
} // namespace inde::project_file

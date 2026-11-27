#pragma once
#include <atomic>
#include <functional>
namespace inde::project_file {
enum class OperationPhase {
  Preparing,
  Objects,
  Reading,
  Checking,
  Writing,
  Committing,
  Finishing,
  Done
};
// One operation, one worker. Cancellation and the irreversible boundary race
// through a single atomic state; accepted cancellation cannot cross COMMIT.
class OperationControl {
public:
  using Observer = std::function<void(OperationPhase, OperationControl &)>;
  explicit OperationControl(Observer = {});
  bool cancel() noexcept;
  bool cancellable() const noexcept;
  bool cancelled() const noexcept;
  OperationPhase phase() const noexcept;
  void check() const;
  void checkpoint(OperationPhase);
  void seal();
  // Close a read-only result atomically, without claiming a file COMMIT.
  void complete();
  void report(OperationPhase);

private:
  enum class State { Open, Cancelled, Sealed };
  std::atomic<State> state_{State::Open};
  std::atomic<OperationPhase> phase_{OperationPhase::Reading};
  Observer observer_;
};
} // namespace inde::project_file

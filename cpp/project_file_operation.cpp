#include "project_file_operation.hpp"
#include "project_file.hpp"
namespace inde::project_file {
OperationControl::OperationControl(Observer observer)
    : observer_(std::move(observer)) {}
bool OperationControl::cancel() noexcept {
  auto expected = State::Open;
  return state_.compare_exchange_strong(expected, State::Cancelled);
}
bool OperationControl::cancellable() const noexcept {
  return state_.load() == State::Open;
}
bool OperationControl::cancelled() const noexcept {
  return state_.load() == State::Cancelled;
}
OperationPhase OperationControl::phase() const noexcept {
  return phase_.load();
}
void OperationControl::check() const {
  if (cancelled())
    throw Error("operation_cancelled",
                "Operação cancelada antes da confirmação. O trabalho foi "
                "preservado.");
}
void OperationControl::checkpoint(OperationPhase phase) {
  check();
  report(phase);
  check();
}
void OperationControl::report(OperationPhase phase) {
  phase_.store(phase);
  if (observer_)
    observer_(phase, *this);
}
void OperationControl::complete() {
  auto expected = State::Open;
  if (!state_.compare_exchange_strong(expected, State::Sealed))
    check();
  report(OperationPhase::Done);
}
void OperationControl::seal() {
  auto expected = State::Open;
  if (!state_.compare_exchange_strong(expected, State::Sealed)) {
    check();
    if (expected != State::Sealed)
      throw Error("operation_state", "Estado de confirmação inválido.");
  }
  report(OperationPhase::Committing);
}
} // namespace inde::project_file

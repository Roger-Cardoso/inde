#include "project_file_task.hpp"
#include "project_file_session.hpp"
#include <thread>
namespace inde::project_file {
struct ConfirmationTask::Impl {
  OperationControl control;
  ConfirmationResult result;
  std::atomic<bool> ready{};
  std::thread worker;
  explicit Impl(Work work, OperationControl::Observer observer)
      : control(std::move(observer)) {
    worker = std::thread([this, work = std::move(work)] {
      try {
        result.sequence = work(control);
        control.complete();
      } catch (const Error &error) {
        if (error.code == "operation_cancelled" && control.cancelled())
          result.cancelled = true;
        else
          result.error = std::current_exception();
      } catch (...) {
        result.error = std::current_exception();
      }
      // Observers are a test seam, not a completion callback into an owner.
      ready.store(true, std::memory_order_release);
    });
  }
};
ConfirmationTask::ConfirmationTask(FileSession &s, OperationControl::Observer o)
    : ConfirmationTask(
          [&s](OperationControl &control) {
            s.save("Confirmar trabalho no editor", &control);
            control.report(OperationPhase::Finishing);
            s.finish();
            return s.baseline().sequence;
          },
          std::move(o)) {}
ConfirmationTask::ConfirmationTask(Work work, OperationControl::Observer o)
    : impl_(std::make_unique<Impl>(std::move(work), std::move(o))) {}
ConfirmationTask::~ConfirmationTask() {
  cancel();
  wait();
}
bool ConfirmationTask::cancel() noexcept { return impl_->control.cancel(); }
bool ConfirmationTask::cancellable() const noexcept {
  return impl_->control.cancellable();
}
OperationPhase ConfirmationTask::phase() const noexcept {
  return impl_->control.phase();
}
bool ConfirmationTask::ready() const noexcept {
  return impl_->ready.load(std::memory_order_acquire);
}
void ConfirmationTask::wait() {
  if (impl_->worker.joinable())
    impl_->worker.join();
}
ConfirmationResult ConfirmationTask::result() {
  if (!ready())
    throw Error("operation_busy", "A confirmação ainda está em andamento.");
  wait();
  return impl_->result;
}
} // namespace inde::project_file

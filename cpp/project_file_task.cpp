#include "project_file_task.hpp"
#include "project_file_session.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
namespace inde::project_file {
struct Preparation::Impl {
  std::mutex mutex;
  std::condition_variable changed;
  bool supplied{};
  ConfirmationTask::Work work;
  std::exception_ptr error;
};
Preparation::Preparation() : impl_(std::make_unique<Impl>()) {}
Preparation::~Preparation() = default;
void Preparation::provide(ConfirmationTask::Work work) {
  std::lock_guard lock(impl_->mutex);
  if (impl_->supplied || !work)
    throw Error("preparation_state", "Preparação já entregue ou vazia.");
  impl_->work = std::move(work);
  impl_->supplied = true;
  impl_->changed.notify_all();
}
void Preparation::fail(std::exception_ptr error) {
  std::lock_guard lock(impl_->mutex);
  if (impl_->supplied || !error)
    throw Error("preparation_state", "Resultado de preparação inválido.");
  impl_->error = std::move(error);
  impl_->supplied = true;
  impl_->changed.notify_all();
}
std::int64_t Preparation::run(OperationControl &control) {
  control.checkpoint(OperationPhase::Preparing);
  ConfirmationTask::Work work;
  std::exception_ptr error;
  {
    std::unique_lock lock(impl_->mutex);
    while (!impl_->supplied) {
      control.check();
      impl_->changed.wait_for(lock, std::chrono::milliseconds(10));
    }
    work = std::move(impl_->work);
    error = impl_->error;
  }
  control.check();
  if (error)
    std::rethrow_exception(error);
  return work(control);
}
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
bool ConfirmationTask::cancel() noexcept {
  return !ready() && impl_->control.cancel();
}
bool ConfirmationTask::cancellable() const noexcept {
  return !ready() && impl_->control.cancellable();
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

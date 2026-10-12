#include "inde/application/document_recovery_coordinator.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
namespace inde::application {
struct DocumentRecoveryCoordinator::Impl {
  Writer write;
  mutable std::mutex mutex;
  std::condition_variable wake, settled;
  std::optional<RecoverySnapshot> pending;
  std::optional<RecoveryResult> ready;
  std::unique_ptr<persistence::RecoveryLease> lease;
  std::string owner_id, session_id, document_id;
  std::filesystem::path path;
  bool running{}, stopping{};
  std::thread worker;
  explicit Impl(Writer w) : write(std::move(w)) {
    if (!write)
      write = [](const RecoverySnapshot &s) {
        persistence::DocumentRecoveryStore{}.checkpoint(s.owner, s.record);
      };
    worker = std::thread([this] {
      for (;;) {
        std::unique_lock lock(mutex);
        wake.wait(lock, [this] { return pending || stopping; });
        if (!pending && stopping)
          return;
        auto s = std::move(*pending);
        pending.reset();
        running = true;
        lock.unlock();
        RecoveryResult r{s.record.session_id, s.record.revision, {}, {}};
        const auto start = std::chrono::steady_clock::now();
        try {
          write(s);
        } catch (const std::exception &e) {
          r.error = e.what();
        } catch (...) {
          r.error = "Falha desconhecida no journal";
        }
        r.elapsed = std::chrono::steady_clock::now() - start;
        lock.lock();
        ready = std::move(r);
        running = false;
        settled.notify_all();
      }
    });
  }
  ~Impl() {
    {
      std::lock_guard lock(mutex);
      stopping = true;
    }
    wake.notify_one();
    worker.join();
  }
};
DocumentRecoveryCoordinator::DocumentRecoveryCoordinator(Writer w)
    : impl_(std::make_unique<Impl>(std::move(w))) {}
DocumentRecoveryCoordinator::~DocumentRecoveryCoordinator() = default;
void DocumentRecoveryCoordinator::begin(const project::Project &owner,
                                        const std::string &session) {
  clear();
  auto lease = std::make_unique<persistence::RecoveryLease>(owner, session);
  std::lock_guard lock(impl_->mutex);
  impl_->owner_id = owner.manifest().project_id;
  impl_->path = owner.path();
  impl_->session_id = session;
  impl_->lease = std::move(lease);
}
void DocumentRecoveryCoordinator::submit(RecoverySnapshot s) {
  std::lock_guard lock(impl_->mutex);
  if (!impl_->lease || impl_->session_id != s.record.session_id ||
      impl_->owner_id != s.owner.manifest().project_id ||
      impl_->path != s.owner.path() ||
      (!impl_->document_id.empty() &&
       impl_->document_id != s.record.state.document.id))
    throw std::runtime_error(
        "Snapshot de recuperação pertence a outro contexto");
  impl_->document_id = s.record.state.document.id;
  impl_->pending = std::move(s);
  impl_->wake.notify_one();
}
std::optional<RecoveryResult> DocumentRecoveryCoordinator::take_latest() {
  std::lock_guard lock(impl_->mutex);
  auto r = std::move(impl_->ready);
  impl_->ready.reset();
  return r;
}
bool DocumentRecoveryCoordinator::busy() const {
  std::lock_guard lock(impl_->mutex);
  return impl_->running || impl_->pending || impl_->ready;
}
void DocumentRecoveryCoordinator::drain() {
  std::unique_lock lock(impl_->mutex);
  impl_->settled.wait(lock,
                      [this] { return !impl_->running && !impl_->pending; });
}
void DocumentRecoveryCoordinator::clear() {
  drain();
  std::lock_guard lock(impl_->mutex);
  impl_->ready.reset();
  impl_->lease.reset();
  impl_->session_id.clear();
  impl_->document_id.clear();
}
} // namespace inde::application

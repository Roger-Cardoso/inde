#include "inde/application/document_save_coordinator.hpp"
#include "inde/application/writing_service.hpp"
#include "inde/persistence/sqlite_writing_repository.hpp"
#include "inde/persistence/document_recovery_store.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace inde::application {
struct DocumentSaveCoordinator::Impl {
  Writer write;
  mutable std::mutex mutex;
  std::condition_variable wake, settled;
  std::optional<DocumentSaveSnapshot> pending;
  std::optional<DocumentSaveResult> ready;
  std::optional<project::Document> persisted;
  std::string owner_id, document_id;
  std::filesystem::path path;
  std::uint64_t epoch{};
  std::uint64_t persisted_revision{};
  bool running{}, stopping{}, has_context{};
  std::thread worker; // Join before releasing owned writer/synchronization.

  explicit Impl(Writer writer) : write(std::move(writer)) {
    if (!write) write = [](const DocumentSaveSnapshot &snapshot) {
      ProjectSession session;
      session.begin(snapshot.owner, {}, snapshot.structural_nodes);
      persistence::SqliteWritingRepository store;
      WritingService service(session, store);
      return service.update_document_if_unchanged(snapshot.document, snapshot.expected);
    };
    worker = std::thread([this] {
      for (;;) {
        std::unique_lock lock(mutex);
        wake.wait(lock, [this] { return pending || stopping; });
        if (!pending && stopping) return;
        auto snapshot = std::move(*pending); pending.reset(); running = true;
        if (persisted) { snapshot.expected = *persisted; snapshot.durable_revision=persisted_revision; }
        lock.unlock();
        DocumentSaveResult result;
        result.project_id=snapshot.owner.manifest().project_id; result.document_id=snapshot.document.id;
        result.epoch=snapshot.epoch; result.revision=snapshot.revision;
        const auto started = std::chrono::steady_clock::now();
        std::optional<project::Document> saved;
        persistence::DocumentRecoveryStore journal;
        if(!snapshot.recovery_session.empty()) try {
          journal.checkpoint(snapshot.owner,{snapshot.owner.manifest().project_id,snapshot.recovery_session,snapshot.revision,snapshot.durable_revision,
            {snapshot.document,snapshot.cursor,snapshot.bound},snapshot.expected,"pending",{}, {}});
        } catch(const std::exception &e) { result.journal_error=e.what(); }
        try { saved = write(snapshot); }
        catch (const std::exception &error) { result.error = error.what(); }
        catch (...) { result.error = "Falha desconhecida ao salvar o Documento"; }
        if(saved && !snapshot.recovery_session.empty()) try {
          journal.acknowledge(snapshot.owner,snapshot.recovery_session,*saved,snapshot.revision);
        } catch(const std::exception &e) { result.journal_error=e.what(); }
        result.elapsed = std::chrono::steady_clock::now() - started;
        lock.lock();
        if (saved) { persisted = std::move(saved); persisted_revision=snapshot.revision; }
        result.persisted = persisted;
        result.persisted_revision=persisted_revision;
        ready = std::move(result);
        running = false;
        settled.notify_all();
      }
    });
  }
  ~Impl() {
    { std::lock_guard lock(mutex); stopping = true; }
    wake.notify_one(); worker.join(); // Pending writes finish; never drop them.
  }
};
DocumentSaveCoordinator::DocumentSaveCoordinator(Writer writer) : impl_(std::make_unique<Impl>(std::move(writer))) {}
DocumentSaveCoordinator::~DocumentSaveCoordinator() = default;
void DocumentSaveCoordinator::submit(DocumentSaveSnapshot snapshot) {
  const auto &owner = snapshot.owner.manifest().project_id;
  const auto &path = snapshot.owner.path();
  std::lock_guard lock(impl_->mutex);
  if (impl_->has_context && (impl_->owner_id != owner || impl_->document_id != snapshot.document.id ||
                            impl_->path != path || impl_->epoch != snapshot.epoch))
    throw std::runtime_error("Conclua/limpe o salvamento antes de trocar o contexto de edição");
  impl_->owner_id = owner; impl_->document_id = snapshot.document.id;
  impl_->path = path; impl_->epoch = snapshot.epoch; impl_->has_context = true;
  impl_->pending = std::move(snapshot);
  impl_->wake.notify_one();
}
std::optional<DocumentSaveResult> DocumentSaveCoordinator::take_latest() {
  std::lock_guard lock(impl_->mutex);
  auto result = std::move(impl_->ready); impl_->ready.reset(); return result;
}
bool DocumentSaveCoordinator::busy() const {
  std::lock_guard lock(impl_->mutex);
  return impl_->running || impl_->pending.has_value() || impl_->ready.has_value();
}
void DocumentSaveCoordinator::drain() {
  std::unique_lock lock(impl_->mutex);
  impl_->settled.wait(lock, [this] { return !impl_->running && !impl_->pending; });
}
void DocumentSaveCoordinator::clear() {
  drain();
  std::lock_guard lock(impl_->mutex);
  impl_->ready.reset(); impl_->persisted.reset(); impl_->persisted_revision=0; impl_->has_context = false;
}
}

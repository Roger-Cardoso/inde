#include "inde/application/text_analysis_coordinator.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace inde::application {

struct TextAnalysisCoordinator::Impl {
  struct Job {
    std::uint64_t generation;
    TextAnalysisSnapshot snapshot;
  };
  Analyzer analyze;
  mutable std::mutex mutex;
  std::condition_variable_any wake;
  std::uint64_t generation{};
  std::optional<Job> pending;
  std::optional<TextAnalysisResult> ready;
  std::stop_source active_stop;
  bool running{};
  // Declared last: join before releasing the analyzer and synchronized state.
  std::jthread worker;

  explicit Impl(Analyzer analyzer) : analyze(std::move(analyzer)) {
    if (!analyze)
      analyze = [](const TextAnalysisSnapshot &snapshot, std::stop_token stop) {
        TextAnalysisResult result;
        result.statistics = project::analyze_document_text(snapshot.text, 200,
                                                           120, stop);
        return result;
      };
    worker = std::jthread([this](std::stop_token lifetime) {
      for (;;) {
        std::unique_lock lock(mutex);
        if (!wake.wait(lock, lifetime, [this] { return pending.has_value(); }))
          return;
        auto job = std::move(*pending);
        pending.reset();
        active_stop = std::stop_source{};
        const auto cancellation = active_stop.get_token();
        running = true;
        lock.unlock();
        const auto start = std::chrono::steady_clock::now();
        TextAnalysisResult result;
        try {
          result = analyze(job.snapshot, cancellation);
        } catch (const std::exception &error) {
          result.error = error.what();
        } catch (...) {
          result.error = "Falha desconhecida na análise de texto";
        }
        result.elapsed = std::chrono::steady_clock::now() - start;
        result.generation = job.generation;
        result.document_id = std::move(job.snapshot.document_id);
        lock.lock();
        running = false;
        if (!lifetime.stop_requested() && !cancellation.stop_requested() &&
            job.generation == generation)
          ready = std::move(result);
      }
    });
  }

  ~Impl() {
    {
      std::lock_guard lock(mutex);
      active_stop.request_stop();
      pending.reset();
      worker.request_stop();
    }
    wake.notify_all();
    worker.join();
  }

  void invalidate_locked() {
    ++generation;
    active_stop.request_stop();
    pending.reset();
    ready.reset();
  }
};

TextAnalysisCoordinator::TextAnalysisCoordinator(Analyzer analyzer)
    : impl_(std::make_unique<Impl>(std::move(analyzer))) {}
TextAnalysisCoordinator::~TextAnalysisCoordinator() = default;

void TextAnalysisCoordinator::invalidate() {
  std::lock_guard lock(impl_->mutex);
  impl_->invalidate_locked();
}

std::uint64_t TextAnalysisCoordinator::submit(TextAnalysisSnapshot snapshot) {
  std::uint64_t generation;
  {
    std::lock_guard lock(impl_->mutex);
    impl_->invalidate_locked();
    generation = impl_->generation;
    impl_->pending = Impl::Job{generation, std::move(snapshot)};
  }
  impl_->wake.notify_one();
  return generation;
}

std::optional<TextAnalysisResult> TextAnalysisCoordinator::take_latest() {
  std::lock_guard lock(impl_->mutex);
  auto result = std::move(impl_->ready);
  impl_->ready.reset();
  return result;
}

bool TextAnalysisCoordinator::busy() const {
  std::lock_guard lock(impl_->mutex);
  // A result can arrive between take_latest() and this query. Keep the owner
  // polling until that result has been consumed, not merely until work ends.
  return impl_->running || impl_->pending.has_value() || impl_->ready.has_value();
}

} // namespace inde::application

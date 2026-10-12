#include "inde/application/text_analysis_coordinator.hpp"
#include "inde/application/proofreading_service.hpp"

#include <cassert>
#include <condition_variable>
#include <future>
#include <iostream>
#include <thread>

using namespace inde::application;
using namespace std::chrono_literals;

namespace {
TextAnalysisResult await_result(TextAnalysisCoordinator &worker) {
  const auto limit = std::chrono::steady_clock::now() + 3s;
  while (std::chrono::steady_clock::now() < limit) {
    if (auto result = worker.take_latest())
      return std::move(*result);
    std::this_thread::sleep_for(1ms);
  }
  throw std::runtime_error("Analysis timeout");
}
struct Gate {
  std::promise<void> entered;
  std::mutex mutex;
  std::condition_variable release;
  bool open{};
  void wait() {
    entered.set_value();
    std::unique_lock lock(mutex);
    release.wait(lock, [&] { return open; });
  }
  void finish() {
    std::lock_guard lock(mutex);
    open = true;
    release.notify_all();
  }
};
class Spelling final : public SpellingProvider {
public:
  bool available() const noexcept override { return true; }
  std::string description() const override { return "Test"; }
  bool check(const std::string &word) const override { return word != "caza" && word != "Caza"; }
  std::vector<std::string> suggest(const std::string &, std::size_t) const override {
    return {"casa"};
  }
};
class Store final : public inde::persistence::DictionaryStore {
public:
  std::vector<inde::project::LexicalDictionary> load() const override { return {}; }
  void save(const std::vector<inde::project::LexicalDictionary> &) const override {}
};
}

int main() {
  {
    const std::string unicode = "Ação e café. 👩‍💻\nAção ação ação — café!";
    TextAnalysisCoordinator worker;
    worker.submit({"document-a", unicode, {}});
    const auto result = await_result(worker);
    const auto expected = inde::project::analyze_document_text(unicode);
    assert(result.document_id == "document-a" && result.statistics);
    assert(result.statistics->words == expected.words);
    assert(result.statistics->characters_with_spaces == expected.characters_with_spaces);
    assert(result.statistics->paragraphs == expected.paragraphs);
    assert(result.statistics->repeated_words.size() == expected.repeated_words.size());
    assert(!worker.take_latest());
    assert(!worker.busy());
    std::cout << "PASS unicode_statistics_parity\n";
  }
  {
    Gate gate;
    auto entered = gate.entered.get_future();
    std::vector<std::string> processed;
    TextAnalysisCoordinator worker([&](const auto &snapshot, auto) {
      if (snapshot.text == "running") gate.wait(); // Intentionally ignores cancellation.
      processed.push_back(snapshot.text);
      TextAnalysisResult result;
      result.error = snapshot.text;
      return result;
    });
    worker.submit({"old-document", "running", {}});
    assert(entered.wait_for(3s) == std::future_status::ready);
    worker.submit({"old-document", "pending-dropped", {}});
    const auto generation = worker.submit({"new-document", "latest", {}});
    gate.finish();
    auto result = await_result(worker);
    assert(result.generation == generation && result.document_id == "new-document");
    assert(result.error == "latest");
    // take_latest synchronizes the producer's writes through the mutex.
    assert((processed == std::vector<std::string>{"running", "latest"}));
    std::cout << "PASS bounded_queue_and_noncooperative_stale_discard\n";
  }
  {
    std::promise<void> entered, cancelled;
    auto entry = entered.get_future();
    auto cancellation = cancelled.get_future();
    TextAnalysisCoordinator worker([&](const auto &, std::stop_token stop) {
      entered.set_value();
      while (!stop.stop_requested()) std::this_thread::sleep_for(1ms);
      cancelled.set_value();
      return TextAnalysisResult{};
    });
    worker.submit({"a", "obsolete", {}});
    assert(entry.wait_for(3s) == std::future_status::ready);
    worker.invalidate(); // Invalidate during debounce; no replacement submitted.
    assert(cancellation.wait_for(3s) == std::future_status::ready);
    while (worker.busy()) std::this_thread::sleep_for(1ms);
    assert(!worker.take_latest());
    std::cout << "PASS immediate_edit_invalidation\n";
  }
  {
    TextAnalysisCoordinator worker([](const auto &snapshot, auto) {
      if (snapshot.text == "fail") throw std::runtime_error("expected error");
      return TextAnalysisResult{};
    });
    worker.submit({"a", "fail", {}});
    assert(await_result(worker).error == "expected error");
    worker.submit({"a", "recover", {}});
    assert(await_result(worker).error.empty());
    std::cout << "PASS exception_isolation_and_recovery\n";
  }
  {
    std::promise<void> entered;
    auto entry = entered.get_future();
    std::atomic<bool> stopped{};
    {
      TextAnalysisCoordinator worker([&](const auto &, std::stop_token stop) {
        entered.set_value();
        while (!stop.stop_requested()) std::this_thread::sleep_for(1ms);
        stopped = true;
        return TextAnalysisResult{};
      });
      worker.submit({"a", "close", {}});
      assert(entry.wait_for(3s) == std::future_status::ready);
    }
    assert(stopped);
    std::cout << "PASS destruction_cancels_and_joins\n";
  }
  {
    Store store;
    Spelling spelling;
    ProofreadingService service(store, spelling);
    const auto empty = service.vocabulary_snapshot();
    auto dictionary = service.create_dictionary("Teste").id;
    service.add_word(dictionary, "caza");
    const auto included = service.vocabulary_snapshot();
    service.set_dictionary_enabled(dictionary, false);
    const auto disabled = service.vocabulary_snapshot();
    assert(empty->empty() && included->contains("caza") && disabled->empty());
    ProofreadingAnalyzer analyzer(spelling);
    assert(analyzer.analyze("Caza.", *included).empty());
    assert(!analyzer.analyze("Caza.", *disabled).empty());
    std::stop_source cancellation;
    cancellation.request_stop();
    assert(analyzer.analyze("caza", *disabled, 300, cancellation.get_token()).empty());
    assert(inde::project::analyze_document_text("Ação", 200, 120,
                                               cancellation.get_token()).words == 0);
    std::cout << "PASS immutable_vocabulary_and_cooperative_analysis\n";
  }
  std::cout << "PASS all text-analysis coordinator tests\n";
}

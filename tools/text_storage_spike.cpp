#include "inde/experimental/text_buffer.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace inde;
using namespace inde::experimental;
using Clock = std::chrono::steady_clock;
using Object = persistence::json::Value::Object;
using Array = persistence::json::Value::Array;
namespace {
template <class F> double measure(F action) {
  auto start = Clock::now(); action();
  return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
}
Object distribution(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  auto at = [&](double p) { return values.at(static_cast<std::size_t>(std::ceil(p * values.size())) - 1); };
  return {{"unit", "microseconds"}, {"samples", static_cast<std::int64_t>(values.size())},
          {"p50", at(.50)}, {"p95", at(.95)}, {"p99", at(.99)}, {"max", values.back()}};
}
std::string corpus(int words) {
  std::string value;
  for (int n = 0; n < words / 5; ++n) value += "Água caminho história memória mundo.\n";
  return value;
}
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
Object checkpoint(TextBackend backend, const TextBuffer &buffer, const std::filesystem::path &path) {
  persistence::SqliteDatabase db(path);
  db.execute("CREATE TABLE benchmark_body(id INTEGER PRIMARY KEY,body TEXT NOT NULL)");
  db.execute("CREATE TABLE benchmark_chunks(position INTEGER PRIMARY KEY,body TEXT NOT NULL)");
  const auto body = buffer.text(); const auto chunks = buffer.fragments();
  std::vector<double> times;
  for (int i = 0; i < 5; ++i) times.push_back(measure([&] {
    persistence::SqliteTransaction transaction(db);
    if (backend == TextBackend::ImmutableChunks) {
      // Deliberately full checkpoint, not a promised incremental chunk store.
      db.execute("DELETE FROM benchmark_chunks");
      auto insert = db.prepare("INSERT INTO benchmark_chunks VALUES(?,?)");
      for (std::size_t j = 0; j < chunks.size(); ++j) {
        insert.bind(1, static_cast<std::int64_t>(j)); insert.bind(2, chunks[j]); insert.run(); insert.reset();
      }
    } else {
      auto flat = buffer.text(); // Hybrid still has to flatten for canonical storage.
      auto insert = db.prepare("INSERT OR REPLACE INTO benchmark_body VALUES(1,?)");
      insert.bind(1, flat); insert.run();
    }
    transaction.commit();
  }));
  std::string restored;
  if (backend == TextBackend::ImmutableChunks) {
    auto read = db.prepare("SELECT body FROM benchmark_chunks ORDER BY position");
    while (read.step()) restored += read.column_text(0);
  } else restored = db.query_text("SELECT body FROM benchmark_body WHERE id=1");
  require(restored == body, "Checkpoint changed Unicode/content");
  require(db.query_text("PRAGMA integrity_check") == "ok", "SQLite integrity failure");
  auto result = distribution(times);
  result["sqlite_bytes"] = static_cast<std::int64_t>(std::filesystem::file_size(path));
  result["rows"] = static_cast<std::int64_t>(backend == TextBackend::ImmutableChunks ? chunks.size() : 1);
  result["experimental_not_project_schema"] = true; result["full_checkpoint_not_incremental"] = true;
  return result;
}
Object run(TextBackend backend, const std::string &body, int samples, const std::filesystem::path &temporary) {
  std::map<std::string, std::vector<double>> timing;
  for (int n = 0; n < samples; ++n) timing["construct"].push_back(measure([&] {
    TextBuffer candidate(backend, body); require(candidate.bytes() == body.size(), "Wrong initial size");
  }));
  TextBuffer buffer(backend, body); std::vector<TextBuffer> history{buffer}; std::string expected = body;
  for (auto workload : {"insert_begin", "insert_middle", "insert_end"}) {
    for (int n = 0; n < samples; ++n) {
      std::size_t point = std::string(workload) == "insert_begin" ? 0 :
                          std::string(workload) == "insert_end" ? expected.size() : expected.size() / 2;
      while (point < expected.size() && (static_cast<unsigned char>(expected[point]) & 0xc0) == 0x80) --point;
      timing[workload].push_back(measure([&] { buffer.replace(point, point, "é🙂"); }));
      expected.insert(point, "é🙂"); history.push_back(buffer);
    }
  }
  require(buffer.text() == expected, "Backends disagree after typing");
  const auto paste = corpus(100000);
  for (int n = 0; n < 7; ++n) {
    auto candidate = buffer;
    timing["paste_100000_words"].push_back(measure([&] { candidate.replace(0, 0, paste); }));
    require(candidate.text() == paste + expected, "Paste fidelity failure");
  }
  for (int n = 0; n < samples; ++n) {
    timing["materialize"].push_back(measure([&] { require(buffer.text() == expected, "Materialization changed text"); }));
    timing["search_with_materialization"].push_back(measure([&] {
      require(buffer.text().find("história") != std::string::npos, "Search missed Unicode");
    }));
    timing["snapshot_undo"].push_back(measure([&] { auto previous = history.front(); require(previous.bytes() == body.size(), "Undo size"); }));
  }
  project::Document d; d.id = project::new_uuid(); d.title = "Synthetic benchmark";
  d.content = body; d.created_at = d.updated_at = project::utc_now();
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 4}};
  d.anchors = {{project::new_uuid(), "Beginning", 0, 4}};
  using K = project::SemanticFieldKind;
  d = application::TextSemanticService::create_document_perspective(d,
      {{"person", {K::Enum, {"third_person"}}}, {"focalization", {K::Enum, {"external"}}}});
  application::DocumentEditState state{d, {0, 0}};
  application::DocumentEditSession session; session.open(state);
  TextBuffer integrated(backend, body);
  for (int n = 0; n < 12; ++n) {
    timing["canonical_metadata_transaction_and_history"].push_back(measure([&] {
      auto next = application::replace_selection(state, 0, 0, "é🙂");
      auto delta = application::text_delta(state.document.content, next.document.content);
      integrated.replace(delta.byte_offset, delta.byte_offset + delta.removed.size(), delta.inserted);
      require(integrated.text() == next.document.content, "Canonical bridge changed text");
      require(session.commit(next), "History transaction absent"); state = std::move(next);
    }));
  }
  const auto annotations = state.document.semantics.annotations.size();
  require(annotations == 1 && state.document.semantics.annotations[0].scope == project::AnnotationScope::Document,
          "Global semantic declaration changed");
  auto undone = session.undo(); require(undone.has_value(), "Canonical undo absent");
  require(session.redo()->document.content == state.document.content, "Canonical redo changed content");
  Object result{{"backend", name(backend)}, {"bytes", static_cast<std::int64_t>(buffer.bytes())},
    {"units", static_cast<std::int64_t>(buffer.units())},
    {"deduplicated_backing_bytes_with_snapshots", static_cast<std::int64_t>(TextBuffer::retained_bytes(history))},
    {"snapshots", static_cast<std::int64_t>(history.size())},
    {"canonical_history_accounted_bytes", static_cast<std::int64_t>(session.history_text_bytes())},
    {"content_and_unicode_equal", true}, {"canonical_history_and_semantics_preserved", true},
    {"sqlite_full_checkpoint", checkpoint(backend, buffer, temporary / (std::string(name(backend)) + ".sqlite3"))}};
  Object times; for (const auto &[key, values] : timing) times[key] = distribution(values);
  result["timings"] = std::move(times); return result;
}
}
int main(int argc, char **argv) {
  try {
    int words = argc > 1 ? std::stoi(argv[1]) : 100000;
    int samples = argc > 2 ? std::stoi(argv[2]) : 40;
    if (argc > 3 || words < 1000 || words > 500000 || words % 5 || samples < 10 || samples > 100)
      throw std::runtime_error("Usage: inde_text_storage_spike [words 1000..500000 multiple of 5] [samples 10..100]");
    const auto root = std::filesystem::temp_directory_path() / ("inde-text-storage-spike-" + project::new_uuid());
    require(std::filesystem::create_directory(root), "Cannot create private benchmark folder");
    const auto body = corpus(words); Array results;
    for (auto backend : {TextBackend::Contiguous, TextBackend::ImmutableChunks, TextBackend::PieceTable})
      results.emplace_back(run(backend, body, samples, root));
    Object report{{"complete", true}, {"synthetic_words", words}, {"samples", samples},
      {"temporary_sqlite_folder", root.string()}, {"results", std::move(results)},
      {"gtk_ime_certification", false}, {"production_storage_changed", false},
      {"scope", "volatile backends, full experimental checkpoints, existing canonical metadata bridge; no rope balance or incremental persistence claim"}};
    std::cout << persistence::json::serialize(report) << '\n'; return 0;
  } catch (const std::exception &error) { std::cerr << "FAIL spike: " << error.what() << '\n'; return 1; }
}

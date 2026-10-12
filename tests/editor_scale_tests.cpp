#include "inde/application/document_save_coordinator.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/text_clipboard.hpp"
#include "inde/application/project_service.hpp"
#include "inde/project/document_state.hpp"
#include "inde/persistence/sqlite_writing_repository.hpp"
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

using namespace inde::application;
using namespace inde::project;
namespace {
Document doc(std::string text) {
  Document d; d.id = new_uuid(); d.title = "QA escala"; d.content = std::move(text);
  d.created_at = d.updated_at = utc_now(); return d;
}
template<class F> void rejects(F f) {
  bool caught{}; try { f(); } catch (const std::exception &) { caught = true; }
  assert(caught);
}
DocumentSaveSnapshot snapshot(Document d, std::uint64_t revision) {
  Manifest m; m.project_id = "qa-owned-project";
  return {Project("/tmp/inde-save-fake.inde", m), {}, d, d, 7, revision, {}, 0, 0, 0};
}
void bounded_worker() {
  auto d = doc("base"); auto a = snapshot(d, 1); a.document.content = "first";
  std::mutex mutex; std::condition_variable cv; bool entered{}, release{};
  std::vector<DocumentSaveSnapshot> writes;
  const auto caller = std::this_thread::get_id();
  DocumentSaveCoordinator worker([&](const auto &s) {
    assert(std::this_thread::get_id() != caller);
    std::unique_lock lock(mutex); writes.push_back(s);
    if (writes.size() == 1) { entered = true; cv.notify_all(); cv.wait(lock, [&] { return release; }); }
    if (s.revision == 4) throw std::runtime_error("disk failure");
    auto saved = s.document; saved.updated_at = "durable-" + std::to_string(s.revision); return saved;
  });
  worker.submit(a);
  { std::unique_lock lock(mutex); assert(cv.wait_for(lock, std::chrono::seconds(5), [&] { return entered; })); }
  auto b = a; b.revision = 2; b.document.content = "obsolete"; worker.submit(b);
  b.revision = 3; b.document.content = "latest"; worker.submit(b);
  auto foreign = b; foreign.epoch++; rejects([&] { worker.submit(foreign); });
  foreign = b; foreign.document.id = new_uuid(); rejects([&] { worker.submit(foreign); });
  assert(worker.busy());
  { std::lock_guard lock(mutex); release = true; } cv.notify_all();
  worker.drain(); auto result = worker.take_latest();
  assert(result && result->revision == 3 && result->error.empty() && result->persisted->content == "latest");
  assert(writes.size() == 2 && writes[0].revision == 1 && writes[1].revision == 3);
  assert(writes[1].expected.content == "first" && writes[1].expected.updated_at == "durable-1");
  assert(!worker.busy());
  b.revision = 4; b.document.content = "failed"; worker.submit(b); worker.drain(); result = worker.take_latest();
  assert(result && result->error == "disk failure" && result->persisted->content == "latest");
  worker.clear(); worker.submit(foreign); worker.drain(); result = worker.take_latest(); assert(result);
  std::cout << "CHECK: worker owns values, bounded latest queue, rebased durable state, failure, context gates\n";
}
void destructor_drains() {
  int count{};
  { DocumentSaveCoordinator w([&](const auto &s) { ++count; return s.document; }); w.submit(snapshot(doc("pending"), 1)); }
  assert(count == 1);
  auto a = snapshot(doc("baseline"), 1); a.document.content = "durable-before-failure";
  DocumentSaveCoordinator w([](const auto &s) {
    if (s.revision == 2) throw std::runtime_error("latest failed");
    if (s.revision == 3) assert(s.expected.content == "durable-before-failure");
    return s.document;
  });
  w.submit(a); w.drain(); // Do not consume the success before the later failure.
  a.revision = 2; a.document.content = "failed"; w.submit(a); w.drain(); auto result = w.take_latest();
  assert(result && result->revision == 2 && !result->error.empty() && result->persisted->content == "durable-before-failure");
  a.revision = 3; a.document.content = "retry"; w.submit(a); w.drain(); result = w.take_latest();
  assert(result && result->error.empty() && result->persisted->content == "retry");
}
void late_ack_and_delta() {
  DocumentEditState a{doc("aá🙂fim"), {0, 0}}, b = a, c = a;
  b.document.content = "aé🙂fim"; c.document.content = "aé🙂fim!";
  DocumentEditSession session; session.open(a); assert(session.commit(b)); assert(session.commit(c));
  session.acknowledge_saved(b.document); assert(session.dirty());
  assert(session.undo()->document.content == b.document.content && !session.dirty());
  assert(session.redo()->document.content == c.document.content && session.dirty());
  session.note_mutation(); session.acknowledge_saved(c.document); assert(session.pending() && session.dirty());
  auto foreign = c.document; foreign.id = new_uuid(); rejects([&] { session.acknowledge_saved(foreign); });
  for (const auto &x : {"", "a", "á", "🙂", "abc🙂fim", "aé🙂fim", "aá🙂fim!"}) {
    auto change = text_delta(a.document.content, x);
    auto restored = a.document.content;
    restored.replace(change.byte_offset, change.removed.size(), change.inserted);
    assert(restored == x);
    auto reverse = text_delta(x, a.document.content); restored.replace(reverse.byte_offset, reverse.removed.size(), reverse.inserted);
    assert(restored == a.document.content);
  }
  rejects([] { static_cast<void>(text_delta("valid", std::string("\xff", 1))); });
  std::cout << "CHECK: old acknowledgement cannot mark newer edits saved; UTF-8 incremental deltas\n";
}
void clipboard_contract() {
  auto source = doc("Aá🙂BCD"); source.formatting = {{DocumentTextStyle::Bold, 0, 4}, {DocumentTextStyle::Italic, 3, 6}};
  auto anchor = new_uuid(); source.anchors = {{anchor, "Private anchor", 1, 4}};
  source.entity_references = {{new_uuid(), new_uuid(), anchor, "Private notes"}};
  auto fragment = copy_fragment(source, 1, 5, true);
  assert(fragment.text == "á🙂BC" && fragment.formatting.size() == 2);
  assert(fragment.formatting[0].start_offset == 0 && fragment.formatting[0].end_offset == 3);
  auto encoded = encode_fragment(fragment); assert(encoded.find(anchor) == std::string::npos && encoded.find("Private") == std::string::npos);
  auto roundtrip = decode_fragment(encoded); assert(roundtrip.text == fragment.text && roundtrip.formatting.size() == 2);
  assert(copy_fragment(source, 1, 5, false).formatting.empty());
  DocumentEditState destination{doc("xy"), {1, 1}};
  auto plain = paste_fragment(destination, 1, 1, fragment, false);
  assert(plain.document.content == "xá🙂BCy" && plain.document.formatting.empty());
  assert(plain.document.anchors.empty() && plain.document.entity_references.empty());
  auto styled = paste_fragment(destination, 1, 1, fragment, true);
  assert(styled.document.formatting.size() == 2 && styled.document.formatting[0].start_offset == 1 && styled.selection.cursor == 5);
  DocumentEditSession session; session.open(destination); assert(session.commit(styled, "Colar com estilos"));
  assert(session.undo()->document.content == "xy"); assert(session.redo()->document.content == styled.document.content);
  destination.document.formatting = {{DocumentTextStyle::Underline, 0, 2}};
  destination.document.anchors = {{anchor, "Destination anchor", 0, 2}};
  destination.document.entity_references = source.entity_references;
  styled = paste_fragment(destination, 0, 2, fragment, true);
  assert(styled.document.anchors[0].id == anchor && styled.document.entity_references[0].id == source.entity_references[0].id);
  assert(styled.document.formatting.size() == 2);
  for (const auto &bad : {"{}", "{\"version\":999,\"text\":\"x\",\"formatting\":[]}",
      "{\"version\":1,\"text\":\"x\",\"formatting\":[],\"bindings\":[]}",
      "{\"version\":1,\"text\":\"x\",\"formatting\":[{\"style\":\"bold\",\"start\":-1,\"end\":1}]}",
      "{\"version\":1,\"text\":\"x\",\"formatting\":[{\"style\":\"bold\",\"start\":0,\"end\":2}]}",
      "{\"version\":1,\"text\":\"x\",\"formatting\":[[[[[[]]]]]]}",
      "{\"version\":1,\"version\":1,\"text\":\"x\",\"formatting\":[]}"}) rejects([&] { static_cast<void>(decode_fragment(bad)); });
  rejects([&] { static_cast<void>(paste_fragment(destination, 0, 0, {std::string("\xff", 1), {}}, false)); });
  rejects([&] { static_cast<void>(paste_fragment(destination, 0, 0, {std::string("a\0b", 3), {}}, false)); });
  rejects([] { static_cast<void>(encode_fragment({std::string(clipboard_limit + 1, 'a'), {}})); });
  rejects([&] { static_cast<void>(copy_fragment(source, 0, 100, false)); });
  auto equal = source; std::reverse(equal.formatting.begin(), equal.formatting.end());
  assert(same_document_state(source, equal)); equal.content += "!"; assert(!same_document_state(source, equal));
  std::cout << "CHECK: plain/styled clipboard, Unicode/layers, no bindings, bounded hostile payload, one-step undo\n";
}
void optimistic_sqlite() {
  const auto root = std::filesystem::temp_directory_path() / ("inde-scale-" + new_uuid());
  const auto path = root / "fixture.inde";
  struct ConfigGuard {
    std::optional<std::string> previous;
    explicit ConfigGuard(const std::filesystem::path &config) {
      if (const char *value = std::getenv("XDG_CONFIG_HOME")) previous = value;
      if (setenv("XDG_CONFIG_HOME", config.c_str(), 1)) throw std::runtime_error("Cannot isolate test config");
    }
    ~ConfigGuard() { if (previous) setenv("XDG_CONFIG_HOME", previous->c_str(), 1); else unsetenv("XDG_CONFIG_HOME"); }
  } config(root / "config");
  ProjectService service; service.create(path, "QA optimistic save");
  auto initial = service.writing().create_document("Atomic"); initial.content = "base"; initial = service.writing().update_document(initial);
  initial.formatting = {{DocumentTextStyle::Bold, 0, 2}, {DocumentTextStyle::Italic, 2, 4}};
  const auto anchor = new_uuid(); initial.anchors = {{anchor, "Protected anchor", 1, 2}};
  initial = service.writing().update_document(initial);
  auto edited = initial; edited.content = "next";
  DocumentSaveCoordinator worker; worker.submit(service.writing().save_snapshot(edited, initial, 1, 1)); worker.drain();
  auto result = worker.take_latest(); assert(result && result->error.empty() && result->persisted->content == "next");
  const auto durable = *result->persisted;
  auto external = durable; external.content = "external"; external = service.writing().update_document(external);
  edited = durable; edited.content = "local";
  worker.submit(service.writing().save_snapshot(edited, durable, 1, 2)); worker.drain(); result = worker.take_latest();
  assert(result && !result->error.empty() && result->persisted->content == "next");
  assert(service.writing().document(initial.id)->content == "external");
  inde::persistence::SqliteWritingRepository repository;
  rejects([&] { repository.save_if_unchanged(path, edited, external, "wrong project"); });
  auto reversed = external; std::reverse(reversed.formatting.begin(), reversed.formatting.end());
  auto reordered = external; reordered.title = "Order independent";
  repository.save_if_unchanged(path, reordered, reversed, service.current()->manifest().project_id);
  assert(service.writing().document(initial.id)->title == reordered.title);
  auto invalid = reordered; invalid.content = "attempted overwrite";
  invalid.entity_references = {{new_uuid(), new_uuid(), std::nullopt, "Missing FK"}};
  rejects([&] { repository.save_if_unchanged(path, invalid, reordered, service.current()->manifest().project_id); });
  assert(same_document_state(*service.writing().document(initial.id), reordered));
  auto source = service.writing().create_document("Incoming protected reference");
  const auto incoming = service.writing().add_text_reference(source.id, initial.id, anchor, "Explicit cross-document use");
  worker.clear(); auto stripped = reordered; stripped.anchors.clear();
  worker.submit(service.writing().save_snapshot(stripped, reordered, 2, 1)); worker.drain(); result = worker.take_latest();
  assert(result && !result->error.empty() && same_document_state(*service.writing().document(initial.id), reordered));
  assert(service.writing().text_references({initial.id, true, anchor, 1, 0}).items[0].reference.id == incoming.id);
  service.writing().delete_document(source.id);
  worker.clear(); service.writing().delete_document(initial.id);
  worker.submit(service.writing().save_snapshot(edited, durable, 2, 3)); worker.drain(); result = worker.take_latest();
  assert(result && !result->error.empty() && !result->persisted && !service.writing().document(initial.id));
  service.close(); std::filesystem::remove_all(root); // Only UUID-owned project and its isolated test config.
  std::cout << "CHECK: independent SQLite connection, atomic same-second conflict, owner identity, FK rollback, protected incoming anchor, no deleted-document resurrection\n";
}
}
int main() {
  bounded_worker(); destructor_drains(); late_ack_and_delta(); clipboard_contract(); optimistic_sqlite();
  std::cout << "PASS: editor scale contracts\n";
}

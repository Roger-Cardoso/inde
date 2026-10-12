#include "../src/persistence/recovery_codec.hpp"
#include "inde/application/document_recovery_coordinator.hpp"
#include "inde/application/document_recovery_service.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace inde;
namespace {
template <class F> void rejects(F f) {
  bool caught{};
  try {
    f();
  } catch (const std::exception &) {
    caught = true;
  }
  assert(caught);
}
project::RecoveryRecord record(const project::Project &owner,
                               project::Document baseline, std::string session,
                               std::uint64_t revision = 1) {
  auto state = baseline;
  state.content = "Água 🙂 checkpoint íntegro\ntexto autoral.";
  state.formatting = {{project::DocumentTextStyle::Bold, 0, 4}};
  return {owner.manifest().project_id,
          std::move(session),
          revision,
          0,
          {state, 7, 2},
          baseline,
          "pending",
          {},
          {}};
}
void codec_and_store(application::ProjectService &service) {
  auto baseline = service.writing().create_document("QA codec");
  baseline.content = "base text";
  baseline = service.writing().update_document(baseline);
  const auto owner = *service.current();
  const auto session = project::new_uuid();
  auto r = record(owner, baseline, session);
  r.state.document.title.clear();
  r.state.document.perspective = "Perspectiva \"autoral\" — 🙂";
  auto encoded = persistence::recovery::encode(r),
       hash = persistence::recovery::checksum(encoded);
  auto decoded = persistence::recovery::decode(encoded, hash);
  assert(
      project::same_document_state(r.state.document, decoded.state.document));
  assert(decoded.state.cursor == 7 && decoded.state.bound == 2);
  rejects([&] { persistence::recovery::decode(encoded + " ", hash); });
  rejects([&] {
    auto bad = r;
    bad.state.bound = 1000;
    persistence::recovery::encode(bad);
  });
  rejects([&] {
    auto bad = r;
    bad.session_id = "../escape";
    persistence::recovery::encode(bad);
  });
  const auto deep = std::string(100, '[') + std::string(100, ']');
  rejects([&] {
    persistence::recovery::decode(deep, persistence::recovery::checksum(deep));
  });
  persistence::DocumentRecoveryStore store;
  {
    persistence::RecoveryLease lease(owner, session);
    store.checkpoint(owner, r);
    assert(store.pending(owner).items.empty());
    rejects([&] {
      static_cast<void>(application::DocumentRecoveryService(service.writing())
                            .inspect(owner, session));
    });
  }
  auto loaded = store.load(owner, session);
  assert(loaded && loaded->state.document.title.empty() &&
         loaded->state.document.content == r.state.document.content);
  assert(store.pending(owner).items.size() == 1);
  // A draft with empty title is retained even though original persistence
  // rejects it.
  auto preview = application::DocumentRecoveryService(service.writing())
                     .inspect(owner, session);
  assert(preview.original_unchanged);
  rejects([&] {
    static_cast<void>(application::DocumentRecoveryService(service.writing())
                          .restore(owner, session, preview.record.checksum));
  });
  auto copy = application::DocumentRecoveryService(service.writing())
                  .create_copy(owner, session, preview.record.checksum);
  assert(copy.content == r.state.document.content && copy.anchors.empty() &&
         copy.entity_references.empty());
  assert(store.load(owner, session)->status == "copy-created" &&
         store.pending(owner).items.empty());
  service.writing().delete_document(copy.id);
  service.writing().delete_document(baseline.id);
  std::cout
      << "CHECK: strict versioned/checksummed UTF-8 checkpoint, draft "
         "metadata, active lease, explicit rescue copy, retained resolution\n";
}
void ordering_and_failures(application::ProjectService &service) {
  const auto owner = *service.current();
  auto base = service.writing().create_document("QA ordering");
  base.content = "base text";
  base = service.writing().update_document(base);
  const auto key = project::new_uuid();
  auto r = record(owner, base, key, 2);
  persistence::DocumentRecoveryStore store;
  {
    persistence::RecoveryLease lease(owner, key);
    store.checkpoint(owner, r);
    auto durable = base;
    durable.content = "older successful";
    store.acknowledge(owner, key, durable, 1);
    auto stale = r;
    stale.revision = 1;
    stale.state.document.content = "stale";
    stale.state.cursor = 0;
    store.checkpoint(owner, stale);
    auto value = *store.load(owner, key);
    assert(value.revision == 2 && value.baseline.content == durable.content &&
           value.status == "pending");
    store.checkpoint(owner, r);
    value = *store.load(owner, key);
    assert(value.durable_revision == 1 &&
           value.baseline.content == durable.content);
    store.acknowledge(owner, key, r.state.document, 2);
    assert(store.load(owner, key)->status == "clean");
  }
  // Journal fails closed on corruption; no rewrite of the damaged payload.
  const auto corrupt = project::new_uuid();
  auto bad = record(owner, base, corrupt);
  {
    persistence::RecoveryLease lease(owner, corrupt);
    store.checkpoint(owner, bad);
  }
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    auto q = db.prepare(
        "UPDATE recovery_records SET checksum='damaged' WHERE session_id=?");
    q.bind(1, corrupt);
    q.run();
  }
  rejects([&] { static_cast<void>(store.load(owner, corrupt)); });
  rejects([&] { store.checkpoint(owner, bad); });
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    auto q =
        db.prepare("SELECT checksum FROM recovery_records WHERE session_id=?");
    q.bind(1, corrupt);
    assert(q.step() && q.column_text(0) == "damaged");
  }
  auto next = base;
  next.content = "Normal save survives journal failure";
  auto snapshot = service.writing().save_snapshot(next, base, 1, 1);
  snapshot.recovery_session = corrupt;
  application::DocumentSaveCoordinator save;
  save.submit(snapshot);
  save.drain();
  auto result = save.take_latest();
  assert(result && result->error.empty() && !result->journal_error.empty() &&
         result->persisted->content == next.content);
  assert(service.writing().document(base.id)->content == next.content);
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    db.execute("PRAGMA user_version=999");
  }
  rejects([&] { static_cast<void>(store.pending(owner)); });
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    assert(db.query_integer("PRAGMA user_version") == 999);
    db.execute("PRAGMA user_version=1");
  }
  service.writing().delete_document(base.id);
  std::cout << "CHECK: monotonic revisions/ACK, no baseline rollback, "
               "corrupted evidence retained, normal save independent, "
               "incompatible version rejected\n";
}
void bounded_worker(application::ProjectService &service) {
  const auto owner = *service.current();
  const auto key = project::new_uuid();
  auto d = service.writing().create_document("QA queue");
  auto r = record(owner, d, key);
  std::mutex mutex;
  std::condition_variable cv;
  bool entered{}, release{};
  std::vector<std::uint64_t> written;
  application::DocumentRecoveryCoordinator worker([&](const auto &s) {
    std::unique_lock lock(mutex);
    written.push_back(s.record.revision);
    if (written.size() == 1) {
      entered = true;
      cv.notify_all();
      cv.wait(lock, [&] { return release; });
    }
  });
  worker.begin(owner, key);
  worker.submit({owner, r});
  {
    std::unique_lock lock(mutex);
    assert(cv.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }));
  }
  r.revision = 2;
  worker.submit({owner, r});
  r.revision = 3;
  worker.submit({owner, r});
  auto wrong = r;
  wrong.session_id = project::new_uuid();
  rejects([&] { worker.submit({owner, wrong}); });
  {
    std::lock_guard lock(mutex);
    release = true;
  }
  cv.notify_all();
  worker.drain();
  const auto result = worker.take_latest();
  assert(result && result->revision == 3 && result->error.empty() &&
         written == std::vector<std::uint64_t>({1, 3}));
  assert(!persistence::DocumentRecoveryStore::inactive(owner, key));
  worker.clear();
  assert(persistence::DocumentRecoveryStore::inactive(owner, key));
  service.writing().delete_document(d.id);
  std::cout << "CHECK: checkpoint worker bounded/coalesced, context gate, "
               "drain and lease release\n";
}
int child(int argc, char **argv) {
  if (argc != 7)
    return 2;
  application::ProjectService service;
  service.open(argv[2]);
  const auto owner = *service.current();
  const auto baseline = *service.writing().document(argv[4]);
  auto r = record(owner, baseline, argv[3]);
  persistence::RecoveryLease lease(owner, r.session_id);
  persistence::DocumentRecoveryStore store;
  store.checkpoint(owner, r);
  const std::string stage = argv[6];
  std::unique_ptr<persistence::SqliteDatabase> db;
  std::unique_ptr<persistence::SqliteTransaction> tx;
  if (stage == "after-main")
    static_cast<void>(service.writing().update_document_if_unchanged(
        r.state.document, baseline));
  if (stage == "partial-journal" || stage == "partial-main") {
    db = std::make_unique<persistence::SqliteDatabase>(
        stage == "partial-journal" ? store.database_path(owner)
                                   : owner.path() / "data/project.sqlite3");
    db->execute("PRAGMA cache_size=1");
    tx = std::make_unique<persistence::SqliteTransaction>(*db);
    auto q = db->prepare(
        stage == "partial-journal"
            ? "UPDATE recovery_records SET payload=zeroblob(2097152) WHERE "
              "session_id=?"
            : "UPDATE documents SET content=zeroblob(2097152) WHERE id=?");
    q.bind(1, stage == "partial-journal" ? r.session_id : baseline.id);
    q.run();
  }
  const int fd = std::stoi(argv[5]);
  const char ready = 'R';
  if (::write(fd, &ready, 1) != 1)
    return 3;
  for (;;)
    ::pause();
}
void interruptions(application::ProjectService &service) {
  const auto owner = *service.current();
  persistence::DocumentRecoveryStore store;
  application::DocumentRecoveryService recovery(service.writing());
  for (const auto &stage :
       {"checkpoint", "after-main", "partial-journal", "partial-main"}) {
    auto base =
        service.writing().create_document("QA SIGKILL " + std::string(stage));
    base.content = "base text";
    const auto anchor = project::new_uuid();
    base.anchors = {{anchor, "Explicit authorial anchor", 1, 3}};
    const auto types = service.narrative().entity_types();
    const auto entity =
        service.narrative().create_entity(types[0].id, "QA recovery entity").id;
    base.entity_references = {
        {project::new_uuid(), entity, anchor, "Retain authorial binding"}};
    base = service.writing().update_document(base);
    auto incoming = service.writing().create_document("QA incoming");
    const auto reference = service.writing().add_text_reference(
        incoming.id, base.id, anchor, "Keep incoming use");
    const auto session = project::new_uuid();
    int pipefd[2];
    assert(::pipe(pipefd) == 0);
    const auto pid = ::fork();
    assert(pid >= 0);
    if (pid == 0) {
      ::close(pipefd[0]);
      const auto fd = std::to_string(pipefd[1]);
      ::execl("/proc/self/exe", "inde_document_recovery_tests", "--child",
              owner.path().c_str(), session.c_str(), base.id.c_str(),
              fd.c_str(), stage, nullptr);
      ::_exit(127);
    }
    ::close(pipefd[1]);
    pollfd p{pipefd[0], POLLIN, 0};
    const auto ready = ::poll(&p, 1, 15000);
    char flag{};
    if (ready > 0)
      assert(::read(pipefd[0], &flag, 1) == 1 && flag == 'R');
    if (ready <= 0) {
      ::kill(pid, SIGKILL);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("Child checkpoint timed out");
    }
    assert(!store.inactive(owner, session));
    if (std::string(stage) == "checkpoint")
      rejects([&] { static_cast<void>(recovery.inspect(owner, session)); });
    assert(::kill(pid, SIGKILL) == 0);
    int status{};
    assert(::waitpid(pid, &status, 0) == pid && WIFSIGNALED(status) &&
           WTERMSIG(status) == SIGKILL);
    ::close(pipefd[0]);
    assert(store.inactive(owner, session));
    auto preview = recovery.inspect(owner, session);
    assert(preview.already_persisted == (std::string(stage) == "after-main"));
    auto restored = recovery.restore(owner, session, preview.record.checksum);
    assert(restored.document.content ==
               "Água 🙂 checkpoint íntegro\ntexto autoral." &&
           restored.cursor == 7 && restored.bound == 2);
    assert(restored.document.anchors[0].id == anchor &&
           restored.document.entity_references[0].id ==
               base.entity_references[0].id);
    assert(service.writing()
               .text_references({base.id, true, anchor, 1, 0})
               .items[0]
               .reference.id == reference.id);
    assert(store.load(owner, session)->status == "restored");
    service.writing().delete_document(incoming.id);
    service.writing().delete_document(base.id);
    service.narrative().delete_entity(entity);
    std::cout << "CHECK: real SIGKILL " << stage
              << ", deterministic snapshot, directed Unicode selection, "
                 "authorial layers/identity/incoming reference\n";
  }
}
void conflict_and_token(application::ProjectService &service) {
  const auto owner = *service.current();
  auto base = service.writing().create_document("QA conflict");
  base.content = "base text";
  base = service.writing().update_document(base);
  const auto session = project::new_uuid();
  auto r = record(owner, base, session);
  persistence::DocumentRecoveryStore store;
  {
    persistence::RecoveryLease lease(owner, session);
    store.checkpoint(owner, r);
  }
  application::DocumentRecoveryService recovery(service.writing());
  auto preview = recovery.inspect(owner, session);
  r.revision = 2;
  r.state.document.content += " latest";
  store.checkpoint(owner, r);
  rejects([&] {
    static_cast<void>(
        recovery.restore(owner, session, preview.record.checksum));
  });
  auto external = base;
  external.content = "external modification";
  external = service.writing().update_document(external);
  preview = recovery.inspect(owner, session);
  assert(!preview.original_unchanged && !preview.already_persisted);
  rejects([&] {
    static_cast<void>(
        recovery.restore(owner, session, preview.record.checksum));
  });
  assert(service.writing().document(base.id)->content == external.content);
  service.writing().delete_document(base.id);
  preview = recovery.inspect(owner, session);
  assert(!preview.original_unchanged);
  const auto copy =
      recovery.create_copy(owner, session, preview.record.checksum);
  assert(copy.id != base.id && copy.content == r.state.document.content &&
         copy.anchors.empty() && copy.entity_references.empty());
  assert(!service.writing().document(base.id));
  service.writing().delete_document(copy.id);
  std::cout
      << "CHECK: stale preview token rejected, external conflict protected, "
         "deleted original not resurrected, explicit independent rescue\n";
}
void partial_resolution(application::ProjectService &service) {
  const auto owner = *service.current();
  auto base = service.writing().create_document("QA second database failure");
  base.content = "base text";
  base = service.writing().update_document(base);
  const auto key = project::new_uuid();
  persistence::DocumentRecoveryStore store;
  auto r = record(owner, base, key);
  {
    persistence::RecoveryLease lease(owner, key);
    store.checkpoint(owner, r);
  }
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    db.execute(
        "CREATE TRIGGER qa_reject_resolution BEFORE UPDATE ON recovery_records "
        "WHEN NEW.status IN ('restored','copy-created') BEGIN SELECT "
        "RAISE(ABORT,'QA finalization failure'); END");
  }
  application::DocumentRecoveryService recovery(service.writing());
  auto preview = recovery.inspect(owner, key);
  bool precise{};
  try {
    static_cast<void>(recovery.restore(owner, key, preview.record.checksum));
  } catch (const std::exception &e) {
    precise = std::string(e.what()).find("Original já persistido") !=
              std::string::npos;
  }
  assert(precise &&
         service.writing().document(base.id)->content ==
             r.state.document.content &&
         store.load(owner, key)->status == "pending");
  preview = recovery.inspect(owner, key);
  assert(preview.already_persisted);
  const auto before = service.writing().documents().size();
  precise = false;
  try {
    static_cast<void>(
        recovery.create_copy(owner, key, preview.record.checksum));
  } catch (const std::exception &e) {
    precise = std::string(e.what()).find("Cópia de resgate já criada") !=
              std::string::npos;
  }
  assert(precise && service.writing().documents().size() == before + 1 &&
         store.load(owner, key)->status == "pending");
  {
    persistence::SqliteDatabase db(store.database_path(owner));
    db.execute("DROP TRIGGER qa_reject_resolution");
  }
  static_cast<void>(recovery.restore(owner, key, preview.record.checksum));
  assert(store.load(owner, key)->status == "restored");
  for (const auto &d : service.writing().documents())
    if (d.title == "Resgate — QA second database failure")
      service.writing().delete_document(d.id);
  service.writing().delete_document(base.id);
  std::cout << "CHECK: second-database failure reports already-persisted "
               "original/copy, pending evidence retained, explicit original "
               "finalization safe\n";
}
void bounded_store(application::ProjectService &service) {
  const auto owner = *service.current();
  auto base = service.writing().create_document("QA capacity");
  base.content = "base text";
  base = service.writing().update_document(base);
  const auto path = owner.path() / "qa-bounded-journal";
  std::filesystem::create_directory(path);
  const project::Project bounded(path, owner.manifest());
  persistence::DocumentRecoveryStore store;
  for (int i = 0; i < 66; ++i) {
    const auto key = project::new_uuid();
    persistence::RecoveryLease lease(bounded, key);
    store.checkpoint(bounded, record(bounded, base, key));
  }
  const auto page = store.pending(bounded);
  assert(page.items.size() == 64 && page.more);
  // max_page_count is connection-local: production verifies it on EVERY open.
  // An unrelated raw SQLite connection does not inherit that configuration.
  assert(store.load(bounded, page.items[0].session_id));
  const auto link_path = owner.path() / "qa-symbolic-journal";
  std::filesystem::create_directory(link_path);
  std::filesystem::create_directory_symlink(bounded.path() / "recovery",
                                            link_path / "recovery");
  const project::Project linked(link_path, owner.manifest());
  rejects([&] { static_cast<void>(store.pending(linked)); });
  rejects([&] {
    persistence::RecoveryLease lease(linked, page.items[0].session_id, false);
  });
  auto foreign = record(bounded, base, project::new_uuid());
  foreign.project_id = project::new_uuid();
  rejects([&] { store.checkpoint(bounded, foreign); });
  service.writing().delete_document(base.id);
  std::cout << "CHECK: discovery bounded to 64 plus more, SQLite 512 MiB hard "
               "page limit, symbolic paths and foreign project rejected\n";
}
} // namespace
int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "--child")
    return child(argc, argv);
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-recovery-" + project::new_uuid());
  const char *old = std::getenv("XDG_CONFIG_HOME");
  const auto previous = old ? std::optional<std::string>(old) : std::nullopt;
  assert(setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1) == 0);
  {
    application::ProjectService service;
    service.create(root / "fixture.inde", "QA recovery");
    codec_and_store(service);
    ordering_and_failures(service);
    bounded_worker(service);
    interruptions(service);
    conflict_and_token(service);
    partial_resolution(service);
    bounded_store(service);
    service.close();
  }
  std::filesystem::remove_all(
      root); // Exact UUID-owned test root, never user data.
  if (previous)
    setenv("XDG_CONFIG_HOME", previous->c_str(), 1);
  else
    unsetenv("XDG_CONFIG_HOME");
  std::cout << "PASS: journal/recovery contracts and four real process "
               "interruptions\n";
}

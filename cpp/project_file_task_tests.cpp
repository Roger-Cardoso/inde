#include "project_file_session.hpp"
#include "project_file_task.hpp"
#include "project_file_transport.hpp"
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <mutex>
#include <sqlite3.h>
#include <sys/stat.h>
#include <thread>
using namespace inde::project_file;
namespace fs = std::filesystem;
using namespace std::chrono_literals;
namespace {
void need(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
void sql(const fs::path &path, const std::string &sql) {
  sqlite3 *db{};
  need(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "Fixture open");
  const auto rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
  sqlite3_close(db);
  need(rc == SQLITE_OK, "Fixture SQL");
}
std::string read(const fs::path &p) {
  std::ifstream f(p);
  return {std::istreambuf_iterator<char>(f), {}};
}
struct Gate {
  OperationPhase phase;
  std::mutex mutex;
  std::condition_variable cv;
  bool entered{}, released{};
  explicit Gate(OperationPhase p) : phase(p) {}
  void observe(OperationPhase p, OperationControl &control) {
    if (p != phase)
      return;
    std::unique_lock lock(mutex);
    if (entered)
      return;
    entered = true;
    cv.notify_all();
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (!released) {
      control.check();
      need(std::chrono::steady_clock::now() < end, "Test gate timeout");
      cv.wait_for(lock, 5ms);
    }
  }
  void wait() {
    std::unique_lock lock(mutex);
    need(cv.wait_for(lock, 5s, [this] { return entered; }),
         "Worker never reached gate");
  }
  void release() {
    std::lock_guard lock(mutex);
    released = true;
    cv.notify_all();
  }
};
ConfirmationResult result(ConfirmationTask &task) {
  const auto end = std::chrono::steady_clock::now() + 10s;
  while (!task.ready()) {
    need(std::chrono::steady_clock::now() < end, "Worker did not finish");
    std::this_thread::sleep_for(2ms);
  }
  return task.result();
}
void verify_file(const fs::path &file, const fs::path &out, std::int64_t seq,
                 const std::string &expected) {
  auto store = Store::open(file);
  store.verify();
  need(store.head().sequence == seq, "Wrong published sequence");
  unpack_directory(store, seq, out);
  need(read(out / "nota.txt") == expected, "Wrong published bytes");
}
} // namespace
int main() {
  auto *id = g_uuid_string_random();
  const std::string owner(id);
  g_free(id);
  const auto root = fs::temp_directory_path() / ("inde-p5-task-" + owner);
  try {
    // Race the two operations at the exact atomic boundary, without SQLite or
    // GTK.
    for (int i = 0; i < 100; ++i) {
      OperationControl c;
      std::atomic<bool> start{}, accepted{}, sealed{};
      std::thread cancel([&] {
        while (!start.load())
          std::this_thread::yield();
        accepted = c.cancel();
      });
      std::thread commit([&] {
        while (!start.load())
          std::this_thread::yield();
        try {
          c.seal();
          sealed = true;
        } catch (const Error &e) {
          need(e.code == "operation_cancelled", "Wrong race error");
        }
      });
      start = true;
      cancel.join();
      commit.join();
      need(accepted.load() != sealed.load(),
           "Cancellation crossed commit boundary");
    }
    {
      auto pending = std::make_shared<Preparation>();
      std::atomic<bool> ran{};
      Gate preparing(OperationPhase::Preparing);
      ConfirmationTask task(
          [pending](auto &c) { return pending->run(c); },
          [&](auto phase, auto &c) { preparing.observe(phase, c); });
      preparing.wait();
      need(!ran && !task.ready(), "Preparation ran before delivery");
      pending->provide([&](auto &c) {
        c.check();
        ran = true;
        return 42;
      });
      preparing.release();
      const auto complete = result(task);
      need(!complete.error && !complete.cancelled && complete.sequence == 42 &&
               ran,
           "Preparation receipt not delivered");
      bool duplicate = false;
      try {
        pending->provide([](auto &) -> std::int64_t { return 0; });
      } catch (const Error &e) {
        duplicate = e.code == "preparation_state";
      }
      need(duplicate, "Preparation accepted duplicate delivery");
    }
    {
      auto pending = std::make_shared<Preparation>();
      std::atomic<bool> ran{};
      Gate preparing(OperationPhase::Preparing);
      ConfirmationTask task(
          [pending](auto &c) { return pending->run(c); },
          [&](auto phase, auto &c) { preparing.observe(phase, c); });
      preparing.wait();
      need(task.cancel(), "Cancel pending preparation refused");
      pending->provide([&](auto &) -> std::int64_t {
        ran = true;
        return 0;
      });
      need(result(task).cancelled && !ran,
           "Cancelled preparation executed work");
    }
    {
      auto pending = std::make_shared<Preparation>();
      ConfirmationTask task([pending](auto &c) { return pending->run(c); });
      pending->fail(std::make_exception_ptr(
          Error("native_flush_failed", "Preserve the workspace")));
      const auto failed = result(task);
      need(bool(failed.error) && !failed.cancelled, "Preparation failure lost");
      need(!task.cancel() && !task.cancellable(),
           "Completed failure accepted cancellation");
      try {
        std::rethrow_exception(failed.error);
      } catch (const Error &e) {
        need(e.code == "native_flush_failed",
             "Preparation changed native error");
      }
    }
    {
      auto pending = std::make_shared<Preparation>();
      Gate preparing(OperationPhase::Preparing);
      {
        ConfirmationTask task(
            [pending](auto &c) { return pending->run(c); },
            [&](auto phase, auto &c) { preparing.observe(phase, c); });
        preparing.wait();
      }
      // Destruction cancels/joins a waiting task without a GTK callback.
    }
    for (const auto profile :
         {TransportProfile::WholeFile, TransportProfile::Blocks}) {
      const auto dir = root / (profile == TransportProfile::Blocks ? "blocks"
                                                                   : "whole"),
                 source = dir / "source.inde", file = dir / "Projeto.inde",
                 state = dir / "state";
      fs::create_directories(source / "data");
      std::ofstream(source / "manifest.json")
          << "{\"format\":\"INDE\",\"format_version\":2,\"project_id\":\""
          << owner << "\"}";
      sql(source / "data/project.sqlite3",
          "CREATE TABLE projects(id TEXT);INSERT INTO projects VALUES('" +
              owner +
              "');CREATE TABLE schema_migrations(version INTEGER);INSERT INTO "
              "schema_migrations VALUES(37);");
      std::ofstream(source / "nota.txt") << "Original";
      const auto first = pack_directory(source, file, profile);
      {
        auto s = FileSession::open(file, state);
        std::ofstream(s->working() / "nota.txt") << "Trabalho preservado";
        Gate gate(OperationPhase::Reading);
        ConfirmationTask task(*s, [&](auto phase, auto &control) {
          gate.observe(phase, control);
        });
        gate.wait();
        need(!task.ready() && task.cancel(), "Early cancellation refused");
        auto r = result(task);
        need(r.cancelled && !r.error, "Cancellation not reported");
        need(s->baseline().sequence == first.sequence,
             "Cancelled task published state");
        need(read(s->working() / "nota.txt") == "Trabalho preservado",
             "Cancelled task lost work");
        Gate teardown(OperationPhase::Reading);
        {
          ConfirmationTask owned(*s, [&](auto phase, auto &control) {
            teardown.observe(phase, control);
          });
          teardown.wait();
        }
        need(s->baseline().sequence == first.sequence,
             "Destruction published work");
      }
      verify_file(file, dir / "Unchanged.inde", first.sequence, "Original");
      std::int64_t saved{};
      {
        auto s = FileSession::open(file, state, RecoveryChoice::Working);
        Gate gate(OperationPhase::Committing);
        ConfirmationTask task(*s, [&](auto phase, auto &control) {
          gate.observe(phase, control);
        });
        gate.wait();
        need(!task.cancellable() && !task.cancel(),
             "Late cancellation accepted");
        gate.release();
        const auto r = result(task);
        need(!r.error && !r.cancelled && r.sequence == first.sequence + 1,
             "Confirmation failed after sealing");
        saved = r.sequence;
        ConfirmationTask noop(*s);
        const auto n = result(noop);
        need(!n.error && !n.cancelled && n.sequence == saved,
             "No-op created revision");
        // A failed receipt AFTER publication must propagate as uncertainty, not
        // as a cancelled operation. The workspace and published state remain
        // intact.
        std::ofstream(s->working() / "nota.txt") << "Confirmado sem recibo";
        const auto parent = s->working().parent_path();
        need(chmod(parent.c_str(), 0500) == 0, "Set receipt failure");
        ConfirmationTask fault(*s);
        auto bad = result(fault);
        need(chmod(parent.c_str(), 0700) == 0, "Reset receipt permission");
        need(bool(bad.error) && !bad.cancelled && s->needs_reopen(),
             "Post-commit failure misreported");
        try {
          std::rethrow_exception(bad.error);
        } catch (const Error &e) {
          need(e.code == "session_uncertain", "Wrong receipt error");
        }
      }
      {
        auto s = FileSession::open(file, state);
        saved = s->baseline().sequence;
        ConfirmationTask t(*s);
        auto r = result(t);
        need(!r.error && !r.cancelled && r.sequence == saved,
             "Reopen duplicated commit");
      }
      verify_file(file, dir / "Confirmed.inde", saved, "Confirmado sem recibo");
      {
        // The same worker owns the session for every file operation. Nothing
        // reads or mutates it from the caller until the result is acquired.
        auto session = FileSession::open(file, state);
        const auto before = session->baseline();
        const auto work = session->working();
        Gate gate(OperationPhase::Writing);
        unsigned writing = 0;
        ConfirmationTask restore(
            [&](auto &control) {
              return session->restore(first.sequence, &control).sequence;
            },
            [&](auto phase, auto &control) {
              // First Writing materializes the reserved directory, second
              // Writing is the actual authoritative restore transaction.
              if (phase == OperationPhase::Writing && ++writing == 2)
                gate.observe(phase, control);
            });
        gate.wait();
        need(restore.cancel(), "Restore cancellation refused");
        const auto cancelled = result(restore);
        need(cancelled.cancelled && !cancelled.error &&
                 session->baseline().hash == before.hash &&
                 session->working() == work,
             "Cancelled restore changed active state");
        need(session->history().back().sequence == before.sequence,
             "Cancelled restore advanced history");
        Gate commit(OperationPhase::Committing);
        ConfirmationTask resumed(
            [&](auto &control) {
              return session->restore(first.sequence, &control).sequence;
            },
            [&](auto phase, auto &control) { commit.observe(phase, control); });
        commit.wait();
        need(!resumed.cancel(), "Restore accepted late cancellation");
        commit.release();
        const auto restored = result(resumed);
        need(!restored.error && !restored.cancelled &&
                 restored.sequence == before.sequence + 1,
             "Restore worker failed");
        need(session->working() != work && fs::exists(work) &&
                 read(session->working() / "nota.txt") == "Original",
             "Restore lost an area");
        need(session->history().back().restored_from == first.sequence,
             "Restore provenance missing");
        session->finish();
        Gate copy_gate(OperationPhase::Writing);
        const auto copy = dir / "Cancelled-copy.inde";
        ConfirmationTask copying(
            [&](auto &control) {
              session->copy_to(copy, &control);
              return 0;
            },
            [&](auto phase, auto &control) {
              copy_gate.observe(phase, control);
            });
        copy_gate.wait();
        need(copying.cancel(), "Copy cancel refused");
        need(result(copying).cancelled && !fs::exists(copy),
             "Cancelled copy published target");
        const auto complete = dir / "History-copy.inde";
        Gate publish(OperationPhase::Committing);
        ConfirmationTask copied(
            [&](auto &control) {
              session->copy_to(complete, &control);
              return 0;
            },
            [&](auto phase, auto &control) {
              publish.observe(phase, control);
            });
        publish.wait();
        need(!fs::exists(complete) && !copied.cancel(),
             "Copy seal boundary invalid");
        publish.release();
        need(!result(copied).error, "History copy failed");
        auto check = Store::open(complete);
        need(check.history().size() == session->history().size() &&
                 check.head().hash == session->baseline().hash,
             "History copy mismatch");
        const auto original_bytes = read(complete);
        ConfirmationTask collision([&](auto &control) {
          session->copy_to(complete, &control);
          return 0;
        });
        need(bool(result(collision).error) && read(complete) == original_bytes,
             "Copy replaced destination");
        auto rows = std::make_shared<std::vector<Revision>>();
        ConfirmationTask listing([&](auto &control) {
          *rows = session->history(&control);
          return 0;
        });
        need(!result(listing).error &&
                 rows->back().sequence == restored.sequence,
             "History worker failed");
        const auto prior_work = session->working(),
                   parent = prior_work.parent_path();
        const auto prior_sequence = session->baseline().sequence;
        ConfirmationTask receipt(
            [&](auto &control) {
              return session->restore(before.sequence, &control).sequence;
            },
            [&](auto phase, auto &) {
              if (phase == OperationPhase::Committing)
                need(chmod(parent.c_str(), 0500) == 0,
                     "Set restore receipt fault");
            });
        const auto uncertain = result(receipt);
        need(chmod(parent.c_str(), 0700) == 0, "Restore receipt permission");
        need(bool(uncertain.error) && !uncertain.cancelled &&
                 session->needs_reopen() && session->working() != prior_work &&
                 fs::exists(prior_work) &&
                 session->baseline().sequence == prior_sequence + 1,
             "Restore lost its post-commit uncertainty or working areas");
        try {
          std::rethrow_exception(uncertain.error);
        } catch (const Error &e) {
          need(e.code == "session_uncertain", "Wrong restore receipt error");
        }
      }
      {
        const auto target = dir / "Cancelled-pack.inde";
        Gate gate(OperationPhase::Writing);
        ConfirmationTask packing(
            [&](auto &control) {
              return pack_directory(source, target, profile, &control).sequence;
            },
            [&](auto phase, auto &control) { gate.observe(phase, control); });
        gate.wait();
        need(packing.cancel(), "Pack cancel refused");
        need(result(packing).cancelled && !fs::exists(target),
             "Cancelled pack published");
        const auto published = dir / "Published-pack.inde";
        Gate seal(OperationPhase::Committing);
        ConfirmationTask packed(
            [&](auto &control) {
              return pack_directory(source, published, profile, &control)
                  .sequence;
            },
            [&](auto phase, auto &control) { seal.observe(phase, control); });
        seal.wait();
        need(!fs::exists(published) && !packed.cancel(),
             "Temporary COMMIT sealed parent operation");
        seal.release();
        need(!result(packed).error, "Pack failed after publication seal");
        const auto cancelled_state = dir / "cancelled-open-state";
        Gate reading(OperationPhase::Checking);
        std::unique_ptr<FileSession> opened;
        ConfirmationTask opening(
            [&](auto &control) {
              opened = FileSession::open(published, cancelled_state,
                                         RecoveryChoice::Ask, &control);
              return opened->baseline().sequence;
            },
            [&](auto phase, auto &control) {
              reading.observe(phase, control);
            });
        reading.wait();
        need(opening.cancel(), "Open cancel refused");
        need(result(opening).cancelled && !opened,
             "Cancelled open changed binding");
        Gate binding(OperationPhase::Committing);
        ConfirmationTask bound(
            [&](auto &control) {
              opened = FileSession::open(published, dir / "opened-state",
                                         RecoveryChoice::Ask, &control);
              return opened->baseline().sequence;
            },
            [&](auto phase, auto &control) {
              binding.observe(phase, control);
            });
        binding.wait();
        need(!bound.cancel(), "Open binding accepted late cancel");
        binding.release();
        need(!result(bound).error && opened &&
                 read(opened->working() / "nota.txt") == "Original",
             "Opening worker failed");
      }
    }
    // Cancel after candidate rows have been written, before COMMIT. A rollback
    // must remove the candidate and retain every earlier state.
    OperationControl c;
    bool cancel_next = false;
    Options opts;
    opts.observe = [&](CommitStage stage) {
      if (cancel_next && stage == CommitStage::BeforeCommit)
        need(c.cancel(), "Cancel before commit refused");
    };
    {
      auto store = Store::create(root / "rollback.inde", owner, opts);
      Record a{"a", "text", "{}", "", {}, 0};
      const auto first =
          store.apply(store.head(), {{"a", 0, a}}, {}, "Inicial");
      a.payload = "{\"changed\":true}";
      cancel_next = true;
      bool cancelled = false;
      try {
        store.apply(first, {{"a", first.records.at("a").version, a}}, {},
                    "Cancelar candidato", &c);
      } catch (const Error &e) {
        cancelled = e.code == "operation_cancelled";
      }
      need(cancelled && store.head().hash == first.hash &&
               store.history().size() == 2,
           "Candidate was published on cancellation");
      store.verify();
    }
    {
      Gate done(OperationPhase::Done);
      ConfirmationTask read_only(
          [](OperationControl &) -> std::int64_t { return 7; },
          [&](auto phase, auto &control) { done.observe(phase, control); });
      done.wait();
      need(!read_only.cancel(), "Completed read accepted cancellation");
      done.release();
      need(result(read_only).sequence == 7, "Read-only completion failed");
      ConfirmationTask error([](OperationControl &) -> std::int64_t {
        throw Error("operation_cancelled",
                    "Injected error without cancellation");
      });
      const auto failed = result(error);
      need(bool(failed.error) && !failed.cancelled,
           "An error masqueraded as cancellation");
    }
    fs::remove_all(root);
    std::cout << "P5-D: worker ownership, cancellation race, rollback, sealed "
                 "commit, teardown, both profiles, "
                 "open/pack/copy/history/restore and uncertain receipt OK\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\nEvidence: " << root << '\n';
    return 1;
  }
}

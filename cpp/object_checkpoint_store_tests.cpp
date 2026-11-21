#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
#include "object_checkpoint_store.hpp"
#include "object_session_checkpoint.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace inde;
using namespace inde::objects;
using namespace inde::objects::planning;
using namespace inde::objects::planning::recoverylab;
namespace fs = std::filesystem;
namespace {
void require(bool yes, const char *why) {
  if (!yes)
    throw std::runtime_error(why);
}
void rejects(const std::function<void()> &fn, const std::string &code = {}) {
  bool refused{};
  try {
    fn();
  } catch (const StoreError &e) {
    refused = code.empty() || code == e.code;
  } catch (const std::exception &) {
    refused = code.empty();
  }
  require(refused, "Expected store refusal");
}
struct Fixture {
  fs::path root = fs::temp_directory_path() /
                  ("inde-obj01-store-tests-" + project::new_uuid());
  Fixture() {
    require(fs::create_directory(root), "New fixture directory");
    fs::permissions(root, fs::perms::owner_all);
  }
  ~Fixture() {
    std::error_code ignored;
    fs::remove_all(root, ignored);
  }
};
struct Source {
  project::Document document;
  std::string owner = project::new_uuid();
  Source() {
    document.id = project::new_uuid();
    document.title = "Checkpoint sintético";
    document.content = "A🙂\nBé\nC";
    document.created_at = document.updated_at = "2026-11-21T12:00:00Z";
    document.paragraphs = project::identify_paragraphs(document.content);
  }
  Scope scope() const { return {owner, document.id, SourceKind::Document}; }
  SessionCheckpoint checkpoint(bool edited = false) const {
    auto r = compat::read_document(owner, document);
    require(bool(r.projection), "Source projection");
    DocumentChangeSession s(*r.projection);
    if (edited) {
      auto t =
          s.prepare({ChangePurpose::AuthorialReorder,
                     {ReorderOccurrences{s.graph().root_layout_id,
                                         {s.graph().layouts[0].children[1]},
                                         s.graph().layouts[0].children[0]}}});
      require(t.ticket && s.apply(*t.ticket, owner, document).accepted,
              "Edited checkpoint");
    }
    return s.checkpoint();
  }
};
std::string record_name(std::uint64_t seq) {
  auto s = std::to_string(seq);
  return "g" + std::string(20 - s.size(), '0') + s + ".checkpoint";
}
void append(const fs::path &p) {
  std::ofstream f(p, std::ios::binary | std::ios::app);
  require(bool(f), "Append fixture corruption");
  f << '!';
}
void basic_versions_and_scope() {
  Fixture f;
  Source s;
  auto store = CheckpointStore::create_new(f.root / "store", s.scope());
  const auto id = store->id();
  const auto a = s.checkpoint(), b = s.checkpoint(true);
  require(store->inventory().generations.empty(), "New store empty");
  auto one = store->publish(a), two = store->publish(b);
  require(one.durable && two.durable && one.sequence == 1 && two.sequence == 2,
          "Monotonic durable generations");
  require(store->load(1).bytes == a.bytes && store->latest().bytes == b.bytes,
          "Versions never replace previous payload");
  struct stat st {};
  require(::stat((f.root / "store").c_str(), &st) == 0 &&
              (st.st_mode & 0777) == 0700,
          "Private root");
  require(::stat((f.root / "store" / record_name(2)).c_str(), &st) == 0 &&
              (st.st_mode & 0777) == 0600,
          "Private generation");
  rejects(
      [&] {
        auto other =
            CheckpointStore::open_existing(f.root / "store", s.scope());
      },
      "store_busy");
  store.reset();
  auto reopened = CheckpointStore::open_existing(f.root / "store", s.scope());
  require(reopened->id() == id && reopened->latest().sha256 == b.sha256,
          "Reopen durable data");
  reopened.reset();
  auto wrong = s.scope();
  wrong.source_id = project::new_uuid();
  rejects(
      [&] { auto x = CheckpointStore::open_existing(f.root / "store", wrong); },
      "store_scope_mismatch");
  const auto descriptor_count = [] {
    return static_cast<std::size_t>(std::distance(
        fs::directory_iterator("/proc/self/fd"), fs::directory_iterator{}));
  };
  const auto descriptors = descriptor_count();
  for (int attempt = 0; attempt < 20; ++attempt)
    rejects(
        [&] {
          auto failed = CheckpointStore::open_existing(f.root / "store", wrong);
        },
        "store_scope_mismatch");
  require(descriptor_count() == descriptors,
          "Failed open releases every descriptor and lease");
  auto again = CheckpointStore::open_existing(f.root / "store", s.scope());
  Source foreign;
  rejects([&] { (void)again->publish(foreign.checkpoint()); },
          "store_checkpoint_scope");
  rejects([&] {
    auto x = CheckpointStore::create_new(f.root / "store", s.scope());
  });
}
void failures_preserve_previous() {
  for (const auto point : {IoPoint::Write, IoPoint::FileSync, IoPoint::Publish,
                           IoPoint::DirectorySync}) {
    Fixture f;
    Source s;
    {
      auto init = CheckpointStore::create_new(f.root / "store", s.scope());
      require(init->publish(s.checkpoint()).durable, "Baseline publication");
    }
    StoreOptions o;
    o.inject_errno = [=](IoPoint p) {
      return p == point ? (p == IoPoint::Write ? ENOSPC : EIO) : 0;
    };
    auto store = CheckpointStore::open_existing(f.root / "store", s.scope(), o);
    const auto r = store->publish(s.checkpoint(true));
    require(!r.durable && !r.diagnostic.empty() &&
                (r.visible == (point == IoPoint::DirectorySync)),
            "Injected I/O outcome explicitly reported");
    require(store->load(1).sha256 == s.checkpoint().sha256,
            "Previous valid record survives all failures");
    rejects([&] { (void)store->publish(s.checkpoint()); },
            "store_reopen_required");
    store.reset();
    auto reopened = CheckpointStore::open_existing(f.root / "store", s.scope());
    const auto inventory = reopened->inventory();
    if (point == IoPoint::DirectorySync)
      require(
          inventory.generations.size() == 2 &&
              reopened->latest().sha256 == s.checkpoint(true).sha256,
          "Visible complete generation revalidated after uncertain durability");
    else
      require(inventory.generations.size() == 1 &&
                  inventory.incomplete.size() == 1 &&
                  reopened->latest().sha256 == s.checkpoint().sha256,
              "Incomplete evidence preserved without replacing previous "
              "generation");
  }
  Fixture f;
  Source s;
  int interrupts = 1;
  StoreOptions o;
  o.inject_errno = [&](IoPoint p) {
    if (p == IoPoint::Write && interrupts-- > 0)
      return EINTR;
    return 0;
  };
  auto store = CheckpointStore::create_new(f.root / "store", s.scope(), o);
  require(store->publish(s.checkpoint()).durable, "Interrupted write retried");
}
void corruption_and_unsafe_entries() {
  Fixture f;
  Source s;
  auto store = CheckpointStore::create_new(f.root / "store", s.scope());
  require(store->publish(s.checkpoint()).durable &&
              store->publish(s.checkpoint(true)).durable,
          "Seed corruption fixture");
  append(f.root / "store" / record_name(2));
  auto inventory = store->inventory();
  require(inventory.generations.size() == 2 && inventory.generations[0].valid &&
              !inventory.generations[1].valid,
          "Corrupt newest remains in inventory");
  rejects([&] { (void)store->latest(); });
  require(store->load(1).sha256 == s.checkpoint().sha256,
          "Older generation selectable explicitly");
  rejects([&] { (void)store->publish(s.checkpoint()); },
          "store_requires_review");
  rejects([&] { (void)store->plan_retention(); }, "store_requires_review");
  auto broken = s.checkpoint();
  broken.sha256 = std::string(64, '0');
  rejects([&] { (void)store->publish(broken); });
  auto clean = CheckpointStore::create_new(f.root / "links", s.scope());
  fs::create_symlink(f.root / "outside",
                     f.root / "links" / "g00000000000000000001.checkpoint");
  rejects([&] { (void)clean->inventory(); }, "store_entry_unsafe");
  auto hard = CheckpointStore::create_new(f.root / "hard", s.scope());
  require(hard->publish(s.checkpoint()).durable, "Hard link seed");
  fs::create_hard_link(f.root / "hard" / record_name(1), f.root / "alias");
  rejects([&] { (void)hard->load(1); }, "store_file_unsafe");
  auto marker = CheckpointStore::create_new(f.root / "marker", s.scope());
  append(f.root / "marker" / ".store");
  rejects([&] { (void)marker->publish(s.checkpoint()); },
          "store_marker_changed");
}
void safe_paths_and_quota() {
  Fixture f;
  Source s;
  fs::create_directory(f.root / "actual");
  fs::create_directory_symlink(f.root / "actual", f.root / "redirect");
  rejects([&] {
    auto x =
        CheckpointStore::create_new(f.root / "redirect" / "new", s.scope());
  });
  require(fs::is_empty(f.root / "actual"), "No path traversal through symlink");
  rejects(
      [&] {
        auto x = CheckpointStore::create_new(fs::path("relative"), s.scope());
      },
      "store_path_invalid");
  rejects(
      [&] {
        auto x =
            CheckpointStore::create_new(f.root / ".." / "outside", s.scope());
      },
      "store_path_invalid");
  auto invalid = (f.root / "prefix").string();
  invalid += '\0';
  invalid += "suffix";
  rejects(
      [&] {
        auto x = CheckpointStore::create_new(fs::path(invalid), s.scope());
      },
      "store_path_invalid");
  require(!fs::exists(f.root / "prefix"),
          "NUL path cannot create truncated name");
  fs::create_directory(f.root / "ordinary");
  fs::permissions(f.root / "ordinary", fs::perms::owner_all);
  rejects([&] {
    auto x = CheckpointStore::open_existing(f.root / "ordinary", s.scope());
  });
  StoreOptions opts;
  opts.max_files = 2;
  auto store = CheckpointStore::create_new(f.root / "quota", s.scope(), opts);
  require(store->publish(s.checkpoint()).durable &&
              store->publish(s.checkpoint(true)).durable,
          "Fill file quota");
  rejects([&] { (void)store->publish(s.checkpoint()); }, "store_quota");
  require(store->inventory().generations.size() == 2,
          "Quota never deletes data implicitly");
  StoreOptions tiny;
  tiny.max_bytes = 1024;
  auto limited = CheckpointStore::create_new(f.root / "bytes", s.scope(), tiny);
  rejects([&] { (void)limited->publish(s.checkpoint()); }, "store_quota");
  require(limited->inventory().generations.empty(),
          "Byte quota checked before file creation");
}
void retention_review_and_partial_results() {
  Fixture f;
  Source s;
  auto store = CheckpointStore::create_new(f.root / "store", s.scope());
  for (int i = 0; i < 5; ++i)
    require(store->publish(s.checkpoint(i % 2 != 0)).durable,
            "Seed generations");
  auto stale = store->plan_retention();
  require(stale->remove().size() == 3, "Review retention selection");
  require(store->publish(s.checkpoint()).durable, "Publish changes inventory");
  rejects([&] { (void)store->apply_retention(*stale); },
          "store_retention_stale");
  require(!stale->consumed(), "Stale plan has no effect");
  auto ready = store->plan_retention();
  auto result = store->apply_retention(*ready);
  require(result.complete && result.removed.size() == 4 && ready->consumed(),
          "Explicit retention removed only selected old generations");
  auto inventory = store->inventory();
  require(inventory.generations.size() == 2 &&
              inventory.generations[0].sequence == 5 &&
              inventory.generations[1].sequence == 6,
          "Newest two preserved");
  rejects([&] { (void)store->apply_retention(*ready); },
          "store_retention_scope");
  rejects([&] { (void)store->plan_retention(1); }, "store_retention_invalid");
  auto previous_lease_plan = store->plan_retention();
  store.reset();
  int calls{};
  StoreOptions options;
  options.inject_errno = [&](IoPoint p) {
    return p == IoPoint::Remove && ++calls == 2 ? EIO : 0;
  };
  auto partial =
      CheckpointStore::open_existing(f.root / "store", s.scope(), options);
  rejects([&] { (void)partial->apply_retention(*previous_lease_plan); },
          "store_retention_scope");
  for (int i = 0; i < 3; ++i)
    require(partial->publish(s.checkpoint()).durable, "Seed partial retention");
  auto plan = partial->plan_retention();
  const auto outcome = partial->apply_retention(*plan);
  require(!outcome.complete && outcome.removed.size() == 1 &&
              !outcome.durability_unknown && !outcome.diagnostic.empty(),
          "Partial retention precisely reports synced removals");
  require(partial->load(8).bytes == s.checkpoint().bytes &&
              partial->load(9).bytes == s.checkpoint().bytes,
          "Partial failure cannot delete newest two");
  partial.reset();
  StoreOptions sync_failure;
  sync_failure.inject_errno = [](IoPoint p) {
    return p == IoPoint::RetentionSync ? EIO : 0;
  };
  auto uncertain =
      CheckpointStore::open_existing(f.root / "store", s.scope(), sync_failure);
  auto sync_plan = uncertain->plan_retention();
  const auto failed_sync = uncertain->apply_retention(*sync_plan);
  require(!failed_sync.complete && failed_sync.removed.size() == 1 &&
              failed_sync.durability_unknown,
          "Retention sync failure reports uncertain durability after deletion");
  require(uncertain->load(8).bytes == s.checkpoint().bytes &&
              uncertain->load(9).bytes == s.checkpoint().bytes,
          "Newest generations survive uncertain retention");
}
int child(int argc, char **argv) {
  require(argc == 7, "Child arguments");
  Scope scope{argv[3], argv[4], SourceKind::Document};
  const std::string mode = argv[1];
  if (mode == "--busy") {
    try {
      auto store = CheckpointStore::open_existing(argv[2], scope);
    } catch (const StoreError &e) {
      return e.code == "store_busy" ? 42 : 43;
    }
    return 44;
  }
  const auto target = std::stoi(argv[5]);
  StoreOptions o;
  o.observe_stage = [=](PublishStage s) {
    if (static_cast<int>(s) == target)
      ::_exit(80 + target);
  };
  auto store = CheckpointStore::open_existing(argv[2], scope, o);
  auto cp = store->latest();
  const auto data = checkpoint_detail::decode(cp);
  const auto original = persistence::decode_document_snapshot(data.base);
  auto restored =
      DocumentChangeSession::restore(cp, scope.project_id, original);
  require(bool(restored.session), "Child checkpoint restore");
  auto prepared = restored.session->prepare(
      {ChangePurpose::VisualOnly,
       {ChangeLayout{restored.session->graph().root_layout_id,
                     LayoutKind::Row}}});
  require(
      prepared.ticket &&
          restored.session->apply(*prepared.ticket, scope.project_id, original)
              .accepted,
      "Child creates distinct checkpoint payload");
  (void)store->publish(restored.session->checkpoint());
  return 45;
}
int execute(const fs::path &self, const fs::path &path, const Scope &scope,
            const std::string &mode, int stage) {
  const auto executable = self.string(), root = path.string(),
             value = std::to_string(stage);
  const auto pid = ::fork();
  require(pid >= 0, "Fork child");
  if (pid == 0) {
    ::execl(executable.c_str(), executable.c_str(), mode.c_str(), root.c_str(),
            scope.project_id.c_str(), scope.source_id.c_str(), value.c_str(),
            "fixture", static_cast<char *>(nullptr));
    ::_exit(127);
  }
  int status{};
  require(::waitpid(pid, &status, 0) == pid && WIFEXITED(status),
          "Child exited normally at requested point");
  return WEXITSTATUS(status);
}
void process_leases_and_crash_windows(const fs::path &self) {
  Fixture f;
  Source s;
  auto held = CheckpointStore::create_new(f.root / "held", s.scope());
  require(execute(self, f.root / "held", s.scope(), "--busy", 0) == 42,
          "Other process refused while lease active");
  const auto pid = ::fork();
  require(pid >= 0, "Fork inherited handle check");
  if (pid == 0) {
    try {
      (void)held->inventory();
    } catch (const StoreError &e) {
      ::_exit(e.code == "store_inherited_handle" ? 46 : 47);
    }
    ::_exit(48);
  }
  int status{};
  require(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
              WEXITSTATUS(status) == 46,
          "Inherited object cannot act under parent's lease");
  for (int stage = 0; stage <= 5; ++stage) {
    const auto path = f.root / ("crash-" + std::to_string(stage));
    {
      auto store = CheckpointStore::create_new(path, s.scope());
      require(store->publish(s.checkpoint()).durable, "Crash baseline durable");
    }
    require(execute(self, path, s.scope(), "--crash", stage) == 80 + stage,
            "Crash at publication boundary");
    auto reopened = CheckpointStore::open_existing(path, s.scope());
    auto inventory = reopened->inventory();
    require(reopened->load(1).bytes == s.checkpoint().bytes,
            "Previous generation survives abrupt process exit");
    require(inventory.generations.size() == (stage >= 4 ? 2U : 1U),
            "Only complete atomically published generation visible");
    require(inventory.incomplete.size() == (stage >= 1 && stage <= 3 ? 1U : 0U),
            "Unpublished evidence retained");
    auto changed = checkpoint_detail::decode(s.checkpoint());
    changed.layout.kind = LayoutKind::Row;
    const auto row = checkpoint_detail::encode(changed);
    require(
        reopened->latest().bytes ==
            (stage >= 4 ? row.bytes : s.checkpoint().bytes),
        "Recovered complete old or new payload at actual publication boundary");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc > 1)
      return child(argc, argv);
    basic_versions_and_scope();
    failures_preserve_previous();
    corruption_and_unsafe_entries();
    safe_paths_and_quota();
    retention_review_and_partial_results();
    process_leases_and_crash_windows(fs::absolute(argv[0]));
    std::cout << "INDE-OBJ-01: 6 durable-store groups passed; versioning, "
                 "injected I/O failures, retention, process lease and 6 crash "
                 "windows verified\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}

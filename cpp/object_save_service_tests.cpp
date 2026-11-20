#include "inde/application/project_service.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "object_change_session.hpp"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace inde;
using namespace inde::objects;
using namespace inde::objects::planning;
namespace {
void require(bool yes, const char *why) {
  if (!yes)
    throw std::runtime_error(why);
}
struct Fixture {
  fs::path root = fs::temp_directory_path() /
                  ("inde-obj01-save-protocol-" + project::new_uuid());
  Fixture() {
    require(fs::create_directory(root), "Exclusive new fixture");
    fs::permissions(root, fs::perms::owner_all);
    g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  }
  ~Fixture() {
    std::error_code ignored;
    fs::remove_all(root, ignored);
  }
};
// Test-only durable store. The library exposes bytes, never filesystem writes.
void write_checkpoint(const fs::path &path, const SessionCheckpoint &c) {
  const auto bytes = c.sha256 + "\n" + c.bytes;
  const int fd = ::open(
      path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  require(fd >= 0, "Exclusive checkpoint file");
  std::size_t offset{};
  while (offset < bytes.size()) {
    const auto n = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (n <= 0) {
      ::close(fd);
      throw std::runtime_error("Checkpoint write failed");
    }
    offset += static_cast<std::size_t>(n);
  }
  const int synced = ::fsync(fd);
  ::close(fd);
  require(synced == 0, "Checkpoint fsync");
  const int dir = ::open(path.parent_path().c_str(),
                         O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  require(dir >= 0, "Checkpoint parent directory");
  const int ds = ::fsync(dir);
  ::close(dir);
  require(ds == 0, "Checkpoint directory fsync");
}
SessionCheckpoint read_checkpoint(const fs::path &path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  require(fd >= 0, "Open checkpoint");
  struct stat st {};
  if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 65 ||
      st.st_size > 64 * 1024 * 1024 + 65) {
    ::close(fd);
    throw std::runtime_error("Checkpoint file refused");
  }
  std::string bytes(static_cast<std::size_t>(st.st_size), '\0');
  std::size_t offset{};
  while (offset < bytes.size()) {
    const auto n = ::read(fd, bytes.data() + offset, bytes.size() - offset);
    if (n <= 0) {
      ::close(fd);
      throw std::runtime_error("Checkpoint read failed");
    }
    offset += static_cast<std::size_t>(n);
  }
  ::close(fd);
  require(bytes[64] == '\n', "Checkpoint hash separator");
  return {bytes.substr(65), bytes.substr(0, 64)};
}
project::StoredComposition save(application::ProjectService &service,
                                const PreparedCompositionSave &t) {
  auto update = t.expected();
  update.value = t.candidate();
  return service.compositions().save(update);
}
int child(const fs::path &path, const std::string &doc_id,
          const std::string &composition_id, const fs::path &folder,
          bool stop_after_document) {
  application::ProjectService service;
  service.open(path);
  const auto owner = service.current()->manifest().project_id;
  const auto d = *service.writing().document(doc_id);
  const auto c = service.compositions().load(composition_id);
  auto dv = compat::read_document(owner, d);
  auto cv = compat::read_composition(c);
  require(dv.projection && cv.projection, "Child captures");
  DocumentChangeSession ds(*dv.projection, {0, 2});
  CompositionChangeSession cs(*cv.projection);
  auto row =
      ds.prepare({ChangePurpose::VisualOnly,
                  {ChangeLayout{ds.graph().root_layout_id, LayoutKind::Row}}});
  require(row.ticket && ds.apply(*row.ticket, owner, d).accepted,
          "Child visual application");
  auto dp = ds.prepare(
      {ChangePurpose::AuthorialReorder,
       {ReorderOccurrences{ds.graph().root_layout_id,
                           {ds.graph().layouts[0].children.back()},
                           ds.graph().layouts[0].children.front()}}});
  auto cp = cs.prepare(
      {ChangePurpose::AuthorialReorder,
       {ReorderOccurrences{cs.graph().root_layout_id,
                           {cs.graph().layouts[0].children.back()},
                           cs.graph().layouts[0].children.front()}}});
  require(dp.ticket && cp.ticket && ds.apply(*dp.ticket, owner, d).accepted &&
              cs.apply(*cp.ticket, c).accepted,
          "Child authorial applications");
  auto di = ds.begin_save();
  auto ci = cs.begin_save();
  require(di.ticket && ci.ticket, "Child save barriers");
  write_checkpoint(folder / "document.checkpoint", ds.checkpoint());
  write_checkpoint(folder / "composition.checkpoint", cs.checkpoint());
  (void)service.writing().update_document_if_unchanged(di.ticket->candidate(),
                                                       di.ticket->expected());
  if (stop_after_document)
    ::_exit(74);
  (void)save(service, *ci.ticket);
  ::_exit(73); // Deliberate process exit AFTER both commits, BEFORE
               // acknowledgements.
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 6 && (std::string(argv[1]) == "--child" ||
                      std::string(argv[1]) == "--child-after-document"))
      return child(argv[2], argv[3], argv[4], argv[5],
                   std::string(argv[1]) == "--child-after-document");
    require(argc == 1, "Unexpected test arguments");
    Fixture fixture;
    const auto path = fixture.root / "Synthetic.inde";
    application::ProjectService service;
    service.create(path, "Protocolo OBJ-01 sintético");
    const auto owner = service.current()->manifest().project_id;
    auto d = service.writing().create_document("Fonte privada");
    d.content = "A🙂\nBé\nC";
    d.paragraphs = project::identify_paragraphs(d.content);
    d.formatting = {{project::DocumentTextStyle::Bold, 0, 2, {}},
                    {project::DocumentTextStyle::Italic, 3, 5, {}},
                    {project::DocumentTextStyle::Bold, 6, 7, {}}};
    d.anchors = {{project::new_uuid(), "Primeira", 0, 2},
                 {project::new_uuid(), "Última", 6, 7}};
    d = service.writing().update_document(d);
    auto c = service.compositions().create("Montagem privada");
    const auto live = service.compositions().import(
        c.value, d, 0, project::utf8_character_count(d.content));
    const auto block = c.value.blocks.occurrence(live).block_id,
               rev = c.value.blocks.block(block).current_revision;
    const auto pin = c.value.blocks.place(block);
    c.value.blocks.pin(pin, rev);
    const auto ref = c.value.blocks.place(block);
    c.value.blocks.reference_only(ref);
    const auto hidden = c.value.blocks.place(block);
    c.value.blocks.set_hidden(hidden, true);
    c = service.compositions().save(c);
    c.value.drafts[block] = {rev, "RASCUNHO PRIVADO"};
    c = service.compositions().save(c, true);
    const auto cid = c.value.blocks.composition_id();
    auto dv = compat::read_document(owner, d);
    auto cv = compat::read_composition(c);
    require(dv.projection && cv.projection, "Initial captures");
    DocumentChangeSession ds(*dv.projection);
    CompositionChangeSession cs(*cv.projection);
    auto dp = ds.prepare(
        {ChangePurpose::AuthorialReorder,
         {ReorderOccurrences{ds.graph().root_layout_id,
                             {ds.graph().layouts[0].children[1]},
                             ds.graph().layouts[0].children.front()}}});
    auto cp = cs.prepare(
        {ChangePurpose::AuthorialReorder,
         {ReorderOccurrences{cs.graph().root_layout_id,
                             {cs.graph().layouts[0].children.back()},
                             cs.graph().layouts[0].children.front()}}});
    require(dp.ticket && cp.ticket && ds.apply(*dp.ticket, owner, d).accepted &&
                cs.apply(*cp.ticket, c).accepted,
            "Initial apply");
    auto di = ds.begin_save();
    auto ci = cs.begin_save();
    require(di.ticket && ci.ticket, "Save intents");
    (void)service.writing().update_document_if_unchanged(di.ticket->candidate(),
                                                         di.ticket->expected());
    (void)save(service, *ci.ticket);
    auto saved_d = *service.writing().document(d.id);
    auto saved_c = service.compositions().load(cid);
    require(ds.acknowledge_save(*di.ticket, owner, saved_d).accepted,
            "Actual Document CAS readback acknowledgement");
    require(cs.acknowledge_save(*ci.ticket, saved_c).accepted,
            "Actual Composition CAS readback acknowledgement");
    require(!ds.source_dirty() && !cs.source_dirty() && ds.can_undo() &&
                cs.can_undo(),
            "Actual savepoints retain history");
    require(ds.undo(ds.revision(), owner, saved_d).accepted &&
                ds.redo(ds.revision(), owner, saved_d).accepted &&
                cs.undo(cs.revision(), saved_c).accepted &&
                cs.redo(cs.revision(), saved_c).accepted,
            "Actual acknowledged history remains reversible");
    // Another service path commits first. Our old expected baseline must fail
    // IN the repository transaction, not merely in an earlier in-memory check.
    auto contested_d = service.writing().create_document("Conflito documental");
    contested_d.content = "X\nY";
    contested_d.paragraphs = project::identify_paragraphs(contested_d.content);
    contested_d = service.writing().update_document(contested_d);
    auto baseline = contested_d;
    contested_d.title += " outro escritor";
    contested_d = service.writing().update_document(contested_d);
    bool conflict{};
    try {
      (void)service.writing().update_document_if_unchanged(baseline, baseline);
    } catch (const std::exception &) {
      conflict = true;
    }
    require(conflict && persistence::encode_document_snapshot(
                            *service.writing().document(baseline.id)) ==
                            persistence::encode_document_snapshot(contested_d),
            "Document transactional CAS conflict preserves winner");
    auto stale = saved_c;
    auto winner = saved_c;
    winner.value.drafts[block].text += " outro escritor";
    winner = service.compositions().save(winner, true);
    conflict = false;
    try {
      (void)service.compositions().save(stale);
    } catch (const std::exception &) {
      conflict = true;
    }
    require(conflict &&
                service.compositions().load(cid).revision == winner.revision &&
                service.compositions().load(cid).sha256 == winner.sha256,
            "Composition transactional CAS conflict preserves winner");
    // Fresh process simulates the unknown-outcome window. Parent does not keep
    // the child's editing state or save tickets in memory.
    const auto executable = fs::absolute(argv[0]).string();
    const auto project_path = path.string(), folder = fixture.root.string();
    const pid_t pid = ::fork();
    require(pid >= 0, "Fork fixture child");
    if (pid == 0) {
      ::execl(executable.c_str(), executable.c_str(), "--child",
              project_path.c_str(), d.id.c_str(), cid.c_str(), folder.c_str(),
              static_cast<char *>(nullptr));
      ::_exit(127);
    }
    int status{};
    require(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
                WEXITSTATUS(status) == 73,
            "Child exited after commit before acknowledgement");
    saved_d = *service.writing().document(d.id);
    saved_c = service.compositions().load(cid);
    const auto dc = read_checkpoint(fixture.root / "document.checkpoint"),
               cc = read_checkpoint(fixture.root / "composition.checkpoint");
    require(!DocumentChangeSession::restore(dc, owner, saved_d).session &&
                !CompositionChangeSession::restore(cc, saved_c).session,
            "Unknown outcome cannot resume against stale origin");
    auto dr = DocumentChangeSession::restore(dc, owner, saved_d,
                                             RecoveryMode::AdoptMatchingSave);
    auto cr = CompositionChangeSession::restore(
        cc, saved_c, RecoveryMode::AdoptMatchingSave);
    require(dr.session && cr.session && !dr.session->source_dirty() &&
                !cr.session->source_dirty() && !dr.session->can_undo() &&
                !cr.session->can_undo(),
            "Fresh process recovered committed candidates with reset history");
    require(dr.session->graph().layouts[0].kind == LayoutKind::Row &&
                dr.session->dirty(),
            "Visual work survives restart without claiming source-schema "
            "persistence");
    require(
        persistence::encode_document_snapshot(dr.session->current().document) ==
                persistence::encode_document_snapshot(saved_d) &&
            persistence::encode_composition(cr.session->current()) ==
                persistence::encode_composition(saved_c.value),
        "Recovered private working payloads equal authoritative readback");
    const auto mixed_folder = fixture.root / "partial-commit";
    require(fs::create_directory(mixed_folder),
            "Fresh partial-save checkpoint folder");
    const auto before_partial = service.compositions().load(cid);
    const auto mixed_string = mixed_folder.string();
    const pid_t mixed = ::fork();
    require(mixed >= 0, "Fork partial-save child");
    if (mixed == 0) {
      ::execl(executable.c_str(), executable.c_str(), "--child-after-document",
              project_path.c_str(), d.id.c_str(), cid.c_str(),
              mixed_string.c_str(), static_cast<char *>(nullptr));
      ::_exit(127);
    }
    status = 0;
    require(::waitpid(mixed, &status, 0) == mixed && WIFEXITED(status) &&
                WEXITSTATUS(status) == 74,
            "Child exited between independent source commits");
    const auto partial_doc = *service.writing().document(d.id);
    const auto partial_comp = service.compositions().load(cid);
    const auto pd = read_checkpoint(mixed_folder / "document.checkpoint"),
               pc = read_checkpoint(mixed_folder / "composition.checkpoint");
    require(DocumentChangeSession::restore(pd, owner, partial_doc,
                                           RecoveryMode::AdoptMatchingSave)
                        .session != nullptr &&
                !DocumentChangeSession::restore(pd, owner, partial_doc).session,
            "Committed half classified independently");
    auto unsaved = CompositionChangeSession::restore(pc, partial_comp);
    require(unsaved.session && unsaved.session->source_dirty() &&
                partial_comp.revision == before_partial.revision &&
                partial_comp.sha256 == before_partial.sha256 &&
                !CompositionChangeSession::restore(
                     pc, partial_comp, RecoveryMode::AdoptMatchingSave)
                     .session,
            "Uncommitted half retained as unsaved work, never falsely "
            "acknowledged");
    {
      persistence::SqliteDatabase db(
          persistence::ProjectDatabaseRepository::database_path(path));
      require(persistence::SchemaMigrator{}.current_version(db) == 37 &&
                  db.query_text("PRAGMA integrity_check") == "ok",
              "Schema 37/integrity preserved");
      auto fk = db.prepare("PRAGMA foreign_key_check");
      require(!fk.step(), "Foreign keys preserved");
    }
    std::cout
        << "INDE-OBJ-01: real CAS saves/conflicts passed; durable fixture "
           "checkpoints survived fresh process exit after commit before ack, "
           "source/visual recovery and schema37 integrity verified\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}

#include "document_recovery_transport.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/document_recovery_store.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <sys/wait.h>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace inde;
void need(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
std::string bytes(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
void sql(sqlite3 *db, const char *s) {
  need(sqlite3_exec(db, s, nullptr, nullptr, nullptr) == SQLITE_OK,
       "Fixture SQL");
}
void interrupted(const fs::path &path, bool hot) {
  const auto child = ::fork();
  need(child >= 0, "fork");
  if (child == 0) {
    sqlite3 *db{};
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
      ::_exit(2);
    sql(db,
        "PRAGMA journal_mode=DELETE;PRAGMA synchronous=FULL;BEGIN IMMEDIATE;");
    sql(db, "UPDATE recovery_records SET title=title||' interrupted'");
    if (hot && sqlite3_db_cacheflush(db) != SQLITE_OK)
      ::_exit(3);
    ::_exit(0); // No SQLite close/rollback, like a process interruption.
  }
  int status{};
  need(::waitpid(child, &status, 0) == child && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0,
       "Interrupted fixture failed");
  const auto journal = bytes(path.string() + "-journal");
  need(journal.size() >= 8, "Missing interrupted journal");
  need((journal.substr(0, 8) != std::string(8, '\0')) == hot,
       "Wrong journal boundary");
}
int main() {
  const auto root = fs::temp_directory_path() /
                    ("inde-recovery-transport-" + project::new_uuid());
  try {
    need(fs::create_directory(root), "Fixture root already exists");
    ::setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
    application::ProjectService service;
    service.create(root / "Source.inde", "Fictício");
    const auto owner = *service.current();
    auto doc = service.writing().create_document("Texto preservado");
    persistence::DocumentRecoveryStore store;
    persistence::prepare_document_recovery_transport(owner);
    need(!fs::exists(store.database_path(owner)),
         "Preparation created an unnecessary journal");
    const auto orphan =
        fs::path(store.database_path(owner).string() + "-journal");
    fs::create_directories(orphan.parent_path());
    std::ofstream(orphan) << "fictitious orphan";
    bool orphan_refused = false;
    try {
      persistence::prepare_document_recovery_transport(owner);
    } catch (const std::exception &) {
      orphan_refused = true;
    }
    need(orphan_refused && bytes(orphan) == "fictitious orphan" &&
             !fs::exists(store.database_path(owner)),
         "Orphan journal was changed");
    fs::remove(orphan); // Only the explicitly created fixture.
    const auto session = project::new_uuid();
    auto working = doc;
    working.content = "Trabalho interrompido.";
    working.paragraphs = project::identify_paragraphs(working.content);
    store.checkpoint(owner, {owner.manifest().project_id,
                             session,
                             1,
                             0,
                             {working, 0, 0},
                             doc,
                             "pending",
                             {},
                             {}});
    const auto original = store.load(owner, session);
    need(bool(original), "Missing recovery record");
    const auto path = store.database_path(owner);
    const auto record_bytes = [&] {
      persistence::SqliteDatabase db(path);
      return db.query_text(
          "SELECT "
          "quote(session_id)||quote(project_id)||quote(document_id)||quote("
          "revision)||quote(durable_revision)||quote(title)||quote(status)||"
          "quote(updated_at)||quote(payload)||quote(checksum) FROM "
          "recovery_records");
    };
    const auto logical = record_bytes();
    for (const bool hot : {false, true}) {
      interrupted(path, hot);
      persistence::prepare_document_recovery_transport(owner);
      need(!fs::exists(path.string() + "-journal"),
           "Interrupted journal remained");
      need(record_bytes() == logical,
           "Private record fields changed during recovery");
      const auto restored = store.load(owner, session);
      need(restored && restored->checksum == original->checksum &&
               restored->status == "pending" &&
               restored->state.document.content ==
                   original->state.document.content,
           "Recovery record changed");
    }
    {
      persistence::SqliteDatabase db(path);
      need(db.query_integer("PRAGMA user_version") == 1,
           "Private format changed");
      db.execute("PRAGMA user_version=2");
    }
    const auto unsupported = bytes(path);
    bool wrong_version = false;
    try {
      persistence::prepare_document_recovery_transport(owner);
    } catch (const std::exception &) {
      wrong_version = true;
    }
    need(wrong_version && bytes(path) == unsupported,
         "Unknown recovery format changed");
    {
      persistence::SqliteDatabase db(path);
      db.execute("PRAGMA user_version=1");
    }
    const auto alias = root / "fixture-hardlink";
    fs::create_hard_link(path, alias);
    const auto linked = bytes(path);
    bool link_refused = false;
    try {
      persistence::prepare_document_recovery_transport(owner);
    } catch (const std::exception &) {
      link_refused = true;
    }
    need(link_refused && bytes(path) == linked,
         "Hardlinked recovery database changed");
    fs::remove(alias);
    const auto before = bytes(path);
    fs::create_symlink(path, path.string() + "-journal");
    bool refused = false;
    try {
      persistence::prepare_document_recovery_transport(owner);
    } catch (const std::exception &) {
      refused = true;
    }
    need(refused && bytes(path) == before &&
             fs::is_symlink(path.string() + "-journal"),
         "Unsafe journal accepted or changed");
    fs::remove(path.string() +
               "-journal"); // Only this explicitly created fixture link.
    {
      sqlite3 *db{};
      need(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "WAL fixture");
      sql(db, "PRAGMA journal_mode=WAL");
      sqlite3_close(db);
    }
    const auto wal = bytes(path);
    refused = false;
    try {
      persistence::prepare_document_recovery_transport(owner);
    } catch (const std::exception &) {
      refused = true;
    }
    need(refused && bytes(path) == wal, "WAL was converted");
    {
      persistence::SqliteDatabase canonical(owner.path() /
                                            "data/project.sqlite3");
      need(canonical.query_integer(
               "SELECT MAX(version) FROM schema_migrations") == 37,
           "Canonical schema changed");
    }
    service.close();
    fs::remove_all(root);
    std::cout
        << "Native recovery transport: hot/cold interruption, exact record, no "
           "new database, unsafe journal refusal and WAL preservation OK\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\nEvidence: " << root << '\n';
    return 1;
  }
}

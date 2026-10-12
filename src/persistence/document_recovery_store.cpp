#include "inde/persistence/document_recovery_store.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "recovery_codec.hpp"
#include "recovery_export_support.hpp"
#include <chrono>
#include <ctime>
#include <set>
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace inde::persistence {
namespace {
void sync_directory(const std::filesystem::path &p) {
  const int fd =
      ::open(p.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    throw std::runtime_error(
        "Não foi possível abrir diretório do journal para sincronizar");
  const int result = ::fsync(fd);
  ::close(fd);
  if (result)
    throw std::runtime_error(
        "Não foi possível sincronizar diretório do journal");
}
void folder(const project::Project &owner) {
  for (const auto &p :
       {owner.path() / "recovery", owner.path() / "recovery/editor-v1"}) {
    if (std::filesystem::is_symlink(p))
      throw std::runtime_error("Journal recusa diretório simbólico");
    if (std::filesystem::create_directory(p)) {
      std::filesystem::permissions(p, std::filesystem::perms::owner_all);
      sync_directory(p.parent_path());
    }
    if (!std::filesystem::is_directory(p))
      throw std::runtime_error("Diretório de recuperação indisponível");
  }
}
std::filesystem::path lock_path(const project::Project &owner,
                                const std::string &session) {
  recovery::safe_id(owner.manifest().project_id);
  recovery::safe_id(session);
  for (const auto &p :
       {owner.path() / "recovery", owner.path() / "recovery/editor-v1"})
    if (std::filesystem::is_symlink(p))
      throw std::runtime_error("Lease recusa diretório simbólico");
  return owner.path() / "recovery/editor-v1" /
         (owner.manifest().project_id + "-" + session + ".lock");
}
std::unique_ptr<SqliteDatabase> database(const project::Project &owner,
                                         bool create) {
  const auto path = DocumentRecoveryStore::database_path(owner);
  if (create)
    folder(owner);
  for (const auto &p :
       {owner.path() / "recovery", owner.path() / "recovery/editor-v1"})
    if (std::filesystem::is_symlink(p))
      throw std::runtime_error("Journal recusa diretório simbólico");
  if (std::filesystem::is_symlink(path))
    throw std::runtime_error("Journal recusa arquivo simbólico");
  const bool fresh = !std::filesystem::exists(path);
  if (fresh && !create)
    throw std::runtime_error("Journal não encontrado");
  auto db = std::make_unique<SqliteDatabase>(path);
  if (fresh) {
    std::filesystem::permissions(path, std::filesystem::perms::owner_read |
                                           std::filesystem::perms::owner_write);
    db->execute("PRAGMA journal_mode=DELETE");
    db->execute("PRAGMA synchronous=EXTRA");
    SqliteTransaction transaction(*db);
    // A second connection can have observed a missing file before this one
    // committed initialization. Re-check under SQLite's writer lock.
    const auto version = db->query_integer("PRAGMA user_version");
    if (version != 0 && version != 1)
      throw std::runtime_error(
          "Versão de journal incompatível; evidência preservada");
    db->execute(
        "CREATE TABLE IF NOT EXISTS recovery_records(session_id TEXT PRIMARY "
        "KEY, project_id TEXT NOT NULL, document_id TEXT NOT NULL, "
        "revision INTEGER NOT NULL, durable_revision INTEGER NOT NULL, title "
        "TEXT NOT NULL, status TEXT NOT NULL, updated_at TEXT NOT NULL, "
        "payload TEXT NOT NULL, checksum TEXT NOT NULL)");
    db->execute("PRAGMA user_version=1");
    transaction.commit();
    sync_directory(path.parent_path());
  }
  if (db->query_integer("PRAGMA user_version") != 1 ||
      db->query_text("PRAGMA journal_mode") != "delete")
    throw std::runtime_error(
        "Versão/modo de journal incompatível; evidência preservada");
  db->execute("PRAGMA synchronous=EXTRA");
  if (db->query_integer("PRAGMA synchronous") != 3)
    throw std::runtime_error(
        "SQLite não confirmou synchronous=EXTRA no journal");
  const auto page_size = db->query_integer("PRAGMA page_size");
  if (page_size <= 0)
    throw std::runtime_error("Tamanho de página inválido no journal");
  const auto max_pages = (512LL * 1024 * 1024) / page_size;
  db->execute("PRAGMA max_page_count=" + std::to_string(max_pages));
  if (db->query_integer("PRAGMA max_page_count") > max_pages)
    throw std::runtime_error(
        "Journal excede limite de 512 MiB; evidência preservada");
  return db;
}
std::optional<project::RecoveryRecord> read(SqliteDatabase &db,
                                            const project::Project &owner,
                                            const std::string &session) {
  recovery::safe_id(session);
  auto q = db.prepare("SELECT "
                      "project_id,document_id,revision,durable_revision,status,"
                      "payload,checksum,length(CAST(payload AS BLOB)),title,updated_at FROM "
                      "recovery_records WHERE session_id=?");
  q.bind(1, session);
  if (!q.step())
    return {};
  if (q.column_integer(7) < 0 || static_cast<std::uint64_t>(q.column_integer(
                                     7)) > project::recovery_payload_limit)
    throw std::runtime_error("Registro de journal excede limite");
  auto r = recovery::decode(q.column_text(5), q.column_text(6));
  if (r.project_id != owner.manifest().project_id ||
      r.project_id != q.column_text(0) || r.session_id != session ||
      r.state.document.id != q.column_text(1) ||
      static_cast<std::int64_t>(r.revision) != q.column_integer(2) ||
      static_cast<std::int64_t>(r.durable_revision) != q.column_integer(3) ||
      r.status != q.column_text(4) || r.state.document.title != q.column_text(8) ||
      r.updated_at != q.column_text(9))
    throw std::runtime_error(
        "Identidade/metadados do journal incompatíveis; evidência preservada");
  return r;
}
void write(SqliteDatabase &db, const project::RecoveryRecord &r) {
  const auto payload = recovery::encode(r), hash = recovery::checksum(payload);
  auto q = db.prepare(
      "INSERT INTO recovery_records VALUES(?,?,?,?,?,?,?,?,?,?) ON "
      "CONFLICT(session_id) DO UPDATE SET "
      "revision=excluded.revision,durable_revision=excluded.durable_revision,"
      "title=excluded.title,status=excluded.status,updated_at=excluded.updated_"
      "at,payload=excluded.payload,checksum=excluded.checksum");
  q.bind(1, r.session_id);
  q.bind(2, r.project_id);
  q.bind(3, r.state.document.id);
  q.bind(4, static_cast<std::int64_t>(r.revision));
  q.bind(5, static_cast<std::int64_t>(r.durable_revision));
  q.bind(6, r.state.document.title);
  q.bind(7, r.status);
  q.bind(8, r.updated_at);
  q.bind(9, payload);
  q.bind(10, hash);
  q.run();
}
bool resolved(const std::string &s) { return s != "pending" && s != "clean"; }
} // namespace
RecoveryLease::RecoveryLease(const project::Project &owner,
                             const std::string &session, bool create) {
  const auto path = lock_path(owner, session);
  if (create)
    folder(owner);
  descriptor_ = ::open(
      path.c_str(),
      (create ? O_RDWR | O_CREAT : O_RDONLY) | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (descriptor_ < 0)
    throw std::runtime_error(
        "Lease de recuperação indisponível; evidência preservada");
  if (::flock(descriptor_, LOCK_EX | LOCK_NB)) {
    ::close(descriptor_);
    descriptor_ = -1;
    throw std::runtime_error(
        "Sessão de recuperação ainda ativa ou lease indisponível");
  }
}
RecoveryLease::~RecoveryLease() {
  if (descriptor_ >= 0)
    ::close(descriptor_);
}
std::filesystem::path
DocumentRecoveryStore::database_path(const project::Project &owner) {
  recovery::safe_id(owner.manifest().project_id);
  return owner.path() / "recovery/editor-v1" /
         (owner.manifest().project_id + ".sqlite3");
}
bool DocumentRecoveryStore::inactive(const project::Project &owner,
                                     const std::string &session) {
  const auto path = lock_path(owner, session);
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0)
    return false; // Missing/unsafe lock is not proof of an inactive writer.
  const bool result = ::flock(fd, LOCK_EX | LOCK_NB) == 0;
  ::close(fd);
  return result;
}
void DocumentRecoveryStore::checkpoint(const project::Project &owner,
                                       project::RecoveryRecord r) const {
  if (r.project_id != owner.manifest().project_id)
    throw std::runtime_error("Checkpoint pertence a outro Projeto");
  recovery::validate(r);
  auto db = database(owner, true);
  SqliteTransaction tx(*db);
  auto previous = read(*db, owner, r.session_id);
  if (previous) {
    if (previous->state.document.id != r.state.document.id ||
        resolved(previous->status))
      throw std::runtime_error(
          "Contexto de recuperação foi resolvido ou mudou");
    if (previous->revision > r.revision)
      return;
    if (previous->durable_revision > r.durable_revision) {
      r.baseline = previous->baseline;
      r.durable_revision = previous->durable_revision;
    }
    // Autosave must establish a checkpoint before writing the main database,
    // but the debounce worker may have already committed this exact snapshot.
    // Validate the existing payload above; never rewrite a confirmed duplicate.
    if (previous->revision == r.revision &&
        previous->durable_revision == r.durable_revision &&
        previous->state.cursor == r.state.cursor &&
        previous->state.bound == r.state.bound &&
        project::same_document_state(previous->state.document,
                                     r.state.document) &&
        project::same_document_state(previous->baseline, r.baseline))
      return;
  } else if (db->query_integer("SELECT count(*) FROM recovery_records") >= 256)
    throw std::runtime_error(
        "Journal atingiu 256 sessões retidas; evidências não foram apagadas");
  if (db->query_integer("PRAGMA page_count") *
          db->query_integer("PRAGMA page_size") >
      512LL * 1024 * 1024)
    throw std::runtime_error(
        "Journal atingiu limite de 512 MiB; evidências preservadas");
  r.status = project::same_document_state(r.state.document, r.baseline, false)
                 ? "clean"
                 : "pending";
  r.updated_at = project::utc_now();
  write(*db, r);
  tx.commit();
}
void DocumentRecoveryStore::acknowledge(const project::Project &owner,
                                        const std::string &session,
                                        const project::Document &saved,
                                        std::uint64_t revision) const {
  if (!std::filesystem::exists(database_path(owner)))
    return;
  auto db = database(owner, false);
  SqliteTransaction tx(*db);
  auto r = read(*db, owner, session);
  if (!r)
    return;
  if (r->state.document.id != saved.id || resolved(r->status))
    throw std::runtime_error(
        "Confirmação de journal pertence a outro contexto");
  if (revision < r->durable_revision)
    return;
  if (revision > r->revision) {
    r->revision = revision;
    r->state = {saved, 0, 0};
  }
  r->baseline = saved;
  r->durable_revision = revision;
  r->status = project::same_document_state(r->state.document, saved, false)
                  ? "clean"
                  : "pending";
  r->updated_at = project::utc_now();
  write(*db, *r);
  tx.commit();
}
project::RecoveryPage DocumentRecoveryStore::pending(
    const project::Project &owner,
    const std::optional<std::string> &document) const {
  project::RecoveryPage page;
  if (!std::filesystem::exists(database_path(owner)))
    return page;
  auto db = database(owner, false);
  auto q = db->prepare(
      "SELECT session_id,document_id,substr(title,1,256),updated_at,revision "
      "FROM recovery_records WHERE project_id=? AND status='pending' "
      "AND (? IS NULL OR document_id=?) ORDER BY updated_at DESC,session_id "
      "LIMIT 257");
  q.bind(1, owner.manifest().project_id);
  if (document) {
    q.bind(2, *document);
    q.bind(3, *document);
  } else {
    q.bind_null(2);
    q.bind_null(3);
  }
  while (q.step()) {
    recovery::safe_id(q.column_text(0));
    recovery::safe_id(q.column_text(1));
    if (q.column_integer(4) < 0)
      throw std::runtime_error("Revisão negativa no journal");
    if (!inactive(owner, q.column_text(0)))
      continue;
    if (page.items.size() == 64) {
      page.more = true;
      break;
    }
    page.items.push_back({q.column_text(0), q.column_text(1), q.column_text(2),
                          q.column_text(3),
                          static_cast<std::uint64_t>(q.column_integer(4))});
  }
  return page;
}
std::optional<project::RecoveryRecord>
DocumentRecoveryStore::load(const project::Project &owner,
                            const std::string &session) const {
  if (!std::filesystem::exists(database_path(owner)))
    return {};
  auto db = database(owner, false);
  return read(*db, owner, session);
}
void DocumentRecoveryStore::resolve(const project::Project &owner,
                                    const std::string &session,
                                    const std::string &hash,
                                    const std::string &resolution) const {
  if (resolution != "restored" && resolution != "copy-created" &&
      resolution != "discarded")
    throw std::runtime_error("Decisão de recuperação inválida");
  auto db = database(owner, false);
  SqliteTransaction tx(*db);
  auto r = read(*db, owner, session);
  if (!r || r->checksum != hash || r->status != "pending")
    throw std::runtime_error(
        "Registro de recuperação mudou; abra a prévia novamente");
  r->status = resolution;
  r->updated_at = project::utc_now();
  write(*db, *r);
  tx.commit();
}
namespace {
std::string cutoff(unsigned days) {
  if (days > 36500) throw std::runtime_error("Retenção fora do limite de 100 anos");
  const auto time = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now() - std::chrono::hours(24LL * days));
  std::tm tm{}; gmtime_r(&time, &tm);
  char value[32]{}; std::strftime(value, sizeof(value), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return value;
}
bool closed(const project::RecoveryRecord &r) {
  return r.status == "clean" || r.status == "restored" ||
         r.status == "copy-created" || r.status == "discarded";
}
bool old_enough(const project::RecoveryRecord &r, const std::string &before) {
  // Never decide eligibility from malformed or future timestamps.
  std::tm tm{};
  const char *end = strptime(r.updated_at.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tm);
  if (!end || *end) return false;
  char canonical[32]{};
  const auto timestamp = timegm(&tm); gmtime_r(&timestamp, &tm);
  std::strftime(canonical, sizeof(canonical), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return r.updated_at == canonical && r.updated_at <= before;
}
}
project::RecoveryRetentionPage DocumentRecoveryStore::retention(
    const project::Project &owner, unsigned keep_days) const {
  project::RecoveryRetentionPage result;
  const auto before = cutoff(keep_days);
  if (!std::filesystem::exists(database_path(owner))) return result;
  auto db = database(owner, false);
  SqliteTransaction tx(*db, SqliteTransaction::Mode::Deferred);
  const auto size = db->query_integer("PRAGMA page_size");
  result.allocated_bytes = db->query_integer("PRAGMA page_count") * size;
  result.reusable_bytes = db->query_integer("PRAGMA freelist_count") * size;
  auto q = db->prepare("SELECT session_id,substr(title,1,256),status,updated_at,checksum,"
                       "length(CAST(payload AS BLOB)) FROM recovery_records ORDER BY updated_at,session_id LIMIT 257");
  while (q.step()) {
    if (result.items.size() == 256) { result.more = true; break; }
    project::RecoveryRetentionEntry e;
    e.session_id=q.column_text(0); e.title=q.column_text(1); e.status=q.column_text(2);
    e.updated_at=q.column_text(3); e.checksum=q.column_text(4);
    e.payload_bytes=static_cast<std::size_t>(q.column_integer(5));
    try {
      if (!inactive(owner,e.session_id)) e.explanation="Protegido: sessão ativa ou lease indisponível";
      else {
        const auto r = read(*db,owner,e.session_id);
        if (!r) throw std::runtime_error("Checkpoint desapareceu");
        e.exportable=true;
        e.removable=closed(*r) && old_enough(*r,before);
        e.explanation = r->status == "pending" ? "Protegido: decisão de recuperação pendente"
                        : !e.removable ? "Protegido pelo período de retenção ou data inválida"
                                       : "Elegível após exportação verificada e confirmação";
      }
    } catch (const std::exception &error) { e.explanation=std::string("Protegido: ")+error.what(); }
    result.items.push_back(std::move(e));
  }
  tx.commit(); return result;
}
std::filesystem::path DocumentRecoveryStore::export_record(
    const project::Project &owner, const std::string &session, const std::string &hash) const {
  RecoveryLease lease(owner,session,false);
  auto db=database(owner,false);
  SqliteTransaction tx(*db,SqliteTransaction::Mode::Deferred);
  const auto r=read(*db,owner,session);
  if (!r || r->checksum != hash) throw std::runtime_error("Checkpoint mudou; refaça a prévia");
  auto q=db->prepare("SELECT payload FROM recovery_records WHERE session_id=?");
  q.bind(1,session); if (!q.step()) throw std::runtime_error("Checkpoint não encontrado");
  const auto payload=q.column_text(0); tx.commit();
  return recovery::publish_export(owner,*r,payload);
}
project::RecoveryRecord DocumentRecoveryStore::read_export(const std::filesystem::path &path) {
  return recovery::read_export(path);
}
std::size_t DocumentRecoveryStore::purge_exported(
    const project::Project &owner, const std::vector<project::RecoveryRetentionEntry> &entries,
    unsigned keep_days) const {
  if (entries.empty() || entries.size()>256) throw std::runtime_error("Seleção de limpeza inválida");
  const auto before=cutoff(keep_days);
  std::set<std::string> unique;
  std::vector<std::unique_ptr<RecoveryLease>> leases;
  for (const auto &e : entries) {
    if (!unique.insert(e.session_id).second) throw std::runtime_error("Sessão duplicada na seleção");
    leases.push_back(std::make_unique<RecoveryLease>(owner,e.session_id,false));
  }
  auto db=database(owner,false); SqliteTransaction tx(*db);
  for (const auto &e : entries) {
    const auto r=read(*db,owner,e.session_id);
    if (!r || r->checksum!=e.checksum || !closed(*r) || !old_enough(*r,before))
      throw std::runtime_error("Seleção mudou ou está protegida; nenhuma limpeza aplicada");
    const auto archive=read_export(recovery::export_path(owner,*r));
    if (archive.checksum != r->checksum || archive.project_id != r->project_id ||
        archive.session_id != r->session_id)
      throw std::runtime_error("Exportação ausente ou divergente; nenhuma limpeza aplicada");
    auto q=db->prepare("DELETE FROM recovery_records WHERE session_id=? AND checksum=?");
    q.bind(1,e.session_id); q.bind(2,e.checksum); q.run();
    if (db->changes()!=1) throw std::runtime_error("Limpeza concorrente; transação cancelada");
  }
  // No VACUUM while other editing sessions can write. SQLite reuses free pages.
  tx.commit(); return entries.size();
}
} // namespace inde::persistence

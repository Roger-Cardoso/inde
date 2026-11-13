#include "comic_service.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <fstream>
namespace inde::comic {
namespace {
using DB = persistence::SqliteDatabase;
using TX = persistence::SqliteTransaction;
void need(bool yes, const char *msg) {
  if (!yes)
    throw std::runtime_error(msg);
}
std::string dump(const V &v) { return persistence::json::serialize(v, false); }
Book load(DB &db, const std::string &id) {
  auto q = db.prepare("SELECT payload,revision,removed FROM comics WHERE id=?");
  q.bind(1, id);
  need(q.step(), "Comic não encontrado");
  auto b = decode(persistence::json::parse(q.column_text(0)));
  need(b.id == id && b.revision == q.column_integer(1) &&
           b.removed == bool(q.column_integer(2)),
       "Registro comic inconsistente");
  return b;
}
void audit(DB &db, const std::string &id, const V &before, const V &after) {
  auto q = db.prepare(
      "INSERT INTO history(comic_id,before_payload,after_payload,created_at) "
      "VALUES(?,?,?,?)");
  q.bind(1, id);
  q.bind(2, dump(before));
  q.bind(3, dump(after));
  q.bind(4, project::utc_now());
  q.run();
}
void write(DB &db, const Book &b) {
  auto q =
      db.prepare("UPDATE comics SET revision=?,removed=?,payload=? WHERE id=?");
  q.bind(1, b.revision);
  q.bind(2, std::int64_t(b.removed));
  q.bind(3, dump(encode(b)));
  q.bind(4, b.id);
  q.run();
}
} // namespace
const project::Project &Service::project() const {
  auto *p = publication_.source().current();
  need(p, "Abra um Projeto INDE");
  return *p;
}
std::filesystem::path Service::database_path() const {
  return project().path() / "data/comics.sqlite3";
}
std::unique_ptr<DB> Service::database(bool create) const {
  const auto path = database_path();
  if (!std::filesystem::exists(path) && !create)
    return {};
  const bool fresh = !std::filesystem::exists(path);
  auto db = std::make_unique<DB>(path);
  if (fresh) {
    TX tx(*db);
    db->execute(
        "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT "
        "NULL);CREATE TABLE comics(id TEXT PRIMARY KEY,revision INTEGER NOT "
        "NULL,removed INTEGER NOT NULL,payload TEXT NOT NULL);CREATE TABLE "
        "history(sequence INTEGER PRIMARY KEY,comic_id TEXT NOT "
        "NULL,before_payload TEXT NOT NULL,after_payload TEXT NOT "
        "NULL,created_at TEXT NOT NULL);CREATE TABLE preparations(id TEXT "
        "PRIMARY KEY,comic_id TEXT NOT NULL,site_id TEXT NOT NULL,revision "
        "INTEGER NOT NULL,site_revision INTEGER NOT NULL,payload_hash TEXT NOT "
        "NULL,payload TEXT NOT NULL);");
    auto q = db->prepare(
        "INSERT INTO metadata VALUES('schema','1'),('project_id',?)");
    q.bind(1, project().manifest().project_id);
    q.run();
    tx.commit();
  }
  need(db->query_text("SELECT value FROM metadata WHERE key='schema'") == "1",
       "Versão da extensão comic incompatível");
  need(db->query_text("SELECT value FROM metadata WHERE key='project_id'") ==
           project().manifest().project_id,
       "Comic pertence à origem da cópia; use Salvar cópia pelo INDE "
       "atualizado");
  return db;
}
std::vector<Book> Service::books(bool removed) const {
  auto db = database();
  if (!db)
    return {};
  auto q = db->prepare(
      removed ? "SELECT id FROM comics ORDER BY rowid"
              : "SELECT id FROM comics WHERE removed=0 ORDER BY rowid");
  std::vector<Book> out;
  while (q.step())
    out.push_back(load(*db, q.column_text(0)));
  return out;
}
Book Service::book(const std::string &id) const {
  auto db = database();
  need(bool(db), "Comic não encontrado");
  return load(*db, id);
}
void Service::validate_work(const Book &b) const {
  const auto &works = publication_.source().catalog().works;
  need(std::any_of(works.begin(), works.end(),
                   [&](const auto &w) { return w.id == b.work_id; }),
       "A Obra vinculada não existe neste Projeto");
}
Book Service::save(Book b, std::int64_t expected) {
  validate(b);
  validate_work(b);
  need(expected >= 0, "Revisão inválida");
  auto db = database(true);
  TX tx(*db);
  V before;
  if (expected == 0) {
    auto q = db->prepare("SELECT count(*) FROM comics WHERE id=?");
    q.bind(1, b.id);
    q.step();
    need(q.column_integer(0) == 0, "Comic já existe; recarregue");
    need(!b.removed && b.revision == 0, "Novo comic tem estado inválido");
    b.revision = 1;
    auto insert = db->prepare("INSERT INTO comics VALUES(?,1,0,?)");
    insert.bind(1, b.id);
    insert.bind(2, dump(encode(b)));
    insert.run();
  } else {
    const auto old = load(*db, b.id);
    need(old.revision == expected && b.revision == expected,
         "O comic mudou em outra sessão. Recarregue antes de salvar");
    need(!old.removed && !b.removed, "Restaure o comic antes de editar");
    need(old.work_id == b.work_id,
         "A Obra de um comic salvo não pode ser trocada neste corte");
    before = encode(old);
    b.revision++;
    write(*db, b);
  }
  audit(*db, b.id, before, encode(b));
  tx.commit();
  return b;
}
Book Service::retire(const std::string &id, std::int64_t expected,
                     bool removed) {
  auto db = database();
  need(bool(db), "Comic ausente");
  TX tx(*db);
  auto b = load(*db, id);
  need(b.revision == expected && b.removed != removed,
       "Comic mudou ou já está nesse estado");
  const auto before = encode(b);
  b.removed = removed;
  b.revision++;
  write(*db, b);
  audit(*db, id, before, encode(b));
  tx.commit();
  return b;
}
Prepared Service::prepare(const std::string &id, const std::string &site_id,
                          std::int64_t expected, std::int64_t site_revision,
                          const std::filesystem::path &resources) {
  auto b = book(id);
  validate_work(b);
  auto site = publication_.site(site_id);
  need(b.revision == expected && site.revision == site_revision,
       "A fonte mudou; recarregue antes de gerar");
  Prepared p;
  p.id = project::new_uuid();
  p.comic_id = id;
  p.site_id = site_id;
  p.revision = expected;
  p.site_revision = site_revision;
  p.payload = dump(public_snapshot(b, publication::encode(site), resources));
  p.payload_hash = publication::hash(p.payload);
  auto db = database();
  TX tx(*db);
  need(load(*db, id).revision == expected &&
           publication_.site(site_id).revision == site_revision,
       "A fonte mudou durante a preparação");
  auto q = db->prepare("INSERT INTO preparations VALUES(?,?,?,?,?,?,?)");
  q.bind(1, p.id);
  q.bind(2, id);
  q.bind(3, site_id);
  q.bind(4, expected);
  q.bind(5, site_revision);
  q.bind(6, p.payload_hash);
  q.bind(7, p.payload);
  q.run();
  tx.commit();
  return p;
}
bool Service::is_current(const Prepared &p) const {
  try {
    auto b = book(p.comic_id);
    validate_work(b);
    auto site = publication_.site(p.site_id);
    if (b.removed || site.removed || b.revision != p.revision ||
        site.revision != p.site_revision ||
        publication::hash(p.payload) != p.payload_hash)
      return false;
    auto db = database();
    auto q = db->prepare(
        "SELECT payload,comic_id,site_id,revision,site_revision,payload_hash "
        "FROM preparations WHERE id=?");
    q.bind(1, p.id);
    return q.step() && q.column_text(0) == p.payload &&
           q.column_text(1) == p.comic_id && q.column_text(2) == p.site_id &&
           q.column_integer(3) == p.revision &&
           q.column_integer(4) == p.site_revision &&
           q.column_text(5) == p.payload_hash;
  } catch (...) {
    return false;
  }
}
std::filesystem::path Service::materialize(const Prepared &p) const {
  need(is_current(p), "Preparação comic obsoleta ou adulterada");
  const auto directory = project().path() / "data/comic-retratos";
  std::filesystem::create_directories(directory);
  const auto path = directory / (p.id + ".json");
  need(!std::filesystem::exists(path), "Este retrato já foi materializado");
  std::ofstream out(path, std::ios::binary);
  out << p.payload;
  out.close();
  need(bool(out), "Falha ao gravar retrato comic");
  return path;
}
void Service::adopt_project_copy(const std::string &previous) {
  if (!std::filesystem::exists(database_path()))
    return;
  DB db(database_path());
  TX tx(db);
  need(db.query_text("SELECT value FROM metadata WHERE key='schema'") == "1" &&
           db.query_text("SELECT value FROM metadata WHERE key='project_id'") ==
               previous,
       "Comic copiado não corresponde à origem");
  auto q = db.prepare("UPDATE metadata SET value=? WHERE key='project_id'");
  q.bind(1, project().manifest().project_id);
  q.run();
  db.execute("DELETE FROM preparations");
  tx.commit();
}
} // namespace inde::comic

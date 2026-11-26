#include "project_file.hpp"
#include "inde/persistence/json.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <glib.h>
#include <limits>
#include <locale.h>
#include <set>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace inde::project_file {
using V = persistence::json::Value;
Error::Error(std::string c, std::string message)
    : std::runtime_error(std::move(message)), code(std::move(c)) {}
namespace {
[[noreturn]] void fail(const char *code, const std::string &message) {
  throw Error(code, message);
}
void need(bool yes, const char *code, const std::string &message) {
  if (!yes)
    fail(code, message);
}
void text(const std::string &s, std::size_t limit, bool empty = false) {
  need((empty || !s.empty()) && s.size() <= limit && s.find('\0') == s.npos &&
           g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr),
       "invalid_text", "Texto vazio, inválido ou maior que o limite.");
}
bool hash_ok(const std::string &s) {
  return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
std::string digest(const std::string &s) {
  auto *raw = g_compute_checksum_for_data(
      G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(s.data()), s.size());
  if (!raw)
    throw std::bad_alloc();
  std::string result(raw);
  g_free(raw);
  return result;
}
void encode(std::string &out, const V &v, unsigned depth) {
  need(depth < 128, "json_depth", "Estrutura de dados profunda demais.");
  const auto &s = v.storage();
  if (std::holds_alternative<std::nullptr_t>(s))
    out += "null";
  else if (const auto *b = std::get_if<bool>(&s))
    out += *b ? "true" : "false";
  else if (const auto *i = std::get_if<std::int64_t>(&s))
    out += std::to_string(*i);
  else if (const auto *d = std::get_if<double>(&s)) {
    need(std::isfinite(*d), "invalid_number", "Número não finito.");
    char buffer[128];
    const auto result = std::to_chars(
        buffer, buffer + sizeof(buffer), *d, std::chars_format::general,
        std::numeric_limits<double>::max_digits10);
    need(result.ec == std::errc{}, "invalid_number", "Número inválido.");
    out.append(buffer, result.ptr);
    // Preserve integer-looking reals, including negative zero, across parsing.
    const std::string number(buffer, result.ptr);
    if (number.find_first_of(".eE") == number.npos)
      out += ".0";
  } else if (const auto *svalue = std::get_if<std::string>(&s)) {
    text(*svalue, std::numeric_limits<std::size_t>::max(), true);
    out += persistence::json::serialize(V(*svalue), false);
  } else if (const auto *a = std::get_if<V::Array>(&s)) {
    out += '[';
    bool comma = false;
    for (const auto &x : *a) {
      if (comma)
        out += ',';
      comma = true;
      encode(out, x, depth + 1);
    }
    out += ']';
  } else {
    out += '{';
    bool comma = false;
    for (const auto &[key, value] : v.as_object()) {
      if (comma)
        out += ',';
      comma = true;
      text(key, std::numeric_limits<std::size_t>::max(), true);
      out += persistence::json::serialize(V(key), false);
      out += ':';
      encode(out, value, depth + 1);
    }
    out += '}';
  }
}
void sql(sqlite3 *db, const std::string &s) {
  char *message = nullptr;
  const int rc = sqlite3_exec(db, s.c_str(), nullptr, nullptr, &message);
  if (rc != SQLITE_OK) {
    const std::string error = message ? message : sqlite3_errmsg(db);
    sqlite3_free(message);
    fail("sqlite_error", error);
  }
}
struct Query {
  sqlite3_stmt *q{};
  Query(sqlite3 *db, const std::string &s) {
    if (sqlite3_prepare_v2(db, s.c_str(), -1, &q, nullptr) != SQLITE_OK)
      fail("sqlite_error", sqlite3_errmsg(db));
  }
  ~Query() { sqlite3_finalize(q); }
  void bind(int i, const std::string &s) {
    need(sqlite3_bind_text64(q, i, s.data(), s.size(), SQLITE_TRANSIENT,
                             SQLITE_UTF8) == SQLITE_OK,
         "sqlite_bind", "Falha ao preparar texto.");
  }
  void bind(int i, std::int64_t n) {
    need(sqlite3_bind_int64(q, i, n) == SQLITE_OK, "sqlite_bind",
         "Falha ao preparar número.");
  }
  void binary(int i, const Bytes &b) {
    static const std::uint8_t empty = 0;
    need(sqlite3_bind_blob64(q, i, b.empty() ? &empty : b.data(), b.size(),
                             SQLITE_TRANSIENT) == SQLITE_OK,
         "sqlite_bind", "Falha ao preparar recurso.");
  }
  void null(int i) {
    need(sqlite3_bind_null(q, i) == SQLITE_OK, "sqlite_bind",
         "Falha ao preparar ausência.");
  }
  bool step() {
    const int rc = sqlite3_step(q);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
      fail("sqlite_error", sqlite3_errmsg(sqlite3_db_handle(q)));
    return rc == SQLITE_ROW;
  }
  std::string str(int i) const {
    const auto *p = sqlite3_column_text(q, i);
    return p ? std::string(reinterpret_cast<const char *>(p),
                           static_cast<std::size_t>(sqlite3_column_bytes(q, i)))
             : std::string{};
  }
  std::int64_t integer(int i) const { return sqlite3_column_int64(q, i); }
};
std::int64_t integer(sqlite3 *db, const std::string &s) {
  Query q(db, s);
  need(q.step(), "missing_value", "Valor ausente.");
  return q.integer(0);
}
std::string schema() {
  std::string s = R"sql(
CREATE TABLE meta(id INTEGER PRIMARY KEY CHECK(id=1),project_id TEXT NOT NULL,store_id TEXT NOT NULL,format INTEGER NOT NULL CHECK(format=1)) STRICT;
CREATE TABLE blobs(hash TEXT PRIMARY KEY,size INTEGER NOT NULL CHECK(size>=0),data BLOB NOT NULL,CHECK(length(data)=size)) STRICT;
CREATE TABLE record_versions(id INTEGER PRIMARY KEY,key TEXT NOT NULL,kind TEXT NOT NULL,payload TEXT NOT NULL,blob_hash TEXT,UNIQUE(id,key),FOREIGN KEY(blob_hash) REFERENCES blobs(hash)) STRICT;
CREATE TABLE version_refs(version_id INTEGER NOT NULL,target_key TEXT NOT NULL,PRIMARY KEY(version_id,target_key),FOREIGN KEY(version_id) REFERENCES record_versions(id)) STRICT;
CREATE TABLE states(seq INTEGER PRIMARY KEY,parent INTEGER,restored_from INTEGER,hash TEXT NOT NULL,label TEXT NOT NULL,created_at TEXT NOT NULL,FOREIGN KEY(parent) REFERENCES states(seq),FOREIGN KEY(restored_from) REFERENCES states(seq)) STRICT;
CREATE TABLE entries(seq INTEGER NOT NULL,key TEXT NOT NULL,version_id INTEGER NOT NULL,PRIMARY KEY(seq,key),FOREIGN KEY(seq) REFERENCES states(seq),FOREIGN KEY(version_id,key) REFERENCES record_versions(id,key)) STRICT;
CREATE TABLE head(id INTEGER PRIMARY KEY CHECK(id=1),seq INTEGER NOT NULL,FOREIGN KEY(seq) REFERENCES states(seq)) STRICT;
CREATE INDEX entry_versions ON entries(version_id);
CREATE INDEX record_keys ON record_versions(key,id);
)sql";
  for (const auto *table : {"meta", "blobs", "record_versions", "version_refs",
                            "states", "entries"})
    for (const auto *op : {"UPDATE", "DELETE"})
      s += "CREATE TRIGGER immutable_" + std::string(table) + "_" + op +
           " BEFORE " + op + " ON " + table +
           " BEGIN SELECT RAISE(ABORT,'immutable history'); END;";
  s += "CREATE TRIGGER immutable_entries_insert BEFORE INSERT ON entries WHEN "
       "NEW.seq <= (SELECT seq FROM head WHERE id=1) BEGIN SELECT "
       "RAISE(ABORT,'published state'); END;";
  s += "CREATE TRIGGER immutable_refs_insert BEFORE INSERT ON version_refs "
       "WHEN EXISTS(SELECT 1 FROM entries WHERE version_id=NEW.version_id) "
       "BEGIN SELECT RAISE(ABORT,'published version'); END;";
  return s;
}
std::string schema_digest(sqlite3 *db) {
  Query q(db, "SELECT type,name,tbl_name,coalesce(sql,'') FROM sqlite_schema "
              "WHERE name NOT LIKE 'sqlite_%' ORDER BY type,name");
  std::string out;
  while (q.step())
    for (int i = 0; i < 4; ++i) {
      const auto part = q.str(i);
      out += std::to_string(part.size()) + ":" + part;
    }
  return digest(out);
}
std::string expected_schema() {
  sqlite3 *db{};
  need(sqlite3_open(":memory:", &db) == SQLITE_OK, "sqlite_open",
       "Falha ao validar formato.");
  try {
    sql(db, schema());
    auto result = schema_digest(db);
    sqlite3_close(db);
    return result;
  } catch (...) {
    sqlite3_close(db);
    throw;
  }
}
struct Transaction {
  sqlite3 *db;
  bool done{};
  explicit Transaction(sqlite3 *d) : db(d) { sql(db, "BEGIN IMMEDIATE"); }
  ~Transaction() {
    if (!done)
      sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
  }
};
std::string now() {
  auto *dt = g_date_time_new_now_utc();
  auto *s = g_date_time_format_iso8601(dt);
  std::string result(s);
  g_free(s);
  g_date_time_unref(dt);
  return result;
}
std::string state_hash(const State &s) {
  std::string out;
  const auto part = [&](const std::string &v) {
    out += std::to_string(v.size()) + ":" + v;
  };
  part(s.project_id);
  part(s.store_id);
  for (const auto &[key, r] : s.records) {
    part(key);
    part(r.kind);
    part(r.payload);
    part(r.blob);
    part(std::to_string(r.references.size()));
    for (const auto &ref : r.references)
      part(ref);
  }
  return digest(out);
}
bool same_record(const Record &a, const Record &b) {
  return a.kind == b.kind && a.payload == b.payload && a.blob == b.blob &&
         a.references == b.references;
}
// The pinned parser uses std::stod. Keep its decimal interpretation local to
// this thread and restore the caller's locale, including on parser failures.
struct NumericLocale {
  locale_t numeric{}, previous{};
  NumericLocale() {
    numeric = newlocale(LC_NUMERIC_MASK, "C", nullptr);
    need(numeric != nullptr, "numeric_locale", "Não foi possível ler números JSON.");
    previous = uselocale(numeric);
    if (!previous) {
      freelocale(numeric);
      fail("numeric_locale", "Não foi possível reservar a leitura numérica.");
    }
  }
  ~NumericLocale() {
    uselocale(previous);
    freelocale(numeric);
  }
  NumericLocale(const NumericLocale &) = delete;
  NumericLocale &operator=(const NumericLocale &) = delete;
};
} // namespace
std::string sha256(const Bytes &bytes) {
  return digest(bytes.empty()
                    ? std::string{}
                    : std::string(reinterpret_cast<const char *>(bytes.data()),
                                  bytes.size()));
}
std::string encode_json(const V &v) {
  std::string out;
  encode(out, v, 0);
  return out;
}
V parse_json(const std::string &s, std::size_t limit) {
  text(s, limit);
  // Bound nesting before the owner's recursive parser allocates its tree.
  unsigned depth = 0;
  bool string = false, escape = false;
  for (char c : s) {
    if (string) {
      if (escape)
        escape = false;
      else if (c == '\\')
        escape = true;
      else if (c == '"')
        string = false;
    } else if (c == '"')
      string = true;
    else if (c == '{' || c == '[') {
      need(++depth < 128, "json_depth", "Estrutura profunda demais.");
    } else if (c == '}' || c == ']') {
      if (depth)
        --depth;
    }
  }
  V value;
  NumericLocale numeric;
  try {
    value = persistence::json::parse(s);
  } catch (const std::exception &e) {
    fail("invalid_json", e.what());
  }
  need(std::holds_alternative<V::Object>(value.storage()), "payload_object",
       "A descrição do elemento precisa ser um objeto JSON.");
  return value;
}
std::string canonical_json(const std::string &s, std::size_t limit) {
  const auto value = parse_json(s, limit);
  std::string result;
  encode(result, value, 0);
  text(result, limit);
  return result;
}

struct Store::Impl {
  std::filesystem::path path;
  Options options;
  int fd{-1};
  sqlite3 *db{};
  dev_t device{};
  ino_t inode{};
  std::string project_id, store_id;
  pid_t owner_pid{getpid()};
  std::int64_t data_version{};
  bool poisoned{};
  Impl(const std::filesystem::path &p, Options o, bool create)
      : path(std::filesystem::absolute(p)), options(std::move(o)) {
    need(options.limits.store_bytes >= 1024 * 1024 &&
             options.limits.blob_bytes <= 64 * 1024 * 1024 &&
             options.limits.payload_bytes <= 1024 * 1024,
         "invalid_limits", "Limites fora do perfil suportado.");
    fd = ::open(path.c_str(),
                O_RDWR | O_CLOEXEC | O_NOFOLLOW |
                    (create ? O_CREAT | O_EXCL : 0),
                0600);
    need(fd >= 0, "file_open",
         "Não foi possível abrir o arquivo exclusivo .inde.");
    try {
      struct stat info {};
      need(fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_nlink == 1,
           "unsafe_file",
           "O projeto precisa ser um arquivo regular, sem links.");
      device = info.st_dev;
      inode = info.st_ino;
      if (!create) {
        std::array<unsigned char, 100> header{};
        need(pread(fd, header.data(), header.size(), 0) ==
                     static_cast<ssize_t>(header.size()) &&
                 std::memcmp(header.data(), "SQLite format 3\0", 16) == 0,
             "unknown_format",
             "O arquivo não tem um cabeçalho INDE-P5-1 reconhecido.");
        const auto number = [&](std::size_t at) {
          return (static_cast<std::uint32_t>(header[at]) << 24) |
                 (static_cast<std::uint32_t>(header[at + 1]) << 16) |
                 (static_cast<std::uint32_t>(header[at + 2]) << 8) |
                 header[at + 3];
        };
        need(header[18] == 1 && header[19] == 1, "unsupported_journal",
             "Este formato exige journal DELETE; reabra uma cópia compatível.");
        need(number(68) == 1229866033 && number(60) == 1, "unknown_format",
             "O arquivo não é do formato INDE-P5-1.");
      }

      need(static_cast<std::uint64_t>(info.st_size) <=
               options.limits.store_bytes,
           "store_limit", "Arquivo excede o limite deste perfil.");
      need(flock(fd, LOCK_EX | LOCK_NB) == 0, "store_busy",
           "Este arquivo .inde já está aberto para trabalho.");
      need(sqlite3_open_v2(path.c_str(), &db,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
                           nullptr) == SQLITE_OK,
           "sqlite_open", "Não foi possível abrir o banco .inde.");
      same_file();
      sqlite3_extended_result_codes(db, 1);
      sqlite3_busy_timeout(db, 1000);
      sqlite3_limit(db, SQLITE_LIMIT_LENGTH, 64 * 1024 * 1024 + 4096);
      sqlite3_limit(db, SQLITE_LIMIT_EXPR_DEPTH, 128);
      sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr);
      sql(db, "PRAGMA foreign_keys=ON; PRAGMA trusted_schema=OFF; PRAGMA "
              "synchronous=FULL");
      if (create)
        sql(db, "PRAGMA journal_mode=DELETE; PRAGMA application_id=1229866033; "
                "PRAGMA user_version=1");
      sql(db, "PRAGMA max_page_count=" +
                  std::to_string(options.limits.store_bytes /
                                 static_cast<std::uint64_t>(
                                     integer(db, "PRAGMA page_size"))));
    } catch (...) {
      if (db)
        sqlite3_close(db);
      db = nullptr;
      ::close(fd);
      fd = -1;
      throw;
    }
  }
  ~Impl() {
    if (db && owner_pid == getpid())
      sqlite3_close(db);
    if (fd >= 0)
      ::close(fd);
  }
  void same_file() const {
    struct stat s {};
    need(lstat(path.c_str(), &s) == 0 && S_ISREG(s.st_mode) &&
             s.st_dev == device && s.st_ino == inode && s.st_nlink == 1,
         "file_replaced",
         "O arquivo foi substituído ou movido; reabra antes de alterar.");
  }
  void ready() const {
    need(owner_pid == getpid(), "fork_inherited",
         "A sessão foi herdada por outro processo; abra uma sessão própria.");
    need(!poisoned, "reopen_required",
         "A confirmação foi incerta; reabra e confira o estado antes de "
         "continuar.");
    same_file();
    need(integer(db, "PRAGMA data_version") == data_version, "external_change",
         "O banco foi alterado fora desta sessão; reabra e confira.");
  }
  void bounds() const {
    const auto &l = options.limits;
    for (const auto &[table, limit] :
         std::vector<std::pair<std::string, std::size_t>>{
             {"states", l.states},
             {"record_versions", l.versions},
             {"entries", l.history_entries},
             {"version_refs", l.references}})
      need(integer(db, "SELECT count(*) FROM " + table) <=
               static_cast<std::int64_t>(limit),
           "history_limit",
           "Limite de versões atingido. As versões anteriores foram "
           "preservadas.");
  }
  State load(std::int64_t sequence) const {
    State result;
    result.project_id = project_id;
    result.store_id = store_id;
    result.sequence = sequence;
    Query state(db, "SELECT hash FROM states WHERE seq=?");
    state.bind(1, sequence);
    need(state.step(), "state_missing", "Estado reservado ausente.");
    result.hash = state.str(0);
    Query rows(db,
               "SELECT v.id,v.key,v.kind,v.payload,coalesce(v.blob_hash,'') "
               "FROM entries e JOIN record_versions v ON v.id=e.version_id AND "
               "v.key=e.key WHERE e.seq=? ORDER BY e.key");
    rows.bind(1, sequence);
    while (rows.step()) {
      need(result.records.size() < options.limits.entries, "entry_limit",
           "Estado excede o limite de elementos.");
      Record r;
      r.version = rows.integer(0);
      r.key = rows.str(1);
      r.kind = rows.str(2);
      r.payload = rows.str(3);
      r.blob = rows.str(4);
      text(r.key, 1024);
      text(r.kind, 64);
      need(canonical_json(r.payload, options.limits.payload_bytes) == r.payload,
           "payload_invalid", "Descrição de elemento inválida.");
      need(r.blob.empty() || hash_ok(r.blob), "blob_hash",
           "Identidade de recurso inválida.");
      Query refs(db, "SELECT target_key FROM version_refs WHERE version_id=? "
                     "ORDER BY target_key");
      refs.bind(1, r.version);
      while (refs.step()) {
        auto ref = refs.str(0);
        text(ref, 1024);
        r.references.push_back(std::move(ref));
      }
      result.records.emplace(r.key, std::move(r));
    }
    for (const auto &[key, r] : result.records) {
      static_cast<void>(key);
      for (const auto &ref : r.references)
        need(result.records.contains(ref), "dangling_reference",
             "Estado com referência ausente.");
    }
    need(hash_ok(result.hash) && state_hash(result) == result.hash,
         "state_hash",
         "O estado não coincide com sua conferência de integridade.");
    return result;
  }
  State current() const {
    return load(integer(db, "SELECT seq FROM head WHERE id=1"));
  }
  void expected(const State &s) const {
    const auto current_state = current();
    need(s.project_id == current_state.project_id &&
             s.store_id == current_state.store_id &&
             s.sequence == current_state.sequence &&
             s.hash == current_state.hash,
         "stale_state",
         "O projeto mudou desde a observação; nenhuma alteração foi aplicada.");
  }
  void publish(State &next, const std::string &label,
               std::optional<std::int64_t> restored, Transaction &tx) {
    text(label, 500);
    next.hash = state_hash(next);
    Query q(db, "INSERT INTO states VALUES(?,?,?,?,?,?)");
    q.bind(1, next.sequence);
    q.bind(2, next.sequence - 1);
    if (restored)
      q.bind(3, *restored);
    else
      q.null(3);
    q.bind(4, next.hash);
    q.bind(5, label);
    q.bind(6, now());
    q.step();
    for (const auto &[key, r] : next.records) {
      Query e(db, "INSERT INTO entries VALUES(?,?,?)");
      e.bind(1, next.sequence);
      e.bind(2, key);
      e.bind(3, r.version);
      e.step();
    }
    Query h(db, "UPDATE head SET seq=? WHERE id=1");
    h.bind(1, next.sequence);
    h.step();
    bounds();
    need(load(next.sequence).hash == next.hash, "state_hash",
         "Candidato inválido.");
    same_file();
    if (options.observe)
      options.observe(CommitStage::BeforeCommit);
    try {
      sql(db, "COMMIT");
      tx.done = true;
    } catch (...) {
      poisoned = true;
      fail("commit_uncertain", "Não foi possível confirmar a gravação. "
                               "Preserve o arquivo e reabra para conferir.");
    }
    if (options.observe) {
      try {
        options.observe(CommitStage::AfterCommit);
      } catch (...) {
        poisoned = true;
        fail("commit_uncertain", "A gravação ocorreu, mas a confirmação foi "
                                 "interrompida; reabra para conferir.");
      }
    }
  }
};

Store::Store(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Store::Store(Store &&) noexcept = default;
Store &Store::operator=(Store &&) noexcept = default;
Store::~Store() = default;
Store Store::create(const std::filesystem::path &path,
                    const std::string &project, Options options) {
  need(g_uuid_string_is_valid(project.c_str()), "project_identity",
       "Identidade de projeto inválida.");
  auto p = std::make_unique<Impl>(path, std::move(options), true);
  Transaction tx(p->db);
  sql(p->db, schema());
  auto *uuid = g_uuid_string_random();
  const std::string store_id(uuid);
  g_free(uuid);
  Query meta(p->db, "INSERT INTO meta VALUES(1,?,?,1)");
  meta.bind(1, project);
  meta.bind(2, store_id);
  meta.step();
  State empty;
  empty.project_id = project;
  empty.store_id = store_id;
  empty.hash = state_hash(empty);
  Query initial(p->db, "INSERT INTO states VALUES(0,NULL,NULL,?,'Início',?)");
  initial.bind(1, empty.hash);
  initial.bind(2, now());
  initial.step();
  sql(p->db, "INSERT INTO head VALUES(1,0)");
  sql(p->db, "COMMIT");
  tx.done = true;
  p->project_id = project;
  p->store_id = store_id;
  p->data_version = integer(p->db, "PRAGMA data_version");
  Store result(std::move(p));
  result.verify();
  need(fsync(result.impl_->fd) == 0, "new_file_sync",
       "O arquivo novo não pôde ser confirmado.");
  const int directory = ::open(result.impl_->path.parent_path().c_str(),
                               O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  need(directory >= 0, "new_file_sync",
       "O diretório do arquivo novo não pôde ser aberto.");
  const int sync_result = fsync(directory);
  ::close(directory);
  need(sync_result == 0, "new_file_sync",
       "A confirmação do nome do arquivo foi incerta; preserve-o e confira.");
  return result;
}
Store Store::open(const std::filesystem::path &path, Options options) {
  auto p = std::make_unique<Impl>(path, std::move(options), false);
  need(integer(p->db, "PRAGMA application_id") == 1229866033 &&
           integer(p->db, "PRAGMA user_version") == 1,
       "unknown_format",
       "Este arquivo não é um projeto INDE-P5-1 reconhecido.");
  need(schema_digest(p->db) == expected_schema(), "schema_changed",
       "A estrutura do arquivo diverge do formato reconhecido.");
  Query meta(p->db, "SELECT project_id,store_id FROM meta WHERE id=1");
  need(meta.step(), "project_identity", "Identificação ausente.");
  p->project_id = meta.str(0);
  p->store_id = meta.str(1);
  need(g_uuid_string_is_valid(p->project_id.c_str()) &&
           g_uuid_string_is_valid(meta.str(1).c_str()),
       "project_identity", "Identificação inválida.");
  p->data_version = integer(p->db, "PRAGMA data_version");
  Store result(std::move(p));
  result.verify();
  return result;
}
State Store::head() const {
  impl_->ready();
  return impl_->current();
}
State Store::state(std::int64_t sequence) const {
  impl_->ready();
  return impl_->load(sequence);
}
std::vector<Revision> Store::history() const {
  impl_->ready();
  std::vector<Revision> result;
  Query q(impl_->db, "SELECT seq,parent,restored_from,hash,label,created_at "
                     "FROM states ORDER BY seq");
  while (q.step()) {
    Revision r;
    r.sequence = q.integer(0);
    if (sqlite3_column_type(q.q, 1) != SQLITE_NULL)
      r.parent = q.integer(1);
    if (sqlite3_column_type(q.q, 2) != SQLITE_NULL)
      r.restored_from = q.integer(2);
    r.hash = q.str(3);
    r.label = q.str(4);
    r.created_at = q.str(5);
    result.push_back(std::move(r));
  }
  return result;
}
Bytes Store::blob(const std::string &hash) const {
  impl_->ready();
  need(hash_ok(hash), "blob_hash", "Identidade de recurso inválida.");
  Query q(impl_->db, "SELECT size,data FROM blobs WHERE hash=?");
  q.bind(1, hash);
  need(q.step(), "blob_missing", "Recurso ausente.");
  const auto count = sqlite3_column_bytes(q.q, 1);
  need(count >= 0 &&
           static_cast<std::size_t>(count) <=
               impl_->options.limits.blob_bytes &&
           q.integer(0) == count,
       "blob_size", "Tamanho de recurso inválido.");
  const auto *data =
      static_cast<const std::uint8_t *>(sqlite3_column_blob(q.q, 1));
  Bytes result;
  if (count)
    result.assign(data, data + count);
  need(sha256(result) == hash, "blob_corrupt",
       "Recurso não coincide com sua conferência de integridade.");
  return result;
}
void Store::verify() const {
  impl_->ready();
  need(schema_digest(impl_->db) == expected_schema(), "schema_changed",
       "Estrutura de armazenamento alterada.");
  Query integrity(impl_->db, "PRAGMA integrity_check");
  need(integrity.step() && integrity.str(0) == "ok" && !integrity.step(),
       "integrity_error", "A integridade do banco não foi confirmada.");
  Query fk(impl_->db, "PRAGMA foreign_key_check");
  need(!fk.step(), "foreign_key", "Há referências de banco inválidas.");
  impl_->bounds();
  const auto list = history();
  need(!list.empty() && list.front().sequence == 0, "history_invalid",
       "Histórico inicial ausente.");
  for (std::size_t i = 0; i < list.size(); ++i) {
    const auto &r = list[i];
    text(r.label, 500);
    text(r.created_at, 128);
    need(r.sequence == static_cast<std::int64_t>(i) &&
             (i ? r.parent == std::optional<std::int64_t>(r.sequence - 1)
                : !r.parent) &&
             (!r.restored_from ||
              (*r.restored_from >= 0 && *r.restored_from < r.sequence)),
         "history_invalid", "Encadeamento de versões inválido.");
    impl_->load(r.sequence);
  }
  need(integer(impl_->db, "SELECT seq FROM head WHERE id=1") ==
           list.back().sequence,
       "history_invalid", "O estado atual não é a última versão confirmada.");
  Query blobs(impl_->db, "SELECT hash FROM blobs");
  while (blobs.step())
    static_cast<void>(blob(blobs.str(0)));
  need(integer(impl_->db,
               "SELECT count(*) FROM record_versions v WHERE NOT EXISTS(SELECT "
               "1 FROM entries e WHERE e.version_id=v.id)") == 0,
       "orphan_version", "Versão sem estado confirmado.");
  need(integer(impl_->db,
               "SELECT count(*) FROM blobs b WHERE NOT EXISTS(SELECT 1 FROM "
               "record_versions v WHERE v.blob_hash=b.hash)") == 0,
       "orphan_blob", "Recurso sem versão identificada.");
}
State Store::apply(const State &expected, const std::vector<Edit> &edits,
                   const std::vector<Blob> &blobs, const std::string &label) {
  impl_->ready();
  Transaction tx(impl_->db);
  impl_->expected(expected);
  State next = impl_->current();
  need(next.sequence < std::numeric_limits<std::int64_t>::max(),
       "revision_limit", "Limite de revisões atingido.");
  ++next.sequence;
  std::set<std::string> keys;
  std::map<std::string, const Bytes *, std::less<>> supplied;
  for (const auto &b : blobs) {
    need(b.bytes.size() <= impl_->options.limits.blob_bytes &&
             hash_ok(b.hash) && sha256(b.bytes) == b.hash,
         "blob_invalid", "Recurso inválido ou maior que o limite.");
    need(supplied.emplace(b.hash, &b.bytes).second, "duplicate_blob",
         "Recurso repetido no pedido.");
  }
  for (const auto &edit : edits) {
    text(edit.key, 1024);
    need(keys.insert(edit.key).second, "duplicate_edit",
         "Identidade repetida no mesmo pedido.");
    const auto before = next.records.find(edit.key);
    const auto old = before == next.records.end() ? 0 : before->second.version;
    need(edit.expected_version == old, "stale_record",
         "O elemento mudou ou não tem a ausência esperada.");
    if (!edit.value) {
      need(before != next.records.end(), "record_missing", "Elemento ausente.");
      next.records.erase(before);
      continue;
    }
    auto r = *edit.value;
    need(r.key == edit.key, "record_identity",
         "Identidades de alteração incompatíveis.");
    text(r.kind, 64);
    r.payload = canonical_json(r.payload, impl_->options.limits.payload_bytes);
    need(r.blob.empty() || hash_ok(r.blob), "blob_hash", "Recurso inválido.");
    std::sort(r.references.begin(), r.references.end());
    need(std::adjacent_find(r.references.begin(), r.references.end()) ==
             r.references.end(),
         "duplicate_reference", "Vínculo repetido.");
    for (const auto &ref : r.references)
      text(ref, 1024);
    Query kind(impl_->db,
               "SELECT kind FROM record_versions WHERE key=? LIMIT 1");
    kind.bind(1, r.key);
    if (kind.step())
      need(kind.str(0) == r.kind, "kind_changed",
           "O tipo de uma identidade existente não pode ser trocado.");
    if (before != next.records.end() && same_record(before->second, r)) {
      r.version = old;
      next.records[edit.key] = std::move(r);
      continue;
    }
    r.version = 0;
    next.records[edit.key] = std::move(r);
  }
  need(next.records.size() <= impl_->options.limits.entries, "entry_limit",
       "Estado excede o limite de elementos.");
  for (const auto &[key, r] : next.records) {
    static_cast<void>(key);
    for (const auto &ref : r.references)
      need(next.records.contains(ref), "dangling_reference",
           "Uma alteração deixaria referência sem destino; nada foi aplicado.");
  }
  if (state_hash(next) == expected.hash)
    return impl_->current();
  for (auto &[key, r] : next.records) {
    static_cast<void>(key);
    if (r.version)
      continue;
    if (!r.blob.empty()) {
      Query exists(impl_->db, "SELECT 1 FROM blobs WHERE hash=?");
      exists.bind(1, r.blob);
      if (!exists.step()) {
        const auto b = supplied.find(r.blob);
        need(b != supplied.end(), "blob_missing",
             "Bytes do recurso não foram fornecidos.");
        Query put(impl_->db, "INSERT INTO blobs VALUES(?,?,?)");
        put.bind(1, r.blob);
        put.bind(2, static_cast<std::int64_t>(b->second->size()));
        put.binary(3, *b->second);
        put.step();
      }
    }
    Query put(impl_->db,
              "INSERT INTO record_versions(key,kind,payload,blob_hash) "
              "VALUES(?,?,?,?)");
    put.bind(1, r.key);
    put.bind(2, r.kind);
    put.bind(3, r.payload);
    if (r.blob.empty())
      put.null(4);
    else
      put.bind(4, r.blob);
    put.step();
    r.version = sqlite3_last_insert_rowid(impl_->db);
    for (const auto &ref : r.references) {
      Query link(impl_->db, "INSERT INTO version_refs VALUES(?,?)");
      link.bind(1, r.version);
      link.bind(2, ref);
      link.step();
    }
  }
  impl_->publish(next, label, std::nullopt, tx);
  return next;
}
State Store::restore(const State &expected, std::int64_t sequence,
                     const std::string &label) {
  impl_->ready();
  Transaction tx(impl_->db);
  impl_->expected(expected);
  auto next = impl_->load(sequence);
  if (next.hash == expected.hash)
    return impl_->current();
  need(expected.sequence < std::numeric_limits<std::int64_t>::max(),
       "revision_limit", "Limite de revisões atingido.");
  next.sequence = expected.sequence + 1;
  impl_->publish(next, label, sequence, tx);
  return next;
}
} // namespace inde::project_file

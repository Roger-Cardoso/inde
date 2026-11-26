#include "project_file_transport.hpp"
#include "inde/persistence/json.hpp"
#include "project_file_operation.hpp"
#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <glib.h>
#include <limits>
#include <set>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>

namespace inde::project_file {
namespace fs = std::filesystem;
using V = persistence::json::Value;
namespace {
constexpr std::uint64_t max_bytes = 128ULL * 1024 * 1024;
constexpr std::size_t max_files = 4096;
constexpr std::size_t block_bytes = 65536;
[[noreturn]] void fail(const std::string &code, const std::string &message) {
  throw Error(code, message);
}
void need(bool yes, const std::string &code, const std::string &message) {
  if (!yes)
    fail(code, message);
}
std::string json(const V &v) { return canonical_json(encode_json(v)); }
std::string name(const fs::path &p) { return p.generic_string(); }
bool below(const fs::path &path, const fs::path &base) {
  auto a = path.begin();
  for (auto b = base.begin(); b != base.end(); ++b, ++a)
    if (a == path.end() || *a != *b)
      return false;
  return true;
}
void safe_relative(const std::string &s) {
  need(!s.empty() && s.size() <= 900 &&
           g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr) &&
           s.find_first_of("\\:\0", 0, 3) == s.npos,
       "unsafe_path", "Caminho de componente inválido.");
  fs::path p(s);
  need(!p.is_absolute() && name(p.lexically_normal()) == s, "unsafe_path",
       "Caminho não canônico.");
  for (const auto &part : p)
    need(part != "." && part != ".." && !part.empty(), "unsafe_path",
         "Caminho fora do projeto.");
}
Bytes read_file(const fs::path &path,
                std::size_t prefix = std::numeric_limits<std::size_t>::max(),
                OperationControl *control = nullptr) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  need(fd >= 0, "source_read", "Não foi possível ler um componente.");
  try {
    struct stat st {};
    need(fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1 &&
             st.st_size >= 0 &&
             static_cast<std::uint64_t>(st.st_size) <= 64ULL * 1024 * 1024,
         "source_file", "Componente excessivo, com links ou inválido.");
    Bytes result(std::min(static_cast<std::size_t>(st.st_size), prefix));
    std::size_t at = 0;
    while (at < result.size()) {
      if (control)
        control->check();
      const auto n = ::read(fd, result.data() + at,
                            std::min<std::size_t>(65536, result.size() - at));
      need(n > 0, "source_read", "Leitura incompleta.");
      at += static_cast<std::size_t>(n);
    }
    std::uint8_t tail{};
    need((result.size() < static_cast<std::size_t>(st.st_size)) ||
             ::read(fd, &tail, 1) == 0,
         "source_changed", "O componente cresceu durante a leitura.");
    struct stat end {};
    need(fstat(fd, &end) == 0 && end.st_size == st.st_size &&
             end.st_mtim.tv_sec == st.st_mtim.tv_sec &&
             end.st_mtim.tv_nsec == st.st_mtim.tv_nsec,
         "source_changed", "O componente mudou durante a leitura.");
    ::close(fd);
    return result;
  } catch (...) {
    ::close(fd);
    throw;
  }
}
std::string checked_hash(const Bytes &bytes, OperationControl *control) {
  if (!control)
    return sha256(bytes);
  const auto checksum = std::unique_ptr<GChecksum, decltype(&g_checksum_free)>(
      g_checksum_new(G_CHECKSUM_SHA256), &g_checksum_free);
  need(bool(checksum), "checksum", "Não foi possível conferir os dados.");
  for (std::size_t at = 0; at < bytes.size(); at += 65536) {
    control->check();
    g_checksum_update(
        checksum.get(), bytes.data() + at,
        static_cast<gssize>(std::min<std::size_t>(65536, bytes.size() - at)));
  }
  control->check();
  return g_checksum_get_string(checksum.get());
}
std::string string_bytes(const Bytes &b) {
  return b.empty()
             ? std::string{}
             : std::string(reinterpret_cast<const char *>(b.data()), b.size());
}
struct ReadDb {
  sqlite3 *db{};
  explicit ReadDb(const fs::path &path) {
    try {
      need(sqlite3_open_v2(path.c_str(), &db,
                           SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
                           nullptr) == SQLITE_OK,
           "source_sqlite", "Banco de origem não pôde ser lido.");
      sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr);
      need(sqlite3_exec(
               db, "PRAGMA trusted_schema=OFF; PRAGMA query_only=ON; BEGIN",
               nullptr, nullptr, nullptr) == SQLITE_OK,
           "source_sqlite", "Não foi possível reservar a leitura da origem.");
      need(query("PRAGMA journal_mode") == "delete", "source_wal",
           "Este primeiro conversor requer uma cópia fechada com journal "
           "DELETE; não converte WAL automaticamente.");
      static_cast<void>(query("SELECT count(*) FROM sqlite_schema"));
      need(query("PRAGMA integrity_check") == "ok", "source_integrity",
           "Banco de origem com integridade inválida.");
      sqlite3_stmt *q{};
      need(sqlite3_prepare_v2(db, "PRAGMA foreign_key_check", -1, &q,
                              nullptr) == SQLITE_OK,
           "source_integrity", "Não foi possível conferir vínculos da origem.");
      const auto rc = sqlite3_step(q);
      sqlite3_finalize(q);
      need(rc == SQLITE_DONE, "source_foreign_key",
           "Vínculos inválidos no banco de origem.");
    } catch (...) {
      sqlite3_close(db);
      db = nullptr;
      throw;
    }
  }
  ~ReadDb() {
    if (db)
      sqlite3_close(db);
  }
  ReadDb(const ReadDb &) = delete;
  ReadDb &operator=(const ReadDb &) = delete;
  std::string query(const char *sql) {
    sqlite3_stmt *q{};
    need(sqlite3_prepare_v2(db, sql, -1, &q, nullptr) == SQLITE_OK,
         "source_sqlite", "Consulta de origem incompatível.");
    const int rc = sqlite3_step(q);
    std::string result;
    if (rc == SQLITE_ROW) {
      const auto *s = sqlite3_column_text(q, 0);
      if (s)
        result = reinterpret_cast<const char *>(s);
    }
    sqlite3_finalize(q);
    need(rc == SQLITE_ROW, "source_sqlite", "Metadado de origem ausente.");
    return result;
  }
};
struct Captured {
  std::string owner;
  std::map<std::string, Record, std::less<>> records;
  std::vector<Blob> blobs;
  std::map<std::string, std::string, std::less<>> witness;
  std::vector<std::unique_ptr<ReadDb>> locks;
  fs::path source;
  std::map<std::string, bool, std::less<>> components;
};
std::map<std::string, bool, std::less<>> inventory(const fs::path &root) {
  std::map<std::string, bool, std::less<>> result;
  need(fs::is_directory(fs::symlink_status(root)), "source_directory",
       "A origem precisa ser uma pasta de projeto, sem link simbólico.");
  for (const auto &entry : fs::recursive_directory_iterator(root)) {
    const auto relative = name(entry.path().lexically_relative(root));
    safe_relative(relative);
    const auto status = entry.symlink_status();
    need(fs::is_regular_file(status) || fs::is_directory(status),
         "unsafe_source", "A origem contém link ou componente não regular.");
    need(!relative.ends_with("-wal") && !relative.ends_with("-shm") &&
             !relative.ends_with("-journal"),
         "source_active",
         "Há arquivo de trabalho SQLite. Feche e confira uma cópia antes da "
         "conversão.");
    need(result.size() < max_files, "file_limit",
         "Quantidade de componentes excede este perfil.");
    result.emplace(relative, fs::is_directory(status));
  }
  return result;
}
std::string parent_key(const std::string &path) {
  const auto parent = name(fs::path(path).parent_path());
  return parent.empty() ? "project" : "dir/" + parent;
}
Captured capture(const fs::path &given, const fs::path &store,
                 TransportProfile profile,
                 OperationControl *control = nullptr) {
  need(!fs::is_symlink(fs::symlink_status(given)), "unsafe_source",
       "Origem simbólica não aceita.");
  Captured c;
  c.source = fs::canonical(given);
  const auto destination =
      fs::weakly_canonical(fs::absolute(store).parent_path()) /
      store.filename();
  need(!below(destination, c.source), "destination_inside_source",
       "O arquivo de destino precisa ficar fora da origem.");
  if (control)
    control->checkpoint(OperationPhase::Reading);
  const auto list = inventory(c.source);
  c.components = list;
  need(list.contains("manifest.json") && !list.at("manifest.json") &&
           list.contains("data/project.sqlite3"),
       "source_project", "Manifesto ou banco canônico ausente.");
  // Hold read transactions on all rollback-journal databases for a coherent
  // cut. No source connection has write access, and journals are never
  // discarded.
  for (const auto &[path, directory] : list)
    if (!directory) {
      const auto raw = read_file(c.source / path, 100, control);
      if (raw.size() >= 16 &&
          string_bytes(Bytes(raw.begin(), raw.begin() + 16)) ==
              std::string("SQLite format 3\0", 16)) {
        need(raw.size() >= 100 && raw[18] == 1 && raw[19] == 1, "source_wal",
             "O banco usa WAL ou outro modo não aceito neste conversor. Use "
             "uma cópia fechada e normalizada; a origem não será alterada.");
        c.locks.push_back(std::make_unique<ReadDb>(c.source / path));
      }
    }
  need(inventory(c.source) == list, "source_changed",
       "O inventário mudou durante a captura.");
  std::uint64_t total = 0;
  std::set<std::string> seen_blobs;
  for (const auto &[path, directory] : list) {
    Record r;
    r.key = (directory ? "dir/" : "file/") + path;
    r.kind = directory                             ? "legacy-directory"
             : profile == TransportProfile::Blocks ? "chunked-file"
                                                   : "legacy-file";
    r.references = {parent_key(path)};
    V::Object body{{"path", path}};
    if (!directory) {
      auto raw = read_file(c.source / path,
                           std::numeric_limits<std::size_t>::max(), control);
      total += raw.size();
      need(total <= max_bytes, "project_limit",
           "A cópia excede 128 MiB neste primeiro perfil.");
      const auto full_hash = checked_hash(raw, control);
      c.witness[path] = full_hash;
      body["bytes"] = static_cast<std::int64_t>(raw.size());
      if (path == "manifest.json") {
        const auto manifest = parse_json(string_bytes(raw));
        need(manifest.at("format").as_string() == "INDE" &&
                 manifest.at("format_version").as_integer() == 2,
             "source_format", "Formato de pasta não reconhecido.");
        c.owner = manifest.at("project_id").as_string();
      }
      if (profile == TransportProfile::WholeFile) {
        r.blob = full_hash;
        if (seen_blobs.insert(r.blob).second)
          c.blobs.push_back({r.blob, std::move(raw)});
      } else {
        V::Array parts;
        std::set<std::string> refs{parent_key(path)};
        for (std::size_t at = 0; at < raw.size(); at += block_bytes) {
          const auto last = std::min(at + block_bytes, raw.size());
          Bytes data(raw.begin() + static_cast<std::ptrdiff_t>(at),
                     raw.begin() + static_cast<std::ptrdiff_t>(last));
          const auto hash = checked_hash(data, control);
          const auto key = "chunk/" + hash;
          parts.emplace_back(hash);
          refs.insert(key);
          Record chunk{key,
                       "transport-chunk",
                       json(V::Object{
                           {"bytes", static_cast<std::int64_t>(data.size())}}),
                       hash,
                       {},
                       0};
          c.records.emplace(key, std::move(chunk));
          if (seen_blobs.insert(hash).second)
            c.blobs.push_back({hash, std::move(data)});
        }
        body["sha256"] = full_hash;
        body["chunks"] = std::move(parts);
        r.references.assign(refs.begin(), refs.end());
      }
    }
    r.payload = json(body);
    c.records.emplace(r.key, std::move(r));
  }
  need(g_uuid_string_is_valid(c.owner.c_str()), "source_identity",
       "Identidade da origem inválida.");
  // Validate the schema/owner directly, without opening/migrating
  // ProjectService.
  ReadDb canonical(c.source / "data/project.sqlite3");
  need(canonical.query("SELECT max(version) FROM schema_migrations") == "37" &&
           canonical.query("SELECT count(*) FROM projects") == "1" &&
           canonical.query("SELECT id FROM projects") == c.owner,
       "source_identity", "Schema ou identidade canônica incompatível.");
  Record root;
  root.key = "project";
  root.kind = profile == TransportProfile::Blocks ? "chunked-project"
                                                  : "legacy-project";
  root.payload = json(V::Object{{"format", profile == TransportProfile::Blocks
                                               ? "INDE-DIRECTORY-37-CHUNKS-1"
                                               : "INDE-DIRECTORY-37"},
                                {"project_id", c.owner}});
  root.references = {"file/data/project.sqlite3", "file/manifest.json"};
  c.records.emplace(root.key, std::move(root));
  return c;
}
void unchanged(const Captured &c, OperationControl *control = nullptr) {
  if (control)
    control->checkpoint(OperationPhase::Checking);
  const auto list = inventory(c.source);
  need(list == c.components, "source_changed", "O inventário de origem mudou.");
  for (const auto &[path, directory] : list) {
    need(c.records.contains((directory ? "dir/" : "file/") + path),
         "source_changed", "A origem mudou de tipo ou caminho.");
    if (!directory)
      need(c.witness.contains(path) &&
               checked_hash(read_file(c.source / path,
                                      std::numeric_limits<std::size_t>::max(),
                                      control),
                            control) == c.witness.at(path),
           "source_changed",
           "O conteúdo da origem mudou. Nenhum estado foi confirmado.");
  }
}
std::vector<Edit> edits(const State &old, const Captured &next) {
  std::vector<Edit> result;
  for (const auto &[key, r] : next.records) {
    const auto at = old.records.find(key);
    if (at != old.records.end() && at->second.kind == r.kind &&
        at->second.payload == r.payload && at->second.blob == r.blob &&
        at->second.references == r.references)
      continue;
    result.push_back(
        {key, at == old.records.end() ? 0 : at->second.version, r});
  }
  for (const auto &[key, r] : old.records)
    if (!next.records.contains(key))
      result.push_back({key, r.version, std::nullopt});
  return result;
}
State publish_capture(Store &store, const State &old, Captured &captured,
                      const std::string &label,
                      OperationControl *control = nullptr) {
  const auto changes = edits(old, captured);
  std::set<std::string> needed;
  for (const auto &change : changes)
    if (change.value && !change.value->blob.empty())
      needed.insert(change.value->blob);
  std::vector<Blob> supplied;
  for (auto &b : captured.blobs)
    if (needed.contains(b.hash))
      supplied.push_back(std::move(b));
  return store.apply(old, changes, supplied, label, control);
}
void sync_dir(const fs::path &p) {
  const int fd = ::open(p.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  need(fd >= 0, "directory_sync", "Não foi possível conferir o diretório.");
  const int rc = fsync(fd);
  ::close(fd);
  need(rc == 0, "directory_sync", "Não foi possível confirmar o diretório.");
}
void write_new(const fs::path &p, const Bytes &b) {
  const int fd = ::open(
      p.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
  need(fd >= 0, "destination_write",
       "Não foi possível criar um componente novo.");
  try {
    std::size_t at = 0;
    while (at < b.size()) {
      const auto n = ::write(fd, b.data() + at, b.size() - at);
      need(n > 0, "destination_write", "Gravação incompleta.");
      at += static_cast<std::size_t>(n);
    }
    need(fsync(fd) == 0, "destination_sync", "Gravação não foi confirmada.");
    ::close(fd);
  } catch (...) {
    ::close(fd);
    throw;
  }
}
} // namespace
TransportProfile directory_profile(const State &state) {
  need(state.records.contains("project") &&
           (state.records.at("project").kind == "legacy-project" ||
            state.records.at("project").kind == "chunked-project"),
       "transport_profile", "Estado sem perfil de transporte reconhecido.");
  const auto root = parse_json(state.records.at("project").payload);
  need(root.as_object().size() == 2 &&
           root.at("project_id").as_string() == state.project_id &&
           state.records.at("project").blob.empty() &&
           state.records.at("project").references ==
               std::vector<std::string>{"file/data/project.sqlite3",
                                        "file/manifest.json"},
       "transport_profile", "Identidade de transporte inválida.");
  const auto format = root.at("format").as_string();
  need(format == "INDE-DIRECTORY-37" || format == "INDE-DIRECTORY-37-CHUNKS-1",
       "transport_profile", "Perfil de transporte não reconhecido.");
  need(state.records.at("project").kind == (format == "INDE-DIRECTORY-37"
                                                ? "legacy-project"
                                                : "chunked-project"),
       "transport_profile", "Tipo e perfil do projeto divergem.");
  return format == "INDE-DIRECTORY-37" ? TransportProfile::WholeFile
                                       : TransportProfile::Blocks;
}
bool directory_matches(Store &store, const State &state, const fs::path &source,
                       const fs::path &file) {
  need(store.head().store_id == state.store_id, "stale_state",
       "Outro armazenamento.");
  const auto captured = capture(source, file, directory_profile(state));
  unchanged(captured);
  if (captured.owner != state.project_id ||
      captured.records.size() != state.records.size())
    return false;
  for (const auto &[key, r] : captured.records) {
    const auto at = state.records.find(key);
    if (at == state.records.end() || at->second.kind != r.kind ||
        at->second.payload != r.payload || at->second.blob != r.blob ||
        at->second.references != r.references)
      return false;
  }
  return true;
}
State pack_directory(const fs::path &source, const fs::path &new_file,
                     TransportProfile profile) {
  need(!fs::exists(fs::symlink_status(new_file)), "destination_exists",
       "O destino precisa ser um arquivo novo.");
  auto captured = capture(source, new_file, profile);
  unchanged(captured);
  auto *uuid = g_uuid_string_random();
  const auto pending = fs::absolute(new_file).parent_path() /
                       (".inde-p5-file-pending-" + std::string(uuid));
  g_free(uuid);
  State result;
  {
    auto store = Store::create(pending, captured.owner);
    const auto old = store.head();
    unchanged(captured);
    result = publish_capture(store, old, captured,
                             "Importar cópia da pasta de projeto");
    store.verify();
  }
  need(::renameat2(AT_FDCWD, pending.c_str(), AT_FDCWD, new_file.c_str(),
                   RENAME_NOREPLACE) == 0,
       "destination_publish",
       "O arquivo foi verificado, mas o destino não pôde ser publicado sem "
       "substituição. A cópia pendente foi preservada.");
  sync_dir(fs::absolute(new_file).parent_path());
  return result;
}
State update_directory(Store &store, const State &expected,
                       const fs::path &source, const fs::path &store_path,
                       const std::string &label, OperationControl *control) {
  auto captured =
      capture(source, store_path, directory_profile(expected), control);
  need(captured.owner == expected.project_id, "source_identity",
       "A pasta pertence a outro projeto.");
  unchanged(captured, control);
  return publish_capture(store, expected, captured, label, control);
}
void unpack_directory(Store &store, std::int64_t sequence,
                      const fs::path &given) {
  store.verify();
  const auto state = store.state(sequence);
  const auto target = fs::absolute(given);
  need(!fs::exists(fs::symlink_status(target)), "destination_exists",
       "A reabertura exige uma pasta nova; nada será sobrescrito.");
  const auto profile = directory_profile(state);
  std::map<std::string, const Record *, std::less<>> files, dirs, chunks;
  std::set<std::string> used_chunks;
  std::uint64_t total = 0;
  for (const auto &[key, r] : state.records) {
    if (key == "project")
      continue;
    if (r.kind == "transport-chunk" && profile == TransportProfile::Blocks) {
      const auto size = parse_json(r.payload);
      need(!r.blob.empty() && key == "chunk/" + r.blob &&
               size.as_object().size() == 1 &&
               size.at("bytes").as_integer() > 0 &&
               size.at("bytes").as_integer() <=
                   static_cast<std::int64_t>(block_bytes) &&
               r.references.empty(),
           "transport_profile", "Bloco inválido.");
      chunks.emplace(key, &r);
      continue;
    }
    const bool directory = r.kind == "legacy-directory";
    need(directory ||
             r.kind == (profile == TransportProfile::Blocks ? "chunked-file"
                                                            : "legacy-file"),
         "transport_profile",
         "Estado contém elementos que este conversor ainda não materializa.");
    const auto body = parse_json(r.payload);
    const auto path = body.at("path").as_string();
    safe_relative(path);
    need(key == (directory ? "dir/" : "file/") + path, "transport_profile",
         "Vínculos de pasta inválidos.");
    if (directory)
      need(r.blob.empty(), "transport_profile",
           "Diretório não pode carregar bytes de arquivo.");
    else {
      const auto size = body.at("bytes").as_integer();
      need(size >= 0 && size <= 64LL * 1024 * 1024 &&
               (profile == TransportProfile::Blocks ? r.blob.empty()
                                                    : !r.blob.empty()),
           "transport_profile",
           "Arquivo sem recurso incorporado ou excessivo.");
      total += static_cast<std::uint64_t>(size);
      need(total <= max_bytes, "project_limit",
           "Estado excede o limite de transporte.");
    }
    need((directory ? dirs : files).emplace(path, &r).second,
         "transport_profile", "Caminho repetido.");
  }
  for (const auto &[path, r] : dirs)
    need(r->references == std::vector<std::string>{parent_key(path)},
         "transport_profile", "Vínculos de diretório inválidos.");
  for (const auto &[path, r] : files) {
    std::set<std::string> refs{parent_key(path)};
    if (profile == TransportProfile::Blocks) {
      const auto body = parse_json(r->payload);
      const auto size = static_cast<std::size_t>(body.at("bytes").as_integer());
      const auto &parts = body.at("chunks").as_array();
      need(body.as_object().size() == 4 &&
               body.at("sha256").as_string().size() == 64 &&
               parts.size() == (size + block_bytes - 1) / block_bytes,
           "transport_profile", "Descritor de blocos inválido.");
      for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto key = "chunk/" + parts[i].as_string();
        need(chunks.contains(key), "transport_profile", "Bloco ausente.");
        const auto bytes =
            parse_json(chunks.at(key)->payload).at("bytes").as_integer();
        need(bytes == static_cast<std::int64_t>(
                          std::min(block_bytes, size - i * block_bytes)),
             "transport_profile", "Comprimento de bloco inválido.");
        refs.insert(key);
        used_chunks.insert(key);
      }
    }
    need(r->references == std::vector<std::string>(refs.begin(), refs.end()),
         "transport_profile", "Vínculos de arquivo inválidos.");
  }
  need(used_chunks.size() == chunks.size(), "transport_profile",
       "Bloco sem arquivo dono.");
  need(files.size() + dirs.size() <= max_files, "file_limit",
       "Quantidade de componentes excede o perfil de transporte.");
  need(files.contains("manifest.json") &&
           files.contains("data/project.sqlite3"),
       "transport_profile", "Estado sem projeto completo.");
  auto *uuid = g_uuid_string_random();
  const auto stage =
      target.parent_path() / (".inde-p5-pending-" + std::string(uuid));
  g_free(uuid);
  need(::mkdir(stage.c_str(), 0700) == 0, "destination_create",
       "Não foi possível reservar a pasta nova.");
  // Incomplete materializations stay named as pending evidence on any failure.
  for (const auto &[path, r] : dirs) {
    static_cast<void>(r);
    need(!files.contains(path), "transport_profile",
         "Caminho simultaneamente arquivo e diretório.");
    const auto absolute = stage / path;
    need(::mkdir(absolute.c_str(), 0700) == 0, "destination_create",
         "Falha ao criar uma pasta do projeto.");
  }
  for (const auto &[path, r] : files) {
    const auto body = parse_json(r->payload);
    Bytes bytes;
    if (profile == TransportProfile::WholeFile)
      bytes = store.blob(r->blob);
    else {
      bytes.reserve(static_cast<std::size_t>(body.at("bytes").as_integer()));
      for (const auto &part : body.at("chunks").as_array()) {
        const auto &chunk = *chunks.at("chunk/" + part.as_string());
        const auto data = store.blob(chunk.blob);
        need(static_cast<std::int64_t>(data.size()) ==
                 parse_json(chunk.payload).at("bytes").as_integer(),
             "transport_profile", "Bloco diverge do descritor.");
        bytes.insert(bytes.end(), data.begin(), data.end());
      }
      need(sha256(bytes) == body.at("sha256").as_string(), "transport_profile",
           "Hash do arquivo reconstruído divergente.");
    }
    need(static_cast<std::int64_t>(bytes.size()) ==
             body.at("bytes").as_integer(),
         "transport_profile", "Tamanho do recurso diverge do descritor.");
    write_new(stage / path, bytes);
  }
  auto candidate = capture(stage, target, profile);
  need(candidate.owner == state.project_id &&
           candidate.records.size() == state.records.size(),
       "transport_compare",
       "A reabertura não corresponde ao estado reservado.");
  for (const auto &[key, r] : candidate.records) {
    const auto &saved = state.records.at(key);
    need(r.kind == saved.kind && r.payload == saved.payload &&
             r.blob == saved.blob && r.references == saved.references,
         "transport_compare", "Componente reaberto divergente.");
  }
  unchanged(candidate);
  for (auto it = dirs.rbegin(); it != dirs.rend(); ++it)
    sync_dir(stage / it->first);
  sync_dir(stage);
  // Linux no-replace publication; never replace even a concurrently created
  // target.
  need(::renameat2(AT_FDCWD, stage.c_str(), AT_FDCWD, target.c_str(),
                   RENAME_NOREPLACE) == 0,
       "destination_publish",
       "A pasta foi verificada, mas não pôde ser publicada sem substituição. A "
       "pasta pendente foi preservada.");
  sync_dir(target.parent_path());
}
} // namespace inde::project_file

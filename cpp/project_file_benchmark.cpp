#include "inde/persistence/json.hpp"
#include "project_file_session.hpp"
#include "project_file_transport.hpp"
#include <chrono>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <sqlite3.h>
#include <sys/resource.h>
using namespace inde::project_file;
namespace fs = std::filesystem;
using V = inde::persistence::json::Value;
namespace {
void check(bool yes, const char *message) {
  if (!yes)
    throw std::runtime_error(message);
}
struct Db {
  sqlite3 *db{};
  explicit Db(const fs::path &path) {
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
      sqlite3_close(db);
      db = nullptr;
      throw std::runtime_error("Benchmark database open failed");
    }
  }
  ~Db() { sqlite3_close(db); }
  void exec(const std::string &sql) {
    check(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK,
          "Benchmark SQL failed");
  }
  std::int64_t scalar(const char *sql) {
    sqlite3_stmt *q{};
    check(sqlite3_prepare_v2(db, sql, -1, &q, nullptr) == SQLITE_OK,
          "Benchmark query failed");
    const auto step = sqlite3_step(q);
    const auto n = sqlite3_column_int64(q, 0);
    sqlite3_finalize(q);
    check(step == SQLITE_ROW, "Benchmark result missing");
    return n;
  }
};
struct Metrics {
  std::int64_t bytes, blobs, file_bytes;
};
Metrics metrics(const fs::path &path) {
  Db db(path);
  return {db.scalar("SELECT coalesce(sum(size),0) FROM blobs"),
          db.scalar("SELECT count(*) FROM blobs"),
          static_cast<std::int64_t>(fs::file_size(path))};
}
double ms(std::chrono::steady_clock::time_point before) {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - before)
      .count();
}
} // namespace
int main(int argc, char **argv) {
  try {
    check(argc == 4, "Use: benchmark NEW_ROOT SIZE_MIB whole|blocks");
    const auto root = fs::absolute(argv[1]);
    check(!fs::exists(root), "Benchmark root must be new");
    const std::string size_string = argv[2], mode = argv[3];
    check(size_string == "1" || size_string == "4" || size_string == "16",
          "Use 1, 4 or 16 MiB");
    check(mode == "whole" || mode == "blocks", "Unknown benchmark mode");
    const auto mib = static_cast<std::size_t>(std::stoul(size_string));
    const auto source = root / "Fonte.inde", packet = root / "Projeto.inde";
    fs::create_directories(source / "data");
    // Fixed identity, deterministic bytes and exactly the same fixture in every
    // run.
    const std::string owner = "1b532ed8-9e00-40c6-a64b-10adc03756ce";
    std::ofstream(source / "manifest.json")
        << "{\"format\":\"INDE\",\"format_version\":2,\"project_id\":\""
        << owner << "\"}";
    {
      Db db(source / "data/project.sqlite3");
      db.exec("PRAGMA journal_mode=DELETE; CREATE TABLE projects(id TEXT); "
              "INSERT INTO projects VALUES('" +
              owner +
              "'); CREATE TABLE schema_migrations(version INTEGER); INSERT "
              "INTO schema_migrations VALUES(37); CREATE TABLE documents(id "
              "TEXT PRIMARY KEY,content TEXT); INSERT INTO documents "
              "VALUES('doc','Estado 0'); CREATE TABLE stable(id INTEGER "
              "PRIMARY KEY,bytes BLOB);");
      db.exec("BEGIN");
      sqlite3_stmt *put{};
      check(sqlite3_prepare_v2(db.db, "INSERT INTO stable VALUES(?,?)", -1,
                               &put, nullptr) == SQLITE_OK,
            "Benchmark insert failed");
      std::uint64_t seed = 0x123456789abcdef0ULL;
      for (std::size_t row = 0; row < mib * 64; ++row) {
        Bytes bytes(16384);
        for (auto &b : bytes) {
          seed ^= seed << 13;
          seed ^= seed >> 7;
          seed ^= seed << 17;
          b = static_cast<std::uint8_t>(seed & 255);
        }
        sqlite3_bind_int64(put, 1, static_cast<sqlite3_int64>(row));
        sqlite3_bind_blob(put, 2, bytes.data(), static_cast<int>(bytes.size()),
                          SQLITE_TRANSIENT);
        const int rc = sqlite3_step(put);
        sqlite3_reset(put);
        check(rc == SQLITE_DONE, "Benchmark insert refused");
      }
      sqlite3_finalize(put);
      db.exec("COMMIT");
    }
    const auto source_size = static_cast<std::int64_t>(
        fs::file_size(source / "data/project.sqlite3"));
    auto before = std::chrono::steady_clock::now();
#ifdef P5C_BLOCK_BENCHMARK
    pack_directory(source, packet,
                   mode == "blocks" ? TransportProfile::Blocks
                                    : TransportProfile::WholeFile);
#else
    check(mode == "whole", "Baseline supports whole files only");
    pack_directory(source, packet);
#endif
    const auto initial_ms = ms(before);
    const auto initial = metrics(packet);
    auto session = FileSession::open(packet, root / "recuperacao");
    const auto initial_seq = session->baseline().sequence;
    V::Array saves;
    constexpr int rounds = 6;
    for (int i = 1; i <= rounds; ++i) {
      {
        Db db(session->working() / "data/project.sqlite3");
        db.exec("UPDATE documents SET content='Estado " + std::to_string(i) +
                "'");
      }
      before = std::chrono::steady_clock::now();
      session->save("Alteração pequena");
      saves.emplace_back(ms(before));
    }
    const auto final = metrics(packet);
    before = std::chrono::steady_clock::now();
    const auto sequence = session->baseline().sequence;
    session->save("Sem alteração");
    const auto noop_ms = ms(before);
    check(session->baseline().sequence == sequence, "No-op added a revision");
    before = std::chrono::steady_clock::now();
    session->restore(initial_seq);
    const auto restore_ms = ms(before);
    {
      Db db(session->working() / "data/project.sqlite3");
      check(db.scalar(
                "SELECT count(*) FROM documents WHERE content='Estado 0'") == 1,
            "Restoration lost original text");
      check(db.scalar("SELECT count(*) FROM stable") ==
                static_cast<std::int64_t>(mib * 64),
            "Restoration lost stable data");
    }
    session->finish();
    session.reset();
    auto store = Store::open(packet);
    store.verify();
    check(store.history().size() == static_cast<std::size_t>(rounds + 3),
          "History was lost");
    struct rusage usage {};
    check(getrusage(RUSAGE_SELF, &usage) == 0, "Cannot measure memory");
    std::cout << encode_json(V::Object{
                     {"format", "INDE-P5C-PERF-1"},
                     {"mode", mode},
                     {"size_mib", static_cast<std::int64_t>(mib)},
                     {"database_bytes", source_size},
                     {"rounds", rounds},
                     {"initial_ms", initial_ms},
                     {"save_ms", saves},
                     {"noop_ms", noop_ms},
                     {"restore_ms", restore_ms},
                     {"initial_blob_bytes", initial.bytes},
                     {"new_blob_bytes", final.bytes - initial.bytes},
                     {"initial_file_bytes", initial.file_bytes},
                     {"new_file_bytes", final.file_bytes - initial.file_bytes},
                     {"new_blobs", final.blobs - initial.blobs},
                     {"max_rss_kib",
                      static_cast<std::int64_t>(usage.ru_maxrss)},
                     {"history_and_restore_verified", true}})
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

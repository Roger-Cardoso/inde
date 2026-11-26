#include "project_file.hpp"
#include "inde/persistence/json.hpp"
#include <cmath>
#include <cstdlib>
#include <glib.h>
#include <iostream>
#include <locale.h>
#include <sqlite3.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace inde::project_file;
void check(bool yes, const char *s) {
  if (!yes)
    throw std::runtime_error(s);
}
template <class F> void refuses(F f, const std::string &code = {}) {
  bool caught = false;
  try {
    f();
  } catch (const Error &e) {
    caught = code.empty() || e.code == code;
    if (!caught)
      throw;
  }
  check(caught, "Expected refusal");
}
Record record(const std::string &key, const std::string &payload = "{}",
              std::vector<std::string> refs = {}) {
  return Record{key, "note", payload, "", std::move(refs), 0};
}
int main() {
  auto *uuid = g_uuid_string_random();
  const std::string owner(uuid);
  g_free(uuid);
  const auto root =
      std::filesystem::temp_directory_path() / ("inde-p5-" + owner);
  std::filesystem::create_directory(root);
  try {
    const auto comma = newlocale(LC_NUMERIC_MASK, "pt_BR.UTF-8", nullptr);
    if (comma) {
      const auto previous = uselocale(comma);
      check(previous != nullptr, "Set test numeric locale");
      try {
        check(std::strtod("1.5", nullptr) == 1.0,
              "Regression fixture must actually use a decimal comma");
        const std::string raw = "{\"x\":0.6120081954998241,\"zero\":-0.0}";
        const auto value = parse_json(raw);
        check(std::get<double>(value.at("x").storage()) == 0.6120081954998241 &&
                  std::signbit(std::get<double>(value.at("zero").storage())),
              "Locale corrupted a JSON decimal or negative zero");
        check(uselocale(nullptr) == comma, "Parser changed caller locale");
        refuses([&] { canonical_json("{invalid}"); }, "invalid_json");
        check(uselocale(nullptr) == comma, "Failure changed caller locale");
        State saved;
        {
          auto store = Store::create(root / "decimal.inde", owner);
          saved = store.apply(store.head(), {{"decimal", 0, record("decimal", raw)}},
                              {}, "Decimal em ambiente brasileiro");
        }
        {
          auto store = Store::open(root / "decimal.inde");
          check(store.head().hash == saved.hash &&
                    store.head().records.at("decimal").payload.find("0.612008195499824") !=
                        std::string::npos, "Reopen lost decimal precision");
        }
      } catch (...) {
        uselocale(previous);
        freelocale(comma);
        throw;
      }
      uselocale(previous);
      freelocale(comma);
      std::cout << "P5 pt_BR decimal precision and caller locale restoration OK\n";
    } else {
      check(g_getenv("P5_REQUIRE_COMMA_LOCALE") == nullptr,
            "Install pt_BR.UTF-8 to exercise required decimal-comma regression");
      std::cout << "P5 decimal-comma locale unavailable; CI requires this case\n";
    }
    const auto path = root / "Obra.inde";
    State before, after;
    {
      auto store = Store::create(path, owner);
      check(store.head().sequence == 0, "Initial state");
      refuses([&] { Store::open(path); }, "store_busy");
      auto r =
          record("texto", "{\"texto\":\"Início 🙂\",\"x\":0.6120081954998241}");
      before = store.apply(store.head(), {{"texto", 0, r}}, {}, "Criar texto");
      check(before.records.at("texto").payload.find("0.612008195499824") !=
                std::string::npos,
            "Precision lost");
      auto f = record("formula", "{\"HP\":100}", {"texto"});
      after = store.apply(before, {{"formula", 0, f}}, {}, "Criar fórmula");
      const auto n = store.history().size();
      refuses(
          [&] {
            store.apply(before, {{"formula", 0, f}}, {}, "Pedido antigo");
          },
          "stale_state");
      refuses(
          [&] {
            store.apply(
                after,
                {{"texto", after.records.at("texto").version, std::nullopt}},
                {}, "Romper vínculo");
          },
          "dangling_reference");
      check(store.head().hash == after.hash && store.history().size() == n,
            "Refusal changed history");
      auto duplicate = record("texto", "{}");
      refuses(
          [&] {
            store.apply(
                after,
                {{"texto", after.records.at("texto").version, duplicate},
                 {"texto", after.records.at("texto").version, duplicate}},
                {}, "Duplicado");
          },
          "duplicate_edit");
      auto noop = store.apply(after,
                              {{"texto", after.records.at("texto").version,
                                after.records.at("texto")}},
                              {}, "Sem efeito");
      check(noop.sequence == after.sequence, "Noop wrote state");
      auto foreign = after;
      foreign.project_id = "outro";
      refuses([&] { store.restore(foreign, 0, "Outro projeto"); },
              "stale_state");
      auto bad = record("texto", "{\"a\":1,\"a\":2}");
      refuses([&] {
        store.apply(after, {{"texto", after.records.at("texto").version, bad}},
                    {}, "JSON repetido");
      });
      Blob b;
      b.bytes = {0, 1, 2, 0, 255};
      b.hash = sha256(b.bytes);
      auto image = record("imagem");
      image.kind = "image";
      image.blob = b.hash;
      auto photo =
          store.apply(after, {{"imagem", 0, image}}, {b}, "Incorporar bytes");
      check(store.blob(b.hash) == b.bytes, "Blob changed");
      auto restored =
          store.restore(photo, before.sequence, "Retomar estado anterior");
      check(restored.sequence > photo.sequence && restored.hash == before.hash,
            "Restore did not create a new state");
      check(store.state(photo.sequence).records.contains("imagem"),
            "Restore destroyed later history");
      check(store.blob(b.hash) == b.bytes, "Restore discarded image bytes");
      auto redo =
          store.restore(restored, photo.sequence, "Retomar estado posterior");
      check(redo.hash == photo.hash, "Later state lost");
      store.verify();
      after = redo;
    }
    {
      auto s = Store::open(path);
      check(s.head().hash == after.hash, "Reopen changed state");
      s.verify();
    }
    // A whole request, including newly supplied bytes, rolls back on failure.
    {
      Options options;
      options.observe = [](CommitStage stage) {
        if (stage == CommitStage::BeforeCommit)
          throw std::runtime_error("Injected precommit failure");
      };
      auto s = Store::open(path, options);
      const auto original = s.head();
      bool failed = false;
      try {
        Blob bytes;
        bytes.bytes.resize(128 * 1024, 37);
        bytes.hash = sha256(bytes.bytes);
        auto element = record("novo");
        element.blob = bytes.hash;
        s.apply(original, {{"novo", 0, element}}, {bytes}, "Falha");
      } catch (const std::runtime_error &) {
        failed = true;
      }
      check(failed && s.head().sequence == original.sequence,
            "Precommit failure changed state");
      s.verify();
    }
    {
      Options options;
      options.observe = [](CommitStage stage) {
        if (stage == CommitStage::AfterCommit)
          throw std::runtime_error("Lost confirmation");
      };
      auto s = Store::open(path, options);
      auto old = s.head();
      refuses(
          [&] {
            s.apply(old, {{"novo", 0, record("novo")}}, {},
                    "Confirmação perdida");
          },
          "commit_uncertain");
      refuses([&] { s.head(); }, "reopen_required");
    }
    {
      auto s = Store::open(path);
      check(s.head().records.contains("novo"),
            "Committed state lost after confirmation failure");
      s.verify();
    }
    // Real process interruption before and after COMMIT.
    for (const auto stage :
         {CommitStage::BeforeCommit, CommitStage::AfterCommit}) {
      const auto child = fork();
      check(child >= 0, "fork");
      if (child == 0) {
        Options o;
        o.observe = [stage](CommitStage reached) {
          if (stage == reached)
            ::_exit(77);
        };
        auto s = Store::open(path, o);
        auto h = s.head();
        auto r =
            record("crash", stage == CommitStage::BeforeCommit ? "{\"n\":1}"
                                                               : "{\"n\":2}");
        // Exceed SQLite's default page cache so the precommit interruption
        // exercises rollback of spilled database pages, not only RAM buffers.
        Blob bytes;
        bytes.bytes.resize(4 * 1024 * 1024, 93);
        bytes.hash = sha256(bytes.bytes);
        r.blob = bytes.hash;
        const auto at = h.records.find("crash");
        s.apply(h,
                {{"crash", at == h.records.end() ? 0 : at->second.version, r}},
                {bytes}, "Interrupção");
        ::_exit(99);
      }
      int status = 0;
      waitpid(child, &status, 0);
      check(WIFEXITED(status) && WEXITSTATUS(status) == 77,
            "Fault process failed");
      auto s = Store::open(path);
      s.verify();
      check(s.head().records.contains("crash") ==
                (stage == CommitStage::AfterCommit),
            "Interruption boundary violated");
    }
    // An atomic deletion can remove both a target and its dependent uses.
    {
      auto s = Store::create(root / "remove.inde", owner);
      auto h = s.apply(s.head(),
                       {{"a", 0, record("a")},
                        {"b", 0, record("b", "{}", {"a"})}}, {}, "Criar vínculos");
      const auto saved = h;
      h = s.apply(h, {{"a", h.records.at("a").version, std::nullopt},
                      {"b", h.records.at("b").version, std::nullopt}}, {},
                  "Retirar os dois usos");
      check(h.records.empty(), "Atomic removal failed");
      h = s.restore(h, saved.sequence, "Retomar usos e vínculo");
      check(h.records.at("a").version == saved.records.at("a").version,
            "Restore duplicated an immutable version");
      auto changed_kind = record("a");
      changed_kind.kind = "other";
      refuses([&] { s.apply(h, {{"a", h.records.at("a").version, changed_kind}},
                            {}, "Troca de tipo"); }, "kind_changed");
      refuses([&] { s.apply(h, {{"a", 0, record("a")}}, {}, "Versão antiga"); },
              "stale_record");
      auto missing = record("zzz");
      missing.blob = std::string(64, 'a');
      refuses([&] { s.apply(h, {{"aaa", 0, record("aaa")},
                                {"zzz", 0, missing}}, {}, "Recurso ausente"); },
              "blob_missing");
      check(s.head().sequence == h.sequence, "Partial request survived");
      s.verify();
      Blob empty{sha256({}), {}};
      auto resource = record("empty");
      resource.blob = empty.hash;
      h = s.apply(h, {{"empty", 0, resource}}, {empty}, "Arquivo vazio");
      check(s.blob(empty.hash).empty(), "Empty bytes lost");
      s.verify();
      const auto child = fork();
      check(child >= 0, "Fork inherited session");
      if (child == 0) {
        try { s.head(); } catch (const Error &e) {
          ::_exit(e.code == "fork_inherited" ? 77 : 78);
        }
        ::_exit(79);
      }
      int status{};
      check(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
                WEXITSTATUS(status) == 77, "Inherited connection accepted");
      check(s.head().sequence == h.sequence, "Child affected parent session");
    }
    // The storage cap produces a real SQLITE_FULL, without losing a prior state.
    {
      Options o;
      o.limits.store_bytes = 1024 * 1024;
      auto s = Store::create(root / "full.inde", owner, o);
      const auto old = s.head();
      Blob big;
      big.bytes.resize(2 * 1024 * 1024, 123);
      big.hash = sha256(big.bytes);
      auto r = record("too-big");
      r.blob = big.hash;
      refuses([&] { s.apply(old, {{r.key, 0, r}}, {big}, "Sem espaço"); },
              "sqlite_error");
      check(s.head().hash == old.hash, "Full disk cap changed committed state");
      s.verify();
      s.apply(old, {{"small", 0, record("small")}}, {}, "Continuar");
      s.verify();
    }
    {
      Options o;
      o.limits.states = 2;
      auto s = Store::create(root / "limit.inde", owner, o);
      const auto h = s.apply(s.head(), {{"a", 0, record("a")}}, {}, "Primeiro");
      refuses([&] { s.apply(h, {{"b", 0, record("b")}}, {}, "Limite"); },
              "history_limit");
      check(s.head().hash == h.hash && s.history().size() == 2,
            "History limit lost older states");
      s.verify();
    }
    {
      auto s = Store::create(root / "external.inde", owner);
      sqlite3 *db{};
      check(sqlite3_open((root / "external.inde").c_str(), &db) == SQLITE_OK,
            "External connection");
      check(sqlite3_exec(db, "UPDATE head SET seq=seq", nullptr, nullptr,
                         nullptr) == SQLITE_OK, "External commit");
      sqlite3_close(db);
      // SQLite may optimize an unchanged update; use a changed schema cookie.
      check(sqlite3_open((root / "external.inde").c_str(), &db) == SQLITE_OK,
            "External schema connection");
      check(sqlite3_exec(db, "PRAGMA user_version=2", nullptr, nullptr,
                         nullptr) == SQLITE_OK, "External schema commit");
      sqlite3_close(db);
      refuses([&] { s.head(); }, "external_change");
    }
    {
      auto s = Store::create(root / "moved.inde", owner);
      std::filesystem::rename(root / "moved.inde", root / "reserved.inde");
      refuses([&] { s.head(); }, "file_replaced");
    }
    refuses([&] { canonical_json("{\"a\":" + std::string(130, '[') + "0" +
                                std::string(130, ']') + "}"); }, "json_depth");
    // Tampering must be diagnosed, never restored silently to an older state.
    {
      sqlite3 *db{};
      check(sqlite3_open(path.c_str(), &db) == SQLITE_OK, "Tamper connection");
      check(sqlite3_exec(db, "UPDATE blobs SET data=x'00'", nullptr, nullptr,
                         nullptr) != SQLITE_OK,
            "Immutable resource guard missing");
      check(sqlite3_exec(db,
                         "DROP TRIGGER immutable_blobs_UPDATE; UPDATE blobs "
                         "SET data=zeroblob(size)",
                         nullptr, nullptr, nullptr) == SQLITE_OK,
            "Tamper seam");
      sqlite3_close(db);
      refuses([&] { Store::open(path); }, "schema_changed");
    }
    std::filesystem::remove_all(root);
    std::cout << "P5 states, links, immutable versions, restore, bytes, stale "
                 "requests, failure and crash recovery OK\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\nSynthetic evidence: " << root << "\n";
    return 1;
  }
}

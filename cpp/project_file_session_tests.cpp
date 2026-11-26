#include "project_file_session.hpp"
#include "project_file_transport.hpp"
#include <fstream>
#include <glib.h>
#include <iostream>
#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace inde::project_file;
namespace fs = std::filesystem;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
void sql(const fs::path &p, const std::string &s) {
  sqlite3 *db{};
  check(sqlite3_open(p.c_str(), &db) == SQLITE_OK, "fixture open");
  const int r = sqlite3_exec(db, s.c_str(), nullptr, nullptr, nullptr);
  sqlite3_close(db);
  check(r == SQLITE_OK, "fixture SQL");
}
std::string text(const fs::path &p) {
  std::ifstream f(p);
  return {std::istreambuf_iterator<char>(f), {}};
}
template <class F> void refuse(F f, const std::string &code) {
  bool caught = false;
  try {
    f();
  } catch (const Error &e) {
    if (e.code != code)
      throw;
    caught = true;
  }
  check(caught, "Expected refusal");
}
int main() {
  auto *id = g_uuid_string_random();
  const std::string owner(id);
  g_free(id);
  const auto root = fs::temp_directory_path() / ("inde-p5-session-" + owner);
  try {
    const auto source = root / "source", file = root / "Projeto.inde",
               state = root / "state";
    fs::create_directories(source / "data");
    std::ofstream(source / "manifest.json")
        << "{\"format\":\"INDE\",\"format_version\":2,\"project_id\":\""
        << owner << "\"}";
    sql(source / "data/project.sqlite3",
        "CREATE TABLE projects(id TEXT); CREATE TABLE "
        "schema_migrations(version INTEGER); INSERT INTO schema_migrations "
        "VALUES(37); INSERT INTO projects VALUES('" +
            owner + "');");
    std::ofstream(source / "texto.txt") << "Original";
    const auto first = pack_directory(source, file,
                                      g_getenv("P5_TRANSPORT_BLOCKS")
                                          ? TransportProfile::Blocks
                                          : TransportProfile::WholeFile);
    fs::path work;
    State second;
    {
      auto s = FileSession::open(file, state);
      work = s->working();
      refuse([&] { FileSession::open(file, state); }, "store_busy");
      std::ofstream(work / "texto.txt") << "Alterado";
      check(s->baseline().sequence == first.sequence,
            "Saved before explicit Save");
      second = s->save("Salvar");
      check(second.sequence == first.sequence + 1, "No new state");
      check(s->save("Sem efeito").sequence == second.sequence,
            "Noop created history");
      s->finish();
      std::ofstream(work / "texto.txt") << "Trabalho interrompido";
    }
    refuse([&] { FileSession::open(file, state); }, "recovery_required");
    check(text(work / "texto.txt") == "Trabalho interrompido",
          "Recovery changed during observation");
    {
      auto s = FileSession::open(file, state, RecoveryChoice::Working);
      check(s->working() == work, "Wrong recovered directory");
      s->save("Retomar trabalho");
      s->finish();
    }
    // A real interrupted process leaves durable workspace data and no clean
    // marker.
    const auto pid = fork();
    check(pid >= 0, "fork");
    if (pid == 0) {
      auto s = FileSession::open(file, state);
      std::ofstream f(s->working() / "texto.txt");
      f << "Outro rascunho";
      f.close();
      ::_exit(77);
    }
    int status{};
    check(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
              WEXITSTATUS(status) == 77,
          "Crash fixture");
    refuse([&] { FileSession::open(file, state); }, "recovery_required");
    {
      auto s = FileSession::open(file, state, RecoveryChoice::Saved);
      check(s->working() != work && fs::exists(work),
            "Saved choice discarded previous work");
      check(text(work / "texto.txt") == "Outro rascunho",
            "Reserved work changed");
      s->copy_to(root / "Copia.inde");
      refuse([&] { s->copy_to(root / "Copia.inde"); }, "destination_exists");
      auto copy = FileSession::open(root / "Copia.inde", state);
      check(copy->working() != s->working() &&
                copy->history().size() == s->history().size(),
            "Copy lost history or shares workspace");
      copy->finish();
      const auto old = s->working();
      const auto restored = s->restore(first.sequence);
      check(restored.hash == first.hash &&
                restored.sequence > second.sequence && fs::exists(old),
            "Restoration lost old state/work");
      check(text(s->working() / "texto.txt") == "Original",
            "Wrong restored bytes");
      s->finish();
      // Metadata failure AFTER commit must not invite a repeated change.
      std::ofstream(s->working() / "texto.txt") << "Confirmado sem recibo";
      const auto parent = s->working().parent_path();
      check(chmod(parent.c_str(), 0500) == 0, "Set refusal fixture");
      refuse([&] { s->save("Sem recibo"); }, "session_uncertain");
      check(chmod(parent.c_str(), 0700) == 0, "Restore fixture permissions");
      refuse([&] { s->save("Não repetir"); }, "reopen_required");
    }
    {
      auto s = FileSession::open(file, state);
      check(text(s->working() / "texto.txt") == "Confirmado sem recibo",
            "Uncertain commit lost data");
      const auto seq = s->baseline().sequence;
      check(s->save("Confirmar reabertura").sequence == seq,
            "Uncertain receipt duplicated state");
      s->finish();
    }
    fs::path meta_parent, reserved_work;
    {
      auto s = FileSession::open(file, state);
      reserved_work = s->working();
      meta_parent = reserved_work.parent_path();
      s->finish();
    }
    std::ofstream(meta_parent / "session.json") << "{";
    refuse([&] { FileSession::open(file, state); }, "recovery_required");
    refuse([&] { FileSession::open(file, state, RecoveryChoice::Working); },
           "session_metadata");
    {
      auto s = FileSession::open(file, state, RecoveryChoice::Saved);
      s->finish();
      check(fs::exists(reserved_work), "Invalid metadata discarded workspace");
      bool found = false;
      for (const auto &e : fs::directory_iterator(meta_parent))
        if (e.path().filename().string().starts_with("reserved-") &&
            text(e.path()) == "{")
          found = true;
      check(found, "Invalid metadata was not preserved");
    }
    fs::remove(meta_parent / "session.json");
    refuse([&] { FileSession::open(file, state); }, "recovery_required");
    {
      auto s = FileSession::open(file, state, RecoveryChoice::Saved);
      s->finish();
      fs::create_symlink(source / "texto.txt", s->working() / "unsafe-link");
    }
    refuse([&] { FileSession::open(file, state); }, "recovery_required");
    refuse([&] { FileSession::open(file, state, RecoveryChoice::Working); },
           "session_unsafe");
    {
      auto s = FileSession::open(file, state, RecoveryChoice::Saved);
      s->finish();
    }
    check(text(source / "texto.txt") == "Original", "Original modified");
    fs::remove_all(root);
    std::cout
        << "P5 native session: explicit Save, crash, both recovery choices, "
           "history/copy, restore, lost receipt and source preservation OK\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\nEvidence: " << root << '\n';
    return 1;
  }
}

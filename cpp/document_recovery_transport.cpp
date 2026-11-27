#include "document_recovery_transport.hpp"
#include "inde/persistence/document_recovery_store.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <array>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
namespace inde::persistence {
namespace {
void regular(const std::filesystem::path &path) {
  struct stat s {};
  if (::lstat(path.c_str(), &s) != 0 || !S_ISREG(s.st_mode) ||
      s.st_nlink != 1 || s.st_uid != ::getuid())
    throw std::runtime_error("Componente de recuperação inseguro; a área foi "
                             "preservada para conferência.");
}
} // namespace
void prepare_document_recovery_transport(const project::Project &owner) {
  namespace fs = std::filesystem;
  const auto path = DocumentRecoveryStore::database_path(owner);
  const fs::path journal = path.string() + "-journal";
  const bool has_journal = fs::exists(fs::symlink_status(journal));
  if (!fs::exists(fs::symlink_status(path))) {
    if (has_journal)
      throw std::runtime_error("Há um journal sem seu banco de recuperação; "
                               "preserve a área para análise.");
    return;
  }
  regular(path);
  if (has_journal)
    regular(journal);
  for (const auto *suffix : {"-wal", "-shm"})
    if (fs::exists(fs::symlink_status(path.string() + suffix)))
      throw std::runtime_error("A recuperação usa arquivos WAL; a área foi "
                               "preservada sem conversão.");
  const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0)
    throw std::runtime_error(
        "Não foi possível conferir o banco de recuperação.");
  std::array<unsigned char, 100> header{};
  const auto count = ::read(fd, header.data(), header.size());
  ::close(fd);
  if (count != static_cast<ssize_t>(header.size()) || header[18] != 1 ||
      header[19] != 1)
    throw std::runtime_error(
        "Modo do banco de recuperação não reconhecido; a área foi preservada.");
  // The native owner validates its version and lets SQLite recover a hot
  // journal. Merely reading can leave an interrupted, non-hot journal behind.
  static_cast<void>(DocumentRecoveryStore{}.pending(owner));
  if (!fs::exists(fs::symlink_status(journal)))
    return;
  regular(journal);
  SqliteDatabase db(path);
  SqliteTransaction tx(db);
  if (db.query_integer("PRAGMA user_version") != 1 ||
      db.query_text("PRAGMA journal_mode") != "delete")
    throw std::runtime_error(
        "Formato de recuperação incompatível; nenhuma conversão foi aplicada.");
  // Reconfirm the SAME private format marker. This changes no record or
  // schema and makes SQLite own/close its interrupted journal normally.
  db.execute("PRAGMA user_version=1");
  tx.commit();
  if (fs::exists(fs::symlink_status(journal)))
    throw std::runtime_error("A recuperação ainda está ocupada; aguarde e "
                             "tente confirmar novamente.");
}
} // namespace inde::persistence

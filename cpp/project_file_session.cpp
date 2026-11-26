#include "project_file_session.hpp"
#include "inde/persistence/json.hpp"
#include "project_file_transport.hpp"
#include <fcntl.h>
#include <fstream>
#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>
namespace inde::project_file {
namespace fs = std::filesystem;
using V = persistence::json::Value;
namespace {
void need(bool b, const char *c, const char *m) {
  if (!b)
    throw Error(c, m);
}
std::string uuid() {
  auto *p = g_uuid_string_random();
  std::string s(p);
  g_free(p);
  return s;
}
void sync_directory(const fs::path &p) {
  const int fd = ::open(p.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  need(fd >= 0, "session_sync",
       "Não foi possível abrir a área de recuperação.");
  const int r = fsync(fd);
  ::close(fd);
  need(r == 0, "session_sync", "A confirmação da recuperação foi incerta.");
}
void private_directory(const fs::path &p) {
  fs::create_directories(p);
  struct stat s {};
  need(lstat(p.c_str(), &s) == 0 && S_ISDIR(s.st_mode) && s.st_uid == getuid(),
       "session_directory",
       "Área de recuperação inválida ou de outro usuário.");
  need(chmod(p.c_str(), 0700) == 0, "session_directory",
       "Não foi possível proteger a área de recuperação.");
}
std::string read(const fs::path &p) {
  const int fd = ::open(p.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  need(fd >= 0, "session_metadata",
       "Não foi possível ler o registro de recuperação.");
  struct stat s {};
  if (fstat(fd, &s) != 0 || !S_ISREG(s.st_mode) || s.st_nlink != 1 ||
      s.st_uid != getuid() || s.st_size < 0 || s.st_size > 16384) {
    ::close(fd);
    throw Error("session_metadata", "Registro de recuperação inválido.");
  }
  std::string out(static_cast<std::size_t>(s.st_size), '\0');
  std::size_t at{};
  while (at < out.size()) {
    const auto n = ::read(fd, out.data() + at, out.size() - at);
    if (n <= 0) {
      ::close(fd);
      throw Error("session_metadata", "Leitura incompleta da recuperação.");
    }
    at += static_cast<std::size_t>(n);
  }
  ::close(fd);
  return out;
}
void atomic_write(const fs::path &p, const std::string &s) {
  const auto temp = p.parent_path() / ("pending-" + uuid());
  const int fd = ::open(
      temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  need(fd >= 0, "session_metadata",
       "Não foi possível reservar o registro de recuperação.");
  try {
    std::size_t at{};
    while (at < s.size()) {
      const auto n = ::write(fd, s.data() + at, s.size() - at);
      need(n > 0, "session_metadata", "Gravação incompleta da recuperação.");
      at += static_cast<std::size_t>(n);
    }
    need(fsync(fd) == 0, "session_sync",
         "A recuperação não pôde ser confirmada.");
    ::close(fd);
  } catch (...) {
    ::close(fd);
    throw;
  }
  const auto status = fs::symlink_status(p);
  need(!fs::exists(status) || fs::is_regular_file(status), "session_metadata",
       "O registro de recuperação foi substituído.");
  need(::rename(temp.c_str(), p.c_str()) == 0, "session_metadata",
       "O registro de recuperação não pôde ser publicado.");
  sync_directory(p.parent_path());
}
} // namespace
RecoveryRequired::RecoveryRequired(fs::path p, bool advanced, bool safe)
    : Error("recovery_required",
            "Há uma área de trabalho diferente do arquivo salvo. Escolha qual "
            "retomar; nenhuma cópia foi apagada."),
      working(std::move(p)), file_advanced(advanced), resumable(safe) {}
struct FileSession::Impl {
  fs::path file, root, work;
  Store store;
  State base;
  bool uncertain{};
  Impl(fs::path f, fs::path r)
      : file(fs::weakly_canonical(fs::absolute(f).parent_path()) /
             f.filename()),
        root(std::move(r)), store(Store::open(file)), base(store.head()) {}
  void metadata(const char *status) {
    atomic_write(root / "session.json",
                 encode_json(V::Object{{"format", "INDE-WORKSPACE-1"},
                                       {"file", file.string()},
                                       {"project", base.project_id},
                                       {"store", base.store_id},
                                       {"sequence", base.sequence},
                                       {"hash", base.hash},
                                       {"working", work.filename().string()},
                                       {"status", status}}));
  }
};
FileSession::FileSession(std::unique_ptr<Impl> p) : impl_(std::move(p)) {}
FileSession::~FileSession() = default;
std::unique_ptr<FileSession> FileSession::open(const fs::path &file,
                                               const fs::path &state_root,
                                               RecoveryChoice choice) {
  auto p = std::make_unique<Impl>(file, state_root);
  const auto raw = p->file.string();
  p->root /= p->base.store_id + "-" + sha256(Bytes(raw.begin(), raw.end()));
  private_directory(p->root);
  const auto meta = p->root / "session.json";
  bool reuse = false;
  if (fs::exists(fs::symlink_status(meta))) {
    V v;
    bool valid = false;
    try {
      v = parse_json(read(meta), 16384);
      need(v.as_object().size() == 8 &&
               v.at("format").as_string() == "INDE-WORKSPACE-1" &&
               v.at("file").as_string() == p->file.string() &&
               v.at("project").as_string() == p->base.project_id &&
               v.at("store").as_string() == p->base.store_id,
           "session_metadata",
           "O registro de recuperação não corresponde ao arquivo.");
      const auto name = v.at("working").as_string();
      need(name.starts_with("work-") && name.size() == 46 &&
               name.ends_with(".inde") &&
               g_uuid_string_is_valid(name.substr(5, 36).c_str()),
           "session_metadata", "Caminho da recuperação inválido.");
      need(v.at("status").as_string() == "working" ||
               v.at("status").as_string() == "clean",
           "session_metadata", "Estado da recuperação desconhecido.");
      p->work = p->root / name;
      need(fs::is_directory(fs::symlink_status(p->work)), "session_missing",
           "A área de recuperação está ausente; preserve o registro para "
           "análise.");
      need(v.at("sequence").as_integer() >= 0 &&
               v.at("hash").as_string().size() == 64,
           "session_metadata", "Revisão da recuperação inválida.");
      valid = true;
    } catch (const std::exception &) {
      if (choice == RecoveryChoice::Ask)
        throw RecoveryRequired(p->root, false, false);
      need(choice == RecoveryChoice::Saved, "session_metadata",
           "O registro não permite retomar automaticamente; abra o salvo e "
           "preserve a área para análise.");
      fs::rename(meta, p->root / ("reserved-" + uuid() + ".json"));
      sync_directory(p->root);
    }
    if (valid) {
      bool safe = true;
      try {
        for (const auto &entry : fs::recursive_directory_iterator(p->work)) {
          const auto status = entry.symlink_status();
          if (!fs::is_directory(status) &&
              (!fs::is_regular_file(status) ||
               fs::hard_link_count(entry.path()) != 1))
            safe = false;
        }
      } catch (const std::exception &) {
        safe = false;
      }
      bool same = false;
      try {
        same = directory_matches(p->store, p->base, p->work, p->file);
      } catch (const std::exception &) {
        // An interrupted SQLite workspace may need recovery by its owner.
        // A damaged/changed authoritative file must still refuse opening.
        static_cast<void>(p->store.head());
      }
      if (same)
        reuse = true;
      else if (choice == RecoveryChoice::Ask)
        throw RecoveryRequired(p->work,
                               v.at("sequence").as_integer() !=
                                       p->base.sequence ||
                                   v.at("hash").as_string() != p->base.hash,
                               safe);
      else if (choice == RecoveryChoice::Working) {
        need(
            safe, "session_unsafe",
            "A área contém componentes inseguros; abra o salvo e preserve essa "
            "área para análise.");
        reuse = true;
      } else {
        // Keep the old descriptor and directory; choosing Saved never deletes
        // work.
        atomic_write(p->root / ("reserved-" + uuid() + ".json"), read(meta));
      }
    }
  } else if (choice == RecoveryChoice::Ask) {
    for (const auto &entry : fs::directory_iterator(p->root))
      if (entry.path().filename().string().starts_with("work-"))
        throw RecoveryRequired(p->root, false, false);
  }
  if (!reuse) {
    p->work = p->root / ("work-" + uuid() + ".inde");
    unpack_directory(p->store, p->base.sequence, p->work);
  }
  p->metadata("working");
  return std::unique_ptr<FileSession>(new FileSession(std::move(p)));
}
const fs::path &FileSession::file() const { return impl_->file; }
const fs::path &FileSession::working() const { return impl_->work; }
const State &FileSession::baseline() const { return impl_->base; }
bool FileSession::needs_reopen() const {
  if (impl_->uncertain)
    return true;
  try {
    static_cast<void>(impl_->store.head());
    return false;
  } catch (const Error &) {
    return true;
  }
}
State FileSession::save(const std::string &label) {
  need(!impl_->uncertain, "reopen_required",
       "Reabra o arquivo para conferir a confirmação anterior; a área de "
       "trabalho foi preservada.");
  // Capture retains all SQLite read reservations until publication completes.
  auto next = update_directory(impl_->store, impl_->base, impl_->work,
                               impl_->file, label);
  impl_->base = std::move(next);
  try {
    impl_->metadata("working");
  } catch (...) {
    impl_->uncertain = true;
    throw Error(
        "session_uncertain",
        "O arquivo foi confirmado, mas o registro da recuperação não pôde ser "
        "confirmado. Reabra para conferir; nenhuma área foi apagada.");
  }
  return impl_->base;
}
void FileSession::finish() {
  need(!impl_->uncertain, "reopen_required",
       "Reabra para conferir a confirmação anterior.");
  need(directory_matches(impl_->store, impl_->base, impl_->work, impl_->file),
       "session_dirty",
       "A área mudou depois do salvamento; ela será preservada para "
       "recuperação.");
  impl_->metadata("clean");
}
std::vector<Revision> FileSession::history() const {
  return impl_->store.history();
}
void FileSession::copy_to(const fs::path &path) const {
  need(directory_matches(impl_->store, impl_->base, impl_->work, impl_->file),
       "session_dirty", "Salve o trabalho antes de copiar o arquivo.");
  impl_->store.copy_to(path);
}
State FileSession::restore(std::int64_t sequence) {
  need(!impl_->uncertain, "reopen_required",
       "Reabra para conferir a confirmação anterior.");
  need(directory_matches(impl_->store, impl_->base, impl_->work, impl_->file),
       "session_dirty", "Salve o trabalho antes de retomar outro estado.");
  if (impl_->store.state(sequence).hash == impl_->base.hash)
    return impl_->base;
  const auto next_work = impl_->root / ("work-" + uuid() + ".inde");
  unpack_directory(impl_->store, sequence, next_work);
  impl_->base =
      impl_->store.restore(impl_->base, sequence, "Retomar estado no editor");
  impl_->work = next_work;
  try {
    impl_->metadata("working");
  } catch (...) {
    impl_->uncertain = true;
    throw Error("session_uncertain",
                "O estado foi retomado, mas a recuperação não pôde ser "
                "registrada. Reabra o arquivo para conferir.");
  }
  return impl_->base;
}
fs::path FileSession::default_root() {
  if (const auto *p = g_getenv("XDG_STATE_HOME"); p && *p)
    return fs::path(p) / "inde/project-files";
  const auto *p = g_getenv("HOME");
  need(p && *p, "session_directory",
       "Defina uma área persistente de recuperação.");
  return fs::path(p) / ".local/state/inde/project-files";
}
} // namespace inde::project_file

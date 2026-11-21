#include "object_checkpoint_store.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
#include "object_session_checkpoint.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <glib.h>
#include <limits>
#include <linux/fs.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

namespace inde::objects::planning::recoverylab {
StoreError::StoreError(std::string c, std::string message)
    : std::runtime_error(std::move(message)), code(std::move(c)) {}
namespace {
constexpr std::size_t record_limit = checkpoint_detail::byte_limit + 256;
constexpr auto marker_magic = "INDE-OBJ01-STORE-LAB-1\n";
constexpr auto record_magic = "INDE-OBJ01-GENERATION-LAB-1\n";
[[noreturn]] void fail(const std::string &code, const std::string &message) {
  throw StoreError(code, message);
}
[[noreturn]] void io_error(const std::string &operation) {
  fail("store_io_error", operation + ": " + std::strerror(errno));
}
class Fd {
public:
  int value{-1};
  explicit Fd(int v = -1) : value(v) {}
  ~Fd() {
    if (value >= 0)
      ::close(value);
  }
  Fd(Fd &&other) noexcept : value(std::exchange(other.value, -1)) {}
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
};
void check_scope(const Scope &s) {
  if (s.project_id.size() != 36 || s.source_id.size() != 36 ||
      !g_uuid_string_is_valid(s.project_id.c_str()) ||
      !g_uuid_string_is_valid(s.source_id.c_str()) ||
      (s.kind != SourceKind::Document && s.kind != SourceKind::Composition))
    fail("store_scope_invalid", "Escopo de laboratório inválido.");
}
void check_options(const StoreOptions &o) {
  if (o.max_files < 2 || o.max_files > 128 || o.max_bytes < 1024 ||
      o.max_bytes > 512 * 1024 * 1024)
    fail("store_limits_invalid",
         "Limites: 2–128 arquivos e até 512 MiB de armazenamento.");
}
void private_file(int fd) {
  struct stat st {};
  if (::fstat(fd, &st) != 0)
    io_error("fstat");
  if (!S_ISREG(st.st_mode) || st.st_uid != ::geteuid() ||
      (st.st_mode & 0777) != 0600 || st.st_nlink != 1)
    fail("store_file_unsafe", "Arquivo deve ser regular, privado, do usuário "
                              "atual e sem hard links.");
}
void private_directory(int fd) {
  struct stat st {};
  if (::fstat(fd, &st) != 0)
    io_error("fstat diretório");
  if (!S_ISDIR(st.st_mode) || st.st_uid != ::geteuid() ||
      (st.st_mode & 0777) != 0700 || st.st_nlink == 0)
    fail("store_directory_unsafe", "Diretório de laboratório deve ser privado "
                                   "e pertencer ao usuário atual.");
}
int safe_directory(const std::filesystem::path &path) {
  const auto text = path.native();
  if (!path.is_absolute() || text.find('\0') != std::string::npos ||
      text.size() > 4096)
    fail("store_path_invalid",
         "Use caminho absoluto sem componentes especiais.");
  int current = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (current < 0)
    io_error("abrir raiz");
  try {
    std::size_t count{};
    for (const auto &part : path.relative_path()) {
      const auto name = part.native();
      if (name.empty() || name == "." || name == ".." || name.size() > 255 ||
          ++count > 128)
        fail("store_path_invalid", "Componente de caminho recusado.");
      const int next =
          ::openat(current, name.c_str(),
                   O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
      if (next < 0)
        io_error("abrir componente sem seguir links");
      ::close(current);
      current = next;
    }
    return current;
  } catch (...) {
    ::close(current);
    throw;
  }
}
std::string read_file(int root, const std::string &name, std::size_t max) {
  Fd fd(::openat(root, name.c_str(),
                 O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
  if (fd.value < 0)
    io_error("abrir arquivo privado");
  private_file(fd.value);
  struct stat st {};
  if (::fstat(fd.value, &st) != 0)
    io_error("medir arquivo");
  if (st.st_size < 0 || static_cast<std::uint64_t>(st.st_size) > max)
    fail("store_file_limit", "Arquivo excede o limite.");
  std::string bytes(static_cast<std::size_t>(st.st_size), '\0');
  std::size_t offset{};
  while (offset < bytes.size()) {
    const auto n =
        ::read(fd.value, bytes.data() + offset, bytes.size() - offset);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0)
      io_error("ler arquivo");
    if (n == 0)
      fail("store_file_changed", "Arquivo encurtou durante a leitura.");
    offset += static_cast<std::size_t>(n);
  }
  char extra{};
  ssize_t tail;
  do {
    tail = ::read(fd.value, &extra, 1);
  } while (tail < 0 && errno == EINTR);
  if (tail < 0)
    io_error("concluir leitura");
  if (tail != 0)
    fail("store_file_changed", "Arquivo cresceu durante a leitura.");
  return bytes;
}
void write_all(int fd, const std::string &bytes,
               const StoreOptions *options = nullptr) {
  std::size_t offset{};
  while (offset < bytes.size()) {
    const int injected = options && options->inject_errno
                             ? options->inject_errno(IoPoint::Write)
                             : 0;
    ssize_t n{};
    if (injected) {
      errno = injected;
      n = -1;
    } else
      n = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0)
      io_error("gravar arquivo");
    if (n == 0)
      fail("store_io_error", "Gravação sem progresso.");
    offset += static_cast<std::size_t>(n);
  }
}
void sync_fd(int fd, const std::string &what,
             const StoreOptions *options = nullptr,
             IoPoint point = IoPoint::DirectorySync) {
  const int injected =
      options && options->inject_errno ? options->inject_errno(point) : 0;
  if (injected) {
    errno = injected;
    io_error(what);
  }
  int rc;
  do {
    rc = ::fsync(fd);
  } while (rc < 0 && errno == EINTR);
  if (rc < 0)
    io_error(what);
}
std::string sequence_text(std::uint64_t sequence) {
  auto s = std::to_string(sequence);
  return std::string(20 - s.size(), '0') + s;
}
std::string filename(std::uint64_t sequence) {
  return "g" + sequence_text(sequence) + ".checkpoint";
}
std::uint64_t parse_sequence(const std::string &s) {
  if (s.size() != 20)
    fail("store_name_invalid", "Nome de geração inválido.");
  std::uint64_t n{};
  const auto r = std::from_chars(s.data(), s.data() + s.size(), n);
  if (r.ec != std::errc{} || r.ptr != s.data() + s.size() || n == 0)
    fail("store_name_invalid", "Sequência inválida.");
  return n;
}
void validate_checkpoint(const SessionCheckpoint &c, const Scope &scope) {
  const auto d = checkpoint_detail::decode(c);
  if (d.project_id != scope.project_id ||
      d.composition != (scope.kind == SourceKind::Composition))
    fail("store_checkpoint_scope", "Checkpoint pertence a outro escopo.");
  if (!d.composition) {
    const auto base = persistence::decode_document_snapshot(d.base);
    if (base.id != scope.source_id ||
        !DocumentChangeSession::restore(c, scope.project_id, base).session)
      fail("store_checkpoint_invalid",
           "Checkpoint documental não admite recuperação íntegra.");
  } else {
    const auto sha = persistence::composition_checksum(d.base);
    project::StoredComposition base{
        persistence::decode_composition(d.base, sha), scope.project_id, sha,
        d.timestamp, d.persisted_revision};
    if (base.value.blocks.composition_id() != scope.source_id ||
        !CompositionChangeSession::restore(c, base).session)
      fail("store_checkpoint_invalid",
           "Checkpoint de Composição não admite recuperação íntegra.");
  }
}
std::string fingerprint(const Inventory &i) {
  std::string text = std::to_string(i.file_bytes) + "\n";
  for (const auto &g : i.generations)
    text += sequence_text(g.sequence) + ":" + g.sha256 + ":" +
            std::to_string(g.file_bytes) + ":" + (g.valid ? "1" : "0") + "\n";
  for (const auto &s : i.incomplete)
    text += s + "\n";
  return persistence::composition_checksum(text);
}
} // namespace
struct CheckpointStore::Impl {
  Fd root, lease;
  Scope scope;
  StoreOptions options;
  std::string store_id, marker_text, lease_id = project::new_uuid();
  pid_t pid = ::getpid();
  bool poisoned{};
  Impl(Fd fd, Scope s, StoreOptions o)
      : root(std::move(fd)), scope(std::move(s)), options(std::move(o)) {}
  void check(bool mutation = false) const {
    if (::getpid() != pid)
      fail("store_inherited_handle", "Reabra o armazenamento depois de fork; o "
                                     "handle pertence ao processo anterior.");
    private_directory(root.value);
    private_file(lease.value);
    struct stat held {
    }, named{};
    if (::fstat(lease.value, &held) != 0 ||
        ::fstatat(root.value, ".lease", &named, AT_SYMLINK_NOFOLLOW) != 0)
      io_error("conferir lease");
    if (held.st_dev != named.st_dev || held.st_ino != named.st_ino)
      fail("store_lease_replaced", "Arquivo de lease foi substituído.");
    if (!marker_text.empty() &&
        read_file(root.value, ".store", 512) != marker_text)
      fail("store_marker_changed",
           "Marcador foi alterado; armazenamento preservado.");
    if (mutation && poisoned)
      fail("store_reopen_required",
           "Resultado de I/O incerto; feche e reabra antes de outra mutação.");
  }
  void lock() {
    lease.value = ::openat(root.value, ".lease",
                           O_RDWR | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (lease.value < 0)
      io_error("abrir lease");
    private_file(lease.value);
    if (::flock(lease.value, LOCK_EX | LOCK_NB) != 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN)
        fail("store_busy", "Armazenamento em uso por outro processo/handle.");
      io_error("adquirir lease");
    }
    check();
  }
  void stage(PublishStage s) const {
    if (options.observe_stage)
      options.observe_stage(s);
  }
  SessionCheckpoint record(std::uint64_t seq,
                           std::size_t *size = nullptr) const {
    const auto data = read_file(root.value, filename(seq), record_limit);
    const std::string prefix =
        std::string(record_magic) + store_id + "\n" + sequence_text(seq) + "\n";
    if (data.size() < prefix.size() + 65 ||
        data.compare(0, prefix.size(), prefix) != 0 ||
        data[prefix.size() + 64] != '\n')
      fail("store_record_invalid", "Cabeçalho/identidade da geração inválido.");
    SessionCheckpoint c{data.substr(prefix.size() + 65),
                        data.substr(prefix.size(), 64)};
    validate_checkpoint(c, scope);
    if (size)
      *size = data.size();
    return c;
  }
  Inventory scan() const {
    check();
    Inventory i;
    const int scanfd = ::openat(
        root.value, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (scanfd < 0)
      io_error("abrir inventário");
    DIR *dir = ::fdopendir(scanfd);
    if (!dir) {
      ::close(scanfd);
      io_error("listar inventário");
    }
    try {
      std::size_t entries{};
      errno = 0;
      while (auto *entry = ::readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name == ".store" || name == ".lease")
          continue;
        if (++entries > 128)
          fail("store_inventory_limit",
               "Mais de 128 arquivos de dados no laboratório.");
        struct stat st {};
        if (::fstatat(root.value, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0)
          io_error("medir entrada");
        if (!S_ISREG(st.st_mode) || st.st_uid != ::geteuid() ||
            (st.st_mode & 0777) != 0600 || st.st_nlink != 1 || st.st_size < 0)
          fail("store_entry_unsafe",
               "Inventário contém entrada insegura; nada será removido.");
        const auto size = static_cast<std::size_t>(st.st_size);
        if (size > record_limit)
          fail("store_file_limit", "Entrada excede limite de geração.");
        if (size > 512 * 1024 * 1024 -
                       std::min<std::size_t>(i.file_bytes, 512 * 1024 * 1024))
          fail("store_inventory_limit",
               "Inventário excede 512 MiB; revisão necessária.");
        i.file_bytes += size;
        if (name.starts_with(".pending-") &&
            g_uuid_string_is_valid(name.substr(9).c_str()))
          i.incomplete.push_back(name);
        else if (name.size() == 32 && name[0] == 'g' &&
                 name.ends_with(".checkpoint")) {
          Generation g;
          g.sequence = parse_sequence(name.substr(1, 20));
          g.file_bytes = size;
          try {
            g.sha256 = record(g.sequence).sha256;
            g.valid = true;
          } catch (const std::bad_alloc &) {
            throw;
          } catch (const std::exception &e) {
            g.diagnostic = e.what();
          }
          i.generations.push_back(std::move(g));
        } else
          fail(
              "store_entry_unknown",
              "Entrada desconhecida preservada; revisão explícita necessária.");
        errno = 0;
      }
      if (errno)
        io_error("listar entradas");
      ::closedir(dir);
    } catch (...) {
      ::closedir(dir);
      throw;
    }
    std::sort(
        i.generations.begin(), i.generations.end(),
        [](const auto &a, const auto &b) { return a.sequence < b.sequence; });
    std::sort(i.incomplete.begin(), i.incomplete.end());
    return i;
  }
};
CheckpointStore::CheckpointStore() = default;
CheckpointStore::~CheckpointStore() = default;
std::unique_ptr<CheckpointStore>
CheckpointStore::create_new(const std::filesystem::path &path, Scope scope,
                            StoreOptions options) {
  check_scope(scope);
  check_options(options);
  if (path.native().find('\0') != std::string::npos ||
      path.native().size() > 4096 || path.filename().native().size() > 255 ||
      !path.is_absolute() || path.filename().empty() ||
      path.filename() == "." || path.filename() == "..")
    fail("store_path_invalid", "Diretório novo inválido.");
  Fd parent(safe_directory(path.parent_path()));
  if (::mkdirat(parent.value, path.filename().c_str(), 0700) != 0)
    io_error("criar diretório exclusivo");
  Fd root(::openat(parent.value, path.filename().c_str(),
                   O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (root.value < 0)
    io_error("abrir diretório novo");
  if (::fchmod(root.value, 0700) != 0)
    io_error("permissão do diretório");
  private_directory(root.value);
  auto result = std::unique_ptr<CheckpointStore>(new CheckpointStore);
  result->impl_ = std::make_unique<Impl>(std::move(root), std::move(scope),
                                         std::move(options));
  root.value = -1;
  const auto &s = result->impl_->scope;
  result->impl_->store_id = project::new_uuid();
  const auto marker = std::string(marker_magic) + result->impl_->store_id +
                      "\n" + s.project_id + "\n" +
                      (s.kind == SourceKind::Document ? "D" : "C") + "\n" +
                      s.source_id + "\n";
  for (const auto &name : {std::string(".lease"), std::string(".store")}) {
    Fd fd(::openat(result->impl_->root.value, name.c_str(),
                   O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (fd.value < 0)
      io_error("criar metadado privado");
    if (::fchmod(fd.value, 0600) != 0)
      io_error("permissão de metadado");
    if (name == ".store")
      write_all(fd.value,
                marker + persistence::composition_checksum(marker) + "\n");
    sync_fd(fd.value, "sincronizar metadado");
    if (name == ".lease")
      result->impl_->lock();
  }
  result->impl_->marker_text =
      marker + persistence::composition_checksum(marker) + "\n";
  sync_fd(result->impl_->root.value, "sincronizar diretório novo");
  sync_fd(parent.value, "sincronizar criação do diretório");
  return result;
}
std::unique_ptr<CheckpointStore>
CheckpointStore::open_existing(const std::filesystem::path &path, Scope scope,
                               StoreOptions options) {
  check_scope(scope);
  check_options(options);
  Fd root(safe_directory(path));
  private_directory(root.value);
  auto result = std::unique_ptr<CheckpointStore>(new CheckpointStore);
  result->impl_ = std::make_unique<Impl>(std::move(root), std::move(scope),
                                         std::move(options));
  root.value = -1;
  result->impl_->lock();
  const auto marker = read_file(result->impl_->root.value, ".store", 512);
  const auto &s = result->impl_->scope;
  const auto head = std::string(marker_magic);
  if (marker.size() < head.size() + 37 || !marker.starts_with(head))
    fail("store_marker_invalid",
         "Diretório não é um laboratório OBJ-01 reconhecido.");
  result->impl_->store_id = marker.substr(head.size(), 36);
  const auto expected = head + result->impl_->store_id + "\n" + s.project_id +
                        "\n" + (s.kind == SourceKind::Document ? "D" : "C") +
                        "\n" + s.source_id + "\n";
  if (!g_uuid_string_is_valid(result->impl_->store_id.c_str()) ||
      marker != expected + persistence::composition_checksum(expected) + "\n")
    fail("store_scope_mismatch",
         "Marcador corrompido ou pertencente a outro escopo.");
  result->impl_->marker_text = marker;
  (void)result->impl_->scan();
  return result;
}
const std::string &CheckpointStore::id() const { return impl_->store_id; }
const Scope &CheckpointStore::scope() const { return impl_->scope; }
Inventory CheckpointStore::inventory() const { return impl_->scan(); }
SessionCheckpoint CheckpointStore::load(std::uint64_t sequence) const {
  impl_->check();
  if (!sequence)
    fail("store_sequence_invalid", "Geração zero não existe.");
  return impl_->record(sequence);
}
SessionCheckpoint CheckpointStore::latest() const {
  auto i = impl_->scan();
  if (i.generations.empty())
    fail("store_empty", "Não há geração publicada.");
  return impl_->record(i.generations.back().sequence);
}
PublishResult CheckpointStore::publish(const SessionCheckpoint &checkpoint) {
  impl_->check(true);
  validate_checkpoint(checkpoint, impl_->scope);
  const auto i = impl_->scan();
  if (std::any_of(i.generations.begin(), i.generations.end(),
                  [](const auto &g) { return !g.valid; }))
    fail("store_requires_review",
         "Geração inválida preservada; publicação exige revisão.");
  const auto last = i.generations.empty() ? 0 : i.generations.back().sequence;
  if (last == std::numeric_limits<std::uint64_t>::max())
    fail("store_sequence_exhausted", "Sequência esgotada.");
  PublishResult out;
  out.sequence = last + 1;
  const auto bytes = std::string(record_magic) + impl_->store_id + "\n" +
                     sequence_text(out.sequence) + "\n" + checkpoint.sha256 +
                     "\n" + checkpoint.bytes;
  if (i.generations.size() + i.incomplete.size() >= impl_->options.max_files ||
      bytes.size() > impl_->options.max_bytes ||
      i.file_bytes > impl_->options.max_bytes - bytes.size())
    fail("store_quota",
         "Quota atingida; versões anteriores foram preservadas.");
  const auto temporary = ".pending-" + project::new_uuid();
  try {
    impl_->stage(out.stage);
    Fd fd(::openat(impl_->root.value, temporary.c_str(),
                   O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (fd.value < 0)
      io_error("criar geração temporária");
    if (::fchmod(fd.value, 0600) != 0)
      io_error("permissão da geração");
    out.stage = PublishStage::Created;
    impl_->stage(out.stage);
    write_all(fd.value, bytes, &impl_->options);
    out.stage = PublishStage::Written;
    impl_->stage(out.stage);
    sync_fd(fd.value, "sincronizar geração", &impl_->options,
            IoPoint::FileSync);
    out.stage = PublishStage::FileSynced;
    impl_->stage(out.stage);
    const int injected = impl_->options.inject_errno
                             ? impl_->options.inject_errno(IoPoint::Publish)
                             : 0;
    if (injected) {
      errno = injected;
      io_error("publicar geração");
    }
    if (::syscall(SYS_renameat2, impl_->root.value, temporary.c_str(),
                  impl_->root.value, filename(out.sequence).c_str(),
                  RENAME_NOREPLACE) != 0)
      io_error("publicar sem substituir geração");
    out.visible = true;
    out.stage = PublishStage::Published;
    impl_->stage(out.stage);
    sync_fd(impl_->root.value, "sincronizar publicação", &impl_->options,
            IoPoint::DirectorySync);
    out.durable = true;
    out.stage = PublishStage::DirectorySynced;
    impl_->stage(out.stage);
    return out;
  } catch (const std::bad_alloc &) {
    impl_->poisoned = true;
    throw;
  } catch (const std::exception &e) {
    impl_->poisoned = true;
    out.diagnostic = e.what();
    return out;
  } catch (...) {
    impl_->poisoned = true;
    throw;
  }
}
std::unique_ptr<RetentionPlan>
CheckpointStore::plan_retention(std::size_t keep) const {
  impl_->check(true);
  if (keep < 2 || keep > 128)
    fail("store_retention_invalid",
         "Retenção conserva pelo menos duas gerações.");
  const auto i = impl_->scan();
  if (std::any_of(i.generations.begin(), i.generations.end(),
                  [](const auto &g) { return !g.valid; }))
    fail("store_requires_review", "Retenção não remove evidência inválida.");
  auto plan = std::unique_ptr<RetentionPlan>(new RetentionPlan);
  plan->lease_id_ = impl_->lease_id;
  plan->inventory_sha256_ = fingerprint(i);
  if (i.generations.size() > keep)
    plan->remove_.assign(i.generations.begin(),
                         i.generations.end() -
                             static_cast<std::ptrdiff_t>(keep));
  return plan;
}
RetentionResult CheckpointStore::apply_retention(RetentionPlan &plan) {
  impl_->check(true);
  if (plan.consumed_ || plan.lease_id_ != impl_->lease_id)
    fail("store_retention_scope", "Plano consumido ou de outro lease.");
  const auto i = impl_->scan();
  if (plan.inventory_sha256_ != fingerprint(i))
    fail("store_retention_stale",
         "Inventário mudou; prepare e confira outro plano.");
  RetentionResult out;
  out.removed.reserve(plan.remove_.size());
  plan.consumed_ = true;
  try {
    for (const auto &g : plan.remove_) {
      const auto checked = impl_->record(g.sequence);
      if (checked.sha256 != g.sha256)
        fail("store_retention_stale", "Geração mudou antes da remoção.");
      const int injected = impl_->options.inject_errno
                               ? impl_->options.inject_errno(IoPoint::Remove)
                               : 0;
      if (injected) {
        errno = injected;
        io_error("remover geração antiga");
      }
      if (::unlinkat(impl_->root.value, filename(g.sequence).c_str(), 0) != 0)
        io_error("remover geração antiga");
      out.removed.push_back(g.sequence);
      out.durability_unknown = true;
      sync_fd(impl_->root.value, "sincronizar retenção", &impl_->options,
              IoPoint::RetentionSync);
      out.durability_unknown = false;
    }
    out.complete = true;
    return out;
  } catch (const std::bad_alloc &) {
    impl_->poisoned = true;
    throw;
  } catch (const std::exception &e) {
    impl_->poisoned = true;
    out.diagnostic = e.what();
    return out;
  } catch (...) {
    impl_->poisoned = true;
    throw;
  }
}
} // namespace inde::objects::planning::recoverylab

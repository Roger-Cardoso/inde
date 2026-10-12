#include "recovery_export_support.hpp"
#include "recovery_codec.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/semantic_store.hpp"
#include "inde/project/manifest.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>

namespace inde::persistence::recovery {
namespace {
constexpr std::size_t export_limit = project::recovery_payload_limit * 2 + 8 * 1024 * 1024;
struct Descriptor {
  int fd{-1};
  explicit Descriptor(int value) : fd(value) {}
  ~Descriptor() { if (fd>=0) ::close(fd); }
  Descriptor(const Descriptor &)=delete;
};
void safe_path(const std::filesystem::path &p) {
  for (auto path=p; !path.empty() && path!=path.root_path(); path=path.parent_path())
    if (std::filesystem::is_symlink(path))
      throw std::runtime_error("Exportação recusa caminho simbólico");
}
void sync_folder(const std::filesystem::path &path) {
  Descriptor dir(::open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
  if (dir.fd<0 || ::fsync(dir.fd)) throw std::runtime_error("Exportação não confirmou sincronização do diretório");
}
}
std::filesystem::path export_path(const project::Project &owner, const project::RecoveryRecord &r) {
  safe_id(r.project_id); safe_id(r.session_id);
  if (r.project_id!=owner.manifest().project_id || r.checksum.size()!=64 ||
      r.checksum.find_first_not_of("0123456789abcdef")!=std::string::npos)
    throw std::runtime_error("Identidade/hash inválidos para exportação");
  const auto path=owner.path()/"recovery/editor-v1/exports"/
      (r.project_id+"-"+r.session_id+"-"+r.checksum+".inde-recovery.json");
  safe_path(path); return path;
}
project::RecoveryRecord read_export(const std::filesystem::path &path) {
  safe_path(path);
  Descriptor file(::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
  struct stat stat{};
  if (file.fd<0 || ::fstat(file.fd,&stat) || !S_ISREG(stat.st_mode) ||
      stat.st_size<0 || static_cast<std::uint64_t>(stat.st_size)>export_limit)
    throw std::runtime_error("Arquivo exportado ausente, irregular ou fora do limite");
  std::string bytes; bytes.reserve(static_cast<std::size_t>(stat.st_size));
  char chunk[16384];
  for (;;) {
    const auto n=::read(file.fd,chunk,sizeof(chunk));
    if (n<0) { if (errno==EINTR) continue; throw std::runtime_error("Falha ao ler exportação"); }
    if (!n) break;
    bytes.append(chunk,static_cast<std::size_t>(n));
    if (bytes.size()>export_limit) throw std::runtime_error("Arquivo exportado cresceu além do limite");
  }
  // Bound structure before the recursive generic JSON parser. Payload/type
  // JSON is stored as strings and has its own existing depth/version guards.
  if (!g_utf8_validate(bytes.data(),static_cast<gssize>(bytes.size()),nullptr))
    throw std::runtime_error("Exportação contém UTF-8 inválido");
  bool quoted{}, escaped{}; int depth{};
  for (const char ch : bytes) {
    if (quoted) {
      if (escaped) escaped=false;
      else if (ch=='\\') escaped=true;
      else if (ch=='\"') quoted=false;
    } else if (ch=='\"') quoted=true;
    else if (ch=='{' || ch=='[') {
      if (++depth>3) throw std::runtime_error("Exportação possui JSON profundo demais");
    } else if (ch=='}' || ch==']') --depth;
  }
  const auto value=json::parse(bytes);
  if (value.as_object().size()!=7 || value.at("format").as_string()!="inde.editor-recovery-export" ||
      value.at("version").as_integer()!=1)
    throw std::runtime_error("Formato/versão de exportação incompatível");
  auto envelope=value.as_object(); envelope.erase("archive_sha256");
  if (checksum(json::serialize(envelope,false))!=value.at("archive_sha256").as_string())
    throw std::runtime_error("Checksum do arquivo exportado inválido");
  for (const auto &type : value.at("types").as_array())
    static_cast<void>(decode_semantic_type(type.as_string()));
  return decode(value.at("payload").as_string(),value.at("sha256").as_string());
}
std::filesystem::path publish_export(const project::Project &owner,
    const project::RecoveryRecord &record, const std::string &payload) {
  auto decoded=decode(payload,record.checksum);
  if (decoded.project_id!=owner.manifest().project_id || decoded.session_id!=record.session_id)
    throw std::runtime_error("Exportação pertence a outro contexto");
  const auto destination=export_path(owner,record), folder=destination.parent_path();
  if (std::filesystem::create_directory(folder)) {
    std::filesystem::permissions(folder,std::filesystem::perms::owner_all);
    sync_folder(folder.parent_path());
  }
  if (std::filesystem::exists(destination)) {
    const auto existing=read_export(destination);
    if (existing.checksum!=record.checksum || existing.session_id!=record.session_id ||
        existing.project_id!=record.project_id) throw std::runtime_error("Destino existente divergente; não sobrescrito");
    return destination;
  }
  json::Value::Array types;
  for (const auto &t : SemanticStore{}.types(owner.path())) {
    const auto used=[&](const project::Document &d) {
      for (const auto &a : d.semantics.annotations)
        if (a.type_key==t.key && a.type_version==t.version) return true;
      return false;
    };
    if (used(record.state.document) || used(record.baseline))
      types.emplace_back(encode_semantic_type(t));
  }
  json::Value::Object envelope{
      {"format","inde.editor-recovery-export"},{"version",1},
      {"exported_at",project::utc_now()},{"payload",payload},
      {"sha256",record.checksum},{"types",std::move(types)}};
  envelope.emplace("archive_sha256",checksum(json::serialize(envelope,false)));
  const auto data=json::serialize(envelope,true);
  if (data.size()>export_limit) throw std::runtime_error("Exportação excede limite");
  const auto temporary=folder/(".export-"+project::new_uuid()+".tmp");
  Descriptor file(::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600));
  if (file.fd<0) throw std::runtime_error("Não foi possível criar temporário exclusivo de exportação");
  try {
    std::size_t done{};
    while (done<data.size()) {
      const auto n=::write(file.fd,data.data()+done,data.size()-done);
      if (n<0 && errno==EINTR) continue;
      if (n<=0) throw std::runtime_error("Falha ao escrever exportação; journal preservado");
      done+=static_cast<std::size_t>(n);
    }
    if (::fsync(file.fd)) throw std::runtime_error("Exportação não confirmou fsync");
    const auto verify=read_export(temporary);
    if (verify.checksum!=record.checksum) throw std::runtime_error("Exportação não passou na reabertura");
    if (::link(temporary.c_str(),destination.c_str())) {
      if (errno!=EEXIST) throw std::runtime_error("Não foi possível promover exportação sem sobrescrita");
      if (read_export(destination).checksum!=record.checksum)
        throw std::runtime_error("Destino concorrente divergente; journal preservado");
    }
    sync_folder(folder);
    if (::unlink(temporary.c_str())) throw std::runtime_error("Exportação concluída; temporário retido");
    sync_folder(folder);
    return destination;
  } catch (...) { ::unlink(temporary.c_str()); throw; }
}
} // namespace inde::persistence::recovery

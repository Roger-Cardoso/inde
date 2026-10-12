#include "inde/application/document_export.hpp"
#include "inde/persistence/json.hpp"
#include "inde/project/manifest.hpp"
#include <glib.h>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <set>
#include <unistd.h>
namespace inde::application {
namespace {
std::string escape(const std::string &text) {
  std::string out;
  for(const auto c:text) {
    switch(c) { case '&': out+="&amp;"; break; case '<': out+="&lt;"; break;
      case '>':out+="&gt;";break;case '"':out+="&quot;";break;case '\r':out+="&#13;";break;
      default:out+=c; }
  }
  return out;
}
std::string hash(const std::string &data) {
  auto *h=g_compute_checksum_for_data(G_CHECKSUM_SHA256,reinterpret_cast<const guchar*>(data.data()),data.size());
  const std::string value=h;g_free(h);return value;
}
std::string style(const project::DocumentFormatSpan &s) {
  using S=project::DocumentTextStyle;
  switch(s.style) {
    case S::Bold: return "font-weight:700;";
    case S::Italic:return "font-style:italic;";
    case S::Underline:return "text-decoration-line:underline;";
    case S::Strikethrough:return "text-decoration-line:line-through;";
    case S::Foreground:return "color:"+s.value+";";
    case S::FontSize:return "font-size:"+s.value+"pt;";
    default:return {};
  }
}
struct Fd { int value; ~Fd(){if(value>=0)::close(value);} };
void write_checked(const std::filesystem::path &path,const std::string &data) {
  Fd fd{::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600)};
  if(fd.value<0) throw std::runtime_error("Não foi possível criar arquivo exclusivo da exportação");
  std::size_t done{};
  while(done<data.size()) {
    const auto n=::write(fd.value,data.data()+done,data.size()-done);
    if(n<0 && errno==EINTR) continue;
    if(n<=0) throw std::runtime_error("Falha de gravação da exportação; pasta incompleta preservada");
    done+=static_cast<std::size_t>(n);
  }
  if(::fsync(fd.value)) throw std::runtime_error("Não foi possível sincronizar a exportação");
  std::ifstream input(path,std::ios::binary);
  const std::string readback((std::istreambuf_iterator<char>(input)),{});
  if(readback!=data) throw std::runtime_error("Readback da exportação divergiu; pasta incompleta preservada");
}
}
DocumentExport prepare_document_export(const std::string &project_id,const project::Document &d) {
  if(project_id.empty() || d.id.empty() ||
      !g_utf8_validate(d.content.data(),d.content.size(),nullptr) ||
      !g_utf8_validate(d.title.data(),d.title.size(),nullptr) ||
      d.content.find('\0')!=std::string::npos || d.title.find('\0')!=std::string::npos)
    throw std::runtime_error("Documento inválido para exportação");
  DocumentExport out{project_id,d.id,d.title,d.updated_at,d.content,{}, {}};
  const auto count=project::utf8_character_count(d.content);
  std::set<std::size_t> cuts{0,count};
  std::set<project::DocumentTextStyle> omitted;
  for(const auto &s:d.formatting) {
    project::validate_format_span(s);
    if(s.start_offset>=s.end_offset || s.end_offset>count) throw std::runtime_error("Formatação fora do texto");
    if(style(s).empty()) omitted.insert(s.style);
    else { cuts.insert(s.start_offset); cuts.insert(s.end_offset); }
  }
  out.html="<!doctype html>\n<html lang=\"pt-BR\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'\">"
      "<title>"+escape(d.title)+"</title><style>"
      "body{max-width:48rem;margin:3rem auto;padding:0 1.5rem;color:#18212c;background:#fff;font-family:serif}"
      "#document-text{white-space:pre-wrap;overflow-wrap:anywhere;font-size:12pt;line-height:1.65}"
      "@media print{body{max-width:none;margin:0}h1{break-after:avoid}}"
      "</style></head><body><h1>"+escape(d.title)+"</h1><main id=\"document-text\">";
  // Walk UTF-8 once; offsets refer to codepoints, never bytes.
  std::vector<std::size_t> bytes;bytes.reserve(count+1);
  for(const char *p=d.content.data();p<d.content.data()+d.content.size();p=g_utf8_next_char(p))
    bytes.push_back(static_cast<std::size_t>(p-d.content.data()));
  bytes.push_back(d.content.size());
  for(auto it=cuts.begin();it!=cuts.end();) {
    const auto a=*it++;if(it==cuts.end())break;const auto b=*it;
    std::string css;bool underline{},strike{};
    for(const auto &s:d.formatting) if(s.start_offset<=a && s.end_offset>=b) {
      if(s.style==project::DocumentTextStyle::Underline) underline=true;
      else if(s.style==project::DocumentTextStyle::Strikethrough) strike=true;
      else css+=style(s);
    }
    if(underline || strike) css+="text-decoration-line:"+std::string(underline?"underline ":"")+(strike?"line-through":"")+";";
    if(!css.empty())out.html+="<span style=\""+css+"\">";
    out.html+=escape(d.content.substr(bytes[a],bytes[b]-bytes[a]));
    if(!css.empty())out.html+="</span>";
  }
  out.html+="</main></body></html>\n";
  out.limitations={"TXT conserva o corpo UTF-8 exato, mas não a formatação.",
      "HTML conserva texto e estilos inline suportados; paginação, layout e fontes dependem do leitor.",
      "Cores autorais são preservadas; confira seu contraste no fundo branco do HTML.",
      "Saída de um Documento, não montagem ordenada da Obra, DOCX, PDF ou EPUB.",
      "Não é backup do Projeto nem formato para reimportação sem perdas."};
  if(!omitted.empty()) {
    std::string list="Formatação não representada no HTML: ";
    for(auto s:omitted)list+=project::to_string(s)+"; ";
    out.limitations.push_back(std::move(list));
  }
  out.limitations.push_back("Camadas semânticas/comentários, âncoras, vínculos, perspectiva, organização e comandos inline não são incluídos na leitura exportada.");
  return out;
}
void write_document_export(const DocumentExport &out,const std::filesystem::path &target) {
  if(!target.is_absolute() || target.filename().empty()) throw std::runtime_error("Escolha uma pasta absoluta para exportar");
  for(auto p=target;!p.empty() && p!=p.root_path();p=p.parent_path())
    if(std::filesystem::is_symlink(p))throw std::runtime_error("Exportação recusa caminho simbólico");
  if(!std::filesystem::is_directory(target.parent_path()) || !std::filesystem::create_directory(target))
    throw std::runtime_error("A pasta de exportação deve ser nova; nada foi sobrescrito");
  std::filesystem::permissions(target,std::filesystem::perms::owner_all);
  write_checked(target/"documento.txt",out.text);
  write_checked(target/"documento.html",out.html);
  persistence::json::Value::Array losses;
  for(const auto &s:out.limitations) losses.emplace_back(s);
  const auto report=persistence::json::serialize(persistence::json::Value::Object{
      {"format","inde.document-reading-export"},{"version",1},{"complete",true},
      {"project_id",out.project_id},{"document_id",out.document_id},{"title",out.title},
      {"source_updated_at",out.source_updated_at},{"exported_at",project::utc_now()},
      {"text_sha256",hash(out.text)},{"html_sha256",hash(out.html)},
      {"limitations",std::move(losses)},{"readback_verified",true}},true);
  write_checked(target/"relatorio.json",report);
  Fd dir{::open(target.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW)};
  Fd parent{::open(target.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW)};
  if(dir.value<0 || parent.value<0 || ::fsync(dir.value) || ::fsync(parent.value))
    throw std::runtime_error("Exportação gravada, mas a sincronização da pasta não pôde ser confirmada");
}
}

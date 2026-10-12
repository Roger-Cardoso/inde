#include "inde/application/document_export.hpp"
#include "inde/application/editorial_review.hpp"
#include "inde/persistence/json.hpp"
#include "inde/project/manifest.hpp"
#include "production_support.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <glib.h>
#include <set>
#include <unistd.h>
namespace inde::application {
namespace {
void valid_text(const std::string &s) {
  if (!g_utf8_validate(s.data(), s.size(), nullptr))
    throw std::runtime_error("Texto UTF-8 inválido para produção");
  for (const char *p = s.data(); p < s.data() + s.size();
       p = g_utf8_next_char(p)) {
    const auto c = g_utf8_get_char(p);
    if ((c < 32 && c != 9 && c != 10 && c != 13) || c == 0xfffe || c == 0xffff)
      throw std::runtime_error("Texto contém caractere não representável em "
                               "XML; fonte não foi alterada");
  }
}
struct Fd {
  int n;
  ~Fd() {
    if (n >= 0)
      ::close(n);
  }
};
void save(const std::filesystem::path &p, const std::string &data) {
  Fd fd{::open(p.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
               0600)};
  if (fd.n < 0)
    throw std::runtime_error(
        "Não foi possível criar arquivo exclusivo da produção");
  for (std::size_t at = 0; at < data.size();) {
    const auto n = ::write(fd.n, data.data() + at, data.size() - at);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      throw std::runtime_error(
          "Falha de gravação; pasta incompleta preservada");
    at += static_cast<std::size_t>(n);
  }
  if (::fsync(fd.n))
    throw std::runtime_error("Falha de sincronização; produção não confirmada");
  std::ifstream in(p, std::ios::binary);
  const std::string back((std::istreambuf_iterator<char>(in)), {});
  if (back != data)
    throw std::runtime_error("Readback divergiu; produção não confirmada");
}
} // namespace
std::vector<project::StructuralNode>
production_order(const std::vector<project::StructuralNode> &nodes) {
  if (nodes.size() > 10000)
    throw std::runtime_error(
        "Estrutura excede 10000 unidades; nada foi truncado");
  std::map<std::string, project::StructuralNode> index;
  std::map<std::string, std::vector<project::StructuralNode>> children;
  for (const auto &n : nodes)
    if (n.id.empty() || !index.emplace(n.id, n).second)
      throw std::runtime_error("Identidade editorial vazia/duplicada");
  for (const auto &n : nodes) {
    if (n.parent_id && !index.contains(*n.parent_id))
      throw std::runtime_error("Estrutura possui ancestral ausente");
    children[n.parent_id.value_or("")].push_back(n);
  }
  for (auto &[parent, siblings] : children) {
    (void)parent;
    std::sort(siblings.begin(), siblings.end(),
              [](auto &a, auto &b) { return a.position < b.position; });
    for (std::size_t i = 1; i < siblings.size(); ++i)
      if (siblings[i].position == siblings[i - 1].position)
        throw std::runtime_error("Ordem entre irmãos ambígua; organize no "
                                 "Manuscrito antes de produzir");
  }
  std::vector<project::StructuralNode> out;
  std::set<std::string> visiting;
  std::function<void(const std::string &, int)> walk = [&](const auto &parent,
                                                           int depth) {
    if (depth > 64)
      throw std::runtime_error("Estrutura excede 64 níveis");
    for (const auto &n : children[parent]) {
      if (!visiting.insert(n.id).second)
        throw std::runtime_error("Ciclo na estrutura editorial");
      out.push_back(n);
      walk(n.id, depth + 1);
    }
  };
  walk("", 0);
  if (out.size() != nodes.size())
    throw std::runtime_error("Ciclo ou ramo editorial inacessível");
  return out;
}
WorkProduction prepare_work_production(
    const std::string &project_id, const std::string &structure_id,
    const project::Work &w, const std::vector<project::StructuralNode> &nodes,
    const std::vector<project::Document> &docs,
    const ProductionChoices &choices, const ProductionOptions &options) {
  using namespace production_detail;
  if (project_id.empty() || structure_id.empty() || w.id.empty() ||
      choices.size() > 10000 || options.author.size() > 1024)
    throw std::runtime_error("Identidade/opções de produção inválidas");
  if (options.profile != ProductionProfile::Manuscript &&
      options.profile != ProductionProfile::Reading)
    throw std::runtime_error("Perfil de produção desconhecido");
  valid_text(w.title);
  valid_text(w.subtitle);
  valid_text(options.author);
  valid_text(w.language);
  if (w.title.empty() || w.language.empty())
    throw std::runtime_error("Título/idioma da Obra obrigatório");
  const auto ordered = production_order(nodes);
  std::map<std::string, project::Document> by_id;
  for (const auto &d : docs)
    if (!by_id.emplace(d.id, d).second)
      throw std::runtime_error("Documento duplicado na fonte");
  std::set<std::string> known, selected;
  std::map<std::string, std::size_t> depth;
  WorkProduction out{
      project_id, structure_id, project::utc_now(), w, options, {}, {}, {}, {}};
  std::size_t bytes{}, body_count{};
  out.preview = w.title + "\n" + (w.subtitle.empty() ? "" : w.subtitle + "\n") +
                (options.author.empty() ? "" : options.author + "\n");
  for (const auto &n : ordered) {
    if (n.work_id != w.id || n.structure_id != structure_id)
      throw std::runtime_error("Unidade pertence a outra Obra/estrutura");
    valid_text(n.title);
    valid_text(n.designator);
    known.insert(n.id);
    depth[n.id] = n.parent_id ? depth.at(*n.parent_id) + 1 : 0;
    const auto it = choices.find(n.id);
    if (it == choices.end()) {
      out.omissions.push_back("Unidade omitida: " + n.id + " — " + n.title);
      continue;
    }
    if (!it->second.heading && it->second.documents.empty())
      throw std::runtime_error("Seleção vazia; omita a unidade explicitamente");
    ProductionUnit unit{n, depth[n.id], it->second.heading, {}};
    if (unit.heading)
      out.preview += "\n" + n.designator + (n.designator.empty() ? "" : " ") +
                     n.title + "\n";
    for (const auto &id : it->second.documents) {
      if (!selected.insert(id).second)
        throw std::runtime_error("Mesmo Documento selecionado mais de uma vez");
      const auto d = by_id.find(id);
      if (d == by_id.end() || d->second.editorial_node_id != n.id)
        throw std::runtime_error(
            "Documento ausente ou não situado nesta unidade");
      valid_text(d->second.content);
      valid_text(d->second.title);
      const auto characters = project::utf8_character_count(d->second.content);
      for (const auto &s : d->second.formatting) {
        project::validate_format_span(s);
        if (s.start_offset >= s.end_offset || s.end_offset > characters)
          throw std::runtime_error("Formatação fora do texto");
      }
      bytes += d->second.content.size();
      if (bytes > 8 * 1024 * 1024)
        throw std::runtime_error(
            "Produção excede 8 MiB de texto; nada foi truncado");
      if (it->second.documents.size() > 1000)
        throw std::runtime_error("Unidade excede 1000 Documentos selecionados");
      const auto state = editorial_state(d->second);
      if (state.state != "approved" || state.outdated)
        out.limitations.push_back(
            "Documento sem decisão de revisão vigente aprovada: " +
            d->second.title);
      out.preview += "\n" + d->second.content + "\n";
      unit.documents.push_back(d->second);
      ++body_count;
    }
    out.units.push_back(std::move(unit));
  }
  for (const auto &[id, c] : choices) {
    (void)c;
    if (!known.contains(id))
      throw std::runtime_error("Seleção contém unidade ausente");
  }
  if (!body_count)
    throw std::runtime_error(
        "Selecione ao menos um Documento; só títulos não produzem uma Obra");
  for (const auto &d : docs)
    if (!selected.contains(d.id))
      out.omissions.push_back("Documento não incluído: " + d.id + " — " +
                              d.title);
  out.limitations.insert(
      out.limitations.end(),
      {"Saída derivada para leitura/avaliação, não backup, publicação "
       "automática ou Composição persistente.",
       "DOCX/EPUB/HTML/PDF conservam negrito, itálico, sublinhado e tachado; "
       "outros estilos autorais são substituídos pelo perfil e declarados por "
       "Documento.",
       "Parágrafos dos formatos profissionais normalizam CRLF/CR para LF. TXT "
       "usa corpos exatos com títulos/separadores gerados.",
       "Camadas semânticas, comentários, sugestões, decisões, âncoras, "
       "vínculos e comandos não são incorporados ao texto nem transferem "
       "aprovação.",
       "PDF não tem certificação PDF/A/PDF/UA, sangria, marcas de corte ou "
       "preparação gráfica para impressão comercial.",
       "EPUB é refluível; leitor pode alterar fontes, espaços e quebras. Sem "
       "capa/imagens/DRM, notas editoriais externas ou colaboração.",
       "Seleção e ordem dos textos são explícitas nesta preparação; não mudam "
       "a organização canônica do Projeto."});
  for (const auto &u : out.units)
    for (const auto &d : u.documents) {
      std::set<std::string> omitted;
      for (const auto &s : d.formatting)
        if (s.style != project::DocumentTextStyle::Bold &&
            s.style != project::DocumentTextStyle::Italic &&
            s.style != project::DocumentTextStyle::Underline &&
            s.style != project::DocumentTextStyle::Strikethrough)
          omitted.insert(project::to_string(s.style));
      if (!omitted.empty()) {
        std::string s = "Estilos substituídos em " + d.title + ": ";
        for (const auto &v : omitted)
          s += v + "; ";
        out.limitations.push_back(s);
      }
    }
  return out;
}
void write_work_production(const WorkProduction &out,
                           const std::filesystem::path &target,
                           std::stop_token stop) {
  using namespace production_detail;
  cancelled(stop);
  // Prepare all format bytes before touching the destination, including
  // missing-font failures.
  std::vector<std::pair<std::string, std::string>> files{
      {"obra.txt", out.preview}};
  std::string html =
      "<!doctype html><html lang=\"" + xml(out.work.language) +
      "\"><head><meta charset=\"utf-8\"/><meta "
      "http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; "
      "style-src 'unsafe-inline'\"/><title>" +
      xml(out.work.title) +
      "</title><style>body{max-width:44em;margin:3em auto;padding:0 "
      "1em;color:#000;background:#fff;font-family:serif;font-size:12pt}p{white-"
      "space:pre-wrap;overflow-wrap:anywhere;line-height:" +
      (out.options.profile == ProductionProfile::Manuscript ? "2" : "1.5") +
      "}h1,h2,h3,h4,h5,h6{break-after:avoid}</style></head><body><h1>" +
      xml(out.work.title) + "</h1>" +
      (out.work.subtitle.empty() ? ""
                                 : "<p>" + xml(out.work.subtitle) + "</p>") +
      "<p>" + xml(out.options.author) + "</p>";
  for (const auto &u : out.units) {
    cancelled(stop);
    const auto level = std::to_string(std::min<std::size_t>(6, u.depth + 2));
    if (u.heading)
      html += "<h" + level + ">" +
              xml(u.node.designator + (u.node.designator.empty() ? "" : " ") +
                  u.node.title) +
              "</h" + level + ">";
    for (const auto &d : u.documents)
      for (const auto &p : paragraphs(d, stop)) {
        html += "<p>";
        for (const auto &r : p.runs)
          html += (r.bold ? "<b>" : "") + std::string(r.italic ? "<i>" : "") +
                  (r.underline ? "<u>" : "") + (r.strike ? "<s>" : "") +
                  xml(r.text) + (r.strike ? "</s>" : "") +
                  (r.underline ? "</u>" : "") + (r.italic ? "</i>" : "") +
                  (r.bold ? "</b>" : "");
        html += "</p>";
      }
  }
  files.emplace_back("obra.html", html + "</body></html>");
  if (out.options.docx)
    files.emplace_back("obra.docx", make_docx(out, stop));
  if (out.options.epub)
    files.emplace_back("obra.epub", make_epub(out, stop));
  if (out.options.pdf)
    files.emplace_back("obra.pdf", make_pdf(out, stop));
  cancelled(stop);
  if (!target.is_absolute() || target.filename().empty())
    throw std::runtime_error("Destino absoluto obrigatório");
  for (auto p = target; !p.empty() && p != p.root_path(); p = p.parent_path())
    if (std::filesystem::is_symlink(p))
      throw std::runtime_error("Destino simbólico recusado");
  if (!std::filesystem::is_directory(target.parent_path()) ||
      !std::filesystem::create_directory(target))
    throw std::runtime_error("Pasta precisa ser nova; nada foi sobrescrito");
  std::filesystem::permissions(target, std::filesystem::perms::owner_all);
  using J = persistence::json::Value;
  J::Array outputs, units, losses, omissions;
  for (const auto &[name, data] : files) {
    cancelled(stop);
    save(target / name, data);
    outputs.emplace_back(
        J::Object{{"file", name},
                  {"sha256", sha(data)},
                  {"bytes", static_cast<std::int64_t>(data.size())}});
  }
  for (const auto &u : out.units) {
    J::Array docs;
    for (const auto &d : u.documents)
      docs.emplace_back(J::Object{{"id", d.id},
                                  {"title", d.title},
                                  {"source_updated_at", d.updated_at},
                                  {"body_sha256", sha(d.content)}});
    units.emplace_back(J::Object{{"node", u.node.id},
                                 {"heading", u.heading},
                                 {"documents", std::move(docs)}});
  }
  for (const auto &s : out.limitations)
    losses.emplace_back(s);
  for (const auto &s : out.omissions)
    omissions.emplace_back(s);
  Fd dir{
      ::open(target.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)},
      parent{::open(target.parent_path().c_str(),
                    O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)};
  if (dir.n < 0 || parent.n < 0 || ::fsync(dir.n) || ::fsync(parent.n))
    throw std::runtime_error("Sincronização da pasta não confirmada");
  cancelled(stop);
  save(target / "relatorio.json",
       persistence::json::serialize(
           J::Object{
               {"format", "inde.work-production"},
               {"version", 1},
               {"complete", true},
               {"readback_verified", true},
               {"project_id", out.project_id},
               {"work_id", out.work.id},
               {"structure_id", out.structure_id},
               {"title", out.work.title},
               {"author", out.options.author},
               {"profile", out.options.profile == ProductionProfile::Manuscript
                               ? "manuscript"
                               : "reading"},
               {"prepared_at", out.prepared_at},
               {"exported_at", project::utc_now()},
               {"outputs", std::move(outputs)},
               {"units", std::move(units)},
               {"limitations", std::move(losses)},
               {"omissions", std::move(omissions)}},
           true));
  if (::fsync(dir.n) || ::fsync(parent.n))
    throw std::runtime_error(
        "Arquivos gravados; sincronização final não confirmada");
}
} // namespace inde::application

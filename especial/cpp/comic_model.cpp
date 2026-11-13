#include "comic_model.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>
namespace inde::comic {
namespace {
void need(bool yes, std::string_view message) {
  if (!yes)
    throw std::runtime_error(std::string(message));
}
std::string str(const V &v, const char *key) { return v.at(key).as_string(); }
double num(const V &v, const char *key) {
  const auto &n = v.at(key).storage();
  if (auto p = std::get_if<double>(&n))
    return *p;
  return double(v.at(key).as_integer());
}
void text(const std::string &s, std::size_t limit, const char *label) {
  need(s.size() <= limit * 4 && g_utf8_validate(s.data(), s.size(), nullptr) &&
           g_utf8_strlen(s.data(), s.size()) <= static_cast<glong>(limit),
       std::string(label) + ": texto inválido ou muito extenso");
  need(s.find('\0') == std::string::npos, "Texto contém caractere nulo");
}
bool language(const std::string &s) {
  static const std::regex pattern("[A-Za-z]{2,8}(-[A-Za-z0-9]{1,8})*");
  return std::regex_match(s, pattern);
}
bool color(const std::string &s) {
  static const std::regex pattern("#[0-9a-fA-F]{6}");
  return std::regex_match(s, pattern);
}
void finite(double n, double low, double high) {
  need(std::isfinite(n) && n >= low && n <= high, "Geometria fora dos limites");
}
std::string number(double n) {
  std::ostringstream s;
  s.imbue(std::locale::classic());
  s << n;
  return s.str();
}
std::string b64(const std::string &raw) {
  auto *s =
      g_base64_encode(reinterpret_cast<const guchar *>(raw.data()), raw.size());
  std::string out = s;
  g_free(s);
  return out;
}
std::string read(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  need(bool(f), "Recurso não encontrado: " + p.string());
  return {std::istreambuf_iterator<char>(f), {}};
}
std::string ext(const std::string &mime) {
  static const std::map<std::string, std::string> e{
      {"image/png", "png"},  {"image/jpeg", "jpg"},  {"image/webp", "webp"},
      {"image/gif", "gif"},  {"font/ttf", "ttf"},    {"font/otf", "otf"},
      {"font/woff", "woff"}, {"font/woff2", "woff2"}};
  auto i = e.find(mime);
  need(i != e.end(), "Formato de recurso não aceito neste corte");
  return i->second;
}
std::string attrs(const Element &e, const Book &b) {
  return " lang=\"" + escape(e.lang.empty() ? b.lang : e.lang) + "\" dir=\"" +
         e.dir + "\" translate=\"" + (e.translate ? "yes" : "no") + "\"";
}
void escaped_character(std::string &out, char c) {
  switch (c) {
  case '&':
    out += "&amp;";
    break;
  case '<':
    out += "&lt;";
    break;
  case '>':
    out += "&gt;";
    break;
  case '"':
    out += "&quot;";
    break;
  case '\'':
    out += "&#39;";
    break;
  default:
    out += c;
    break;
  }
}
std::string lines(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\n')
      out += "<br>";
    else
      escaped_character(out, c);
  }
  return out;
}
std::string content(const Element &e, const Book &b) {
  return "<p>" + lines(e.text) + "</p>" +
         (e.gloss.empty() ? ""
                          : "<p class=\"sfx-gloss\" translate=\"yes\" lang=\"" +
                                escape(b.lang) + "\" dir=\"auto\">" +
                                lines(e.gloss) + "</p>");
}
std::string html(const Book &b, const V &site, const std::string &body,
                 bool plain, const std::string &fonts) {
  return "<!doctype html><html lang=\"" + escape(b.lang) +
         "\" dir=\"auto\" translate=\"yes\"><head><meta "
         "charset=\"utf-8\"><meta name=\"viewport\" "
         "content=\"width=device-width,initial-scale=1\"><meta "
         "name=\"description\" content=\"" +
         escape(str(site, "description")) + "\"><title>" + escape(b.title) +
         "</title><link rel=\"stylesheet\" href=\"reader.css\"><style>" +
         fonts + "</style></head><body class=\"" +
         (plain ? std::string("plain-reader") : "font-sans") +
         "\" style=\"--accent:" +
         (str(site, "theme") == "ambar"   ? "#e8b777"
          : str(site, "theme") == "verde" ? "#86c7b2"
                                          : "#c3adf0") +
         "\"><header class=\"reader-header\"><a href=\"../home/\">" +
         escape(str(site, "name")) + "</a><h1>" + escape(b.title) + "</h1><p>" +
         escape(str(site, "author")) + "</p></header>" + body +
         "<footer class=\"reader-footer\">Texto vivo · edição comic do "
         "INDE</footer>" +
         (plain ? "" : "<script src=\"reader.js\" defer></script>") +
         "</body></html>";
}
} // namespace
std::string escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s)
    escaped_character(out, c);
  return out;
}
V encode(const Book &b) {
  A panels;
  panels.reserve(b.panels.size());
  O assets;
  for (const auto &[id, a] : b.assets)
    assets[id] = O{{"id", a.id},
                   {"name", a.name},
                   {"mime", a.mime},
                   {"data", a.data},
                   {"role", a.role}};
  for (const auto &p : b.panels) {
    A elements;
    elements.reserve(p.elements.size());
    for (const auto &e : p.elements)
      elements.emplace_back(O{{"id", e.id},
                              {"kind", e.kind},
                              {"shape", e.shape},
                              {"text", e.text},
                              {"gloss", e.gloss},
                              {"speaker", e.speaker},
                              {"lang", e.lang},
                              {"dir", e.dir},
                              {"translate", e.translate},
                              {"align", e.align},
                              {"valign", e.valign},
                              {"font", e.font},
                              {"font_asset", e.font_asset},
                              {"animation", e.animation},
                              {"ink", e.ink},
                              {"background", e.background},
                              {"source_document", e.source_document},
                              {"source_hash", e.source_hash},
                              {"x", e.x},
                              {"y", e.y},
                              {"w", e.w},
                              {"h", e.h},
                              {"size", e.size}});
    O encoded_panel{
        {"id", p.id},       {"title", p.title},           {"alt", p.alt},
        {"image", p.image}, {"background", p.background}, {"ratio", p.ratio}};
    encoded_panel.emplace("elements", std::move(elements));
    panels.emplace_back(std::move(encoded_panel));
  }
  O encoded_book{{"protocol", "inde.comic.project"},
                 {"version", 1},
                 {"id", b.id},
                 {"work_id", b.work_id},
                 {"title", b.title},
                 {"slug", b.slug},
                 {"lang", b.lang},
                 {"layout", b.layout},
                 {"revision", b.revision},
                 {"removed", b.removed}};
  encoded_book.emplace("panels", std::move(panels));
  encoded_book.emplace("assets", std::move(assets));
  return encoded_book;
}
Book decode(const V &v) {
  need(str(v, "protocol") == "inde.comic.project" &&
           v.at("version").as_integer() == 1,
       "Versão de comic incompatível");
  Book b;
  b.id = str(v, "id");
  b.work_id = str(v, "work_id");
  b.title = str(v, "title");
  b.slug = str(v, "slug");
  b.lang = str(v, "lang");
  b.layout = str(v, "layout");
  b.revision = v.at("revision").as_integer();
  b.removed = v.at("removed").as_boolean();
  for (const auto &[id, a] : v.at("assets").as_object())
    b.assets[id] = {str(a, "id"), str(a, "name"), str(a, "mime"),
                    str(a, "data"), str(a, "role")};
  b.panels.reserve(v.at("panels").as_array().size());
  for (const auto &p : v.at("panels").as_array()) {
    Panel panel;
    panel.id = str(p, "id");
    panel.title = str(p, "title");
    panel.alt = str(p, "alt");
    panel.image = str(p, "image");
    panel.background = str(p, "background");
    panel.ratio = num(p, "ratio");
    panel.elements.reserve(p.at("elements").as_array().size());
    for (const auto &e : p.at("elements").as_array()) {
      Element el;
      el.id = str(e, "id");
      el.kind = str(e, "kind");
      el.shape = str(e, "shape");
      el.text = str(e, "text");
      el.gloss = str(e, "gloss");
      el.speaker = str(e, "speaker");
      el.lang = str(e, "lang");
      el.dir = str(e, "dir");
      el.translate = e.at("translate").as_boolean();
      el.align = str(e, "align");
      el.valign = str(e, "valign");
      el.font = str(e, "font");
      el.font_asset = str(e, "font_asset");
      el.animation = str(e, "animation");
      el.ink = str(e, "ink");
      el.background = str(e, "background");
      el.source_document = str(e, "source_document");
      el.source_hash = str(e, "source_hash");
      el.x = num(e, "x");
      el.y = num(e, "y");
      el.w = num(e, "w");
      el.h = num(e, "h");
      el.size = num(e, "size");
      panel.elements.push_back(std::move(el));
    }
    b.panels.push_back(std::move(panel));
  }
  validate(b);
  return b;
}
std::string raw_asset(const Asset &a) {
  need(a.data.size() <= 45 * 1024 * 1024, "Recurso excede o limite");
  need(a.data.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
                                "uvwxyz0123456789+/=") == std::string::npos &&
           a.data.size() % 4 == 0,
       "Base64 inválido");
  gsize n = 0;
  auto *data = g_base64_decode(a.data.c_str(), &n);
  std::string raw(reinterpret_cast<char *>(data), n);
  g_free(data);
  need(b64(raw) == a.data, "Base64 não canônico");
  return raw;
}
Asset import_asset(const std::filesystem::path &path, bool font) {
  need(std::filesystem::is_regular_file(path), "Escolha um arquivo local");
  need(std::filesystem::file_size(path) <= (font ? 32 : 12) * 1024 * 1024,
       "Arquivo excede o limite");
  Asset a;
  a.id = project::new_uuid();
  a.name = path.filename().string();
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  const std::map<std::string, std::string> types{
      {".png", "image/png"},   {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"}, {".webp", "image/webp"},
      {".gif", "image/gif"},   {".ttf", "font/ttf"},
      {".otf", "font/otf"},    {".woff", "font/woff"},
      {".woff2", "font/woff2"}};
  need(types.contains(extension),
       "Use imagem raster ou fonte TTF/OTF/WOFF/WOFF2");
  a.mime = types.at(extension);
  need(a.mime.starts_with(font ? "font/" : "image/"),
       "Tipo de recurso diferente do solicitado");
  a.data = b64(read(path));
  return a;
}
void validate(const Book &b) {
  text(b.title, 200, "Título");
  need(!b.title.empty(), "Informe o título do comic");
  text(b.work_id, 100, "Obra");
  need(language(b.lang), "Idioma da obra inválido");
  static const std::regex slug_pattern("[a-z0-9]+(-[a-z0-9]+)*");
  need(b.slug.size() <= 80 && std::regex_match(b.slug, slug_pattern),
       "Endereço: use letras minúsculas sem acento, números e hífen");
  need(b.slug != "home" && b.slug != "assets", "Endereço reservado");
  need(b.layout == "strip" || b.layout == "mosaic", "Montagem inválida");
  need(b.panels.size() <= 100 && b.assets.size() <= 80,
       "Limite de quadros ou recursos excedido");
  std::set<std::string> ids;
  static const std::regex id_pattern("[A-Za-z0-9_-]+");
  auto identity = [&](const std::string &id) {
    need(id.size() <= 80 && std::regex_match(id, id_pattern) &&
             ids.insert(id).second,
         "Identidade vazia, inválida ou repetida");
  };
  identity(b.id);
  std::size_t total = 0;
  for (const auto &[id, a] : b.assets) {
    identity(id);
    need(a.id == id, "Identidade de recurso divergente");
    text(a.name, 240, "Nome de recurso");
    static_cast<void>(ext(a.mime));
    const auto raw = raw_asset(a);
    total += raw.size();
    need(raw.size() <= (a.mime.starts_with("font/") ? 32 : 12) * 1024 * 1024 &&
             total <= 128 * 1024 * 1024,
         "Recursos excedem o limite");
    need(a.role == "general" || language(a.role), "Idioma da fonte inválido");
    if (a.mime.starts_with("font/")) {
      need(raw.size() >= 12, "Fonte truncada");
      static const std::map<std::string, std::string> magic{
          {"font/ttf", std::string("\0\1\0\0", 4)},
          {"font/otf", "OTTO"},
          {"font/woff", "wOFF"},
          {"font/woff2", "wOF2"}};
      need(raw.substr(0, 4) == magic.at(a.mime),
           "Cabeçalho da fonte não corresponde ao formato");
    } else {
      GError *error = nullptr;
      auto *loader =
          gdk_pixbuf_loader_new_with_mime_type(a.mime.c_str(), &error);
      need(loader, "Imagem não suportada");
      bool excessive = false;
      g_signal_connect(
          loader, "size-prepared",
          G_CALLBACK(+[](GdkPixbufLoader *l, int w, int h, gpointer data) {
            if (w > 16384 || h > 16384 ||
                static_cast<long long>(w) * h > 40000000) {
              *static_cast<bool *>(data) = true;
              gdk_pixbuf_loader_set_size(l, 1, 1);
            }
          }),
          &excessive);
      const bool wrote = gdk_pixbuf_loader_write(
          loader, reinterpret_cast<const guchar *>(raw.data()), raw.size(),
          &error);
      const bool closed =
          gdk_pixbuf_loader_close(loader, wrote ? &error : nullptr);
      auto *pix = gdk_pixbuf_loader_get_pixbuf(loader);
      bool ok = !excessive && wrote && closed && pix &&
                gdk_pixbuf_get_width(pix) <= 16384 &&
                gdk_pixbuf_get_height(pix) <= 16384;
      g_clear_error(&error);
      g_object_unref(loader);
      need(ok, "Imagem inválida ou dimensões excessivas");
    }
  }
  for (const auto &p : b.panels) {
    identity(p.id);
    text(p.title, 200, "Título do quadro");
    text(p.alt, 2000, "Descrição da arte");
    need(color(p.background), "Cor do quadro inválida");
    finite(p.ratio, .4, 3);
    need(p.image.empty() || (b.assets.contains(p.image) &&
                             b.assets.at(p.image).mime.starts_with("image/")),
         "Ilustração ausente");
    need(p.elements.size() <= 100, "Use até 100 elementos por quadro");
    for (const auto &e : p.elements) {
      identity(e.id);
      text(e.text, 10000, "Texto");
      text(e.gloss, 600, "Sentido do som");
      text(e.speaker, 100, "Locutor");
      need(e.lang.empty() || language(e.lang), "Idioma do texto inválido");
      need(e.dir == "auto" || e.dir == "ltr" || e.dir == "rtl",
           "Direção inválida");
      need(e.kind == "bubble" || e.kind == "text" || e.kind == "sfx",
           "Tipo de elemento inválido");
      need(e.shape == "rounded" || e.shape == "oval" || e.shape == "caption" ||
               e.shape == "cloud" || e.shape == "shout",
           "Forma inválida");
      need(e.align == "start" || e.align == "center" || e.align == "end",
           "Alinhamento inválido");
      need(e.valign == "start" || e.valign == "center" || e.valign == "end",
           "Centralização inválida");
      need(e.font == "sans-serif" || e.font == "serif" || e.font == "monospace",
           "Família inválida");
      need(e.font_asset.empty() ||
               (b.assets.contains(e.font_asset) &&
                b.assets.at(e.font_asset).mime.starts_with("font/")),
           "Fonte ausente");
      need(e.animation == "none" || e.animation == "fade" ||
               e.animation == "float" || e.animation == "pulse",
           "Animação inválida");
      need(color(e.ink) && color(e.background), "Cor inválida");
      finite(e.x, 0, 100);
      finite(e.y, 0, 100);
      finite(e.w, 5, 100);
      finite(e.h, 5, 100);
      need(e.x + e.w <= 100.001 && e.y + e.h <= 100.001,
           "Elemento ultrapassa o quadro");
      finite(e.size, 10, 120);
      text(e.source_document, 100, "Origem");
      need(e.source_document.empty() == e.source_hash.empty(),
           "Origem incompleta");
      need(e.source_hash.empty() ||
               std::regex_match(e.source_hash, std::regex("[a-f0-9]{64}")),
           "Hash de origem inválido");
    }
  }
}
Element copy_document(const project::Document &d) {
  Element e;
  e.id = project::new_uuid();
  e.text = d.content;
  e.source_document = d.id;
  e.source_hash = publication::hash(d.content);
  return e;
}
Position snap(const Element &e, double x, double y, bool enabled) {
  Position p{std::clamp(x, 0., 100 - e.w), std::clamp(y, 0., 100 - e.h), false,
             false};
  if (enabled) {
    const double cx = 50 - e.w / 2, cy = 50 - e.h / 2;
    if (std::abs(p.x - cx) <= 1.5) {
      p.x = cx;
      p.snap_x = true;
    }
    if (std::abs(p.y - cy) <= 1.5) {
      p.y = cy;
      p.snap_y = true;
    }
  }
  return p;
}
Book example(const std::string &work) {
  Book b;
  b.id = project::new_uuid();
  b.work_id = work;
  b.title = "A passagem · comic";
  b.slug = "a-passagem";
  for (int i = 0; i < 3; i++) {
    Panel p;
    p.id = project::new_uuid();
    p.title =
        std::vector<std::string>{"A luz", "Do outro lado", "A passagem"}[i];
    p.alt =
        "Ilustração abstrata de demonstração; substitua por arte importada.";
    p.background = i == 1 ? "#354c60" : "#1c3040";
    Element e;
    e.id = project::new_uuid();
    e.text = i == 0   ? "A história também pode ser contada em quadros."
             : i == 1 ? "تستمر القصة خلف الضوء."
                      : "VUUUM";
    if (i == 1) {
      e.lang = "ar";
      e.dir = "auto";
    }
    if (i == 2) {
      e.kind = "sfx";
      e.gloss = "Som grave do portal.";
      e.size = 56;
      e.w = 70;
      e.h = 40;
      e.ink = "#fffaf0";
    }
    p.elements.push_back(e);
    b.panels.push_back(p);
  }
  return b;
}
V public_snapshot(const Book &b, const V &site,
                  const std::filesystem::path &resources) {
  validate(b);
  need(!b.removed && !site.at("removed").as_boolean(),
       "Restaure o comic e o Site antes de gerar");
  need(!b.panels.empty(), "Adicione ao menos um quadro");
  const std::string root = b.slug + "/";
  O files;
  std::map<std::string, std::string> urls;
  std::string fontcss;
  std::vector<std::pair<std::string, std::string>> fonts;
  std::set<std::string> used;
  for (const auto &p : b.panels) {
    if (!p.image.empty())
      used.insert(p.image);
    for (const auto &e : p.elements)
      if (!e.font_asset.empty())
        used.insert(e.font_asset);
  }
  for (const auto &[id, a] : b.assets)
    if (a.mime.starts_with("font/"))
      used.insert(id);
  for (const auto &id : used) {
    const auto &a = b.assets.at(id);
    const auto name = "assets/" +
                      publication::hash(raw_asset(a)).substr(0, 32) + "." +
                      ext(a.mime);
    files[root + name] = a.data;
    urls[id] = name;
    if (a.mime.starts_with("font/")) {
      fontcss += "@font-face{font-family:F_" + id + ";src:url('" + name +
                 "');font-display:swap;}";
      fonts.emplace_back(id, a.role);
    }
  }
  const std::string system =
      "'Noto Sans','Noto Sans CJK JP','Noto Sans CJK SC','Noto Sans CJK "
      "TC','Noto Sans CJK KR','Noto Naskh Arabic',sans-serif";
  std::string all;
  for (const auto &[id, role] : fonts)
    all += "F_" + id + ",";
  fontcss += ":root{--font-fallback:" + all + system + ";--fb-general:" + all +
             system + ";}";
  for (const auto &role :
       {"ja", "zh-Hans", "zh-Hant", "ko", "ar", "he", "hi", "th"}) {
    std::string preferred;
    for (const auto &[id, r] : fonts)
      if (r == role)
        preferred += "F_" + id + ",";
    fontcss += std::string(":lang(") + role + "){--font-fallback:" + preferred +
               all + system + ";}body{--fb-" + role + ":" + preferred + all +
               system + ";}";
  }
  std::string panels, transcript;
  int n = 0;
  for (const auto &p : b.panels) {
    const auto heading = std::to_string(++n) + " · " + escape(p.title);
    transcript += "<article><h2>" + heading +
                  "</h2><p class=\"art-description\">" + escape(p.alt) + "</p>";
    panels +=
        "<section class=\"comic-panel\"><h2 class=\"panel-heading\">" +
        heading + "</h2><div class=\"scene\" dir=\"ltr\" style=\"--ratio:" +
        number(p.ratio) + ";background:" + p.background + "\">" +
        (p.image.empty()
             ? "<div class=\"empty-art\" aria-hidden=\"true\">◇</div>"
             : "<img class=\"art\" src=\"" + urls.at(p.image) + "\" alt=\"" +
                   escape(p.alt) + "\" style=\"object-fit:cover\">");
    for (const auto &e : p.elements) {
      const auto content_html = content(e, b), a = attrs(e, b);
      panels += "<div class=\"balloon " + e.kind + " shape-" + e.shape +
                " valign-" + e.valign + " anim-" + e.animation + "\"" + a +
                " style=\"--ex:" + number(e.x) + "%;--ey:" + number(e.y) +
                "%;--ew:" + number(e.w) + "%;--eh:" + number(e.h) +
                "%;left:" + number(e.x) + "%;top:" + number(e.y) +
                "%;width:" + number(e.w) + "%;height:" + number(e.h) +
                "%;--font-px:" + number(e.size) +
                "px;--font-fluid:" + number(e.size / 9) +
                "cqw;--bubble-bg:" + e.background + ";--bubble-ink:" + e.ink +
                ";--bubble-border:" + e.ink + ";text-align:" + e.align +
                ";font-family:" +
                (e.font_asset.empty() ? "" : "F_" + e.font_asset + ",") +
                "var(--font-fallback)," + e.font +
                "\" role=\"group\" aria-label=\"" +
                escape(e.speaker.empty() ? "Texto" : e.speaker) +
                "\"><div class=\"bubble-scroll\" tabindex=\"0\"><div "
                "class=\"bubble-text\"" +
                a + ">" + content_html +
                "</div></div><span class=\"overflow-hint\" translate=\"yes\" "
                "lang=\"pt-BR\" dir=\"ltr\" hidden>Role para ler</span></div>";
      transcript +=
          "<div class=\"transcript-line\"><strong><bdi dir=\"auto\">" +
          escape(e.speaker) + "</bdi></strong><div class=\"transcript-text\"" +
          a + ">" + content_html + "</div></div>";
    }
    panels += "</div></section>";
    transcript += "</article>";
  }
  const auto toolbar = std::string(
      "<nav class=\"reader-toolbar\"><label>Leitura <select "
      "id=\"reading-mode\"><option value=\"auto\">Automática</option><option "
      "value=\"page\">Página</option><option "
      "value=\"strip\">Vertical</option><option "
      "value=\"mobile\">Celular</option><option value=\"focus\">Quadro a "
      "quadro</option></select></label><button id=\"motion\" "
      "aria-pressed=\"false\">Pausar animações</button><button "
      "id=\"transcript-toggle\" "
      "aria-expanded=\"false\">Transcrição</button><label>Fonte <select "
      "id=\"font-profile\"><option value=\"author\">Da obra</option><option "
      "value=\"general\">Ampla cobertura</option><option "
      "value=\"ja\">Japonês</option><option value=\"zh-Hans\">Chinês "
      "simplificado</option><option value=\"zh-Hant\">Chinês "
      "tradicional</option><option value=\"ko\">Coreano</option><option "
      "value=\"ar\">Árabe</option></select></label><button id=\"gloss-toggle\" "
      "aria-pressed=\"true\">Legendas dos sons</button><a "
      "href=\"texto.html\">Texto contínuo</a></nav>");
  const auto body =
      toolbar +
      "<main id=\"comic\" dir=\"ltr\" class=\"comic\" data-layout=\"" +
      (b.layout == "strip" ? "strip" : "mosaic") + "\">" + panels +
      "</main><nav id=\"focus-nav\" class=\"focus-nav\" hidden><button "
      "id=\"previous\">← Anterior</button><span id=\"counter\"></span><button "
      "id=\"next\">Próximo →</button></nav><section id=\"transcript\" "
      "class=\"transcript\" hidden>" +
      transcript + "</section>";
  const auto css = read(resources / "comic/reader.css") +
                   read(resources / "comic/international.css") +
                   read(resources / "comic/native.css");
  files[root + "reader.css"] = b64(css);
  files[root + "reader.js"] = b64(read(resources / "comic/reader.js"));
  files[root + "index.html"] = b64(html(b, site, body, false, fontcss));
  files[root + "texto.html"] =
      b64(html(b, site,
               "<main>" + transcript +
                   "</main><a href=\"index.html\">Voltar à comic</a>",
               true, fontcss));
  const auto home =
      "<!doctype html><html lang=\"" + escape(b.lang) +
      "\" translate=\"yes\"><meta charset=\"utf-8\"><meta name=\"viewport\" "
      "content=\"width=device-width,initial-scale=1\"><title>" +
      escape(str(site, "name")) +
      "</title><link rel=\"stylesheet\" href=\"../" + root +
      "reader.css\"><body class=\"plain-reader\"><h1>" +
      escape(str(site, "name")) + "</h1><p>" +
      escape(str(site, "description")) + "</p><p>" +
      escape(str(site, "author")) + "</p><a href=\"../" + root + "\">Ler " +
      escape(b.title) +
      "</a><p>Webcomic · edição experimental</p></body></html>";
  files["home/index.html"] = b64(home);
  files["index.html"] =
      b64("<!doctype html><html lang=\"pt-BR\"><meta charset=\"utf-8\"><meta "
          "http-equiv=\"refresh\" "
          "content=\"0;url=home/\"><title>Biblioteca</title><a "
          "href=\"home/\">Abrir biblioteca</a></html>");
  files[".nojekyll"] = "";
  // Media-specific contract is explicit; old novel consumers must not ignore
  // it.
  return O{{"protocol", "inde.web.comic"},
           {"version", 1},
           {"media", "comic"},
           {"site", site},
           {"comic_id", b.id},
           {"work_id", b.work_id},
           {"revision", b.revision},
           {"files", files}};
}
} // namespace inde::comic

#include "production_support.hpp"
#include <algorithm>
#include <array>
#include <glib.h>
#include <set>
namespace inde::application::production_detail {
std::string xml(const std::string &s) {
  std::string out;
  for (char c : s)
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
    case '\r':
      out += "&#13;";
      break;
    default:
      out += c;
    }
  return out;
}
std::string sha(const std::string &s) {
  auto *h = g_compute_checksum_for_data(
      G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(s.data()), s.size());
  std::string out = h;
  g_free(h);
  return out;
}
std::vector<Paragraph> paragraphs(const project::Document &d,
                                  std::stop_token stop) {
  std::map<std::size_t, std::array<int, 4>> events;
  std::array<int, 4> counts{};
  for (const auto &s : d.formatting) {
    int kind = -1;
    switch (s.style) {
    case project::DocumentTextStyle::Bold:
      kind = 0;
      break;
    case project::DocumentTextStyle::Italic:
      kind = 1;
      break;
    case project::DocumentTextStyle::Underline:
      kind = 2;
      break;
    case project::DocumentTextStyle::Strikethrough:
      kind = 3;
      break;
    default:
      break;
    }
    if (kind >= 0) {
      ++events[s.start_offset][kind];
      --events[s.end_offset][kind];
    }
  }
  auto event = events.begin();
  std::vector<Paragraph> out(1);
  std::size_t offset{};
  for (const char *p = d.content.data();
       p < d.content.data() + d.content.size();
       p = g_utf8_next_char(p), ++offset) {
    if (offset % 4096 == 0)
      cancelled(stop);
    if (event != events.end() && event->first == offset) {
      for (int i = 0; i < 4; ++i)
        counts[i] += event->second[i];
      ++event;
    }
    const auto c = g_utf8_get_char(p);
    if (c == '\r' && p + 1 < d.content.data() + d.content.size() &&
        p[1] == '\n')
      continue;
    if (c == '\n' || c == '\r') {
      out.emplace_back();
      continue;
    }
    Run r{std::string(p, static_cast<std::size_t>(g_utf8_next_char(p) - p)),
          counts[0] > 0, counts[1] > 0, counts[2] > 0, counts[3] > 0};
    auto &runs = out.back().runs;
    if (!runs.empty() && runs.back().bold == r.bold &&
        runs.back().italic == r.italic &&
        runs.back().underline == r.underline && runs.back().strike == r.strike)
      runs.back().text += r.text;
    else
      runs.push_back(std::move(r));
  }
  return out;
}
namespace {
void le(std::string &o, std::uint32_t n, int bytes) {
  for (int i = 0; i < bytes; ++i)
    o += static_cast<char>((n >> (i * 8)) & 255);
}
std::uint32_t crc(const std::string &s) {
  std::uint32_t c = 0xffffffff;
  for (unsigned char b : s) {
    c ^= b;
    for (int i = 0; i < 8; ++i)
      c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
  }
  return ~c;
}
const std::string declaration = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
std::string title(const ProductionUnit &u) {
  return u.node.designator + (u.node.designator.empty() ? "" : " ") +
         u.node.title;
}
std::string word_text(const std::string &s) {
  std::string o;
  std::size_t start{};
  for (std::size_t i = 0; i <= s.size(); ++i)
    if (i == s.size() || s[i] == '\t') {
      o += "<w:t xml:space=\"preserve\">" + xml(s.substr(start, i - start)) +
           "</w:t>";
      if (i < s.size())
        o += "<w:tab/>";
      start = i + 1;
    }
  return o;
}
std::string word_p(const std::string &s, const std::string &style) {
  return "<w:p><w:pPr><w:pStyle w:val=\"" + style + "\"/></w:pPr><w:r>" +
         word_text(s) + "</w:r></w:p>";
}
} // namespace
std::string zip(const std::vector<std::pair<std::string, std::string>> &files) {
  if (files.size() > 60000)
    throw std::runtime_error("Pacote excede o limite ZIP32");
  std::string out, central;
  std::set<std::string> names;
  for (const auto &[name, data] : files) {
    if (name.empty() || name.size() > 60000 || name.front() == '/' ||
        name.find("..") != std::string::npos || !names.insert(name).second ||
        out.size() + data.size() > 128 * 1024 * 1024)
      throw std::runtime_error(
          "Entrada de pacote inválida ou acima de 128 MiB");
    const auto pos = static_cast<std::uint32_t>(out.size()), sum = crc(data),
               size = static_cast<std::uint32_t>(data.size());
    le(out, 0x04034b50, 4);
    le(out, 20, 2);
    le(out, 0x800, 2);
    le(out, 0, 2);
    le(out, 0, 2);
    le(out, 33, 2);
    le(out, sum, 4);
    le(out, size, 4);
    le(out, size, 4);
    le(out, name.size(), 2);
    le(out, 0, 2);
    out += name;
    out += data;
    le(central, 0x02014b50, 4);
    le(central, 20, 2);
    le(central, 20, 2);
    le(central, 0x800, 2);
    le(central, 0, 2);
    le(central, 0, 2);
    le(central, 33, 2);
    le(central, sum, 4);
    le(central, size, 4);
    le(central, size, 4);
    le(central, name.size(), 2);
    le(central, 0, 2);
    le(central, 0, 2);
    le(central, 0, 2);
    le(central, 0, 2);
    le(central, 0, 4);
    le(central, pos, 4);
    central += name;
  }
  const auto pos = out.size();
  out += central;
  le(out, 0x06054b50, 4);
  le(out, 0, 2);
  le(out, 0, 2);
  le(out, files.size(), 2);
  le(out, files.size(), 2);
  le(out, central.size(), 4);
  le(out, pos, 4);
  le(out, 0, 2);
  return out;
}
std::string make_docx(const WorkProduction &w, std::stop_token stop) {
  std::string doc = declaration +
                    "<w:document "
                    "xmlns:w=\"http://schemas.openxmlformats.org/"
                    "wordprocessingml/2006/main\" "
                    "xmlns:r=\"http://schemas.openxmlformats.org/"
                    "officeDocument/2006/relationships\"><w:body>";
  doc += word_p(w.work.title, "Title");
  if (!w.work.subtitle.empty())
    doc += word_p(w.work.subtitle, "Subtitle");
  if (!w.options.author.empty())
    doc += word_p(w.options.author, "Subtitle");
  doc += "<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>";
  for (const auto &u : w.units) {
    cancelled(stop);
    if (u.heading)
      doc += word_p(title(u), "Heading" + std::to_string(std::min<std::size_t>(
                                              6, u.depth + 1)));
    for (const auto &d : u.documents)
      for (const auto &p : paragraphs(d, stop)) {
        cancelled(stop);
        doc += "<w:p>";
        for (const auto &r : p.runs)
          doc += "<w:r><w:rPr>" + std::string(r.bold ? "<w:b/>" : "") +
                 (r.italic ? "<w:i/>" : "") +
                 (r.underline ? "<w:u w:val=\"single\"/>" : "") +
                 (r.strike ? "<w:strike/>" : "") + "</w:rPr>" +
                 word_text(r.text) + "</w:r>";
        doc += "</w:p>";
      }
  }
  doc += "<w:sectPr><w:footerReference w:type=\"default\" "
         "r:id=\"footer\"/><w:pgSz w:w=\"12240\" w:h=\"15840\"/><w:pgMar "
         "w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\" "
         "w:header=\"720\" w:footer=\"720\"/></w:sectPr></w:body></w:document>";
  std::string styles =
      declaration +
      "<w:styles "
      "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/"
      "main\"><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts "
      "w:ascii=\"Liberation Serif\" w:hAnsi=\"Liberation Serif\"/><w:sz "
      "w:val=\"24\"/><w:color w:val=\"000000\"/><w:lang w:val=\"" +
      xml(w.work.language) +
      "\"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing "
      "w:after=\"120\" w:line=\"" +
      (w.options.profile == ProductionProfile::Manuscript ? "480" : "360") +
      "\" "
      "w:lineRule=\"auto\"/><w:widowControl/></w:pPr></w:pPrDefault></"
      "w:docDefaults><w:style w:type=\"paragraph\" w:default=\"1\" "
      "w:styleId=\"Normal\"><w:name w:val=\"Normal\"/></w:style>";
  for (const auto &name : std::vector<std::string>{
           "Title", "Subtitle", "Heading1", "Heading2", "Heading3", "Heading4",
           "Heading5", "Heading6"}) {
    const bool heading = name.starts_with("Heading");
    styles += "<w:style w:type=\"paragraph\" w:styleId=\"" + name +
              "\"><w:name w:val=\"" + name +
              "\"/><w:basedOn w:val=\"Normal\"/><w:next "
              "w:val=\"Normal\"/><w:pPr><w:keepNext/><w:keepLines/><w:spacing "
              "w:before=\"240\" w:after=\"160\" w:line=\"240\"/>" +
              (heading ? "<w:outlineLvl w:val=\"" +
                             std::to_string(name.back() - '1') + "\"/>"
                       : "") +
              "</w:pPr><w:rPr><w:color w:val=\"000000\"/><w:sz w:val=\"" +
              (name == "Title"      ? "48"
               : name == "Subtitle" ? "24"
                                    : "28") +
              "\"/>" + (heading ? "<w:b/>" : "") + "</w:rPr></w:style>";
  }
  styles += "</w:styles>";
  const auto rels =
      declaration +
      "<Relationships "
      "xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
  return zip(
      {{"[Content_Types].xml",
        declaration +
            "<Types "
            "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
            "content-types\"><Default Extension=\"rels\" "
            "ContentType=\"application/"
            "vnd.openxmlformats-package.relationships+xml\"/><Default "
            "Extension=\"xml\" ContentType=\"application/xml\"/><Override "
            "PartName=\"/word/document.xml\" "
            "ContentType=\"application/"
            "vnd.openxmlformats-officedocument.wordprocessingml.document.main+"
            "xml\"/><Override PartName=\"/word/styles.xml\" "
            "ContentType=\"application/"
            "vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/"
            "><Override PartName=\"/word/footer1.xml\" "
            "ContentType=\"application/"
            "vnd.openxmlformats-officedocument.wordprocessingml.footer+xml\"/"
            "></Types>"},
       {"_rels/.rels",
        rels + "<Relationship Id=\"root\" "
               "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
               "relationships/officeDocument\" "
               "Target=\"word/document.xml\"/></Relationships>"},
       {"word/_rels/document.xml.rels",
        rels +
            "<Relationship Id=\"styles\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
            "relationships/styles\" Target=\"styles.xml\"/><Relationship "
            "Id=\"footer\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
            "relationships/footer\" Target=\"footer1.xml\"/></Relationships>"},
       {"word/document.xml", doc},
       {"word/styles.xml", styles},
       {"word/footer1.xml",
        declaration +
            "<w:ftr "
            "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/"
            "main\"><w:p><w:pPr><w:jc "
            "w:val=\"right\"/></w:pPr><w:r><w:rPr><w:color "
            "w:val=\"000000\"/><w:sz w:val=\"20\"/></w:rPr><w:fldChar "
            "w:fldCharType=\"begin\"/></w:r><w:r><w:instrText> PAGE "
            "</w:instrText></w:r><w:r><w:fldChar "
            "w:fldCharType=\"end\"/></w:r></w:p></w:ftr>"}});
}
std::string make_epub(const WorkProduction &w, std::stop_token stop) {
  const std::string head =
      declaration +
      "<!DOCTYPE html><html xmlns=\"http://www.w3.org/1999/xhtml\" "
      "xmlns:epub=\"http://www.idpf.org/2007/ops\" xml:lang=\"" +
      xml(w.work.language) + "\" lang=\"" + xml(w.work.language) +
      "\"><head><title>" + xml(w.work.title) +
      "</title><meta charset=\"utf-8\"/><link rel=\"stylesheet\" "
      "href=\"style.css\"/></head><body>";
  std::vector<std::pair<std::string, std::string>> files{
      {"mimetype", "application/epub+zip"},
      {"META-INF/container.xml",
       declaration +
           "<container version=\"1.0\" "
           "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><"
           "rootfiles><rootfile full-path=\"EPUB/package.opf\" "
           "media-type=\"application/oebps-package+xml\"/></rootfiles></"
           "container>"},
      {"EPUB/style.css",
       "body{font-family:serif;color:#000;background:#fff;line-height:" +
           std::string(w.options.profile == ProductionProfile::Manuscript
                           ? "2"
                           : "1.5") +
           "}p{"
           "white-space:pre-wrap;overflow-wrap:anywhere}h1,h2,h3,h4,h5,h6{"
           "color:#"
           "000;break-after:avoid} .title{margin:2em 0}"},
      {"EPUB/title.xhtml",
       head + "<section epub:type=\"titlepage\"><h1>" + xml(w.work.title) +
           "</h1><p>" + xml(w.work.subtitle) + "</p><p>" +
           xml(w.options.author) + "</p></section></body></html>"}};
  std::string manifest =
                  "<item id=\"title\" href=\"title.xhtml\" "
                  "media-type=\"application/xhtml+xml\"/><item id=\"nav\" "
                  "href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" "
                  "properties=\"nav\"/><item id=\"css\" href=\"style.css\" "
                  "media-type=\"text/css\"/>",
              spine = "<itemref idref=\"title\"/>", nav = "";
  std::size_t i{};
  for (const auto &u : w.units) {
    cancelled(stop);
    const auto id = "unit" + std::to_string(++i), name = id + ".xhtml",
               level = std::to_string(std::min<std::size_t>(6, u.depth + 1));
    std::string body = head + "<section>";
    if (u.heading)
      body += "<h" + level + ">" + xml(title(u)) + "</h" + level + ">";
    for (const auto &d : u.documents)
      for (const auto &p : paragraphs(d, stop)) {
        cancelled(stop);
        body += "<p>";
        for (const auto &r : p.runs)
          body += (r.bold ? "<b>" : "") + std::string(r.italic ? "<i>" : "") +
                  (r.underline ? "<u>" : "") + (r.strike ? "<s>" : "") +
                  xml(r.text) + (r.strike ? "</s>" : "") +
                  (r.underline ? "</u>" : "") + (r.italic ? "</i>" : "") +
                  (r.bold ? "</b>" : "");
        body += "</p>";
      }
    files.emplace_back("EPUB/" + name, body + "</section></body></html>");
    manifest += "<item id=\"" + id + "\" href=\"" + name +
                "\" media-type=\"application/xhtml+xml\"/>";
    spine += "<itemref idref=\"" + id + "\"/>";
    nav += "<li><a href=\"" + name + "\">" + xml(title(u)) + "</a></li>";
  }
  files.emplace_back(
      "EPUB/nav.xhtml",
      head +
          "<nav epub:type=\"toc\" id=\"toc\"><h1>Sumário</h1><ol><li><a "
          "href=\"title.xhtml\">" +
          xml(w.work.title) + "</a></li>" + nav + "</ol></nav></body></html>");
  files.emplace_back(
      "EPUB/package.opf",
      declaration +
          "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
          "unique-identifier=\"book\"><metadata "
          "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:identifier "
          "id=\"book\">urn:inde:" +
          xml(w.project_id) + ":" + xml(w.work.id) + ":" + sha(w.preview) +
          "</dc:identifier><dc:title>" + xml(w.work.title) +
          "</dc:title><dc:language>" + xml(w.work.language) + "</dc:language>" +
          (w.options.author.empty()
               ? ""
               : "<dc:creator>" + xml(w.options.author) + "</dc:creator>") +
          "<meta property=\"dcterms:modified\">" + xml(w.prepared_at) +
          "</meta></metadata><manifest>" + manifest + "</manifest><spine>" +
          spine + "</spine></package>");
  return zip(files);
}
} // namespace inde::application::production_detail

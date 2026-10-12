#include "inde/application/text_clipboard.hpp"
#include "inde/persistence/json.hpp"
#include <algorithm>
#include <glib.h>
#include <stdexcept>
namespace inde::application {
namespace {
void validate(const TextFragment &fragment) {
  if (fragment.text.size() > clipboard_limit || fragment.formatting.size() > 10000 ||
      !g_utf8_validate(fragment.text.data(), static_cast<gssize>(fragment.text.size()), nullptr))
    throw std::runtime_error("Clipboard excede o limite ou contém texto UTF-8 inválido");
  const auto size = project::utf8_character_count(fragment.text);
  for (const auto &span : fragment.formatting) {
    if (span.start_offset >= span.end_offset || span.end_offset > size)
      throw std::runtime_error("Formato do clipboard está fora do texto");
    project::validate_format_span(span);
  }
}
void bounded_json(const std::string &text) {
  if (text.size() > clipboard_limit) throw std::runtime_error("Clipboard formatado excede 16 MiB");
  int depth = 0; bool string = false, escaped = false;
  for (char ch : text) {
    if (string) {
      if (escaped) escaped = false;
      else if (ch == '\\') escaped = true;
      else if (ch == '"') string = false;
    } else if (ch == '"') string = true;
    else if (ch == '{' || ch == '[') {
      if (++depth > 4) throw std::runtime_error("Clipboard possui estrutura JSON profunda demais");
    } else if (ch == '}' || ch == ']') --depth;
  }
}
}
TextFragment copy_fragment(const project::Document &doc, std::size_t start, std::size_t end, bool styles) {
  if (!g_utf8_validate(doc.content.data(), static_cast<gssize>(doc.content.size()), nullptr) ||
      start > end || end > project::utf8_character_count(doc.content))
    throw std::runtime_error("Seleção de cópia inválida");
  const char *a = g_utf8_offset_to_pointer(doc.content.data(), static_cast<glong>(start));
  const char *b = g_utf8_offset_to_pointer(a, static_cast<glong>(end-start));
  TextFragment result{std::string(a, b), {}};
  if (styles) for (auto span : doc.formatting) {
    const auto left = std::max(start, span.start_offset), right = std::min(end, span.end_offset);
    if (left < right) result.formatting.push_back({span.style, left-start, right-start, span.value});
  }
  validate(result); return result;
}
std::string encode_fragment(const TextFragment &fragment) {
  validate(fragment);
  using namespace persistence::json;
  Value::Array spans;
  for (const auto &s : fragment.formatting) spans.push_back(Value::Object{
      {"style", project::to_string(s.style)}, {"start", static_cast<std::int64_t>(s.start_offset)},
      {"end", static_cast<std::int64_t>(s.end_offset)}, {"value",s.value}});
  auto result = serialize(Value::Object{{"version", 2}, {"text", fragment.text}, {"formatting", std::move(spans)}}, false);
  bounded_json(result); return result;
}
TextFragment decode_fragment(const std::string &text) {
  bounded_json(text);
  auto root = persistence::json::parse(text);
  const auto version=root.at("version").as_integer();
  if (root.as_object().size() != 3 || (version!=1&&version!=2))
    throw std::runtime_error("Versão/campos do clipboard formatado incompatíveis");
  TextFragment result{root.at("text").as_string(), {}};
  const auto &spans = root.at("formatting").as_array();
  if (spans.size() > 10000) throw std::runtime_error("Clipboard possui estilos demais");
  for (const auto &s : spans) {
    const auto a = s.at("start").as_integer(), b = s.at("end").as_integer();
    if (s.as_object().size() != (version==1?3:4) || a < 0 || b < 0) throw std::runtime_error("Estilo de clipboard inválido");
    result.formatting.push_back({project::document_text_style_from_string(s.at("style").as_string()),
                                static_cast<std::size_t>(a), static_cast<std::size_t>(b),version==1?"":s.at("value").as_string()});
  }
  validate(result); return result;
}
DocumentEditState paste_fragment(const DocumentEditState &before, std::size_t start,
    std::size_t end, const TextFragment &fragment, bool styles) {
  validate(fragment);
  auto after = replace_selection(before, start, end, fragment.text);
  const auto stop = start + project::utf8_character_count(fragment.text);
  if (styles && start < stop) {
    std::vector<project::DocumentFormatSpan> retained;
    for (const auto &span : after.document.formatting) {
      if (span.end_offset <= start || span.start_offset >= stop) retained.push_back(span);
      else {
        if (span.start_offset < start) retained.push_back({span.style, span.start_offset, start,span.value});
        if (span.end_offset > stop) retained.push_back({span.style, stop, span.end_offset,span.value});
      }
    }
    for (const auto &span : fragment.formatting) retained.push_back({span.style, start+span.start_offset, start+span.end_offset,span.value});
    after.document.formatting = std::move(retained);
  }
  after.selection = {stop, stop};
  project::validate(after.document);
  return after;
}
}

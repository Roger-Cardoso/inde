#include "inde/application/document_edit_session.hpp"
#include "inde/application/semantic_anchor_engine.hpp"

#include <algorithm>
#include <glib.h>
#include <stdexcept>

namespace inde::application {
namespace {
struct Change { std::size_t start, end, new_start, new_end; };
std::size_t format_offset(std::size_t point, const std::vector<Change> &changes) {
  auto it = std::upper_bound(changes.begin(), changes.end(), point,
      [](auto p, const Change &c) { return p < c.start; });
  if (it == changes.begin()) return point;
  --it;
  if (point == it->start) return it->new_start;
  if (point <= it->end) return it->new_end;
  return point - it->end + it->new_end;
}
std::size_t anchor_offset(std::size_t point, const std::vector<Change> &changes, bool right) {
  if (right) {
    auto it = std::upper_bound(changes.begin(), changes.end(), point,
        [](auto p, const Change &c) { return p < c.start; });
    if (it == changes.begin()) return point;
    --it;
    if (point <= it->end) return it->new_end;
    return point - it->end + it->new_end;
  }
  auto it = std::lower_bound(changes.begin(), changes.end(), point,
      [](const Change &c, auto p) { return c.end < p; });
  if (it != changes.end() && point >= it->start) return it->new_start;
  if (it == changes.begin()) return point;
  --it;
  return point - it->end + it->new_end;
}
}
static DocumentEditState replace_impl(const DocumentEditState &before,
    const TextSearchResult &plan, const std::string &replacement, bool insertion) {
  project::validate(before.document);
  if (plan.truncated) throw std::runtime_error("Busca incompleta; refine antes de substituir tudo");
  if (!g_utf8_validate(replacement.data(), static_cast<gssize>(replacement.size()), nullptr) ||
      !g_utf8_validate(before.document.content.data(), static_cast<gssize>(before.document.content.size()), nullptr))
    throw std::runtime_error("Substituição exige texto UTF-8 válido");
  if (plan.matches.empty()) return before;
  std::vector<std::size_t> bytes;
  const auto &text = before.document.content;
  for (const char *cursor = text.data(), *end = cursor + text.size(); cursor < end; cursor = g_utf8_next_char(cursor))
    bytes.push_back(static_cast<std::size_t>(cursor - text.data()));
  bytes.push_back(text.size());
  if (before.selection.cursor >= bytes.size() || before.selection.bound >= bytes.size())
    throw std::runtime_error("Seleção fora do Documento");
  const auto replacement_size = project::utf8_character_count(replacement);
  std::vector<Change> changes;
  std::string content;
  std::size_t previous = 0, output_offset = 0, validated_end = 0;
  for (const auto &match : plan.matches) {
    if (match.start < validated_end || match.start > match.end ||
        (!insertion && match.start == match.end) || match.end >= bytes.size())
      throw std::runtime_error("Plano de substituição sobreposto ou fora do Documento");
    validated_end = match.end;
    if (text.compare(bytes[match.start], bytes[match.end] - bytes[match.start], replacement) == 0)
      continue;
    content.append(text, bytes[previous], bytes[match.start] - bytes[previous]);
    output_offset += match.start - previous;
    changes.push_back({match.start, match.end, output_offset, output_offset + replacement_size});
    content += replacement; output_offset += replacement_size; previous = match.end;
  }
  if (changes.empty()) return before;
  content.append(text, bytes[previous], text.size() - bytes[previous]);
  auto after = before;
  after.document.content = std::move(content);
  for (auto &span : after.document.formatting) {
    span.start_offset = format_offset(span.start_offset, changes);
    span.end_offset = format_offset(span.end_offset, changes);
  }
  std::erase_if(after.document.formatting, [](const auto &span) { return span.start_offset == span.end_offset; });
  for (auto &anchor : after.document.anchors) {
    anchor.start_offset = anchor_offset(anchor.start_offset, changes, false);
    anchor.end_offset = anchor_offset(anchor.end_offset, changes, true);
  }
  if(!before.document.semantics.anchors.empty()){
    auto transformed=before.document;
    for(auto it=changes.rbegin();it!=changes.rend();++it){
      SemanticAnchorEngine::edit(transformed,it->start,it->end,replacement);
      auto*p=g_utf8_offset_to_pointer(transformed.content.c_str(),it->start);
      auto*q=g_utf8_offset_to_pointer(transformed.content.c_str(),it->end);
      transformed.content.replace(p-transformed.content.c_str(),q-p,replacement);
    }
    after.document.semantics=std::move(transformed.semantics);
    for(auto&e:after.document.semantics.anchors){auto a=std::find_if(after.document.anchors.begin(),after.document.anchors.end(),[&](auto&x){return x.id==e.anchor_id;});auto b=std::find_if(transformed.anchors.begin(),transformed.anchors.end(),[&](auto&x){return x.id==e.anchor_id;});*a=*b;}
  }
  const bool caret = before.selection.cursor == before.selection.bound;
  const bool cursor_right = caret || before.selection.cursor > before.selection.bound;
  after.selection.cursor = anchor_offset(before.selection.cursor, changes, cursor_right);
  after.selection.bound = anchor_offset(before.selection.bound, changes, caret || !cursor_right);
  project::validate(after.document);
  return after;
}
DocumentEditState replace_text(const DocumentEditState &before,
    const TextSearchResult &plan, const std::string &replacement) {
  return replace_impl(before, plan, replacement, false);
}
DocumentEditState replace_selection(const DocumentEditState &before,
    std::size_t start, std::size_t end, const std::string &replacement) {
  return replace_impl(before, {{{start, end}}, false}, replacement, true);
}
} // namespace inde::application

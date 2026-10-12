#include "inde/application/editorial_review.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <charconv>
#include <glib.h>
#include <set>
#include <stdexcept>
#include <tuple>
namespace inde::application {
namespace {
constexpr auto state_type = "inde.editor.review-state";
constexpr auto suggestion_type = "inde.editor.suggestion";
using J = persistence::json::Value;
void text(const std::string &v, std::size_t limit = 4096) {
  if (v.size() > limit || v.find('\0') != std::string::npos ||
      !g_utf8_validate(v.data(), v.size(), nullptr))
    throw std::runtime_error("Texto editorial inválido ou acima do limite");
}
project::TextAnnotation &annotation(project::Document &d, const std::string &id,
                                    const char *type) {
  auto it = std::find_if(d.semantics.annotations.begin(),
                         d.semantics.annotations.end(),
                         [&](const auto &a) { return a.id == id; });
  if (it == d.semantics.annotations.end() || it->type_key != type ||
      it->type_version != 1 || it->status == "archived")
    throw std::runtime_error("Item editorial indisponível; nada foi decidido");
  return *it;
}
void field(project::TextAnnotation &a, const std::string &key,
           const std::string &value,
           project::SemanticFieldKind kind = project::SemanticFieldKind::Text) {
  a.fields[key] = {kind, {value}};
}
std::string required_field(const project::TextAnnotation &a,
                           const std::string &key,
                           project::SemanticFieldKind kind) {
  const auto it = a.fields.find(key);
  if (it == a.fields.end() || it->second.kind != kind ||
      it->second.values.size() != 1)
    throw std::runtime_error("Item editorial incompleto/incompatível; nenhuma "
                             "aplicação ou aprovação inferida");
  return it->second.values.front();
}
template <class T> void sort_ids(std::vector<T> &values) {
  std::sort(values.begin(), values.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
}
std::string fingerprint(const project::Document &d) {
  auto sem = d.semantics;
  std::erase_if(sem.annotations,
                [](const auto &a) { return a.type_key == state_type; });
  std::sort(
      sem.anchors.begin(), sem.anchors.end(),
      [](const auto &a, const auto &b) { return a.anchor_id < b.anchor_id; });
  sort_ids(sem.annotations);
  sort_ids(sem.relations);
  for (auto &a : sem.annotations) {
    sort_ids(a.fictional_time);
    sort_ids(a.dates);
  }
  auto formats = d.formatting;
  std::sort(formats.begin(), formats.end(), [](const auto &a, const auto &b) {
    return std::tie(a.style, a.start_offset, a.end_offset, a.value) <
           std::tie(b.style, b.start_offset, b.end_offset, b.value);
  });
  J::Array formatting, anchors, references, commands;
  for (const auto &s : formats)
    formatting.emplace_back(J::Array{
        project::to_string(s.style), static_cast<std::int64_t>(s.start_offset),
        static_cast<std::int64_t>(s.end_offset), s.value});
  auto as = d.anchors;
  sort_ids(as);
  for (const auto &a : as)
    anchors.emplace_back(J::Array{a.id, a.label,
                                  static_cast<std::int64_t>(a.start_offset),
                                  static_cast<std::int64_t>(a.end_offset)});
  auto refs = d.entity_references;
  sort_ids(refs);
  for (const auto &r : refs)
    references.emplace_back(
        J::Array{r.id, r.entity_id, r.anchor_id.value_or(""), r.notes});
  for (const auto &c : d.inline_commands)
    commands.emplace_back(J::Array{c.prefix, c.keyword, c.label, c.content});
  const auto value = persistence::json::serialize(
      J::Object{{"id", d.id},
                {"title", d.title},
                {"body", d.content},
                {"format", std::move(formatting)},
                {"anchors", std::move(anchors)},
                {"entities", std::move(references)},
                {"semantics", persistence::encode_semantics(sem)},
                {"commands", std::move(commands)},
                {"placement", d.editorial_node_id.value_or("")},
                {"group", d.group_id.value_or("")},
                {"purpose", project::to_string(d.purpose)},
                {"revision", d.revision_of_id.value_or("")},
                {"label", d.revision_label},
                {"perspective", d.perspective},
                {"goal", d.word_goal
                             ? J(static_cast<std::int64_t>(*d.word_goal))
                             : J()}},
      false);
  auto *raw = g_compute_checksum_for_data(
      G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(value.data()),
      value.size());
  std::string result = raw;
  g_free(raw);
  return result;
}
std::int64_t sequence(const project::TextAnnotation &a) {
  const auto v = review_field(a, "sequence");
  std::int64_t n{};
  const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
  if (ec != std::errc{} || p != v.data() + v.size() || n < 1 || n > 1000000)
    throw std::runtime_error(
        "Histórico editorial inválido; decisões preservadas");
  return n;
}
const project::TextAnnotation *latest(const project::Document &d) {
  const project::TextAnnotation *result{};
  std::int64_t last{};
  std::set<std::int64_t> sequences;
  for (const auto &a : d.semantics.annotations)
    if (a.type_key == state_type) {
      if (a.status == "archived")
        throw std::runtime_error(
            "Decisão editorial arquivada; nenhuma aprovação inferida");
      if (a.type_version != 1 || a.scope != project::AnnotationScope::Document)
        throw std::runtime_error("Versão/escopo editorial incompatível");
      const auto s =
          required_field(a, "state", project::SemanticFieldKind::Enum);
      if (s != "draft" && s != "in_review" && s != "changes_requested" &&
          s != "approved")
        throw std::runtime_error("Estado editorial desconhecido");
      (void)required_field(a, "sequence", project::SemanticFieldKind::Integer);
      const auto reviewer =
          required_field(a, "reviewer", project::SemanticFieldKind::Text);
      const auto hash =
          required_field(a, "fingerprint", project::SemanticFieldKind::Text);
      if (reviewer.find_first_not_of(" \t\r\n") == std::string::npos ||
          hash.size() != 64 ||
          hash.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("Responsável/fonte editorial inválidos; "
                                 "nenhuma aprovação inferida");
      const auto n = sequence(a);
      if (!sequences.insert(n).second)
        throw std::runtime_error(
            "Decisões editoriais concorrentes; nenhuma aprovação inferida");
      if (n > last) {
        result = &a;
        last = n;
      }
    }
  return result;
}
std::vector<std::string> lines(const std::string &body) {
  text(body, 32 * 1024 * 1024);
  std::vector<std::string> out;
  for (std::size_t a = 0; a < body.size();) {
    const auto p = body.find('\n', a);
    const auto b = p == std::string::npos ? body.size() : p + 1;
    out.push_back(body.substr(a, b - a));
    a = b;
  }
  return out;
}
} // namespace
std::string review_field(const project::TextAnnotation &a,
                         const std::string &key, const std::string &fallback) {
  const auto it = a.fields.find(key);
  if (it == a.fields.end() || it->second.values.empty())
    return fallback;
  if (it->second.values.size() != 1)
    throw std::runtime_error("Campo editorial ambíguo");
  return it->second.values.front();
}
ReviewCounts review_counts(const project::Document &d) {
  ReviewCounts out;
  for (const auto &a : d.semantics.annotations)
    if (a.status != "archived") {
      if (a.type_key == "inde.editor.comment") {
        const auto s = review_field(a, "state", "open");
        if (s == "resolved")
          ++out.resolved_comments;
        else
          ++out.open_comments;
      } else if (a.type_key == suggestion_type &&
                 review_field(a, "state", "pending") == "pending")
        ++out.pending_suggestions;
    }
  return out;
}
EditorialState editorial_state(const project::Document &d) {
  const auto *a = latest(d);
  if (!a)
    return {};
  const auto s = review_field(*a, "state");
  if (s != "draft" && s != "in_review" && s != "changes_requested" &&
      s != "approved")
    throw std::runtime_error("Estado editorial desconhecido");
  return {s, review_field(*a, "reviewer"), review_field(*a, "note"),
          a->created_at, review_field(*a, "fingerprint") != fingerprint(d)};
}
project::Document set_editorial_state(const project::Document &input,
                                      const std::string &s,
                                      const std::string &reviewer,
                                      const std::string &note) {
  text(reviewer, 256);
  text(note);
  if (reviewer.find_first_not_of(" \t\r\n") == std::string::npos)
    throw std::runtime_error("Identifique quem registra a decisão");
  if (s != "draft" && s != "in_review" && s != "changes_requested" &&
      s != "approved")
    throw std::runtime_error("Escolha um estado editorial válido");
  const auto counts = review_counts(input);
  if (s == "approved" && (counts.open_comments || counts.pending_suggestions))
    throw std::runtime_error(
        "Há comentários ou sugestões pendentes; decida-os antes de aprovar");
  const auto *previous = latest(input);
  const auto n = previous ? sequence(*previous) + 1 : 1;
  if (n > 1000000)
    throw std::runtime_error("Histórico editorial excede o limite");
  auto d = input;
  project::TextAnnotation a;
  a.id = project::new_uuid();
  a.type_key = state_type;
  a.scope = project::AnnotationScope::Document;
  a.created_at = a.updated_at = project::utc_now();
  a.source = "Decisão editorial explícita";
  field(a, "state", s, project::SemanticFieldKind::Enum);
  field(a, "sequence", std::to_string(n), project::SemanticFieldKind::Integer);
  field(a, "reviewer", reviewer);
  field(a, "note", note);
  field(a, "fingerprint", fingerprint(input));
  d.semantics.annotations.push_back(std::move(a));
  project::validate(d);
  return d;
}
project::Document decide_comment(const project::Document &input,
                                 const std::string &id, bool resolved) {
  auto d = input;
  auto &a = annotation(d, id, "inde.editor.comment");
  field(a, "state", resolved ? "resolved" : "open",
        project::SemanticFieldKind::Enum);
  a.updated_at = project::utc_now();
  project::validate(d);
  return d;
}
project::Document propose_change(const project::Document &input,
                                 std::size_t start, std::size_t end,
                                 const std::string &replacement,
                                 const std::string &note) {
  text(replacement);
  text(note);
  project::validate(input);
  if (start >= end || end > project::utf8_character_count(input.content))
    throw std::runtime_error("Selecione um trecho válido para sugerir mudança");
  const auto a = g_utf8_offset_to_pointer(input.content.c_str(), start),
             b = g_utf8_offset_to_pointer(input.content.c_str(), end);
  const std::string expected(a, b);
  text(expected);
  if (expected == replacement)
    throw std::runtime_error("A sugestão não muda o texto");
  const auto *type = project::find_annotation_type(
      project::builtin_annotation_types(), suggestion_type, 1);
  if (!type)
    throw std::runtime_error("Tipo de sugestão indisponível");
  return TextSemanticService::create(
      input, *type, start, end,
      {{"expected", {project::SemanticFieldKind::Text, {expected}}},
       {"replacement", {project::SemanticFieldKind::Text, {replacement}}},
       {"note", {project::SemanticFieldKind::Text, {note}}},
       {"state", {project::SemanticFieldKind::Enum, {"pending"}}}});
}
project::Document decide_suggestion(const project::Document &input,
                                    const std::string &id, bool accept) {
  auto d = input;
  auto &item = annotation(d, id, suggestion_type);
  if (required_field(item, "state", project::SemanticFieldKind::Enum) !=
      "pending")
    throw std::runtime_error("Esta sugestão já foi decidida");
  if (accept) {
    const auto expected =
        required_field(item, "expected", project::SemanticFieldKind::Text);
    const auto replacement =
        required_field(item, "replacement", project::SemanticFieldKind::Text);
    if (expected.empty() || item.scope != project::AnnotationScope::Interval)
      throw std::runtime_error("Trecho observado ausente; nada foi aplicado");
    SemanticAnchorEngine::inspect(d);
    const auto a =
        std::find_if(d.anchors.begin(), d.anchors.end(),
                     [&](const auto &v) { return v.id == item.anchor_id; });
    const auto e = std::find_if(
        d.semantics.anchors.begin(), d.semantics.anchors.end(),
        [&](const auto &v) { return v.anchor_id == item.anchor_id; });
    if (a == d.anchors.end() || e == d.semantics.anchors.end() ||
        e->state != project::AnchorResolution::Resolved)
      throw std::runtime_error(
          "Trecho ambíguo/órfão; reavalie a sugestão sem aplicação automática");
    const auto p = g_utf8_offset_to_pointer(d.content.c_str(), a->start_offset),
               q = g_utf8_offset_to_pointer(d.content.c_str(), a->end_offset);
    if (std::string(p, q) != expected)
      throw std::runtime_error("O trecho mudou; sugestão não aplicada");
    d = replace_selection({d, {a->end_offset, a->start_offset}},
                          a->start_offset, a->end_offset, replacement)
            .document;
  }
  auto &a = annotation(d, id, suggestion_type);
  field(a, "state", accept ? "accepted" : "rejected",
        project::SemanticFieldKind::Enum);
  a.updated_at = project::utc_now();
  project::validate(d);
  return d;
}
DocumentComparison compare_documents(const project::Document &left,
                                     const project::Document &right,
                                     std::stop_token stop) {
  auto a = lines(left.content), b = lines(right.content);
  DocumentComparison result;
  auto l = left, r = right;
  l.id = r.id = "comparison";
  l.title = r.title = "";
  l.content = r.content = "";
  result.layers_differ = !project::same_document_state(l, r, false);
  const auto check = [&] {
    if (stop.stop_requested())
      throw std::runtime_error("Comparação cancelada");
  };
  check();
  const auto add = [&](DifferenceKind kind, const std::string &value,
                       std::size_t i, std::size_t j) {
    result.lines.push_back({kind, value, i, j});
    if (kind == DifferenceKind::Removed)
      ++result.removed;
    if (kind == DifferenceKind::Added)
      ++result.added;
  };
  if (a.size() > 20000 || b.size() > 20000 ||
      (a.size() + 1) * (b.size() + 1) > 2000000) {
    result.coarse = true;
    std::size_t prefix{}, suffix{};
    while (prefix < std::min(a.size(), b.size()) && a[prefix] == b[prefix]) {
      check();
      ++prefix;
    }
    while (suffix < std::min(a.size(), b.size()) - prefix &&
           a[a.size() - 1 - suffix] == b[b.size() - 1 - suffix]) {
      check();
      ++suffix;
    }
    for (std::size_t i = 0; i < prefix; ++i)
      add(DifferenceKind::Equal, a[i], i + 1, i + 1);
    for (std::size_t i = prefix; i < a.size() - suffix; ++i) {
      check();
      add(DifferenceKind::Removed, a[i], i + 1, 0);
    }
    for (std::size_t j = prefix; j < b.size() - suffix; ++j) {
      check();
      add(DifferenceKind::Added, b[j], 0, j + 1);
    }
    for (std::size_t i = 0; i < suffix; ++i)
      add(DifferenceKind::Equal, a[a.size() - suffix + i],
          a.size() - suffix + i + 1, b.size() - suffix + i + 1);
    return result;
  }
  const auto width = b.size() + 1;
  std::vector<std::uint32_t> table((a.size() + 1) * width);
  for (std::size_t i = a.size(); i-- > 0;) {
    check();
    for (std::size_t j = b.size(); j-- > 0;)
      table[i * width + j] = a[i] == b[j] ? 1 + table[(i + 1) * width + j + 1]
                                          : std::max(table[(i + 1) * width + j],
                                                     table[i * width + j + 1]);
  }
  for (std::size_t i = 0, j = 0; i < a.size() || j < b.size();) {
    check();
    if (i < a.size() && j < b.size() && a[i] == b[j]) {
      add(DifferenceKind::Equal, a[i], i + 1, j + 1);
      ++i;
      ++j;
    } else if (i < a.size() &&
               (j == b.size() ||
                table[(i + 1) * width + j] >= table[i * width + j + 1])) {
      add(DifferenceKind::Removed, a[i], i + 1, 0);
      ++i;
    } else {
      add(DifferenceKind::Added, b[j], 0, j + 1);
      ++j;
    }
  }
  return result;
}
} // namespace inde::application

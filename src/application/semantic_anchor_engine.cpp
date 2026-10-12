#include "inde/application/semantic_anchor_engine.hpp"
#include <algorithm>
#include <glib.h>
#include <limits>
#include <stdexcept>
namespace inde::application {
namespace {
std::string slice(const std::string &s, std::size_t a, std::size_t b) {
  auto *p = g_utf8_offset_to_pointer(s.c_str(), a);
  auto *q = g_utf8_offset_to_pointer(s.c_str(), b);
  return {p, q};
}
std::string hash(const std::string &s) {
  auto *h = g_compute_checksum_for_data(
      G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(s.data()), s.size());
  std::string out(h);
  g_free(h);
  return out;
}
void valid_text(const std::string &s) {
  if (!g_utf8_validate(s.data(), s.size(), nullptr))
    throw std::runtime_error("Âncoras exigem UTF-8 válido");
}
void refresh(const project::Document &d, const project::DocumentAnchor &a,
             project::SemanticAnchorEvidence &e) {
  auto n = project::utf8_character_count(d.content);
  e.selected_hash = hash(slice(d.content, a.start_offset, a.end_offset));
  e.before_hash =
      hash(slice(d.content, a.start_offset > 32 ? a.start_offset - 32 : 0,
                 a.start_offset));
  e.after_hash =
      hash(slice(d.content, a.end_offset, std::min(n, a.end_offset + 32)));
}
std::size_t move(std::size_t p, std::size_t a, std::size_t b, std::size_t n,
                 project::AnchorGravity gravity) {
  if (p < a)
    return p;
  if (p > b)
    return p - (b - a) + n;
  return a + (gravity == project::AnchorGravity::Right ? n : 0);
}
} // namespace
project::SemanticAnchorEvidence
SemanticAnchorEngine::evidence(const project::Document &d,
                               const project::DocumentAnchor &a) {
  valid_text(d.content);
  if (a.start_offset >= a.end_offset ||
      a.end_offset > project::utf8_character_count(d.content))
    throw std::runtime_error(
        "Selecione um trecho não vazio para a camada semântica");
  project::SemanticAnchorEvidence e;
  e.anchor_id = a.id;
  refresh(d, a, e);
  return e;
}
void SemanticAnchorEngine::edit(project::Document &d, std::size_t start,
                                std::size_t end,
                                const std::string &replacement) {
  valid_text(d.content);
  valid_text(replacement);
  const auto length = project::utf8_character_count(d.content);
  if (start > end || end > length)
    throw std::runtime_error("Edição fora do Documento");
  // A trusted new keystroke must not silently bless evidence already divergent
  // at load time (external modification/import). Repair stays a human command.
  inspect(d);
  project::Document after;
  after.content = d.content;
  auto *p = g_utf8_offset_to_pointer(d.content.c_str(), start);
  auto *q = g_utf8_offset_to_pointer(d.content.c_str(), end);
  if (std::string(p, q) == replacement)
    return;
  after.content.replace(p - d.content.c_str(), q - p, replacement);
  auto n = project::utf8_character_count(replacement);
  for (auto &e : d.semantics.anchors) {
    auto it = std::find_if(d.anchors.begin(), d.anchors.end(),
                           [&](auto &a) { return a.id == e.anchor_id; });
    if (it == d.anchors.end())
      throw std::runtime_error("Evidência sem âncora");
    auto old = *it;
    if (e.revision == std::numeric_limits<std::uint64_t>::max())
      throw std::runtime_error("Revisão semântica excedida");
    ++e.revision;
    it->start_offset = move(old.start_offset, start, end, n, e.start_gravity);
    it->end_offset = move(old.end_offset, start, end, n, e.end_gravity);
    if (it->start_offset > it->end_offset)
      it->start_offset = it->end_offset;
    if (e.state == project::AnchorResolution::Resolved && start < end &&
        start <= old.start_offset && end >= old.end_offset) {
      e.state = n ? project::AnchorResolution::Ambiguous
                  : project::AnchorResolution::Orphaned;
      e.reason =
          n ? "O trecho inteiro foi substituído; confirme uma nova ancoragem"
            : "O trecho foi removido; a anotação foi preservada";
    }
    if (e.state == project::AnchorResolution::Resolved &&
        it->start_offset == it->end_offset) {
      e.state = project::AnchorResolution::Orphaned;
      e.reason = "O intervalo ficou vazio; reancoragem humana necessária";
    }
    if (e.state == project::AnchorResolution::Resolved) {
      refresh(after, *it, e);
      e.reason.clear();
    }
  }
}
void SemanticAnchorEngine::inspect(project::Document &d) {
  valid_text(d.content);
  for (auto &e : d.semantics.anchors) {
    auto a = std::find_if(d.anchors.begin(), d.anchors.end(),
                          [&](auto &x) { return x.id == e.anchor_id; });
    if (a == d.anchors.end())
      throw std::runtime_error("Evidência sem âncora");
    if (a->start_offset > a->end_offset ||
        a->end_offset > project::utf8_character_count(d.content))
      throw std::runtime_error("Âncora semântica fora do Documento");
    if (e.state != project::AnchorResolution::Resolved)
      continue;
    auto expected = e;
    refresh(d, *a, expected);
    if (expected.selected_hash != e.selected_hash ||
        expected.before_hash != e.before_hash ||
        expected.after_hash != e.after_hash) {
      e.state = project::AnchorResolution::Ambiguous;
      e.reason = "Fingerprint do trecho divergiu; confirmar reancoragem humana";
    }
  }
}
void SemanticAnchorEngine::reanchor(project::Document &d, const std::string &id,
                                    std::size_t start, std::size_t end) {
  auto annotation = std::find_if(d.semantics.annotations.begin(),
                                 d.semantics.annotations.end(),
                                 [&](auto &a) { return a.id == id; });
  if (annotation == d.semantics.annotations.end())
    throw std::runtime_error("Anotação ausente");
  auto a = std::find_if(d.anchors.begin(), d.anchors.end(),
                        [&](auto &x) { return x.id == annotation->anchor_id; });
  if (a == d.anchors.end())
    throw std::runtime_error("Âncora ausente");
  auto candidate = *a;
  candidate.start_offset = start;
  candidate.end_offset = end;
  auto fresh = evidence(d, candidate);
  auto e = std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                        [&](auto &x) { return x.anchor_id == a->id; });
  if (e == d.semantics.anchors.end())
    throw std::runtime_error("Evidência ausente");
  if (e->revision == std::numeric_limits<std::uint64_t>::max())
    throw std::runtime_error("Revisão semântica excedida");
  fresh.revision = e->revision + 1;
  fresh.start_gravity = e->start_gravity;
  fresh.end_gravity = e->end_gravity;
  *a = candidate;
  *e = fresh;
}
} // namespace inde::application

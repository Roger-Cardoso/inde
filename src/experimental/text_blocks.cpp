#include "inde/experimental/text_blocks.hpp"
#include "inde/experimental/text_buffer.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <glib.h>
#include <stdexcept>
namespace inde::experimental {
namespace {
void check_text(const std::string &text) {
  if (text.size() > TextBuffer::limit || text.find('\0') != std::string::npos ||
      !g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr))
    throw std::runtime_error("Bloco exige UTF-8 sem NUL e até 32 MiB");
}
void check_label(const std::string &label) {
  check_text(label);
  if (label.empty() || label.size() > 256) throw std::runtime_error("Nome vazio ou longo demais");
}
}
const char *name(PlacementMode mode) {
  switch (mode) {
  case PlacementMode::Live: return "live";
  case PlacementMode::Pinned: return "pinned";
  case PlacementMode::Forked: return "forked";
  case PlacementMode::ReferenceOnly: return "reference-only";
  }
  throw std::runtime_error("Modo desconhecido");
}
TextBlocksLab::TextBlocksLab() : composition_id_(project::new_uuid()) {}
void TextBlocksLab::budget(std::size_t extra) const {
  std::size_t bytes{}, revisions{};
  for (const auto &[id, block] : blocks_) for (const auto &r : block.revisions) {
    bytes += r.text.size(); ++revisions;
  }
  if (extra > TextBuffer::limit - std::min(bytes, TextBuffer::limit) || revisions >= 4096)
    throw std::runtime_error("Limite do laboratório: 32 MiB de revisões ou 4096 revisões");
}
const TextBlock &TextBlocksLab::block(const std::string &id) const {
  auto found = blocks_.find(id);
  if (found == blocks_.end()) throw std::runtime_error("Bloco inexistente");
  return found->second;
}
TextPlacement &TextBlocksLab::placement(const std::string &id) {
  auto found = std::find_if(placements_.begin(), placements_.end(), [&](auto &p) { return p.id == id; });
  if (found == placements_.end()) throw std::runtime_error("Colocação inexistente");
  return *found;
}
const BlockRevision &TextBlocksLab::revision(const TextPlacement &p) const {
  name(p.mode);
  const auto &b = block(p.block_id);
  if (p.mode == PlacementMode::Forked && !b.fork_origin) throw std::runtime_error("Modo forked exige bifurcação real");
  const auto id = p.mode == PlacementMode::Pinned ? p.pinned_revision.value_or("") : b.current_revision;
  if (p.mode != PlacementMode::Pinned && p.pinned_revision) throw std::runtime_error("Revisão fixa em modo não fixado");
  auto r = std::find_if(b.revisions.begin(), b.revisions.end(), [&](auto &v) { return v.id == id; });
  if (r == b.revisions.end()) throw std::runtime_error("Revisão ausente ou pertence a outro bloco");
  return *r;
}
std::string TextBlocksLab::import_document(const project::Document &d, std::size_t start, std::size_t end) {
  project::validate(d); check_text(d.content);
  if (start > end || end > project::utf8_character_count(d.content) || blocks_.size() >= 256)
    throw std::runtime_error("Extração experimental fora do Documento ou limite de blocos");
  auto left = g_utf8_offset_to_pointer(d.content.c_str(), static_cast<glong>(start));
  auto right = g_utf8_offset_to_pointer(d.content.c_str(), static_cast<glong>(end));
  std::string text(left, right); budget(text.size());
  TextBlock b{project::new_uuid(), d.title, d.id, project::new_uuid(), {}, {}};
  auto raw = g_compute_checksum_for_data(G_CHECKSUM_SHA256,
      reinterpret_cast<const guchar *>(d.content.data()), d.content.size());
  std::string hash(raw); g_free(raw);
  b.revisions.push_back({b.current_revision, {}, std::move(text), SourceRange{d.id, hash, start, end}, slice_layers(d, start, end)});
  const auto id = b.id; blocks_.emplace(id, std::move(b)); return id;
}
std::string TextBlocksLab::place(const std::string &id, PlacementMode mode, std::optional<std::string> revision_id) {
  if (placements_.size() >= 2048) throw std::runtime_error("Limite de colocações do laboratório");
  TextPlacement p{project::new_uuid(), id, mode, std::move(revision_id), {}, {}, {}};
  (void)revision(p); const auto result = p.id; placements_.push_back(std::move(p)); return result;
}
void TextBlocksLab::pin(const std::string &id, const std::string &expected) {
  auto &p = placement(id);
  if (revision(p).id != expected) throw std::runtime_error("Revisão mudou; atualize a prévia");
  if (p.mode == PlacementMode::ReferenceOnly) throw std::runtime_error("Referência não incorpora texto para fixar");
  p.pinned_revision = expected; p.mode = PlacementMode::Pinned;
}
void TextBlocksLab::reference_only(const std::string &id) {
  auto &p = placement(id); p.mode = PlacementMode::ReferenceOnly; p.pinned_revision.reset();
}
void TextBlocksLab::fork(const std::string &id, const std::string &expected) {
  auto &p = placement(id); const auto r = revision(p);
  if (r.id != expected || p.mode == PlacementMode::ReferenceOnly || blocks_.size() >= 256)
    throw std::runtime_error("Bifurcação exige revisão apresentada e texto incorporado");
  budget(r.text.size()); auto b = block(p.block_id);
  b.fork_origin = std::make_pair(b.id, r.id); b.id = project::new_uuid();
  b.current_revision = project::new_uuid();
  b.revisions = {{b.current_revision, {}, r.text, r.exact_source, r.layers}};
  const auto new_id = b.id; blocks_.emplace(new_id, std::move(b));
  p.block_id = new_id; p.pinned_revision.reset(); p.mode = PlacementMode::Forked;
}
void TextBlocksLab::revise(const std::string &id, const std::string &expected, const std::string &text) {
  const auto &old = block(id); check_text(text);
  if (old.current_revision != expected) throw std::runtime_error("Conflito: revisão corrente mudou");
  if (old.revisions.back().text == text) return;
  budget(text.size()); auto &b = blocks_.at(id);
  auto next = project::new_uuid(); b.revisions.push_back({next, expected, text, {}, edit_layers(old.revisions.back().layers, text)});
  b.current_revision = std::move(next); // Old source offsets are not asserted for changed text.
}
EditImpact TextBlocksLab::impact(const std::string &id) const {
  (void)block(id); EditImpact result;
  for (const auto &p : placements_) if (p.block_id == id) {
    if (p.mode == PlacementMode::Pinned) result.preserved_pinned.push_back(p.id);
    else if (p.mode == PlacementMode::ReferenceOnly) result.navigation_only.push_back(p.id);
    else result.changed_live.push_back(p.id);
  }
  return result;
}
void TextBlocksLab::reorder(const std::vector<std::string> &ids) {
  if (ids.size() != placements_.size()) throw std::runtime_error("Reordenação exige permutação completa");
  std::set<std::string> seen; std::vector<TextPlacement> ordered;
  for (const auto &id : ids) {
    if (!seen.insert(id).second) throw std::runtime_error("Colocação duplicada na ordem");
    ordered.push_back(placement(id));
  }
  placements_.swap(ordered);
}
void TextBlocksLab::remove_placement(const std::string &id) {
  placement(id); std::erase_if(placements_, [&](auto &p) { return p.id == id; });
}
void TextBlocksLab::remove_block(const std::string &id) {
  (void)block(id);
  if (std::any_of(placements_.begin(), placements_.end(), [&](auto &p) { return p.block_id == id; }))
    throw std::runtime_error("Bloco ainda usado, inclusive por colocações ocultas/referências");
  if (std::any_of(blocks_.begin(), blocks_.end(), [&](auto &b) { return b.second.fork_origin && b.second.fork_origin->first == id; }))
    throw std::runtime_error("Origem de bifurcação preservada; remoção exige contrato de tombstone");
  blocks_.erase(id);
}
std::string TextBlocksLab::add_variant(const std::string &label) {
  check_label(label); if (variants_.size() >= 32) throw std::runtime_error("Limite de variantes");
  auto id = project::new_uuid(); variants_.emplace(id, label); return id;
}
std::string TextBlocksLab::add_layer(const std::string &label) {
  check_label(label); if (layers_.size() >= 32) throw std::runtime_error("Limite de camadas");
  auto id = project::new_uuid(); layers_.emplace(id, label); return id;
}
void TextBlocksLab::classify(const std::string &id, std::optional<std::string> variant, std::optional<std::string> layer) {
  if ((variant && !variants_.contains(*variant)) || (layer && !layers_.contains(*layer)))
    throw std::runtime_error("Variante/camada inexistente");
  auto &p = placement(id); p.variant = std::move(variant); p.layer = std::move(layer);
  p.variant_group = p.variant && variant_groups_.contains(*p.variant) ?
      std::optional<std::string>{variant_groups_.at(*p.variant)} : std::nullopt;
}
TextProjection TextBlocksLab::project(const ReadingProfile &profile) const {
  if (profile.variant && !variants_.contains(*profile.variant)) throw std::runtime_error("Perfil aponta variante inexistente");
  for (const auto &l : profile.visible_layers) if (!layers_.contains(l)) throw std::runtime_error("Perfil aponta camada inexistente");
  for (const auto &[group, variant] : profile.selected_variants)
    if (!variant_bases_.contains(group) || !variant_groups_.contains(variant) || variant_groups_.at(variant) != group)
      throw std::runtime_error("Alternativa não pertence à família deste perfil");
  TextProjection out; std::size_t offset{};
  for (const auto &p : placements_) {
    if (!visible(p, profile)) continue;
    const auto &r = revision(p); const bool ref = p.mode == PlacementMode::ReferenceOnly;
    const auto end = offset + (ref ? 0 : project::utf8_character_count(r.text));
    if (!ref) {
      if (r.text.size() > TextBuffer::limit - out.text.size()) throw std::runtime_error("Projeção excede limite do laboratório");
      out.text += r.text;
    }
    out.segments.push_back({p.id, p.block_id, r.id, offset, end, ref, r.exact_source}); offset = end;
  }
  return out;
}
}

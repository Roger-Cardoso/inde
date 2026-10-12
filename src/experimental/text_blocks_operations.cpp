#include "inde/experimental/text_blocks.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <glib.h>
#include <stdexcept>
namespace inde::experimental {
namespace {
void label_ok(const std::string &label) {
  if (label.empty() || label.size() > 256 || label.find('\0') != std::string::npos ||
      !g_utf8_validate(label.data(), label.size(), nullptr))
    throw std::runtime_error("Dê um nome ao trecho, com até 256 bytes UTF-8");
}
std::string slice(const std::string &text, std::size_t a, std::size_t b) {
  return {g_utf8_offset_to_pointer(text.c_str(), a), g_utf8_offset_to_pointer(text.c_str(), b)};
}
}
const TextPlacement &TextBlocksLab::occurrence(const std::string &id) const {
  auto p = std::find_if(placements_.begin(), placements_.end(), [&](auto &v) { return v.id == id; });
  if (p == placements_.end()) throw std::runtime_error("Ocorrência não existe mais");
  return *p;
}
const BlockRevision &TextBlocksLab::version(const std::string &id, const std::string &rev) const {
  const auto &b = block(id);
  auto r = std::find_if(b.revisions.begin(), b.revisions.end(), [&](auto &v) { return v.id == rev; });
  if (r == b.revisions.end()) throw std::runtime_error("Versão não pertence a este bloco");
  return *r;
}
void TextBlocksLab::rename_block(const std::string &id, const std::string &label) {
  label_ok(label); (void)block(id); blocks_.at(id).label = label;
}
void TextBlocksLab::rename_variant(const std::string &id, const std::string &label) {
  label_ok(label); if (!variants_.contains(id)) throw std::runtime_error("Alternativa inexistente");
  variants_.at(id) = label;
}
void TextBlocksLab::rename_layer(const std::string &id, const std::string &label) {
  label_ok(label); if (!layers_.contains(id)) throw std::runtime_error("Camada inexistente");
  layers_.at(id) = label;
}
void TextBlocksLab::follow_current(const std::string &id) {
  auto &p = placement(id); p.mode = block(p.block_id).fork_origin ? PlacementMode::Forked : PlacementMode::Live;
  p.pinned_revision.reset();
}
void TextBlocksLab::set_hidden(const std::string &id, bool hidden) { placement(id).hidden = hidden; }
void TextBlocksLab::use_version(const std::string &id, const std::string &rev, const std::string &expected) {
  auto &p = placement(id);
  if (revision(p).id != expected || p.mode == PlacementMode::ReferenceOnly)
    throw std::runtime_error("A ocorrência mudou; confira a versão apresentada");
  (void)version(p.block_id, rev); p.pinned_revision = rev; p.mode = PlacementMode::Pinned;
}
bool TextBlocksLab::visible(const TextPlacement &p, const ReadingProfile &profile) const {
  if (p.hidden || (p.layer && !profile.visible_layers.contains(*p.layer))) return false;
  if (!p.variant) return true;
  if (!p.variant_group) return p.variant == profile.variant;
  auto choice = profile.selected_variants.find(*p.variant_group);
  const auto selected = choice == profile.selected_variants.end() ? variant_bases_.at(*p.variant_group) : choice->second;
  return *p.variant == selected;
}
std::string TextBlocksLab::split_selection(const std::string &id, std::size_t start,
    std::size_t end, const std::string &label) {
  label_ok(label); const auto p = occurrence(id); const auto r = revision(p);
  const auto count = project::utf8_character_count(r.text);
  if (p.mode == PlacementMode::ReferenceOnly || start > end || end > count ||
      (start == 0 && end == count) || (start == end && (start == 0 || start == count)))
    throw std::runtime_error("Selecione parte do trecho ou coloque o cursor num ponto interno");
  // Build on a disposable copy for strong exception safety (limits/validation).
  auto next = *this; std::vector<TextPlacement> inserted;
  std::vector<std::pair<std::size_t, std::size_t>> ranges;
  if (start) ranges.emplace_back(0, start);
  if (start < end) ranges.emplace_back(start, end);
  if (end < count) ranges.emplace_back(end, count);
  std::string selected;
  const auto original = block(p.block_id);
  for (auto [a, b] : ranges) {
    if (next.blocks_.size() >= 256) throw std::runtime_error("Limite de blocos atingido");
    const auto text = slice(r.text, a, b); next.budget(text.size());
    auto block = original; block.id = project::new_uuid(); block.current_revision = project::new_uuid();
    const bool middle = (start < end && a == start && b == end) || (start == end && a == end);
    block.label = middle ? label : (a == 0 ? "Antes — " : "Depois — ") + original.label;
    if (block.label.size() > 256) block.label = middle ? label : original.label;
    block.fork_origin = std::make_pair(original.id, r.id);
    auto origin = r.exact_source;
    if (origin) { origin->start += a; origin->end = origin->start + (b - a); }
    auto layers = r.layers.document ? slice_layers(*r.layers.document, a, b) : LayerSnapshot{};
    layers.warnings.insert(layers.warnings.end(), r.layers.warnings.begin(), r.layers.warnings.end());
    block.revisions = {{block.current_revision, {}, text, origin, std::move(layers)}};
    const auto block_id = block.id; next.blocks_.emplace(block_id, std::move(block));
    auto occurrence = p; occurrence.id = project::new_uuid(); occurrence.block_id = block_id;
    occurrence.mode = PlacementMode::Forked; occurrence.pinned_revision.reset();
    if (middle) selected = occurrence.id;
    inserted.push_back(std::move(occurrence));
  }
  if (next.placements_.size() - 1 + inserted.size() > 2048) throw std::runtime_error("Limite de ocorrências");
  std::vector<TextPlacement> ordered;
  for (const auto &old : placements_) {
    if (old.id == id) ordered.insert(ordered.end(), inserted.begin(), inserted.end()); else ordered.push_back(old);
  }
  next.placements_ = std::move(ordered); (void)next.project({});
  *this = std::move(next); return selected.empty() ? inserted.front().id : selected;
}
std::string TextBlocksLab::alternative(const std::string &id, const std::string &label) {
  label_ok(label); auto next = *this; auto p = next.occurrence(id);
  if (p.mode == PlacementMode::ReferenceOnly) throw std::runtime_error("Uma referência não pode criar alternativa de texto");
  std::string group;
  if (p.variant_group) group = *p.variant_group;
  else {
    group = project::new_uuid();
    auto base_label = "Base — " + block(p.block_id).label;
    if (base_label.size() > 256) base_label = "Base";
    const auto base = next.add_variant(base_label);
    next.variant_groups_[base] = group; next.variant_bases_[group] = base;
    next.classify(id, base, p.layer);
  }
  const auto alt = next.add_variant(label); next.variant_groups_[alt] = group;
  const auto revision_id = next.revision(p).id;
  const auto other = next.place(p.block_id, p.mode == PlacementMode::Pinned ? PlacementMode::Pinned : PlacementMode::Live, p.pinned_revision);
  next.fork(other, revision_id); next.rename_block(next.occurrence(other).block_id, label);
  next.classify(other, alt, p.layer);
  std::vector<std::string> order;
  for (const auto &v : next.placements_) if (v.id != other) {
    order.push_back(v.id); if (v.id == id) order.push_back(other);
  }
  next.reorder(order); *this = std::move(next); return other;
}
std::string TextBlocksLab::promote(const std::string &id, const std::string &label) {
  label_ok(label); auto next = *this; const auto p = next.occurrence(id);
  if (p.mode == PlacementMode::ReferenceOnly) throw std::runtime_error("Promova uma alternativa que incorpore texto");
  next.fork(id, next.revision(p).id); next.rename_block(next.occurrence(id).block_id, label);
  next.classify(id, {}, p.layer); // Base/other alternatives stay recorded but inactive here.
  next.set_hidden(id, false);
  if (p.variant_group) for (auto &old : next.placements_) if (old.id != id && old.variant_group == p.variant_group) old.hidden = true;
  const auto result = next.occurrence(id).block_id; *this = std::move(next); return result;
}
}

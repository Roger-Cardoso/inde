#include "inde/experimental/text_blocks.hpp"
#include <glib.h>
#include <stdexcept>
#include <uuid/uuid.h>
namespace inde::experimental {
void TextBlocksLab::assign_origin_snapshot(const std::string &id,
                                           const std::string &hash) {
  auto &b = blocks_.at(id);
  if (b.origin_snapshot_sha256 || hash.size() != 64)
    throw std::runtime_error("Origem congelada já atribuída ou inválida");
  b.origin_snapshot_sha256 = hash;
}
TextBlocksSnapshot TextBlocksLab::snapshot() const {
  return {composition_id_, blocks_,         placements_,   variants_,
          layers_,         variant_groups_, variant_bases_};
}
TextBlocksLab TextBlocksLab::restore(TextBlocksSnapshot s) {
  auto id = [](const std::string &value) {
    uuid_t parsed{};
    if (value.size() != 36 || uuid_parse(value.c_str(), parsed))
      throw std::runtime_error("Identidade inválida na Composição");
  };
  auto text = [](const std::string &value, std::size_t limit) {
    if (value.size() > limit || value.find('\0') != std::string::npos ||
        !g_utf8_validate(value.data(), value.size(), nullptr))
      throw std::runtime_error(
          "Texto inválido ou limite excedido na Composição");
  };
  id(s.composition_id);
  if (s.blocks.size() > 256 || s.placements.size() > 2048 ||
      s.variants.size() > 32 || s.layers.size() > 32 ||
      s.variant_bases.size() > 32)
    throw std::runtime_error("Limites de estrutura da Composição excedidos");
  std::set<std::string> revision_ids, placement_ids;
  std::size_t bytes{}, revisions{};
  for (const auto &[key, b] : s.blocks) {
    id(key);
    id(b.id);
    id(b.origin_document_id);
    id(b.current_revision);
    text(b.label, 1024);
    if (key != b.id || b.label.empty() || b.revisions.empty() ||
        b.current_revision != b.revisions.back().id)
      throw std::runtime_error("Bloco/revisão corrente incompatíveis");
    std::string previous;
    for (const auto &r : b.revisions) {
      id(r.id);
      text(r.text, 32 * 1024 * 1024);
      bytes += r.text.size();
      ++revisions;
      if (bytes > 32 * 1024 * 1024 || revisions > 4096 ||
          !revision_ids.insert(r.id).second || r.parent_id != previous)
        throw std::runtime_error("Cadeia/limite de revisões inválidos");
      previous = r.id;
      if (!r.layers.document || r.layers.document->content != r.text)
        throw std::runtime_error("Snapshot de camadas ausente ou divergente");
      project::validate(*r.layers.document);
      if (r.layers.warnings.size() > 4096)
        throw std::runtime_error("Avisos de camadas excedidos");
      for (const auto &warning : r.layers.warnings)
        text(warning, 4096);
      if (r.exact_source) {
        const auto &source = *r.exact_source;
        id(source.document_id);
        if (source.source_body_sha256.size() != 64 ||
            source.start > source.end ||
            source.end - source.start != project::utf8_character_count(r.text))
          throw std::runtime_error("Faixa de origem inválida");
      }
    }
  }
  auto names = [&](const auto &map) {
    for (const auto &[key, name] : map) {
      id(key);
      text(name, 256);
      if (name.empty())
        throw std::runtime_error("Nome vazio");
    }
  };
  names(s.variants);
  names(s.layers);
  TextBlocksLab result;
  result.composition_id_ = std::move(s.composition_id);
  result.blocks_ = std::move(s.blocks);
  result.placements_ = std::move(s.placements);
  result.variants_ = std::move(s.variants);
  result.layers_ = std::move(s.layers);
  result.variant_groups_ = std::move(s.variant_groups);
  result.variant_bases_ = std::move(s.variant_bases);
  for (const auto &[variant, group] : result.variant_groups_) {
    id(group);
    if (!result.variants_.contains(variant) ||
        !result.variant_bases_.contains(group))
      throw std::runtime_error("Família de alternativa inválida");
  }
  for (const auto &[group, base] : result.variant_bases_) {
    id(group);
    if (!result.variant_groups_.contains(base) ||
        result.variant_groups_.at(base) != group)
      throw std::runtime_error("Base de alternativa ausente");
  }
  for (const auto &[key, b] : result.blocks_) {
    std::set<std::string> ancestry{key};
    auto current = &b;
    while (current->fork_origin) {
      const auto &[parent, revision] = *current->fork_origin;
      (void)result.version(parent, revision);
      if (!ancestry.insert(parent).second)
        throw std::runtime_error("Ciclo de bifurcação");
      current = &result.block(parent);
    }
  }
  for (const auto &p : result.placements_) {
    id(p.id);
    if (!placement_ids.insert(p.id).second ||
        (p.variant && !result.variants_.contains(*p.variant)) ||
        (p.layer && !result.layers_.contains(*p.layer)))
      throw std::runtime_error("Ocorrência ou classificação inválida");
    const auto group =
        p.variant && result.variant_groups_.contains(*p.variant)
            ? std::optional(result.variant_groups_.at(*p.variant))
            : std::nullopt;
    if (p.variant_group != group)
      throw std::runtime_error("Família da ocorrência incompatível");
    (void)result.revision(p);
  }
  (void)result.project({});
  return result;
}
} // namespace inde::experimental

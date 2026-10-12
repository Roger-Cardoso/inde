#include "inde/application/editor_interaction.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
namespace inde::application {
namespace {
using Intervals = std::vector<TextRange>;
std::size_t coverage(Intervals values) {
  std::sort(values.begin(), values.end(),
            [](auto a, auto b) { return a.start < b.start; });
  std::size_t count{}, end{};
  for (const auto r : values) {
    count +=
        r.end > std::max(end, r.start) ? r.end - std::max(end, r.start) : 0;
    end = std::max(end, r.end);
  }
  return count;
}
} // namespace
SemanticDistribution semantic_distribution(
    project::Document d,
    const std::vector<project::AnnotationTypeDefinition> &types,
    TextRange scope) {
  if (scope.start > scope.end ||
      scope.end > project::utf8_character_count(d.content))
    throw std::runtime_error("Escopo da análise inválido");
  SemanticAnchorEngine::inspect(d);
  SemanticDistribution out;
  out.scope = scope;
  out.characters = scope.end - scope.start;
  std::map<std::string, DistributionRow> rows;
  std::map<std::string, Intervals> ranges;
  for (const auto &t : types)
    rows.emplace(t.key, DistributionRow{t.key, t.label, 0, 0, 0, 0, 0});
  auto anchor_range = [&](const std::string &id) -> std::optional<TextRange> {
    auto a = std::find_if(d.anchors.begin(), d.anchors.end(),
                          [&](const auto &a) { return a.id == id; });
    if (a == d.anchors.end())
      return {};
    TextRange r{std::max(scope.start, a->start_offset),
                std::min(scope.end, a->end_offset)};
    return r.start < r.end ? std::optional{r} : std::nullopt;
  };
  std::set<std::string> included;
  auto &entity_row =
      rows.emplace("editor.entities",
                   DistributionRow{"editor.entities",
                                   "Ocorrências de entidades", 0, 0, 0, 0, 0})
          .first->second;
  for (const auto &a : d.semantics.annotations) {
    if (a.status == "archived")
      continue;
    auto &row =
        rows.try_emplace(a.type_key,
                         DistributionRow{a.type_key, a.type_key, 0, 0, 0, 0, 0})
            .first->second;
    if (a.scope == project::AnnotationScope::Document) {
      ++row.global;
      continue;
    }
    auto e =
        std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                     [&](const auto &e) { return e.anchor_id == a.anchor_id; });
    if (e == d.semantics.anchors.end() ||
        e->state != project::AnchorResolution::Resolved) {
      ++row.unresolved;
      continue;
    }
    const auto r = anchor_range(a.anchor_id);
    if (!r)
      continue;
    included.insert(a.id);
    ++row.markings;
    if (a.status == "incomplete")
      ++row.incomplete;
    ranges[a.type_key].push_back(*r);
    bool has_entity = false;
    for (const auto &[key, f] : a.fields) {
      (void)key;
      if (f.kind == project::SemanticFieldKind::Entity && !f.values.empty()) {
        out.entity_ids.insert(f.values.begin(), f.values.end());
        has_entity = true;
      }
    }
    if (has_entity) {
      ++entity_row.markings;
      ranges[entity_row.key].push_back(*r);
    }
  }
  for (const auto &ref : d.entity_references)
    if (ref.anchor_id) {
      const auto r = anchor_range(*ref.anchor_id);
      if (!r)
        continue;
      ++out.native_entity_occurrences;
      ++entity_row.markings;
      ranges[entity_row.key].push_back(*r);
      out.entity_ids.insert(ref.entity_id);
    }
  // Coverage counts semantic roles, not the same entity mention twice.
  Intervals all;
  std::map<std::size_t, int> events;
  for (auto &[key, row] : rows) {
    row.characters = coverage(ranges[key]);
    if (key != "editor.entities") {
      auto values = ranges[key];
      std::sort(values.begin(), values.end(),
                [](auto a, auto b) { return a.start < b.start; });
      Intervals merged;
      for (auto r : values) {
        if (!merged.empty() && r.start <= merged.back().end)
          merged.back().end = std::max(merged.back().end, r.end);
        else
          merged.push_back(r);
      }
      for (auto r : merged) {
        all.push_back(r);
        ++events[r.start];
        --events[r.end];
      }
    }
    out.rows.push_back(row);
  }
  out.covered = coverage(all);
  int active{};
  std::size_t previous{};
  for (const auto &[pos, delta] : events) {
    if (active > 1)
      out.overlapping += pos - previous;
    active += delta;
    previous = pos;
  }
  for (const auto &rel : d.semantics.relations)
    if (included.contains(rel.source_annotation_id) ||
        included.contains(rel.target_annotation_id))
      ++out.annotation_relations;
  return out;
}
} // namespace inde::application

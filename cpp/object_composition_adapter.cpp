#include "object_composition_adapter.hpp"
#include "object_source_capture.hpp"
#include "inde/persistence/composition_store.hpp"
#include <new>
#include <unordered_set>
#include <uuid/uuid.h>

namespace inde::objects::compat {
namespace {
using detail::refuse;
std::string subject(const project::StoredComposition &s) {
  const auto &id = s.value.blocks.composition_id();
  return id.size() <= 36 ? id : "composition";
}
bool uuid(const std::string &s) { uuid_t id{}; return s.size() == 36 && !uuid_parse(s.c_str(), id); }
void valid_limits(CompositionReadLimits l) {
  const CompositionReadLimits cap;
  if (!l.blocks || l.blocks > cap.blocks || !l.occurrences || l.occurrences > cap.occurrences ||
      !l.revisions || l.revisions > cap.revisions || !l.source_string_bytes ||
      l.source_string_bytes > cap.source_string_bytes || !l.snapshot_bytes || l.snapshot_bytes > cap.snapshot_bytes)
    refuse("invalid_composition_projection_limits");
}
void preflight(const project::StoredComposition &s, CompositionReadLimits limits) {
  valid_limits(limits);
  if (!uuid(s.project_id) || s.revision < 1 || s.updated_at.empty() || s.updated_at.size() > 128)
    refuse("invalid_composition_envelope");
  if (s.sha256.size() != 64) refuse("composition_hash_mismatch");
  const auto &c = s.value;
  if (c.blocks.blocks().size() > limits.blocks || c.blocks.placements().size() > limits.occurrences)
    refuse("composition_structure_limit");
  detail::InputBudget b(limits.source_string_bytes);
  b.string(s.project_id); b.string(s.sha256); b.string(s.updated_at);
  b.string(c.name); b.string(c.blocks.composition_id());
  b.collection(c.blocks.blocks()); b.collection(c.blocks.placements());
  std::size_t remaining = limits.revisions;
  for (const auto &[id, block] : c.blocks.blocks()) {
    b.string(id); b.string(block.id); b.string(block.label); b.string(block.origin_document_id);
    b.string(block.current_revision); b.optional(block.origin_snapshot_sha256);
    if (block.fork_origin) { b.string(block.fork_origin->first); b.string(block.fork_origin->second); }
    if (block.revisions.size() > remaining) refuse("composition_revision_limit");
    remaining -= block.revisions.size(); b.collection(block.revisions);
    for (const auto &r : block.revisions) {
      b.string(r.id); b.string(r.parent_id); b.string(r.text);
      if (r.exact_source) { b.string(r.exact_source->document_id); b.string(r.exact_source->source_body_sha256); }
      if (!r.layers.document) refuse("missing_composition_revision_snapshot");
      detail::account_document(*r.layers.document, b);
      b.collection(r.layers.warnings);
      for (const auto &warning : r.layers.warnings) b.string(warning);
    }
  }
  for (const auto &p : c.blocks.placements()) {
    b.string(p.id); b.string(p.block_id); b.optional(p.pinned_revision);
    b.optional(p.variant); b.optional(p.layer); b.optional(p.variant_group);
  }
  auto strings = [&](const auto &values) {
    b.collection(values);
    for (const auto &[key, value] : values) { b.string(key); b.string(value); }
  };
  strings(c.blocks.variants()); strings(c.blocks.layers()); strings(c.blocks.variant_groups()); strings(c.blocks.variant_bases());
  b.optional(c.profile.variant); b.collection(c.profile.visible_layers);
  for (const auto &id : c.profile.visible_layers) b.string(id);
  strings(c.profile.selected_variants);
  b.collection(c.origins); b.collection(c.drafts);
  for (const auto &[sha, d] : c.origins) { b.string(sha); detail::account_document(d, b); }
  for (const auto &[id, draft] : c.drafts) { b.string(id); b.string(draft.expected_revision); b.string(draft.text); }
}
std::string capture(const project::StoredComposition &s, CompositionReadLimits limits) {
  preflight(s, limits);
  const auto bytes = persistence::encode_composition(s.value);
  if (bytes.size() > limits.snapshot_bytes) refuse("composition_snapshot_limit");
  if (persistence::composition_checksum(bytes) != s.sha256) refuse("composition_hash_mismatch");
  return bytes;
}
} // namespace
CompositionReadResult read_composition(const project::StoredComposition &s, CompositionReadLimits limits) {
  try {
    const auto bytes = capture(s, limits);
    auto p = std::unique_ptr<CompositionReadProjection>(new CompositionReadProjection);
    p->limits_ = limits;
    p->snapshot_ = {persistence::decode_composition(bytes, s.sha256), s.project_id, s.sha256, s.updated_at, s.revision};
    if (persistence::encode_composition(p->snapshot_.value) != bytes) refuse("composition_snapshot_roundtrip_mismatch");
    p->stamp_ = {s.project_id, s.value.blocks.composition_id(), s.sha256, s.updated_at, s.revision};
    const auto scope = s.project_id + "/" + p->stamp_.composition_id;
    auto &graph = p->graph_;
    graph.id = "obj01/composition/" + scope;
    graph.root_layout_id = "obj01/composition-layout/" + scope;
    graph.layouts.push_back({graph.root_layout_id, LayoutKind::Stack, 0, {}});
    const auto &source = p->snapshot_.value;
    std::unordered_set<std::string> objects;
    for (const auto &use : source.blocks.placements()) {
      const auto &r = source.blocks.revision(use);
      const bool ref = use.mode == experimental::PlacementMode::ReferenceOnly;
      const auto oid = std::string(ref ? "obj01/text-reference/" : "obj01/text-block/") + scope + "/" + use.block_id + "/" + r.id;
      const auto id = "obj01/composition-occurrence/" + scope + "/" + use.id;
      if (objects.insert(oid).second) graph.objects.push_back({oid, ref ? ObjectKind::TextReference : ObjectKind::TextBlock, {}, {}});
      graph.occurrences.push_back({id, oid, {}}); graph.layouts[0].children.push_back(id);
      p->rows_.emplace(id, p->bindings_.size());
      p->bindings_.push_back({use.id, use.block_id, r.id, id, oid, use.mode, use.hidden,
                              source.blocks.visible(use, source.profile), ref,
                              use.pinned_revision, use.variant, use.layer, use.variant_group});
    }
    p->reading_ = source.blocks.project(source.profile); // Exact owner semantics: no separators invented, refs zero-width.
    auto errors = validate(graph);
    if (!errors.empty()) return {{}, std::move(errors)};
    if (!source.drafts.empty()) p->notices_.push_back({"private_drafts_not_applied", p->stamp_.composition_id});
    if (!source.origins.empty()) p->notices_.push_back({"frozen_origins_retained", p->stamp_.composition_id});
    p->notices_.push_back({"authoring_private_composition_capture", p->stamp_.composition_id});
    return {std::move(p), {}};
  } catch (const detail::Refusal &r) { return {{}, {{r.code, subject(s)}}}; }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &) { return {{}, {{"invalid_source_composition", subject(s)}}}; }
}
const project::Document *CompositionReadProjection::revision_snapshot(const std::string &id) const {
  const auto row = rows_.find(id);
  if (row == rows_.end()) return nullptr;
  const auto &b = bindings_[row->second];
  if (b.reference_only) return nullptr;
  return snapshot_.value.blocks.version(b.source_block_id, b.source_revision_id).layers.document.get();
}
std::optional<std::string_view> CompositionReadProjection::incorporated_text(const std::string &id) const {
  const auto row = rows_.find(id);
  if (row == rows_.end() || !bindings_[row->second].visible || bindings_[row->second].reference_only) return {};
  return revision_snapshot(id)->content;
}
CompositionSourceCheck CompositionReadProjection::check_source(const project::StoredComposition &s) const {
  if (s.project_id != stamp_.project_id) return {false, {{"source_project_conflict", "project"}}};
  if (s.value.blocks.composition_id() != stamp_.composition_id) return {false, {{"source_composition_conflict", subject(s)}}};
  try {
    (void)capture(s, limits_);
    if (s.revision != stamp_.revision) return {false, {{"source_composition_revision_changed", subject(s)}}};
    if (s.sha256 != stamp_.sha256 || s.updated_at != stamp_.updated_at)
      return {false, {{"source_composition_snapshot_changed", subject(s)}}};
    return {true, {}};
  } catch (const detail::Refusal &r) { return {false, {{r.code, subject(s)}}}; }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &) { return {false, {{"invalid_source_composition", subject(s)}}}; }
}
} // namespace inde::objects::compat

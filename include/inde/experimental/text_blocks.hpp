#pragma once
#include "inde/project/writing.hpp"
#include "inde/experimental/block_layers.hpp"
#include <map>
#include <set>

namespace inde::experimental {
enum class PlacementMode { Live, Pinned, Forked, ReferenceOnly };
const char *name(PlacementMode);
struct SourceRange {
  std::string document_id, source_body_sha256;
  std::size_t start{}, end{}; // Unicode character coordinates in frozen source.
};
struct BlockRevision {
  std::string id, parent_id, text;
  std::optional<SourceRange> exact_source;
  LayerSnapshot layers;
};
struct TextBlock {
  std::string id, label, origin_document_id, current_revision;
  std::optional<std::pair<std::string, std::string>> fork_origin; // block, revision
  std::vector<BlockRevision> revisions;
  std::optional<std::string> origin_snapshot_sha256{};
};
struct TextPlacement {
  std::string id, block_id;
  PlacementMode mode{PlacementMode::Live};
  std::optional<std::string> pinned_revision, variant, layer;
  std::optional<std::string> variant_group{};
  bool hidden{};
};
struct ReadingProfile {
  std::optional<std::string> variant;
  std::set<std::string> visible_layers;
  std::map<std::string, std::string> selected_variants;
};
struct ProjectedSegment {
  std::string placement_id, block_id, revision_id;
  std::size_t start{}, end{}; // Unicode coordinates in the derived view.
  bool reference_only{};
  std::optional<SourceRange> exact_source;
};
struct TextProjection {
  std::string text;
  std::vector<ProjectedSegment> segments;
};
struct EditImpact {
  std::vector<std::string> changed_live, preserved_pinned, navigation_only;
};
struct TextBlocksSnapshot {
  std::string composition_id;
  std::map<std::string, TextBlock> blocks;
  std::vector<TextPlacement> placements;
  std::map<std::string, std::string> variants, layers, variant_groups, variant_bases;
};
// Volatile identities. Materialized output is an independent snapshot, never
// double-write. Representable layers survive; boundary/global issues are visible.
class TextBlocksLab {
public:
  TextBlocksLab();
  [[nodiscard]] TextBlocksSnapshot snapshot() const;
  [[nodiscard]] static TextBlocksLab restore(TextBlocksSnapshot);
  [[nodiscard]] const std::string &composition_id() const { return composition_id_; }
  std::string import_document(const project::Document &, std::size_t start, std::size_t end);
  std::string place(const std::string &block, PlacementMode mode = PlacementMode::Live,
                    std::optional<std::string> revision = {});
  void pin(const std::string &placement, const std::string &expected_revision);
  void reference_only(const std::string &placement);
  void fork(const std::string &placement, const std::string &expected_revision);
  [[nodiscard]] const TextPlacement &occurrence(const std::string &) const;
  [[nodiscard]] const BlockRevision &version(const std::string &block,
                                           const std::string &revision) const;
  void rename_block(const std::string &, const std::string &);
  void assign_origin_snapshot(const std::string &block, const std::string &sha256);
  void follow_current(const std::string &placement);
  void use_version(const std::string &placement, const std::string &revision,
                   const std::string &expected);
  std::string split_selection(const std::string &placement, std::size_t start,
                              std::size_t end, const std::string &label);
  std::string promote(const std::string &placement, const std::string &label);
  std::string alternative(const std::string &placement, const std::string &label);
  void rename_variant(const std::string &, const std::string &);
  void rename_layer(const std::string &, const std::string &);
  void set_hidden(const std::string &, bool);
  [[nodiscard]] bool visible(const TextPlacement &, const ReadingProfile &) const;
  [[nodiscard]] const std::map<std::string, std::string> &variant_groups() const { return variant_groups_; }
  [[nodiscard]] const std::map<std::string, std::string> &variant_bases() const { return variant_bases_; }
  void revise(const std::string &block, const std::string &expected_revision, const std::string &text);
  void reorder(const std::vector<std::string> &exact_permutation);
  void remove_placement(const std::string &id);
  void remove_block(const std::string &id); // protected even by hidden references.
  std::string add_variant(const std::string &label);
  std::string add_layer(const std::string &label);
  void classify(const std::string &placement, std::optional<std::string> variant,
                std::optional<std::string> layer);
  [[nodiscard]] TextProjection project(const ReadingProfile &) const;
  [[nodiscard]] EditImpact impact(const std::string &block) const;
  [[nodiscard]] const BlockRevision &revision(const TextPlacement &) const;
  [[nodiscard]] const TextBlock &block(const std::string &) const;
  [[nodiscard]] const std::map<std::string, TextBlock> &blocks() const { return blocks_; }
  [[nodiscard]] const std::vector<TextPlacement> &placements() const { return placements_; }
  [[nodiscard]] const std::map<std::string, std::string> &variants() const { return variants_; }
  [[nodiscard]] const std::map<std::string, std::string> &layers() const { return layers_; }
private:
  std::string composition_id_;
  std::map<std::string, TextBlock> blocks_;
  std::vector<TextPlacement> placements_;
  std::map<std::string, std::string> variants_, layers_;
  std::map<std::string, std::string> variant_groups_, variant_bases_;
  TextPlacement &placement(const std::string &);
  void budget(std::size_t additional) const;
};
}

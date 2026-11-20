#pragma once
#include "object_model.hpp"
#include "inde/project/composition.hpp"
#include <memory>
#include <string_view>
#include <unordered_map>

namespace inde::objects::compat {
struct CompositionSourceStamp {
  std::string project_id, composition_id, sha256, updated_at;
  std::int64_t revision{};
  bool operator==(const CompositionSourceStamp &) const = default;
};
struct CompositionOccurrenceBinding {
  std::string source_occurrence_id, source_block_id, source_revision_id;
  std::string occurrence_id, object_id;
  experimental::PlacementMode mode{experimental::PlacementMode::Live};
  bool declared_hidden{}, visible{}, reference_only{};
  std::optional<std::string> pinned_revision, variant, layer, variant_group;
  bool operator==(const CompositionOccurrenceBinding &) const = default;
};
struct CompositionReadLimits {
  std::size_t blocks{256}, occurrences{2048}, revisions{4096};
  std::size_t source_string_bytes{8 * 1024 * 1024};
  std::size_t snapshot_bytes{2 * 1024 * 1024};
};
struct CompositionSourceCheck { bool current{}; std::vector<Diagnostic> diagnostics; };
class CompositionReadProjection;
struct CompositionReadResult {
  std::unique_ptr<CompositionReadProjection> projection;
  std::vector<Diagnostic> diagnostics;
};
[[nodiscard]] CompositionReadResult read_composition(const project::StoredComposition &,
                                                     CompositionReadLimits limits = {});
// Authoring-private, complete frozen capture. Caller supplies the stored
// envelope; this API does not load SQLite, prove ownership, apply drafts or save.
class CompositionReadProjection final {
public:
  [[nodiscard]] const CompositionSourceStamp &source() const { return stamp_; }
  [[nodiscard]] const project::StoredComposition &source_snapshot() const { return snapshot_; }
  [[nodiscard]] const Composition &graph() const { return graph_; } // All source occurrences, including hidden/reference.
  [[nodiscard]] const std::vector<CompositionOccurrenceBinding> &occurrences() const { return bindings_; }
  [[nodiscard]] const experimental::TextProjection &reading() const { return reading_; } // Owner's current profile only.
  [[nodiscard]] const std::vector<Diagnostic> &notices() const { return notices_; }
  // Only a visible, text-incorporating occurrence returns text. A visible empty
  // block is a present empty view; hidden/reference/missing return nullopt.
  [[nodiscard]] std::optional<std::string_view> incorporated_text(const std::string &occurrence_id) const;
  // Explicit private inspection of an incorporating revision, including hidden
  // text; references never return a payload. Pointer lifetime = projection.
  [[nodiscard]] const project::Document *revision_snapshot(const std::string &occurrence_id) const;
  [[nodiscard]] CompositionSourceCheck check_source(const project::StoredComposition &) const;
private:
  CompositionReadProjection() = default;
  friend CompositionReadResult read_composition(const project::StoredComposition &, CompositionReadLimits);
  CompositionSourceStamp stamp_;
  project::StoredComposition snapshot_;
  Composition graph_;
  std::vector<CompositionOccurrenceBinding> bindings_;
  experimental::TextProjection reading_;
  std::vector<Diagnostic> notices_;
  std::unordered_map<std::string, std::size_t> rows_;
  CompositionReadLimits limits_;
};
} // namespace inde::objects::compat

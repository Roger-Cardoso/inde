#pragma once
#include "object_model.hpp"
#include "inde/project/writing.hpp"
#include <memory>
#include <string_view>
#include <unordered_map>

namespace inde::objects::compat {
// Authoring-private capture. The project ID is supplied by the caller; this
// in-memory adapter cannot prove repository ownership or grant access rights.
struct DocumentSourceStamp {
  std::string project_id, document_id, snapshot_sha256;
  bool operator==(const DocumentSourceStamp &) const = default;
};
struct ParagraphBinding {
  std::string paragraph_id, object_id, occurrence_id, text_sha256;
  std::size_t source_start{}, source_end{}; // Unicode scalars; excludes LF.
  bool operator==(const ParagraphBinding &) const = default;
};
struct ReadingGroupBinding {
  std::string source_block_id;
  std::vector<std::string> occurrence_ids; // Legacy reading_blocks, not inferred narrative groups.
  bool operator==(const ReadingGroupBinding &) const = default;
};
struct DocumentReadLimits {
  std::size_t paragraphs{10000};
  std::size_t source_string_bytes{8 * 1024 * 1024};
  std::size_t snapshot_bytes{16 * 1024 * 1024};
};
struct SourceCheck {
  bool current{};
  std::vector<Diagnostic> diagnostics;
};
class DocumentReadProjection;
struct DocumentReadResult {
  std::unique_ptr<DocumentReadProjection> projection;
  std::vector<Diagnostic> diagnostics; // Failure => no partial projection.
};
[[nodiscard]] DocumentReadResult read_document(const std::string &project_id,
    const project::Document &, DocumentReadLimits limits = {});

// No writeback, persistence, publication, filtering or automatic paragraph ID
// adoption. Frozen deep capture, independently owned even for covered leaves.
class DocumentReadProjection final {
public:
  [[nodiscard]] const DocumentSourceStamp &source() const { return stamp_; }
  [[nodiscard]] const Composition &graph() const { return graph_; }
  [[nodiscard]] const project::Document &source_snapshot() const { return snapshot_; }
  [[nodiscard]] const std::vector<ParagraphBinding> &paragraphs() const { return paragraphs_; }
  [[nodiscard]] const std::vector<ReadingGroupBinding> &reading_groups() const { return groups_; }
  [[nodiscard]] const std::vector<Diagnostic> &notices() const { return notices_; }
  // nullopt means unknown occurrence, not an empty paragraph. View lifetime is
  // the projection lifetime. LF separators belong to the continuous source.
  [[nodiscard]] std::optional<std::string_view> paragraph_text(const std::string &occurrence_id) const;
  // This is a freshness check against supplied in-memory data, not a lock or a
  // check of what is currently stored in SQLite. All source fields participate.
  [[nodiscard]] SourceCheck check_source(const std::string &project_id,
                                         const project::Document &) const;
private:
  DocumentReadProjection() = default;
  friend DocumentReadResult read_document(const std::string &, const project::Document &, DocumentReadLimits);
  DocumentSourceStamp stamp_;
  project::Document snapshot_;
  Composition graph_;
  std::vector<ParagraphBinding> paragraphs_;
  std::vector<ReadingGroupBinding> groups_;
  std::vector<Diagnostic> notices_;
  std::unordered_map<std::string, std::size_t> occurrence_rows_;
  DocumentReadLimits limits_;
};
} // namespace inde::objects::compat

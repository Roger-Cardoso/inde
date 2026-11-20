#include "object_document_adapter.hpp"
#include "object_source_capture.hpp"
#include "inde/application/document_reading_order.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include <glib.h>
#include <new>
#include <stdexcept>
#include <uuid/uuid.h>

namespace inde::objects::compat {
namespace {
using Refusal = detail::Refusal;
[[noreturn]] void refuse(const std::string &code) { throw Refusal{code}; }
bool uuid(const std::string &id) {
  uuid_t value{};
  return id.size() == 36 && uuid_parse(id.c_str(), value) == 0;
}
std::string hash(std::string_view bytes) {
  std::unique_ptr<gchar, decltype(&g_free)> value(g_compute_checksum_for_data(
      G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(bytes.data()), bytes.size()), &g_free);
  if (!value) throw std::runtime_error("SHA-256 unavailable");
  return value.get();
}
void valid_limits(DocumentReadLimits limits) {
  const DocumentReadLimits cap;
  if (limits.paragraphs == 0 || limits.paragraphs > cap.paragraphs ||
      limits.source_string_bytes == 0 || limits.source_string_bytes > cap.source_string_bytes ||
      limits.snapshot_bytes == 0 || limits.snapshot_bytes > cap.snapshot_bytes)
    refuse("invalid_projection_limits");
}
// Fail before owner validation/serialization for oversized input. This counts
// string bytes, not RSS; source payloads may be counted more than once by design.
void preflight(const project::Document &d, DocumentReadLimits limits) {
  valid_limits(limits);
  if (!d.paragraphs.enabled) refuse("paragraph_identity_required");
  if (d.paragraphs.rows.size() > limits.paragraphs) refuse("projection_paragraph_limit");
  if (!project::canonical_paragraphs(d.paragraphs)) refuse("canonical_paragraphs_required");
  for (const auto &row : d.paragraphs.rows)
    if (!row.content) refuse("canonical_paragraphs_required");
  detail::InputBudget budget(limits.source_string_bytes);
  detail::account_document(d, budget);
}

std::string capture(const project::Document &d, DocumentReadLimits limits) {
  preflight(d, limits);
  auto bytes = persistence::encode_document_snapshot(d); // Owning validation; no repository access.
  if (bytes.size() > limits.snapshot_bytes) refuse("projection_snapshot_limit");
  return bytes;
}
std::string subject(const project::Document &d) { return d.id.size() <= 36 ? d.id : "document"; }
} // namespace

DocumentReadResult read_document(const std::string &project_id, const project::Document &d,
                                 DocumentReadLimits limits) {
  if (!uuid(project_id)) return {{}, {{"invalid_source_project_id", "project"}}};
  try {
    const auto bytes = capture(d, limits);
    auto p = std::unique_ptr<DocumentReadProjection>(new DocumentReadProjection);
    p->limits_ = limits;
    // Decode the complete owning snapshot: no alias to mutable covered payloads
    // or paragraph allocations retained by the caller, and no layer truncation.
    p->snapshot_ = persistence::decode_document_snapshot(bytes);
    if (persistence::encode_document_snapshot(p->snapshot_) != bytes)
      refuse("source_snapshot_roundtrip_mismatch");
    p->stamp_ = {project_id, d.id, hash(bytes)};
    const auto scope = project_id + "/" + d.id; // Both are validated UUID strings.
    p->graph_.id = "obj01/document/" + scope;
    p->graph_.root_layout_id = "obj01/layout/" + scope;
    p->graph_.layouts.push_back({p->graph_.root_layout_id, LayoutKind::Stack, 0, {}});
    const auto count = p->snapshot_.paragraphs.rows.size();
    p->graph_.objects.reserve(count); p->graph_.occurrences.reserve(count);
    p->graph_.layouts[0].children.reserve(count);
    p->paragraphs_.reserve(count); p->occurrence_rows_.reserve(count);
    for (const auto &row : p->snapshot_.paragraphs.rows) {
      const auto object_id = "obj01/paragraph/" + scope + "/" + row.id;
      const auto occurrence_id = "obj01/occurrence/" + scope + "/" + row.id;
      p->graph_.objects.push_back({object_id, ObjectKind::Paragraph, {}, {}});
      p->graph_.occurrences.push_back({occurrence_id, object_id, {}});
      p->graph_.layouts[0].children.push_back(occurrence_id);
      p->occurrence_rows_.emplace(occurrence_id, p->paragraphs_.size());
      p->paragraphs_.push_back({row.id, object_id, occurrence_id, hash(*row.content), row.start, row.end});
    }
    const auto groups = application::reading_blocks(p->snapshot_);
    for (const auto &g : groups) {
      ReadingGroupBinding group{g.id, {}};
      for (auto i = g.first; i <= g.last; ++i) group.occurrence_ids.push_back(p->paragraphs_[i].occurrence_id);
      p->groups_.push_back(std::move(group));
    }
    auto errors = validate(p->graph_);
    if (!errors.empty()) return {{}, std::move(errors)};
    if (!d.resmas.empty()) p->notices_.push_back({"private_alternatives_retained", d.id});
    if (!d.visibility_profiles.empty()) p->notices_.push_back({"editor_visibility_not_applied", d.id});
    if (!d.anchors.empty() || !d.resmas.empty())
      p->notices_.push_back({"textual_move_constraints_retained", d.id});
    return {std::move(p), {}};
  } catch (const Refusal &r) {
    return {{}, {{r.code, subject(d)}}};
  } catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &) {
    return {{}, {{"invalid_source_document", subject(d)}}};
  }
}
std::optional<std::string_view> DocumentReadProjection::paragraph_text(const std::string &id) const {
  const auto row = occurrence_rows_.find(id);
  if (row == occurrence_rows_.end()) return {};
  return *snapshot_.paragraphs.rows[row->second].content;
}
SourceCheck DocumentReadProjection::check_source(const std::string &project_id, const project::Document &d) const {
  if (project_id != stamp_.project_id) return {false, {{"source_project_conflict", "project"}}};
  if (d.id != stamp_.document_id) return {false, {{"source_document_conflict", subject(d)}}};
  try {
    if (hash(capture(d, limits_)) != stamp_.snapshot_sha256)
      return {false, {{"source_snapshot_changed", d.id}}};
    return {true, {}};
  } catch (const Refusal &r) {
    return {false, {{r.code, subject(d)}}};
  } catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &) {
    return {false, {{"invalid_source_document", subject(d)}}};
  }
}
} // namespace inde::objects::compat

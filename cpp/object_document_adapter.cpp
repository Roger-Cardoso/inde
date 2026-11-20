#include "object_document_adapter.hpp"
#include "inde/application/document_reading_order.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include <glib.h>
#include <new>
#include <stdexcept>
#include <uuid/uuid.h>

namespace inde::objects::compat {
namespace {
struct Refusal { std::string code; };
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
class InputBudget {
public:
  explicit InputBudget(std::size_t cap) : bytes_left_(cap) {}
  void string(const std::string &s) {
    if (s.size() > bytes_left_) refuse("source_string_limit");
    bytes_left_ -= s.size();
    if (s.find('\0') != std::string::npos || !g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr))
      refuse("invalid_source_text");
  }
  void optional(const std::optional<std::string> &s) { if (s) string(*s); }
  template<class Collection> void collection(const Collection &c) {
    if (c.size() > items_left_) refuse("source_item_limit");
    items_left_ -= c.size();
  }
private:
  std::size_t bytes_left_, items_left_{100000};
};
void preflight(const project::Document &d, DocumentReadLimits limits) {
  valid_limits(limits);
  if (!d.paragraphs.enabled) refuse("paragraph_identity_required");
  if (d.paragraphs.rows.size() > limits.paragraphs) refuse("projection_paragraph_limit");
  if (!project::canonical_paragraphs(d.paragraphs)) refuse("canonical_paragraphs_required");
  InputBudget budget(limits.source_string_bytes);
  for (const auto *s : {&d.id, &d.title, &d.content, &d.created_at, &d.updated_at,
                        &d.revision_label, &d.perspective}) budget.string(*s);
  budget.optional(d.editorial_node_id); budget.optional(d.group_id); budget.optional(d.revision_of_id);
  auto formats = [&](const auto &spans) {
    budget.collection(spans);
    for (const auto &f : spans) budget.string(f.value);
  };
  formats(d.formatting);
  budget.collection(d.anchors);
  for (const auto &a : d.anchors) { budget.string(a.id); budget.string(a.label); }
  budget.collection(d.entity_references);
  for (const auto &r : d.entity_references) {
    budget.string(r.id); budget.string(r.entity_id); budget.optional(r.anchor_id); budget.string(r.notes);
  }
  budget.collection(d.inline_commands);
  for (const auto &c : d.inline_commands)
    for (const auto *s : {&c.prefix, &c.keyword, &c.label, &c.content}) budget.string(*s);
  budget.collection(d.paragraphs.rows); budget.collection(d.paragraphs.retired);
  for (const auto &p : d.paragraphs.rows) {
    budget.string(p.id);
    if (!p.content) refuse("canonical_paragraphs_required");
    budget.string(*p.content);
  }
  for (const auto &id : d.paragraphs.retired) budget.string(id);
  budget.collection(d.resmas);
  for (const auto &r : d.resmas) {
    budget.string(r.id); budget.string(r.label); budget.string(r.top_id); budget.collection(r.leaves);
    for (const auto &l : r.leaves) {
      budget.string(l.id); budget.string(l.label);
      if (l.covered) { budget.string(l.covered->text); formats(l.covered->formatting); }
    }
  }
  budget.collection(d.visibility_profiles);
  for (const auto &p : d.visibility_profiles) {
    budget.string(p.id); budget.string(p.name);
    for (const auto *rules : {&p.include, &p.exclude}) {
      budget.collection(*rules);
      for (const auto &r : *rules) budget.string(r.key);
    }
  }
  budget.collection(d.semantics.anchors);
  for (const auto &a : d.semantics.anchors)
    for (const auto *s : {&a.anchor_id, &a.selected_hash, &a.before_hash, &a.after_hash, &a.reason}) budget.string(*s);
  budget.collection(d.semantics.annotations);
  for (const auto &a : d.semantics.annotations) {
    for (const auto *s : {&a.id, &a.anchor_id, &a.type_key, &a.status, &a.origin, &a.source,
                          &a.notes, &a.opaque_payload, &a.created_at, &a.updated_at}) budget.string(*s);
    budget.collection(a.fields);
    for (const auto &[key, value] : a.fields) {
      budget.string(key); budget.collection(value.values);
      for (const auto &s : value.values) budget.string(s);
    }
    budget.collection(a.fictional_time);
    for (const auto &t : a.fictional_time) {
      for (const auto *s : {&t.id, &t.fictional_axis_id, &t.fictional_start_point_id,
                            &t.origin, &t.source, &t.notes, &t.created_at}) budget.string(*s);
      budget.optional(t.fictional_end_point_id);
    }
    budget.collection(a.dates);
    for (const auto &date : a.dates)
      for (const auto *s : {&date.id, &date.reference_id, &date.role, &date.notes, &date.created_at}) budget.string(*s);
  }
  budget.collection(d.semantics.relations);
  for (const auto &r : d.semantics.relations)
    for (const auto *s : {&r.id, &r.source_annotation_id, &r.target_annotation_id,
                          &r.type_key, &r.origin, &r.notes, &r.created_at}) budget.string(*s);
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

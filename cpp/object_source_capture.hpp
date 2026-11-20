#pragma once
// Internal bounded-input accounting shared by private source captures.
#include "inde/project/writing.hpp"
#include <glib.h>
#include <string>

namespace inde::objects::compat::detail {
struct Refusal { std::string code; };
[[noreturn]] inline void refuse(const std::string &code) { throw Refusal{code}; }
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
inline void account_document(const project::Document &d, InputBudget &budget) {
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
    if (p.content) budget.string(*p.content);
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
} // namespace inde::objects::compat::detail

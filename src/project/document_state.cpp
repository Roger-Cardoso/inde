#include "inde/project/document_state.hpp"
#include <algorithm>
#include <tuple>
namespace inde::project {
namespace {
template<class T, class Key> bool same(const std::vector<T> &a, const std::vector<T> &b, Key key) {
  if (a.size() != b.size()) return false;
  using K = decltype(key(std::declval<T>()));
  std::vector<K> left, right;
  for (const auto &item : a) left.push_back(key(item));
  for (const auto &item : b) right.push_back(key(item));
  std::sort(left.begin(), left.end()); std::sort(right.begin(), right.end());
  return left == right;
}
}
bool same_document_state(const Document &a, const Document &b, bool timestamps) {
  return std::tie(a.id, a.title, a.content, a.editorial_node_id, a.word_goal, a.group_id,
                  a.purpose, a.revision_of_id, a.revision_label, a.perspective) ==
         std::tie(b.id, b.title, b.content, b.editorial_node_id, b.word_goal, b.group_id,
                  b.purpose, b.revision_of_id, b.revision_label, b.perspective) &&
    (!timestamps || std::tie(a.created_at, a.updated_at) == std::tie(b.created_at, b.updated_at)) &&
    a.inline_commands == b.inline_commands &&
    same(a.formatting, b.formatting, [](const auto &v) { return std::tuple(v.style, v.start_offset, v.end_offset, v.value); }) &&
    same(a.anchors, b.anchors, [](const auto &v) { return std::tuple(v.id, v.label, v.start_offset, v.end_offset); }) &&
    same(a.entity_references, b.entity_references, [](const auto &v) { return std::tuple(v.id, v.entity_id, v.anchor_id, v.notes); }) && same_semantics(a.semantics,b.semantics);
}
}

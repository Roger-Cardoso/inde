#include "object_commands.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace inde::objects {
static_assert(std::is_nothrow_move_assignable_v<Composition>);
static_assert(std::is_nothrow_move_constructible_v<Composition>);
namespace {
// Bound command payloads before allocating selection/replacement indexes.
bool bounded_commands(const std::vector<Command> &commands) {
  std::size_t references{}, bytes{};
  bool valid = true;
  auto str = [&](const std::string &s) {
    constexpr std::size_t cap = 32 * 1024 * 1024;
    if (s.size() > cap - bytes) valid = false;
    else bytes += s.size();
  };
  auto list = [&](const auto &values) {
    constexpr std::size_t cap = 400000;
    if (values.size() > cap - references) { valid = false; return; }
    references += values.size();
    for (const auto &v : values) {
      if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) str(v);
      else str(v.occurrence_id);
    }
  };
  for (const auto &command : commands) {
    if (command.valueless_by_exception()) return false;
    std::visit([&](const auto &v) {
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, ReorderOccurrences>) {
        str(v.layout_id); list(v.occurrence_ids); if (v.before) str(*v.before);
      } else if constexpr (std::is_same_v<T, TransferOccurrences>) {
        str(v.destination_layout_id); list(v.occurrence_ids); list(v.replacements);
        if (v.before) str(*v.before);
      } else if constexpr (std::is_same_v<T, SetFrame>) str(v.occurrence_id);
      else if constexpr (std::is_same_v<T, SetPaintOrder>) { str(v.layout_id); list(v.order); }
      else { str(v.layout_id); list(v.replacements); }
    }, command);
    if (!valid) return false;
  }
  return true;
}

struct Rejected { Diagnostic diagnostic; };
[[noreturn]] void reject(const std::string &code, const std::string &id) {
  throw Rejected{{code, id}};
}
bool spatial(const Layout &l) {
  return l.kind == LayoutKind::Overlay || l.kind == LayoutKind::Freeform;
}
Layout &layout(Composition &c, const std::string &id) {
  auto it = std::find_if(c.layouts.begin(), c.layouts.end(),
                         [&](const auto &l) { return l.id == id; });
  if (it == c.layouts.end()) reject("missing_layout", id);
  return *it;
}
Occurrence &occurrence(Composition &c, const std::string &id) {
  auto it = std::find_if(c.occurrences.begin(), c.occurrences.end(),
                         [&](const auto &o) { return o.id == id; });
  if (it == c.occurrences.end()) reject("missing_occurrence", id);
  return *it;
}
Layout &parent(Composition &c, const std::string &id) {
  for (auto &l : c.layouts)
    if (std::find(l.children.begin(), l.children.end(), id) != l.children.end())
      return l;
  reject("missing_occurrence", id);
}
void freeze_paint(Layout &l) {
  if (spatial(l) && l.paint_order.empty()) l.paint_order = l.children;
}
bool permutation(const std::vector<std::string> &a, const std::vector<std::string> &b) {
  const std::unordered_set<std::string> ids(a.begin(), a.end());
  const std::unordered_set<std::string> other(b.begin(), b.end());
  return a.size() == b.size() && other.size() == b.size() && ids == other;
}
std::vector<std::string> selection(const Layout &source, const std::vector<std::string> &ids) {
  if (ids.empty()) reject("empty_selection", source.id);
  const std::unordered_set<std::string> chosen(ids.begin(), ids.end());
  if (chosen.size() != ids.size()) reject("duplicate_selection", source.id);
  std::vector<std::string> ordered;
  for (const auto &id : source.children)
    if (chosen.contains(id)) ordered.push_back(id);
  if (ordered.size() != ids.size()) reject("selection_outside_source", source.id);
  return ordered;
}
void insert(Layout &dest, const std::vector<std::string> &ordered,
            const std::optional<std::string> &before) {
  auto position = dest.children.end();
  if (before) {
    position = std::find(dest.children.begin(), dest.children.end(), *before);
    if (position == dest.children.end()) reject("invalid_destination_anchor", *before);
  }
  dest.children.insert(position, ordered.begin(), ordered.end());
}
void remove(std::vector<std::string> &from, const std::vector<std::string> &ids) {
  const std::unordered_set<std::string> chosen(ids.begin(), ids.end());
  std::erase_if(from, [&](const auto &id) { return chosen.contains(id); });
}
void frames(Composition &c, const std::vector<std::string> &ids, FramePolicy policy,
            const std::vector<FrameAssignment> &replacements) {
  switch (policy) {
    case FramePolicy::Preserve:
    case FramePolicy::Clear:
      if (!replacements.empty()) reject("unexpected_frame_assignments", c.id);
      break;
    case FramePolicy::Replace: break;
    default: reject("unknown_frame_policy", c.id);
  }
  std::unordered_map<std::string, Frame> values;
  for (const auto &r : replacements)
    if (!values.emplace(r.occurrence_id, r.frame).second)
      reject("duplicate_frame_assignment", r.occurrence_id);
  if (policy == FramePolicy::Replace && values.size() != ids.size())
    reject("incomplete_frame_assignments", c.id);
  // A single index avoids quadratic updates for large selections.
  std::unordered_map<std::string, Occurrence *> occurrences;
  for (auto &o : c.occurrences) occurrences.emplace(o.id, &o);
  for (const auto &id : ids) {
    auto &o = *occurrences.at(id);
    if (policy == FramePolicy::Clear) o.frame.reset();
    if (policy == FramePolicy::Replace) {
      if (!values.contains(id)) reject("incomplete_frame_assignments", id);
      o.frame = values.at(id);
    }
  }
}
void execute(Composition &c, const ReorderOccurrences &command) {
  auto &source = layout(c, command.layout_id);
  const auto ordered = selection(source, command.occurrence_ids);
  auto reordered = source;
  remove(reordered.children, ordered);
  insert(reordered, ordered, command.before);
  if (reordered.children == source.children) return;
  // Freeze painting before reading order changes, including implicit order.
  freeze_paint(source);
  source.children = std::move(reordered.children);
}
void execute(Composition &c, const TransferOccurrences &command) {
  if (command.occurrence_ids.empty()) reject("empty_selection", c.id);
  auto &source = parent(c, command.occurrence_ids.front());
  auto &dest = layout(c, command.destination_layout_id);
  if (source.id == dest.id) reject("transfer_same_layout", dest.id);
  const auto ordered = selection(source, command.occurrence_ids);
  freeze_paint(source); freeze_paint(dest);
  remove(source.children, ordered);
  if (spatial(source)) remove(source.paint_order, ordered);
  insert(dest, ordered, command.before);
  // New placements are painted on top in source reading order; no coordinate inference.
  if (spatial(dest)) dest.paint_order.insert(dest.paint_order.end(), ordered.begin(), ordered.end());
  frames(c, ordered, command.frames, command.replacements);
}
void execute(Composition &c, const SetFrame &command) {
  if (!spatial(parent(c, command.occurrence_id)))
    reject("frame_requires_spatial_layout", command.occurrence_id);
  occurrence(c, command.occurrence_id).frame = command.frame;
}
void execute(Composition &c, const SetPaintOrder &command) {
  auto &l = layout(c, command.layout_id);
  if (!spatial(l)) reject("paint_requires_spatial_layout", l.id);
  if (!permutation(l.children, command.order)) reject("invalid_paint_order", l.id);
  l.paint_order = command.order;
}
void execute(Composition &c, const ChangeLayout &command) {
  auto &l = layout(c, command.layout_id);
  if (l.kind == command.kind && l.columns == command.columns &&
      command.frames == FramePolicy::Preserve && command.replacements.empty()) return;
  freeze_paint(l);
  l.kind = command.kind; l.columns = command.columns;
  frames(c, l.children, command.frames, command.replacements);
  if (spatial(l)) freeze_paint(l);
  else l.paint_order.clear();
}
// Conservative retained storage accounting (not RSS). Count capacity so caller
// reserves cannot bypass the budget. Saturation is deliberately over budget.
std::size_t weight(const Composition &c) {
  std::size_t n = sizeof(c);
  auto add = [&](std::size_t amount) {
    const auto cap = std::numeric_limits<std::size_t>::max();
    n = amount > cap - n ? cap : n + amount;
  };
  auto vector = [&](const auto &v) {
    using Value = typename std::decay_t<decltype(v)>::value_type;
    const auto cap = std::numeric_limits<std::size_t>::max();
    add(v.capacity() > cap / sizeof(Value) ? cap : v.capacity() * sizeof(Value));
  };
  auto str = [&](const std::string &s) { add(s.capacity()); };
  str(c.id); str(c.root_layout_id);
  vector(c.objects); vector(c.occurrences); vector(c.resources); vector(c.layouts);
  for (const auto &o : c.objects) {
    str(o.id); if (o.resource_id) str(*o.resource_id); if (o.layout_id) str(*o.layout_id);
  }
  for (const auto &o : c.occurrences) { str(o.id); str(o.object_id); }
  for (const auto &r : c.resources) { str(r.id); str(r.sha256); str(r.media_type); }
  for (const auto &l : c.layouts) {
    str(l.id); vector(l.children); vector(l.paint_order);
    for (const auto &id : l.children) str(id);
    for (const auto &id : l.paint_order) str(id);
  }
  return n;
}
} // namespace

CompositionEditor::CompositionEditor(Composition initial, std::size_t budget,
                                     std::size_t limit)
    : state_(std::move(initial)), budget_(budget), limit_(limit) {
  if (!validate(state_).empty()) throw std::invalid_argument("Invalid OBJ-01 composition");
}
std::size_t CompositionEditor::history_bytes() const {
  std::size_t n{};
  const auto cap = std::numeric_limits<std::size_t>::max();
  for (const auto *history : {&undo_, &redo_})
    for (const auto &c : *history) {
      const auto amount = weight(c);
      n = amount > cap - n ? cap : n + amount;
    }
  return n;
}
void CompositionEditor::trim_history() {
  while (undo_.size() + redo_.size() > limit_ || history_bytes() > budget_) {
    // Preserve nearest history first in either direction.
    if (!undo_.empty()) undo_.erase(undo_.begin());
    else if (!redo_.empty()) redo_.erase(redo_.begin());
    else break;
  }
}
CommandResult CompositionEditor::apply(std::uint64_t expected, const std::vector<Command> &commands) {
  if (expected != revision_) return {false, false, revision_, {{"revision_conflict", state_.id}}};
  if (commands.empty()) return {true, false, revision_, {}};
  if (commands.size() > 4096 || !bounded_commands(commands)) return {false, false, revision_, {{"command_limit", state_.id}}};
  Composition candidate = state_;
  try {
    for (const auto &command : commands) {
      if (command.valueless_by_exception()) reject("invalid_command", state_.id);
      std::visit([&](const auto &v) { execute(candidate, v); }, command);
      auto errors = validate(candidate);
      if (!errors.empty()) return {false, false, revision_, std::move(errors)};
    }
  } catch (const Rejected &error) {
    return {false, false, revision_, {error.diagnostic}};
  }
  if (candidate == state_) return {true, false, revision_, {}};
  if (revision_ == std::numeric_limits<std::uint64_t>::max())
    return {false, false, revision_, {{"revision_exhausted", state_.id}}};
  // All potentially throwing work precedes the commit point.
  if (limit_ > 0 && budget_ > 0) undo_.push_back(state_);
  state_ = std::move(candidate);
  redo_.clear();
  ++revision_;
  trim_history();
  return {true, true, revision_, {}};
}
CommandResult CompositionEditor::replay(std::uint64_t expected, bool forward) {
  if (expected != revision_) return {false, false, revision_, {{"revision_conflict", state_.id}}};
  auto &from = forward ? redo_ : undo_;
  auto &to = forward ? undo_ : redo_;
  if (from.empty()) return {false, false, revision_, {{"history_empty", state_.id}}};
  if (revision_ == std::numeric_limits<std::uint64_t>::max())
    return {false, false, revision_, {{"revision_exhausted", state_.id}}};
  to.push_back(state_); // Strong exception guarantee before mutation.
  state_ = std::move(from.back());
  from.pop_back();
  ++revision_;
  trim_history();
  return {true, true, revision_, {}};
}
CommandResult CompositionEditor::undo(std::uint64_t expected) { return replay(expected, false); }
CommandResult CompositionEditor::redo(std::uint64_t expected) { return replay(expected, true); }
} // namespace inde::objects

#include "object_commands.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace o = inde::objects;
namespace {
int failures{};
void check(bool b, const std::string &label) {
  if (!b) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
bool code(const o::CommandResult &r, const std::string &s) {
  return std::any_of(r.diagnostics.begin(), r.diagnostics.end(),
                     [&](const auto &d) { return d.code == s; });
}
o::Composition sample(o::LayoutKind kind = o::LayoutKind::Stack) {
  o::Composition c;
  c.id = "composition"; c.root_layout_id = "root";
  c.objects = {{"p", o::ObjectKind::Paragraph, {}, {}},
               {"container", o::ObjectKind::Container, {}, "nested"}};
  c.occurrences = {{"a", "p", {}}, {"b", "p", {}}, {"c", "p", {}},
                   {"group", "container", {}}, {"d", "p", {}}};
  c.layouts = {{"root", kind, kind == o::LayoutKind::Grid ? 2U : 0U, {"a", "b", "c", "group"}},
               {"nested", o::LayoutKind::Row, 0, {"d"}}};
  if (kind == o::LayoutKind::Freeform)
    for (std::size_t i = 0; i < 4; ++i) c.occurrences[i].frame = o::Frame{0, 0, 10, 10};
  return c;
}
o::CommandResult run(o::CompositionEditor &e, o::Command c) {
  return e.apply(e.revision(), {std::move(c)});
}
void unchanged_rejection(o::CompositionEditor &e, o::Command c, const std::string &label) {
  const auto before = e.current(); const auto rev = e.revision();
  const auto bytes = e.history_bytes(); const bool undo = e.can_undo(), redo = e.can_redo();
  auto r = run(e, std::move(c));
  check(!r.accepted && !r.changed && !r.diagnostics.empty() && e.current() == before &&
        e.revision() == rev && e.history_bytes() == bytes && e.can_undo() == undo &&
        e.can_redo() == redo, label);
}
void reorder_arrangements() {
  for (const auto kind : {o::LayoutKind::Stack, o::LayoutKind::Row, o::LayoutKind::Grid,
                          o::LayoutKind::Overlay, o::LayoutKind::Freeform}) {
    const auto initial = sample(kind); o::CompositionEditor e(initial);
    auto r = run(e, o::ReorderOccurrences{"root", {"c", "a"}, "group"});
    check(r.accepted && r.changed && e.current().layouts[0].children ==
          std::vector<std::string>{"b", "a", "c", "group"}, "multi reorder in source order for every arrangement");
    check(e.current().objects == initial.objects && e.current().occurrences == initial.occurrences,
          "reorder preserves objects, occurrence IDs and frames");
    if (kind == o::LayoutKind::Overlay || kind == o::LayoutKind::Freeform)
      check(e.current().layouts[0].paint_order == initial.layouts[0].children, "reading reorder preserves implicit painting");
    check(e.undo(e.revision()).accepted && e.current() == initial, "exact undo of reorder");
    check(e.redo(e.revision()).accepted && e.revision() == 3, "redo monotonic revision");
  }
}
void transfer_and_identity() {
  o::CompositionEditor e(sample()); const auto initial = e.current();
  auto r = run(e, o::TransferOccurrences{{"c", "a"}, "nested", "d"});
  check(r.accepted && e.current().layouts[0].children == std::vector<std::string>{"b", "group"} &&
        e.current().layouts[1].children == std::vector<std::string>{"a", "c", "d"}, "atomic multi transfer preserves source order");
  check(e.current().objects == initial.objects && e.current().occurrences == initial.occurrences,
        "transfer does not move other uses of shared object");
  check(e.undo(e.revision()).accepted && e.current() == initial, "transfer undo exact");
}
void cycles_and_bad_destinations() {
  o::CompositionEditor e(sample());
  unchanged_rejection(e, o::TransferOccurrences{{"group"}, "nested", {}}, "self descendant transfer rejected atomically");
  unchanged_rejection(e, o::TransferOccurrences{{"a"}, "missing", {}}, "missing target");
  unchanged_rejection(e, o::TransferOccurrences{{"a"}, "root", {}}, "same parent transfer explicit rejection");
  unchanged_rejection(e, o::TransferOccurrences{{"a", "d"}, "nested", {}}, "mixed parent selection");
  unchanged_rejection(e, o::TransferOccurrences{{"missing"}, "nested", {}}, "missing source");
  unchanged_rejection(e, o::ReorderOccurrences{"root", {"a"}, "a"}, "anchor in selection");
  unchanged_rejection(e, o::ReorderOccurrences{"root", {"a", "a"}, {}}, "duplicate selection");
  unchanged_rejection(e, o::ReorderOccurrences{"root", {}, {}}, "empty selection");
  unchanged_rejection(e, o::ReorderOccurrences{"root", {"a"}, "d"}, "anchor in different parent");
}
void geometry_policies() {
  o::CompositionEditor e(sample(o::LayoutKind::Freeform));
  unchanged_rejection(e, o::TransferOccurrences{{"a"}, "nested", {}}, "no silent frame discard moving to flow");
  check(run(e, o::TransferOccurrences{{"a"}, "nested", {}, o::FramePolicy::Clear}).accepted,
        "explicit frame discard permitted");
  check(!e.current().occurrences[0].frame, "frame cleared only explicitly");
  unchanged_rejection(e, o::TransferOccurrences{{"a"}, "root", {}}, "freeform requires explicit destination frame");
  check(run(e, o::TransferOccurrences{{"a"}, "root", "b", o::FramePolicy::Replace,
         {{"a", {-1, 2, 3, 4}}}}).accepted, "explicit target local geometry");
  check(e.current().occurrences[0].frame == o::Frame{-1, 2, 3, 4}, "target coordinates exact");
  unchanged_rejection(e, o::TransferOccurrences{{"a"}, "nested", {}, o::FramePolicy::Replace,
                      {{"a", {}}}}, "absolute frame forbidden in flow");
  unchanged_rejection(e, o::TransferOccurrences{{"b"}, "nested", {}, o::FramePolicy::Clear,
                      {{"b", {}}}}, "unexpected replacement payload");
  unchanged_rejection(e, o::TransferOccurrences{{"b"}, "nested", {}, static_cast<o::FramePolicy>(99)}, "unknown frame policy");
}
void frames_and_layers() {
  o::CompositionEditor e(sample(o::LayoutKind::Overlay)); const auto initial = e.current();
  check(run(e, o::SetFrame{"a", {-5, 10, 20, 30}}).accepted, "move and resize frame");
  check(e.current().layouts == initial.layouts && e.current().occurrences[1] == initial.occurrences[1],
        "geometry never reorders or moves shared object occurrences");
  check(run(e, o::SetPaintOrder{"root", {"group", "c", "a", "b"}}).accepted, "paint permutation");
  check(e.current().layouts[0].children == initial.layouts[0].children, "paint order independent of reading");
  unchanged_rejection(e, o::SetPaintOrder{"root", {"a", "a", "b", "group"}}, "duplicate paint entry");
  unchanged_rejection(e, o::SetPaintOrder{"root", {"a"}}, "missing paint entries");
  unchanged_rejection(e, o::SetPaintOrder{"nested", {"d"}}, "no paint order on flow");
  for (double invalid : {0., -1., std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    unchanged_rejection(e, o::SetFrame{"a", {0, 0, invalid, 1}}, "bad frame dimensions");
  unchanged_rejection(e, o::SetFrame{"a", {std::numeric_limits<double>::max(), 0,
                      std::numeric_limits<double>::max(), 1}}, "frame bounding edge overflow");
  unchanged_rejection(e, o::SetFrame{"a", {0, std::numeric_limits<double>::quiet_NaN(), 1, 1}}, "frame NaN position");
  unchanged_rejection(e, o::SetFrame{"d", {}}, "no frame on flow");
  unchanged_rejection(e, o::SetFrame{"missing", {}}, "missing occurrence frame");
}
void change_arrangements() {
  o::CompositionEditor e(sample());
  check(run(e, o::ChangeLayout{"root", o::LayoutKind::Grid, 3}).accepted, "flow to grid");
  unchanged_rejection(e, o::ChangeLayout{"root", o::LayoutKind::Grid, 0}, "grid needs columns");
  unchanged_rejection(e, o::ChangeLayout{"root", o::LayoutKind::Row, 2}, "columns only grid");
  unchanged_rejection(e, o::ChangeLayout{"root", o::LayoutKind::Freeform}, "no invented geometry");
  unchanged_rejection(e, o::ChangeLayout{"root", static_cast<o::LayoutKind>(90)}, "unknown layout");
  std::vector<o::FrameAssignment> frames;
  for (const auto &id : e.current().layouts[0].children) frames.push_back({id, {0, 0, 100, 100}});
  check(run(e, o::ChangeLayout{"root", o::LayoutKind::Freeform, 0, o::FramePolicy::Replace, frames}).accepted, "explicit flow to spatial");
  unchanged_rejection(e, o::ChangeLayout{"root", o::LayoutKind::Stack}, "no silent geometry loss on layout change");
  check(run(e, o::ChangeLayout{"root", o::LayoutKind::Stack, 0, o::FramePolicy::Clear}).accepted,
        "explicit spatial to flow conversion");
  check(e.current() == sample(), "round trip preserves identity and hierarchy");
}
void frame_assignment_errors() {
  o::CompositionEditor e(sample());
  unchanged_rejection(e, o::ChangeLayout{"root", o::LayoutKind::Freeform, 0, o::FramePolicy::Replace, {}}, "incomplete assignment");
  unchanged_rejection(e, o::TransferOccurrences{{"a"}, "nested", {}, o::FramePolicy::Replace,
                       {{"b", {}}}}, "foreign replacement ID");
  unchanged_rejection(e, o::ChangeLayout{"root", o::LayoutKind::Freeform, 0, o::FramePolicy::Replace,
                       {{"a", {}}, {"a", {}}}}, "duplicate assignment");
}
void transactions_and_conflicts() {
  o::CompositionEditor e(sample()); const auto initial = e.current();
  auto r = e.apply(0, {o::ReorderOccurrences{"root", {"b"}, "a"}, o::SetFrame{"a", {}}});
  check(!r.accepted && e.current() == initial && !e.can_undo() && e.revision() == 0, "failed batch rolls back every command");
  check(e.apply(0, {o::ReorderOccurrences{"root", {"b"}, "a"},
                     o::TransferOccurrences{{"c"}, "nested", {}}}).accepted, "successful batch");
  check(e.revision() == 1 && e.undo(1).accepted && e.current() == initial, "batch one undo unit");
  r = e.apply(0, {o::ReorderOccurrences{"root", {"a"}, {}}});
  check(!r.accepted && code(r, "revision_conflict") && e.current() == initial && e.can_redo(), "stale write after undo rejected, redo preserved");
  check(!e.redo(1).accepted && e.can_redo(), "stale redo does not consume history");
  check(run(e, o::ReorderOccurrences{"root", {"a"}, {}}).accepted && !e.can_redo(), "new edit drops redo");
}
void noops_and_budget() {
  o::CompositionEditor e(sample(o::LayoutKind::Overlay));
  auto r = run(e, o::ReorderOccurrences{"root", {"a"}, "b"});
  check(r.accepted && !r.changed && e.revision() == 0 && !e.can_undo(), "no-op spatial reorder leaves implicit painting untouched");
  check(e.apply(0, {}).accepted && e.revision() == 0, "empty batch no-op");
  check(run(e, o::ReorderOccurrences{"root", {"a"}, {}}).accepted, "edit before redo no-op test");
  check(e.undo(e.revision()).accepted, "undo before no-op");
  r = run(e, o::ReorderOccurrences{"root", {"a"}, "b"});
  check(r.accepted && !r.changed && e.can_redo(), "no-op preserves redo");
  o::CompositionEditor tiny(sample(), 1);
  check(run(tiny, o::ReorderOccurrences{"root", {"a"}, {}}).accepted &&
        !tiny.can_undo() && tiny.history_bytes() == 0, "oversized history evicted within byte budget");
  o::CompositionEditor disabled(sample(), 0);
  check(run(disabled, o::ReorderOccurrences{"root", {"a"}, {}}).accepted && !disabled.can_undo(), "zero budget history disabled");
  o::CompositionEditor limited(sample(), 1024 * 1024, 1);
  check(run(limited, o::ReorderOccurrences{"root", {"a"}, {}}).accepted, "history entry one");
  check(run(limited, o::ReorderOccurrences{"root", {"b"}, {}}).accepted, "history entry two");
  check(limited.undo(limited.revision()).accepted && !limited.can_undo() && limited.can_redo(), "entry bound with undo redo");
}
void limits_and_invalid_initial() {
  auto c = sample();
  for (auto limits : {o::ValidationLimits{1, 400000, 100000}, o::ValidationLimits{100000, 1, 100000},
                      o::ValidationLimits{100000, 400000, 1}}) {
    auto errors = o::validate(c, limits);
    check(errors.size() == 1 && errors[0].code == "input_limit", "preflight rejects entity/reference/string cap");
  }
  c.layouts[0].paint_order = {"a"};
  check(!o::validate(c).empty(), "validator checks paint independently");
  try { o::CompositionEditor invalid(c); check(false, "invalid initial should throw"); }
  catch (const std::invalid_argument &) {}
  o::CompositionEditor e(sample());
  std::vector<o::Command> too_many(4097, o::ReorderOccurrences{"root", {"a"}, "b"});
  check(code(e.apply(0, too_many), "command_limit") && e.revision() == 0, "batch size capped");
}
void randomized_sequences() {
  // Independent vector oracle, fixed seed; reused paragraph object catches object-ID movement.
  std::mt19937 random(0x01B1EC7U);
  o::Composition c;
  c.id = "random"; c.root_layout_id = "root";
  c.objects = {{"text", o::ObjectKind::Paragraph, {}, {}}};
  c.layouts = {{"root", o::LayoutKind::Grid, 7, {}}};
  for (int i = 0; i < 60; ++i) {
    auto id = "occ" + std::to_string(i);
    c.occurrences.push_back({id, "text", {}}); c.layouts[0].children.push_back(id);
  }
  o::CompositionEditor e(c, 1024 * 1024);
  auto oracle = c.layouts[0].children;
  for (int i = 0; i < 4000; ++i) {
    const auto from = static_cast<std::size_t>(random() % oracle.size());
    const auto to = static_cast<std::size_t>(random() % oracle.size());
    const auto id = oracle[from];
    const auto before = e.current();
    oracle.erase(oracle.begin() + static_cast<std::ptrdiff_t>(from));
    const std::optional<std::string> anchor = to < oracle.size() ? std::optional{oracle[to]} : std::nullopt;
    oracle.insert(oracle.begin() + static_cast<std::ptrdiff_t>(to), id);
    auto r = run(e, o::ReorderOccurrences{"root", {id}, anchor});
    check(r.accepted && o::validate(e.current()).empty() && e.current().layouts[0].children == oracle &&
          e.current().occurrences == c.occurrences && e.current().objects == c.objects,
          "seeded reorder sequence matches independent oracle");
    if (r.changed && i % 17 == 0) {
      const auto after = e.current();
      check(e.undo(e.revision()).accepted && e.current() == before, "generated exact undo");
      check(e.redo(e.revision()).accepted && e.current() == after, "generated exact redo");
    }
  }
  check(e.history_bytes() <= 1024 * 1024, "random history within budget");
}
void randomized_graphs() {
  std::mt19937 random(0xA0D17U);
  for (int trial = 0; trial < 250; ++trial) {
    auto c = sample();
    // Valid tree of variable breadth/depth. Each container owns one layout.
    for (int i = 0; i < 40; ++i) {
      const auto id = std::to_string(i);
      const auto parent = static_cast<std::size_t>(random() % c.layouts.size());
      c.objects.push_back({"object" + id, o::ObjectKind::Container, {}, "layout" + id});
      c.occurrences.push_back({"use" + id, "object" + id, {}});
      c.layouts[parent].children.push_back("use" + id);
      c.layouts.push_back({"layout" + id, o::LayoutKind::Stack, 0, {}});
    }
    check(o::validate(c).empty(), "generated rooted tree valid");
    o::CompositionEditor e(c);
    unchanged_rejection(e, o::TransferOccurrences{{"use0"}, "layout0", {}}, "generated descendant cycle atomic");
    auto bad = c;
    bad.layouts.back().children.push_back("a");
    check(!o::validate(bad).empty(), "generated graph multiple placement rejected");
    bad = c; bad.layouts[0].children.push_back("unknown");
    check(!o::validate(bad).empty(), "generated dangling child rejected");
    bad = c; bad.objects.back().layout_id = "root";
    check(!o::validate(bad).empty(), "generated cycle or disconnection rejected");
  }
}
void nested_subtrees_and_deep_operations() {
  auto c = sample();
  c.objects.push_back({"holder", o::ObjectKind::Container, {}, "target"});
  c.occurrences.push_back({"holder-use", "holder", {}});
  c.layouts[0].children.push_back("holder-use");
  c.layouts.push_back({"target", o::LayoutKind::Stack, 0, {}});
  o::CompositionEditor e(c);
  check(run(e, o::TransferOccurrences{{"group"}, "target", {}}).accepted &&
        e.current().layouts[1] == c.layouts[1], "subtree transfer preserves entire nested layout");
  unchanged_rejection(e, o::TransferOccurrences{{"holder-use"}, "nested", {}}, "indirect descendant cycle rejected");
  check(e.undo(e.revision()).accepted && e.current() == c, "subtree transfer exact undo");
  o::Composition deep;
  deep.id = "deep"; deep.root_layout_id = "layout0";
  for (int i = 0; i <= 7000; ++i) {
    const auto suffix = std::to_string(i);
    deep.layouts.push_back({"layout" + suffix, o::LayoutKind::Stack, 0, {}});
    if (i == 7000) continue;
    deep.objects.push_back({"object" + suffix, o::ObjectKind::Container, {}, "layout" + std::to_string(i + 1)});
    deep.occurrences.push_back({"use" + suffix, "object" + suffix, {}});
    deep.layouts.back().children.push_back("use" + suffix);
  }
  o::CompositionEditor de(deep);
  unchanged_rejection(de, o::TransferOccurrences{{"use0"}, "layout7000", {}}, "7000 deep cycle rejected without recursion");
  check(run(de, o::TransferOccurrences{{"use6999"}, "layout0", {}}).accepted &&
        de.undo(de.revision()).accepted && de.current() == deep, "7000 deep transfer and undo exact");
}
void randomized_transfers() {
  std::mt19937 random(0x7A45FE2U);
  auto c = sample(o::LayoutKind::Overlay);
  c.layouts[0].paint_order = c.layouts[0].children;
  o::CompositionEditor e(c);
  std::vector<std::string> root{"a", "b", "c"}, nested{"d"};
  // Never select group: oracle's parent tree is intentionally fixed.
  for (int i = 0; i < 1500; ++i) {
    const bool from_root = nested.empty() || (!root.empty() && random() % 2 == 0);
    auto &from = from_root ? root : nested;
    auto &to = from_root ? nested : root;
    const auto at = static_cast<std::size_t>(random() % from.size());
    const auto id = from[at];
    const auto before = e.current();
    auto r = run(e, o::TransferOccurrences{{id}, from_root ? "nested" : "root", {}});
    check(r.accepted && o::validate(e.current()).empty(), "generated transfer valid");
    from.erase(from.begin() + static_cast<std::ptrdiff_t>(at)); to.push_back(id);
    auto actual_root = e.current().layouts[0].children;
    std::erase(actual_root, "group");
    check(actual_root == root && e.current().layouts[1].children == nested &&
          e.current().objects == c.objects && e.current().occurrences == c.occurrences,
          "generated transfers match independent two-parent oracle");
    if (i % 23 == 0) {
      const auto after = e.current();
      check(e.undo(e.revision()).accepted && e.current() == before, "generated transfer exact undo");
      check(e.redo(e.revision()).accepted && e.current() == after, "generated transfer exact redo");
    }
  }
}
void command_payload_limit() {
  o::CompositionEditor e(sample());
  std::vector<std::string> ids(400001, "a");
  unchanged_rejection(e, o::ReorderOccurrences{"root", std::move(ids), {}}, "command adjacency preflight cap");
  check(code(run(e, o::SetFrame{std::string(32 * 1024 * 1024 + 1, 'x'), {}}), "command_limit"),
        "command string preflight cap");
}

} // namespace
int main() {
  reorder_arrangements(); transfer_and_identity(); cycles_and_bad_destinations();
  geometry_policies(); frames_and_layers(); change_arrangements(); frame_assignment_errors();
  transactions_and_conflicts(); noops_and_budget(); limits_and_invalid_initial();
  randomized_sequences(); randomized_graphs();
  nested_subtrees_and_deep_operations(); randomized_transfers(); command_payload_limit();
  if (failures) { std::cerr << failures << " failures\n"; return 1; }
  std::cout << "INDE-OBJ-01: 15 command test groups passed (4000 oracle reorders, 1500 oracle transfers, 250 generated trees, depth 7000)\n";
}

#include "object_composition_adapter.hpp"
#include "object_commands.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

namespace o = inde::objects;
namespace c = inde::objects::compat;
namespace p = inde::project;
namespace e = inde::experimental;
namespace db = inde::persistence;
namespace {
int failures{};
void check(bool yes, const std::string &label) { if (!yes) { ++failures; std::cerr << "FAIL: " << label << '\n'; } }
bool code(const std::vector<o::Diagnostic> &ds, const std::string &s) {
  return std::any_of(ds.begin(), ds.end(), [&](const auto &d) { return d.code == s; });
}
void seal(p::StoredComposition &s) { s.sha256 = db::composition_checksum(db::encode_composition(s.value)); }
p::Document document(std::string text = "Água🙂\nLinha.\n") {
  p::Document d; d.id = p::new_uuid(); d.title = "Origem declarada";
  d.content = std::move(text); d.created_at = d.updated_at = "2026-11-20T16:00:00Z";
  // Deliberately valid without paragraph IDs: a block need not be a paragraph.
  return d;
}
std::string import(p::Composition &c, const p::Document &d, bool place = true) {
  const auto sha = db::composition_checksum(db::encode_document_snapshot(d));
  c.origins.emplace(sha, d);
  const auto block = c.blocks.import_document(d, 0, p::utf8_character_count(d.content));
  c.blocks.assign_origin_snapshot(block, sha);
  return place ? c.blocks.place(block) : block;
}
p::StoredComposition empty() {
  p::StoredComposition s; s.project_id = "10000000-0000-4000-8000-000000000001";
  s.value.name = "Montagem de teste"; s.revision = 3; s.updated_at = "2026-11-20T16:00:00Z";
  seal(s); return s;
}
struct Fixture {
  p::StoredComposition stored;
  std::string live, same, pinned, ref, forked, hidden, layer_use, variant_use, alternative;
  std::string block, old_revision, layer, variant;
};
Fixture fixture() {
  Fixture f; f.stored = empty(); auto &c = f.stored.value;
  auto d = document();
  d.formatting = {{p::DocumentTextStyle::Bold, 0, 4, {}}};
  d.anchors = {{p::new_uuid(), "Marcação explícita", 0, 4}};
  f.live = import(c, d); f.block = c.blocks.occurrence(f.live).block_id;
  f.old_revision = c.blocks.block(f.block).current_revision;
  f.same = c.blocks.place(f.block); f.pinned = c.blocks.place(f.block); c.blocks.pin(f.pinned, f.old_revision);
  f.ref = c.blocks.place(f.block); c.blocks.reference_only(f.ref);
  f.forked = c.blocks.place(f.block); c.blocks.fork(f.forked, f.old_revision);
  c.blocks.revise(f.block, f.old_revision, "Texto aplicado novo🙂\n");
  f.hidden = c.blocks.place(f.block); c.blocks.set_hidden(f.hidden, true);
  f.layer = c.blocks.add_layer("Camada explicitamente declarada");
  f.layer_use = c.blocks.place(f.block); c.blocks.classify(f.layer_use, {}, f.layer);
  f.variant = c.blocks.add_variant("Variante global declarada");
  f.variant_use = c.blocks.place(f.block); c.blocks.classify(f.variant_use, f.variant, {});
  f.alternative = c.blocks.alternative(f.live, "Alternativa da família");
  c.drafts[f.block] = {c.blocks.block(f.block).current_revision, "RASCUNHO PRIVADO NÃO APLICADO"};
  seal(f.stored); return f;
}
std::unique_ptr<c::CompositionReadProjection> read(const p::StoredComposition &s) {
  auto r = c::read_composition(s);
  if (!r.projection) {
    for (const auto &d : r.diagnostics) std::cerr << d.code << ':' << d.subject_id << '\n';
    throw std::runtime_error("Valid composition fixture refused");
  }
  return std::move(r.projection);
}
const c::CompositionOccurrenceBinding &binding(const c::CompositionReadProjection &v, const std::string &source) {
  const auto it = std::find_if(v.occurrences().begin(), v.occurrences().end(), [&](const auto &b) { return b.source_occurrence_id == source; });
  if (it == v.occurrences().end()) throw std::runtime_error("Missing binding");
  return *it;
}
void same_reading(const e::TextProjection &a, const e::TextProjection &b) {
  check(a.text == b.text && a.segments.size() == b.segments.size(), "exact owner reading body/segment count");
  if (a.segments.size() != b.segments.size()) return;
  for (std::size_t i = 0; i < a.segments.size(); ++i) {
    const auto &x = a.segments[i], &y = b.segments[i];
    check(x.placement_id == y.placement_id && x.block_id == y.block_id && x.revision_id == y.revision_id &&
          x.start == y.start && x.end == y.end && x.reference_only == y.reference_only,
          "exact owner reading identities, Unicode offsets and reference width");
    check(x.exact_source.has_value() == y.exact_source.has_value(), "exact-source presence preserved");
    if (x.exact_source && y.exact_source)
      check(x.exact_source->document_id == y.exact_source->document_id &&
            x.exact_source->source_body_sha256 == y.exact_source->source_body_sha256 &&
            x.exact_source->start == y.exact_source->start && x.exact_source->end == y.exact_source->end,
            "exact frozen source range preserved");
  }
}
void modes_and_identity() {
  const auto f = fixture(); auto v = read(f.stored);
  const auto &live = binding(*v, f.live), &same = binding(*v, f.same), &pin = binding(*v, f.pinned);
  const auto &ref = binding(*v, f.ref), &fork = binding(*v, f.forked);
  check(live.source_revision_id == f.stored.value.blocks.block(f.block).current_revision &&
        pin.source_revision_id == f.old_revision && fork.mode == e::PlacementMode::Forked,
        "live follows block current, pinned keeps old revision, fork explicit");
  check(live.object_id == same.object_id && live.object_id != pin.object_id && live.object_id != ref.object_id,
        "same captured version reused; distinct revision/reference gets distinct object");
  check(v->revision_snapshot(pin.occurrence_id)->content == "Água🙂\nLinha.\n" &&
        v->revision_snapshot(fork.occurrence_id)->content == "Água🙂\nLinha.\n",
        "old pinned/fork payloads remain intact");
  check(!v->revision_snapshot(ref.occurrence_id) && !v->incorporated_text(ref.occurrence_id), "reference never incorporates payload");
  check(o::validate(v->graph()).empty() && v->graph().occurrences.size() == f.stored.value.blocks.placements().size(),
        "complete valid occurrence graph");
  for (const auto &object : v->graph().objects)
    check(object.kind == o::ObjectKind::TextBlock || object.kind == o::ObjectKind::TextReference,
          "block/reference never mislabeled as paragraph");
  auto second = read(f.stored);
  check(second->graph() == v->graph() && second->occurrences() == v->occurrences() && second->source() == v->source(), "deterministic capture");
  auto other = f.stored; other.project_id = p::new_uuid(); auto elsewhere = read(other);
  check(binding(*elsewhere, f.live).occurrence_id != live.occurrence_id, "project-qualified placement identity");
}
void reading_and_visibility() {
  auto f = fixture(); auto v = read(f.stored);
  same_reading(v->reading(), f.stored.value.blocks.project(f.stored.value.profile));
  for (const auto &b : v->occurrences()) {
    const auto &source_use = f.stored.value.blocks.occurrence(b.source_occurrence_id);
    check(b.visible == f.stored.value.blocks.visible(source_use, f.stored.value.profile) && b.declared_hidden == source_use.hidden,
          "visibility comes from owner profile, never coordinates or inferred story");
    const auto text = v->incorporated_text(b.occurrence_id);
    check(text.has_value() == (b.visible && !b.reference_only), "hidden/reference excluded from incorporation resolver");
    if (text) check(*text == f.stored.value.blocks.revision(source_use).text, "effective revision text exact");
  }
  const auto &ref = binding(*v, f.ref);
  auto seg = std::find_if(v->reading().segments.begin(), v->reading().segments.end(), [&](const auto &s) { return s.placement_id == ref.source_occurrence_id; });
  check(seg != v->reading().segments.end() && seg->start == seg->end, "visible reference is zero width in exact reading");
  check(!binding(*v, f.hidden).visible && !binding(*v, f.layer_use).visible && !binding(*v, f.variant_use).visible,
        "hidden, excluded layer and global variant remain in graph but not reading");
  auto &c = f.stored.value;
  c.profile.visible_layers.insert(f.layer); c.profile.variant = f.variant;
  const auto &alt = c.blocks.occurrence(f.alternative);
  c.profile.selected_variants[*alt.variant_group] = *alt.variant;
  seal(f.stored); auto selected = read(f.stored);
  check(!binding(*selected, f.live).visible && binding(*selected, f.alternative).visible &&
        binding(*selected, f.layer_use).visible && binding(*selected, f.variant_use).visible,
        "declared family/layer/global selections honored separately");
  same_reading(selected->reading(), c.blocks.project(c.profile));
}
void complete_capture_and_drafts() {
  const auto f = fixture(); const auto before = db::encode_composition(f.stored.value); auto v = read(f.stored);
  check(db::encode_composition(v->source_snapshot().value) == before && db::encode_composition(f.stored.value) == before,
        "all origins/history/profiles/warnings/drafts preserved byte-for-byte through owner codec");
  check(code(v->notices(), "private_drafts_not_applied") && code(v->notices(), "frozen_origins_retained"), "draft and origin boundaries explicit");
  check(v->reading().text.find("RASCUNHO") == std::string::npos &&
        v->source_snapshot().value.drafts.at(f.block).text.find("RASCUNHO") != std::string::npos,
        "durable draft conserved privately without applying");
  for (const auto &[sha, d] : f.stored.value.origins)
    check(db::encode_document_snapshot(v->source_snapshot().value.origins.at(sha)) == db::encode_document_snapshot(d), "frozen origin full fidelity");
  check(!v->source_snapshot().value.origins.begin()->second.paragraphs.enabled,
        "no automatic paragraph identity adoption inside composition origin");
}
void independent_snapshot() {
  auto f = fixture(); auto state = f.stored.value.blocks.snapshot();
  auto mutable_doc = std::make_shared<p::Document>(*state.blocks.at(f.block).revisions.back().layers.document);
  state.blocks.at(f.block).revisions.back().layers.document = mutable_doc;
  f.stored.value.blocks = e::TextBlocksLab::restore(std::move(state)); seal(f.stored);
  const auto before = db::encode_composition(f.stored.value); auto v = read(f.stored);
  mutable_doc->title = "Alteração posterior na alocação do chamador";
  f.stored.value.name = "Outro nome"; f.stored.value.drafts[f.block].text = "Outro rascunho";
  check(db::encode_composition(v->source_snapshot().value) == before, "deep capture has no mutable revision aliases");
}
void freshness_and_envelope() {
  auto f = fixture(); auto v = read(f.stored);
  check(v->check_source(f.stored).current, "exact stored envelope fresh");
  auto changed = f.stored; ++changed.revision;
  check(code(v->check_source(changed).diagnostics, "source_composition_revision_changed"), "revision-only conflict even same hash");
  changed = f.stored; changed.updated_at += "Z";
  check(code(v->check_source(changed).diagnostics, "source_composition_snapshot_changed"), "envelope timestamp participates");
  changed = f.stored; changed.value.drafts[f.block].text += " posterior"; seal(changed);
  check(code(v->check_source(changed).diagnostics, "source_composition_snapshot_changed"), "draft-only valid hash change conflict");
  changed = f.stored; changed.value.name += " posterior"; seal(changed);
  check(code(v->check_source(changed).diagnostics, "source_composition_snapshot_changed"), "name-only conflict without relying on timestamp");
  changed = f.stored; changed.value.profile.visible_layers.insert(f.layer); seal(changed);
  check(!v->check_source(changed).current, "profile-only conflict");
  changed = f.stored; changed.sha256 = std::string(64, '0');
  check(code(v->check_source(changed).diagnostics, "composition_hash_mismatch"), "claimed hash must match bytes");
  changed = f.stored; changed.project_id = p::new_uuid();
  check(code(v->check_source(changed).diagnostics, "source_project_conflict"), "foreign project conflict");
  auto other = empty(); other.project_id = f.stored.project_id;
  check(code(v->check_source(other).diagnostics, "source_composition_conflict"), "foreign composition conflict");
}
void refusals_without_partial_graph() {
  const auto f = fixture();
  auto refuses = [](const p::StoredComposition &s, const std::string &expected) {
    auto r = c::read_composition(s); check(!r.projection && code(r.diagnostics, expected), "no partial projection: " + expected);
  };
  auto bad = f.stored; bad.project_id = "not-a-project"; refuses(bad, "invalid_composition_envelope");
  bad = f.stored; bad.revision = 0; refuses(bad, "invalid_composition_envelope");
  bad = f.stored; bad.updated_at.clear(); refuses(bad, "invalid_composition_envelope");
  bad = f.stored; bad.sha256 = std::string(64, 'f'); refuses(bad, "composition_hash_mismatch");
  bad = f.stored; bad.value.origins.clear(); refuses(bad, "invalid_source_composition");
  bad = f.stored; bad.value.drafts[f.block].expected_revision = p::new_uuid(); refuses(bad, "invalid_source_composition");
  bad = f.stored; bad.value.drafts[f.block].text = std::string(1, static_cast<char>(0xff)); refuses(bad, "invalid_source_text");
  bad = f.stored; bad.value.name.push_back('\0'); refuses(bad, "invalid_source_text");
  check(db::encode_composition(f.stored.value) == db::encode_composition(read(f.stored)->source_snapshot().value), "refusals never modify source");
}
void limits_empty_and_boundary() {
  const auto f = fixture(); const c::CompositionReadLimits defaults;
  for (int i = 0; i < 7; ++i) {
    auto l = defaults;
    if (i == 0) l.blocks = 1;
    if (i == 1) l.occurrences = 1;
    if (i == 2) l.revisions = 1;
    if (i == 3) l.source_string_bytes = 1;
    if (i == 4) l.snapshot_bytes = 1;
    if (i == 5) l.blocks = 257;
    if (i == 6) l.occurrences = 0;
    const auto r = c::read_composition(f.stored, l);
    check(!r.projection && r.diagnostics.size() == 1, "each cap fails closed without truncation");
  }
  auto s = empty(); auto v = read(s);
  check(v->graph().occurrences.empty() && v->reading().text.empty(), "empty valid stored composition");
  const auto use = import(s.value, document("")); const auto block = s.value.blocks.occurrence(use).block_id;
  for (int i = 1; i < 2048; ++i) (void)s.value.blocks.place(block);
  seal(s); v = read(s);
  check(v->occurrences().size() == 2048 && v->graph().objects.size() == 1 &&
        v->incorporated_text(v->occurrences()[0].occurrence_id).has_value() &&
        v->incorporated_text(v->occurrences()[0].occurrence_id)->empty(), "2048 placements intact, shared empty payload distinguished from absent");
  auto less = defaults; less.occurrences = 2047;
  check(code(c::read_composition(s, less).diagnostics, "composition_structure_limit"), "one above chosen occurrence limit refused");
}
void library_and_private_origin() {
  auto s = empty(); auto d = document("Corrente");
  d.paragraphs = p::identify_paragraphs(d.content);
  const auto top = p::new_uuid();
  d.resmas = {{p::new_uuid(), "Folhas", top, 0, 8,
                {{top, "Topo", {}}, {p::new_uuid(), "Privada", std::make_shared<const p::DocumentLeafContent>(p::DocumentLeafContent{"FOLHA PRIVADA", {}})}}}};
  const auto block = import(s.value, d, false); seal(s); auto v = read(s);
  check(v->graph().objects.empty() && v->source_snapshot().value.blocks.blocks().contains(block), "unplaced library block remains in source, not invented as occurrence");
  check(v->source_snapshot().value.origins.begin()->second.resmas[0].leaves[1].covered->text == "FOLHA PRIVADA", "private leaf retained in complete origin");
}
void object_commands_independent() {
  const auto f = fixture(); auto v = read(f.stored); const auto original = db::encode_composition(f.stored.value);
  const auto reading = v->reading().text; o::CompositionEditor editor(v->graph());
  auto r = editor.apply(0, {o::ReorderOccurrences{v->graph().root_layout_id, {v->occurrences().back().occurrence_id}, v->occurrences().front().occurrence_id},
                             o::ChangeLayout{v->graph().root_layout_id, o::LayoutKind::Row}});
  check(r.accepted && r.changed && db::encode_composition(v->source_snapshot().value) == original &&
        db::encode_composition(f.stored.value) == original && v->reading().text == reading && v->check_source(f.stored).current,
        "copied graph operations do not reorder source or apply its draft");
  check(editor.undo(editor.revision()).accepted && editor.current() == v->graph(), "object undo exact for block/reference graph");
}
void live_change_preserves_pinned_capture() {
  auto f = fixture(); auto before = read(f.stored);
  const auto old_live = binding(*before, f.live), old_pin = binding(*before, f.pinned);
  f.stored.value.blocks.revise(f.block, old_live.source_revision_id, "Outra revisão aplicada🙂");
  ++f.stored.revision; seal(f.stored); auto after = read(f.stored);
  const auto &live = binding(*after, f.live), &pin = binding(*after, f.pinned);
  check(live.occurrence_id == old_live.occurrence_id && live.object_id != old_live.object_id &&
        pin.object_id == old_pin.object_id && pin.source_revision_id == old_pin.source_revision_id,
        "placement identity stable across live advance; old pin preserves version-qualified object");
  check(!before->check_source(f.stored).current && before->revision_snapshot(old_live.occurrence_id)->content == "Texto aplicado novo🙂\n",
        "old projection frozen and stale after applied revision");
}
void generated_orders() {
  auto f = fixture(); auto initial = read(f.stored); std::mt19937 random(0xC041B1EU);
  std::vector<std::string> order; for (const auto &p : f.stored.value.blocks.placements()) order.push_back(p.id);
  for (int i = 0; i < 200; ++i) {
    std::shuffle(order.begin(), order.end(), random); f.stored.value.blocks.reorder(order); ++f.stored.revision; seal(f.stored);
    const auto bytes = db::encode_composition(f.stored.value); auto v = read(f.stored);
    for (std::size_t j = 0; j < order.size(); ++j) {
      const auto &b = v->occurrences()[j], &old = binding(*initial, order[j]);
      check(b.source_occurrence_id == order[j] && b.occurrence_id == old.occurrence_id &&
            b.object_id == old.object_id && b.source_revision_id == old.source_revision_id,
            "generated source orders preserve occurrence/version identity without using indexes as IDs");
    }
    same_reading(v->reading(), f.stored.value.blocks.project(f.stored.value.profile));
    check(db::encode_composition(f.stored.value) == bytes && v->check_source(f.stored).current, "generated captures remain read-only and fresh");
  }
}
} // namespace
int main() {
  try {
    modes_and_identity(); reading_and_visibility(); complete_capture_and_drafts(); independent_snapshot();
    freshness_and_envelope(); refusals_without_partial_graph(); limits_empty_and_boundary();
    library_and_private_origin(); object_commands_independent(); live_change_preserves_pinned_capture(); generated_orders();
  } catch (const std::exception &error) { std::cerr << "ERROR: " << error.what() << '\n'; return 1; }
  if (failures) { std::cerr << failures << " failures\n"; return 1; }
  std::cout << "INDE-OBJ-01: 11 composition read-adapter groups passed (200 generated orders, 2048 occurrence boundary)\n";
}

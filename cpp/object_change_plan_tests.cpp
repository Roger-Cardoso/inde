#include "object_change_plan.hpp"
#include "inde/application/document_reading_order.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

namespace o = inde::objects;
namespace c = inde::objects::compat;
namespace q = inde::objects::planning;
namespace p = inde::project;
namespace a = inde::application;
namespace db = inde::persistence;
namespace {
int failures{};
const std::string owner = "10000000-0000-4000-8000-000000000001";
void check(bool yes, const std::string &label) { if (!yes) { ++failures; std::cerr << "FAIL: " << label << '\n'; } }
bool code(const std::vector<q::PlanDiagnostic> &ds, const std::string &s) {
  return std::any_of(ds.begin(), ds.end(), [&](const auto &d) { return d.code == s && !d.reason.empty(); });
}
p::Document document(std::string body = "A🙂\nBé\nC\n") {
  p::Document d; d.id = p::new_uuid(); d.title = "Plano sintético"; d.content = std::move(body);
  d.created_at = d.updated_at = "2026-11-20T17:00:00Z";
  d.paragraphs = p::identify_paragraphs(d.content); return d;
}
std::unique_ptr<c::DocumentReadProjection> read(const p::Document &d) {
  auto r = c::read_document(owner, d); if (!r.projection) throw std::runtime_error("Document fixture refused"); return std::move(r.projection);
}
q::ChangeRequest move(q::ChangePurpose purpose, const c::DocumentReadProjection &v, std::size_t from,
                       std::optional<std::size_t> before) {
  return {purpose, {o::ReorderOccurrences{v.graph().root_layout_id, {v.paragraphs()[from].occurrence_id},
                                         before ? std::optional(v.paragraphs()[*before].occurrence_id) : std::nullopt}}};
}
void document_intents_and_engine() {
  auto d = document(); auto v = read(d); const auto original = db::encode_document_snapshot(d);
  auto request = move(q::ChangePurpose::VisualOnly, *v, 1, 0);
  auto visual = q::plan_document_change(*v, owner, d, request, {0, 0});
  request.purpose = q::ChangePurpose::AuthorialReorder;
  auto author = q::plan_document_change(*v, owner, d, request, {0, 0});
  check(visual.plan && author.plan, "same command has two explicit valid intents");
  if (!visual.plan || !author.plan) return;
  check(visual.plan->after_graph() == author.plan->after_graph() && visual.plan->review().graph_changed &&
        !visual.plan->review().source_changed && author.plan->review().source_changed,
        "visual and authorial share graph target but never share source authority");
  check(db::encode_document_snapshot(visual.plan->preview().document) == original, "visual preview preserves entire Document");
  auto expected = a::move_reading_block({d, {0, 0}}, d.paragraphs.rows[1].id, d.paragraphs.rows[0].id);
  check(db::encode_document_snapshot(author.plan->preview().document) == db::encode_document_snapshot(expected.document) &&
        author.plan->preview().selection.cursor == expected.selection.cursor &&
        author.plan->preview().selection.bound == expected.selection.bound, "authorial preview delegates exact owner engine and Unicode selection");
  check(db::encode_document_snapshot(d) == original && author.plan->check_source(owner, d).current, "no in-place source change");
}
void resma_group_boundaries() {
  auto d = document("Antes\nX\nY🙂\nDepois"); const auto top = p::new_uuid();
  d.resmas = {{p::new_uuid(), "Resma", top, 6, 10,
                {{top, "Corrente", {}}, {p::new_uuid(), "Privada", std::make_shared<const p::DocumentLeafContent>(p::DocumentLeafContent{"PRIVADO", {}})}}}};
  auto v = read(d); const auto before = db::encode_document_snapshot(d);
  auto partial = q::plan_document_change(*v, owner, d, move(q::ChangePurpose::AuthorialReorder, *v, 2, 0), {0, 0});
  check(!partial.plan && code(partial.diagnostics, "whole_reading_group_required"), "partial resma selection refused without auto-expansion");
  auto split = q::plan_document_change(*v, owner, d, move(q::ChangePurpose::AuthorialReorder, *v, 0, 2), {0, 0});
  check(!split.plan && code(split.diagnostics, "destination_splits_reading_group"), "destination inside resma refused without snapping");
  q::ChangeRequest whole{q::ChangePurpose::AuthorialReorder, {o::ReorderOccurrences{v->graph().root_layout_id,
                           {v->paragraphs()[2].occurrence_id, v->paragraphs()[1].occurrence_id}, {}}}};
  auto plan = q::plan_document_change(*v, owner, d, whole, {0, 0});
  check(plan.plan && plan.plan->preview().document.content == "Antes\nDepois\nX\nY🙂", "explicit whole group moved in source order");
  if (plan.plan) check(plan.plan->preview().document.resmas[0].id == d.resmas[0].id &&
                       plan.plan->preview().document.resmas[0].leaves == d.resmas[0].leaves, "resma/leaf identities and private text preserved");
  check(db::encode_document_snapshot(d) == before, "refused and accepted plans leave original unchanged");
}
void marked_interval_refusal() {
  auto d = document("A\nB\nC"); d.anchors = {{p::new_uuid(), "A e B", 0, 3}};
  auto v = read(d); const auto before = db::encode_document_snapshot(d);
  const auto r = q::plan_document_change(*v, owner, d, move(q::ChangePurpose::AuthorialReorder, *v, 1, 0), {0, 0});
  check(!r.plan && code(r.diagnostics, "source_text_engine_refused"), "owner refusal for inverted marked span retained with reason");
  check(db::encode_document_snapshot(d) == before, "owner refusal does not expose partial text result");
  auto visual = q::plan_document_change(*v, owner, d, move(q::ChangePurpose::VisualOnly, *v, 1, 0), {0, 0});
  check(visual.plan && !visual.plan->review().source_changed, "visual ordering does not claim it can move marked text");
}
void noops_and_equal_prose() {
  auto d = document("igual\nigual\nC"); auto v = read(d);
  auto noop = q::plan_document_change(*v, owner, d, move(q::ChangePurpose::AuthorialReorder, *v, 0, 1), {1, 1});
  check(noop.plan && !noop.plan->review().source_changed && !noop.plan->review().graph_changed &&
        noop.plan->preview().selection.cursor == 1, "no-op preserves source and selection");
  auto swap = q::plan_document_change(*v, owner, d, move(q::ChangePurpose::AuthorialReorder, *v, 1, 0), {0, 0});
  check(swap.plan && swap.plan->review().source_changed && swap.plan->preview().document.content == d.content &&
        swap.plan->preview_sha256() != swap.plan->source().snapshot_sha256, "identical prose reorder still changes paragraph identity sequence");
}
void unsupported_and_bounded_requests() {
  auto d = document(); auto v = read(d);
  auto refuses = [&](const q::ChangeRequest &r, const std::string &expected) {
    const auto x = q::plan_document_change(*v, owner, d, r, {0, 0});
    check(!x.plan && code(x.diagnostics, expected), "explicit refusal: " + expected);
  };
  refuses({static_cast<q::ChangePurpose>(90), {}}, "unknown_change_purpose");
  refuses({q::ChangePurpose::AuthorialReorder, {o::ChangeLayout{v->graph().root_layout_id, o::LayoutKind::Row}}}, "unsupported_authorial_command");
  refuses({q::ChangePurpose::AuthorialReorder, {o::TransferOccurrences{{v->paragraphs()[0].occurrence_id}, "other", {}}}}, "unsupported_authorial_command");
  auto two = move(q::ChangePurpose::AuthorialReorder, *v, 1, 0); two.commands.push_back(two.commands[0]);
  refuses(two, "document_authorial_batch_unsupported");
  q::ChangeRequest large; large.commands.resize(65, o::ChangeLayout{v->graph().root_layout_id, o::LayoutKind::Row});
  refuses(large, "plan_command_limit");
  refuses({q::ChangePurpose::VisualOnly, {o::SetFrame{std::string(513, 'a'), {}}}}, "plan_request_limit");
  refuses({q::ChangePurpose::VisualOnly, {o::ReorderOccurrences{v->graph().root_layout_id,
            std::vector<std::string>(20001, "x"), {}}}}, "plan_request_limit");
  q::ChangeRequest bytes;
  bytes.commands.resize(64, o::ReorderOccurrences{v->graph().root_layout_id,
                         std::vector<std::string>(200, std::string(512, 'x')), {}});
  refuses(bytes, "plan_request_limit");
  refuses({q::ChangePurpose::VisualOnly, {o::SetFrame{v->paragraphs()[0].occurrence_id, {}}}}, "frame_requires_spatial_layout");
  auto selection = q::plan_document_change(*v, owner, d, {}, {10000, 0});
  check(!selection.plan && code(selection.diagnostics, "invalid_text_selection"), "invalid Unicode selection rejected");
}
void freshness_lifetime_and_undo() {
  auto d = document(); auto v = read(d); const auto before = db::encode_document_snapshot(d);
  auto request = move(q::ChangePurpose::AuthorialReorder, *v, 1, 0);
  auto result = q::plan_document_change(*v, owner, d, request, {0, 0});
  check(bool(result.plan), "document plan before lifetime test"); if (!result.plan) return;
  const auto preview = db::encode_document_snapshot(result.plan->preview().document); v.reset();
  check(db::encode_document_snapshot(result.plan->before().document) == before && result.plan->check_source(owner, d).current,
        "plan owns its preview after projection destruction");
  a::DocumentEditSession session; session.open(result.plan->before());
  check(session.commit(result.plan->preview(), "Ensaio de plano"), "existing in-memory history accepts validated preview");
  const auto undo = session.undo(), redo = session.redo();
  check(undo && redo && db::encode_document_snapshot(undo->document) == before &&
        db::encode_document_snapshot(redo->document) == preview, "existing undo/redo restores exact source layers");
  d.title += " alterado";
  check(!result.plan->check_source(owner, d).current, "plan becomes stale on metadata-only source edit");
  auto fresh_capture = read(result.plan->before().document);
  auto stale = q::plan_document_change(*fresh_capture, owner, d, request, {0, 0});
  check(!stale.plan && code(stale.diagnostics, "source_snapshot_changed"), "stale source cannot produce a plan");
}
struct Fixture { p::StoredComposition stored; std::string live, pin, ref, fork, hidden, block; };
void seal(p::StoredComposition &s) { s.sha256 = db::composition_checksum(db::encode_composition(s.value)); }
std::string import(p::Composition &c, const p::Document &d) {
  const auto sha = db::composition_checksum(db::encode_document_snapshot(d)); c.origins.emplace(sha, d);
  const auto block = c.blocks.import_document(d, 0, p::utf8_character_count(d.content));
  c.blocks.assign_origin_snapshot(block, sha); return c.blocks.place(block);
}
Fixture composition() {
  Fixture f; auto &s = f.stored; s.project_id = owner; s.revision = 3; s.updated_at = "2026-11-20T17:00:00Z";
  s.value.name = "Montagem para plano"; auto &c = s.value;
  f.live = import(c, document("A🙂\n")); f.block = c.blocks.occurrence(f.live).block_id;
  const auto old = c.blocks.block(f.block).current_revision;
  f.pin = c.blocks.place(f.block); c.blocks.pin(f.pin, old);
  f.ref = c.blocks.place(f.block); c.blocks.reference_only(f.ref);
  f.fork = c.blocks.place(f.block); c.blocks.fork(f.fork, old);
  f.hidden = import(c, document("OCULTO\n")); c.blocks.set_hidden(f.hidden, true);
  c.blocks.revise(f.block, old, "A2🙂\n");
  (void)c.blocks.alternative(f.live, "Alternativa explícita");
  c.drafts[f.block] = {c.blocks.block(f.block).current_revision, "RASCUNHO PRIVADO"};
  seal(s); return f;
}
std::unique_ptr<c::CompositionReadProjection> read(const p::StoredComposition &s) {
  auto r = c::read_composition(s); if (!r.projection) throw std::runtime_error("Composition fixture refused"); return std::move(r.projection);
}
void composition_translation_and_visual_intent() {
  auto f = composition(); auto v = read(f.stored); const auto original = db::encode_composition(f.stored.value);
  q::ChangeRequest request{q::ChangePurpose::AuthorialReorder, {
    o::ReorderOccurrences{v->graph().root_layout_id, {v->occurrences()[2].occurrence_id}, v->occurrences()[0].occurrence_id},
    o::ReorderOccurrences{v->graph().root_layout_id, {v->occurrences()[1].occurrence_id}, {}}}};
  auto author = q::plan_composition_change(*v, f.stored, request);
  request.purpose = q::ChangePurpose::VisualOnly;
  auto visual = q::plan_composition_change(*v, f.stored, request);
  check(author.plan && visual.plan, "composition supports explicit reorder batch"); if (!author.plan || !visual.plan) return;
  auto restore = author.plan->preview(); restore.blocks.reorder(author.plan->source_order());
  check(db::encode_composition(restore) == original && db::encode_composition(visual.plan->preview()) == original,
        "all modes, versions, profiles, origins and pending drafts preserved, visual source unchanged");
  check(author.plan->after_graph() == visual.plan->after_graph() && author.plan->review().source_changed &&
        !visual.plan->review().source_changed && author.plan->source().revision == f.stored.revision,
        "same graph target has explicit different source effect without inventing a saved revision");
  auto oracle = f.stored.value; oracle.blocks.reorder(author.plan->preview_order());
  check(db::encode_composition(oracle) == db::encode_composition(author.plan->preview()) &&
        oracle.blocks.project(oracle.profile).text == author.plan->preview_reading().text &&
        author.plan->preview_reading().text.find("RASCUNHO") == std::string::npos,
        "exact owner reading preview, draft never applied");
  check(db::encode_composition(f.stored.value) == original && author.plan->check_source(f.stored).current, "source aggregate not mutated");
}
void hidden_order_and_composition_conflicts() {
  auto f = composition(); auto v = read(f.stored);
  auto hidden = std::find_if(v->occurrences().begin(), v->occurrences().end(), [&](const auto &b) { return b.source_occurrence_id == f.hidden; });
  auto result = q::plan_composition_change(*v, f.stored, {q::ChangePurpose::AuthorialReorder,
                  {o::ReorderOccurrences{v->graph().root_layout_id, {hidden->occurrence_id}, v->occurrences()[0].occurrence_id}}});
  check(result.plan && result.plan->review().source_changed && result.plan->before_reading().text == result.plan->preview_reading().text,
        "hidden occurrence movement is an authorial order change even if visible text is identical");
  if (!result.plan) return;
  v.reset(); check(result.plan->check_source(f.stored).current, "composition plan outlives source projection");
  ++f.stored.revision;
  check(!result.plan->check_source(f.stored).current, "revision-only update stales plan");
  v = read(f.stored); auto changed = f.stored; changed.value.drafts[f.block].text += " outra sessão"; seal(changed);
  auto stale = q::plan_composition_change(*v, changed, {});
  check(!stale.plan && code(stale.diagnostics, "source_composition_snapshot_changed"), "draft-only source change refuses new plan");
  auto unsupported = q::plan_composition_change(*v, f.stored, {q::ChangePurpose::AuthorialReorder,
                       {o::ChangeLayout{v->graph().root_layout_id, o::LayoutKind::Row}}});
  check(!unsupported.plan && code(unsupported.diagnostics, "unsupported_authorial_command"), "no fallback of unsupported authorial arrangement to visual intent");
}
void generated_document_plans() {
  auto d = document("A🙂\nBé\nC\nigual\nigual\n"); std::mt19937 random(0xC1A0B1U);
  for (int i = 0; i < 150; ++i) {
    auto v = read(d); const auto n = v->paragraphs().size();
    const auto from = static_cast<std::size_t>(random() % n);
    auto dest = static_cast<std::size_t>(random() % (n + 1)); if (dest == from) dest = n;
    const auto before = db::encode_document_snapshot(d);
    auto request = move(q::ChangePurpose::AuthorialReorder, *v, from, dest == n ? std::nullopt : std::optional(dest));
    auto plan = q::plan_document_change(*v, owner, d, request, {0, 0});
    auto expected = a::move_reading_block({d, {0, 0}}, d.paragraphs.rows[from].id, dest == n ? "" : d.paragraphs.rows[dest].id);
    check(plan.plan && db::encode_document_snapshot(plan.plan->preview().document) == db::encode_document_snapshot(expected.document) &&
          db::encode_document_snapshot(d) == before, "generated document plan matches owner engine without mutating source");
    if (plan.plan) d = plan.plan->preview().document; // Test-only next source state, no repository.
  }
}
void generated_composition_plans() {
  auto f = composition(); std::mt19937 random(0xC04A01U);
  for (int i = 0; i < 150; ++i) {
    auto v = read(f.stored); const auto n = v->occurrences().size();
    const auto from = static_cast<std::size_t>(random() % n);
    auto dest = static_cast<std::size_t>(random() % (n + 1)); if (dest == from) dest = n;
    const auto id = v->occurrences()[from].occurrence_id;
    const std::optional<std::string> anchor = dest == n ? std::nullopt : std::optional(v->occurrences()[dest].occurrence_id);
    auto plan = q::plan_composition_change(*v, f.stored, {q::ChangePurpose::AuthorialReorder,
                  {o::ReorderOccurrences{v->graph().root_layout_id, {id}, anchor}}});
    auto order = std::vector<std::string>{}; for (const auto &b : v->occurrences()) order.push_back(b.source_occurrence_id);
    const auto selected = order[from]; const std::optional<std::string> target = dest == n ? std::nullopt : std::optional(order[dest]);
    std::erase(order, selected); order.insert(target ? std::find(order.begin(), order.end(), *target) : order.end(), selected);
    auto oracle = f.stored.value; oracle.blocks.reorder(order);
    check(plan.plan && plan.plan->preview_order() == order &&
          db::encode_composition(plan.plan->preview()) == db::encode_composition(oracle), "generated composition plan matches independent order oracle");
    if (plan.plan) { f.stored.value = plan.plan->preview(); ++f.stored.revision; seal(f.stored); }
  }
}
} // namespace
int main() {
  try {
    document_intents_and_engine(); resma_group_boundaries(); marked_interval_refusal(); noops_and_equal_prose();
    unsupported_and_bounded_requests(); freshness_lifetime_and_undo(); composition_translation_and_visual_intent();
    hidden_order_and_composition_conflicts(); generated_document_plans(); generated_composition_plans();
  } catch (const std::exception &error) { std::cerr << "ERROR: " << error.what() << '\n'; return 1; }
  if (failures) { std::cerr << failures << " failures\n"; return 1; }
  std::cout << "INDE-OBJ-01: 10 change-plan groups passed (150 document plans, 150 composition plans, existing undo/redo)\n";
}

#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
#include "object_change_session.hpp"
#include "object_session_checkpoint.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <type_traits>

namespace o = inde::objects;
namespace c = inde::objects::compat;
namespace q = inde::objects::planning;
namespace p = inde::project;
namespace a = inde::application;
namespace db = inde::persistence;
namespace {
const std::string owner = "10000000-0000-4000-8000-000000000001";
void require(bool yes, const char *why) {
  if (!yes)
    throw std::runtime_error(why);
}
bool code(const q::SessionResult &r, const std::string &expected) {
  return !r.accepted && !r.changed &&
         std::any_of(r.diagnostics.begin(), r.diagnostics.end(),
                     [&](const auto &d) { return d.code == expected; });
}
p::Document document(std::string text = "A🙂\nBé\nC\nigual\nigual") {
  p::Document d;
  d.id = p::new_uuid();
  d.title = "Sessão sintética";
  d.content = std::move(text);
  d.created_at = d.updated_at = "2026-11-20T17:00:00Z";
  d.paragraphs = p::identify_paragraphs(d.content);
  return d;
}
std::unique_ptr<c::DocumentReadProjection> view(const p::Document &d) {
  auto r = c::read_document(owner, d);
  require(bool(r.projection), "Document capture");
  return std::move(r.projection);
}
std::unique_ptr<c::CompositionReadProjection>
view(const p::StoredComposition &s) {
  auto r = c::read_composition(s);
  require(bool(r.projection), "Composition capture");
  return std::move(r.projection);
}
q::ChangeRequest
move(const o::Composition &g, std::size_t from,
     std::optional<std::size_t> before,
     q::ChangePurpose purpose = q::ChangePurpose::AuthorialReorder) {
  return {purpose,
          {o::ReorderOccurrences{
              g.root_layout_id,
              {g.layouts[0].children[from]},
              before ? std::optional(g.layouts[0].children[*before])
                     : std::nullopt}}};
}
q::ChangeRequest row(const o::Composition &g) {
  return {q::ChangePurpose::VisualOnly,
          {o::ChangeLayout{g.root_layout_id, o::LayoutKind::Row}}};
}
std::string snapshot(const q::DocumentChangeSession &s) {
  return db::encode_document_snapshot(s.current().document);
}
std::string snapshot(const q::CompositionChangeSession &s) {
  return db::encode_composition(s.current());
}
void apply(q::DocumentChangeSession &s, const p::Document &origin,
           const q::ChangeRequest &r) {
  auto prepared = s.prepare(r);
  require(bool(prepared.ticket), "Prepare document ticket");
  const auto &plan = prepared.ticket->plan();
  const auto expected = db::encode_document_snapshot(plan.preview().document);
  const auto graph = plan.after_graph();
  const auto result = s.apply(*prepared.ticket, owner, origin);
  require(result.accepted && prepared.ticket->consumed() &&
              snapshot(s) == expected && s.graph() == graph,
          "Apply exactly reviewed document candidate");
}
void apply(q::CompositionChangeSession &s, const p::StoredComposition &origin,
           const q::ChangeRequest &r) {
  auto prepared = s.prepare(r);
  require(bool(prepared.ticket), "Prepare Composition ticket");
  const auto expected =
      db::encode_composition(prepared.ticket->plan().preview());
  const auto graph = prepared.ticket->plan().after_graph();
  const auto result = s.apply(*prepared.ticket, origin);
  require(result.accepted && prepared.ticket->consumed() &&
              snapshot(s) == expected && s.graph() == graph,
          "Apply exactly reviewed Composition candidate");
}
p::StoredComposition composition() {
  p::StoredComposition s;
  s.project_id = owner;
  s.revision = 4;
  s.updated_at = "2026-11-20T17:00:00Z";
  s.value.name = "Composição sintética";
  auto d = document("BLOCO🙂\n");
  const auto hash = db::composition_checksum(db::encode_document_snapshot(d));
  s.value.origins.emplace(hash, d);
  const auto b =
      s.value.blocks.import_document(d, 0, p::utf8_character_count(d.content));
  s.value.blocks.assign_origin_snapshot(b, hash);
  const auto live = s.value.blocks.place(b);
  const auto old = s.value.blocks.block(b).current_revision;
  const auto pin = s.value.blocks.place(b);
  s.value.blocks.pin(pin, old);
  const auto ref = s.value.blocks.place(b);
  s.value.blocks.reference_only(ref);
  const auto fork = s.value.blocks.place(b);
  s.value.blocks.fork(fork, old);
  const auto hidden = s.value.blocks.place(b);
  s.value.blocks.set_hidden(hidden, true);
  (void)s.value.blocks.alternative(live, "Outra variante");
  s.value.blocks.revise(b, old, "APLICADO🙂\n");
  s.value.drafts[b] = {s.value.blocks.block(b).current_revision,
                       "PRIVADO PENDENTE"};
  s.sha256 = db::composition_checksum(db::encode_composition(s.value));
  return s;
}
p::Document receipt(const q::PreparedDocumentSave &t) {
  auto d = t.candidate();
  d.updated_at = "2026-11-20T21:30:00Z";
  return d;
}
p::StoredComposition receipt(const q::PreparedCompositionSave &t) {
  auto s = t.expected();
  s.value = t.candidate();
  ++s.revision;
  s.updated_at = "2026-11-20T21:30:00Z";
  s.sha256 = db::composition_checksum(db::encode_composition(s.value));
  return s;
}
void document_save_history() {
  auto d = document();
  auto v = view(d);
  q::DocumentChangeSession s(*v), other(*v);
  require(!s.begin_save().ticket, "Clean source never writes presentation");
  auto old = s.prepare({});
  apply(s, d, move(s.graph(), 1, 0));
  const auto graph = s.graph();
  const auto bytes = s.history_bytes();
  auto intent = s.begin_save();
  require(intent.ticket && s.save_pending(),
          "Save begins an immutable barrier");
  require(db::encode_document_snapshot(intent.ticket->expected()) ==
              db::encode_document_snapshot(d),
          "Save expected is actual opening source");
  require(!s.prepare({}).ticket &&
              code(s.apply(*old.ticket, owner, d), "session_save_pending") &&
              code(s.undo(s.revision(), owner, d), "session_save_pending") &&
              !s.begin_save().ticket,
          "Pending write freezes all edits/history");
  auto saved = receipt(*intent.ticket);
  auto bad = saved;
  bad.title += " changed";
  require(code(s.acknowledge_save(*intent.ticket, owner, bad),
               "session_save_receipt_mismatch") &&
              s.save_pending() && !intent.ticket->consumed(),
          "Wrong receipt cannot release barrier");
  require(code(other.acknowledge_save(*intent.ticket, owner, saved),
               "session_save_scope"),
          "Save scope is session-specific");
  const auto revision = s.revision();
  require(s.acknowledge_save(*intent.ticket, owner, saved).accepted &&
              s.revision() == revision + 1 && !s.source_dirty() && !s.dirty() &&
              s.graph() == graph && s.history_bytes() == bytes && s.can_undo(),
          "Save acknowledgement advances base while retaining entire history");
  require(!s.save_pending() && intent.ticket->consumed() &&
              code(s.acknowledge_save(*intent.ticket, owner, saved),
                   "session_save_consumed"),
          "Acknowledgement exactly once");
  require(code(s.undo(s.revision(), owner, d), "source_snapshot_changed"),
          "Old external origin no longer accepted");
  require(s.undo(s.revision(), owner, saved).accepted && s.source_dirty() &&
              s.current().document.updated_at == saved.updated_at,
          "Undo after save keeps server timestamp and old authorial layers");
  require(s.redo(s.revision(), owner, saved).accepted && !s.source_dirty() &&
              snapshot(s) == db::encode_document_snapshot(saved),
          "Redo returns to acknowledged saved payload");
}
void reordered_storage_receipt() {
  auto d = document();
  d.formatting = {{p::DocumentTextStyle::Bold, 0, 2, {}},
                  {p::DocumentTextStyle::Italic, 3, 5, {}}};
  auto v = view(d);
  q::DocumentChangeSession s(*v);
  apply(s, d, move(s.graph(), 1, 0));
  auto t = s.begin_save();
  auto saved = receipt(*t.ticket);
  std::reverse(saved.formatting.begin(), saved.formatting.end());
  require(s.acknowledge_save(*t.ticket, owner, saved).accepted &&
              !s.source_dirty() &&
              snapshot(s) == db::encode_document_snapshot(saved),
          "Domain-equal reordered storage accepted, actual readback adopted");
  require(s.undo(s.revision(), owner, saved).accepted &&
              s.redo(s.revision(), owner, saved).accepted && !s.source_dirty(),
          "Storage ordering does not break historical replay");
}
void cancel_and_exact_quota() {
  auto d = document();
  d.updated_at = "x";
  auto v = view(d);
  q::DocumentChangeSession measured(*v);
  apply(measured, d, move(measured.graph(), 1, 0));
  const auto budget = measured.history_bytes();
  q::DocumentChangeSession s(*v, {}, {budget, 64});
  apply(s, d, move(s.graph(), 1, 0));
  auto intent = s.begin_save();
  auto saved = receipt(*intent.ticket);
  require(!s.cancel_save(*intent.ticket, owner, saved).accepted &&
              s.save_pending(),
          "Cancellation cannot pretend a changed origin is unchanged");
  require(s.cancel_save(*intent.ticket, owner, d).accepted &&
              !s.save_pending() && intent.ticket->consumed(),
          "Confirmed unchanged origin permits cancellation");
  auto again = s.begin_save();
  require(s.acknowledge_save(*again.ticket, owner, receipt(*again.ticket))
                  .accepted &&
              s.history_bytes() <= budget && s.can_undo(),
          "Timestamp reserve preserves history at exact quota");
  require(s.undo(s.revision(), owner, receipt(*again.ticket)).accepted,
          "Exact quota still reversible after save");
}
void composition_save_and_conflicts() {
  auto origin = composition();
  auto v = view(origin);
  q::CompositionChangeSession s(*v);
  apply(s, origin, row(s.graph()));
  require(!s.begin_save().ticket,
          "Visual layout cannot be saved into Composition payload");
  apply(s, origin, move(s.graph(), 4, 0));
  auto intent = s.begin_save();
  require(bool(intent.ticket), "Composition save intent");
  const auto source = db::encode_composition(intent.ticket->expected().value);
  auto saved = receipt(*intent.ticket);
  auto bad = saved;
  ++bad.revision;
  require(!s.acknowledge_save(*intent.ticket, bad).accepted && s.save_pending(),
          "CAS receipt must be exactly next revision");
  bad = saved;
  bad.sha256 = std::string(64, '0');
  require(!s.acknowledge_save(*intent.ticket, bad).accepted,
          "Receipt declared hash checked against bytes");
  require(!s.cancel_save(*intent.ticket, saved).accepted,
          "Committed Composition cannot be cancelled as untouched");
  const auto graph = s.graph();
  const auto bytes = s.history_bytes();
  require(s.acknowledge_save(*intent.ticket, saved).accepted &&
              !s.source_dirty() && s.dirty() &&
              s.origin().revision == saved.revision && s.graph() == graph &&
              s.history_bytes() == bytes,
          "Save retains visual work without claiming it persisted");
  require(s.undo(s.revision(), saved).accepted &&
              db::encode_composition(s.current()) == source && s.source_dirty(),
          "Undo crosses acknowledged Composition savepoint");
  require(s.redo(s.revision(), saved).accepted && !s.source_dirty(),
          "Composition redo returns to savepoint");
}
void checkpoint_roundtrip_and_commit_window() {
  auto d = document();
  auto dv = view(d);
  q::DocumentChangeSession ds(*dv, {1, 2});
  q::ChangeRequest spatial{q::ChangePurpose::VisualOnly,
                           {o::ChangeLayout{ds.graph().root_layout_id,
                                            o::LayoutKind::Freeform,
                                            0,
                                            o::FramePolicy::Replace,
                                            {}}}};
  auto &layout = std::get<o::ChangeLayout>(spatial.commands[0]);
  for (const auto &id : ds.graph().layouts[0].children)
    layout.replacements.push_back({id, {-2.1234567890123456, 0, 4.25, 8}});
  apply(ds, d, spatial);
  apply(ds, d, move(ds.graph(), 2, 0));
  auto old = ds.prepare({});
  auto save = ds.begin_save();
  const auto checkpoint = ds.checkpoint();
  auto restored = q::DocumentChangeSession::restore(checkpoint, owner, d);
  require(restored.session && snapshot(*restored.session) == snapshot(ds) &&
              restored.session->graph() == ds.graph() &&
              restored.session->current().selection.cursor ==
                  ds.current().selection.cursor &&
              !restored.session->can_undo() &&
              !restored.session->save_pending(),
          "Restart recovers private source, selection and exact presentation, "
          "with fresh session/history");
  require(code(restored.session->apply(*old.ticket, owner, d),
               "session_ticket_scope"),
          "Old tickets cannot cross restart");
  const auto saved = receipt(*save.ticket);
  require(!q::DocumentChangeSession::restore(checkpoint, owner, saved).session,
          "Changed durable origin blocks unsaved-resume mode");
  auto committed = q::DocumentChangeSession::restore(
      checkpoint, owner, saved, q::RecoveryMode::AdoptMatchingSave);
  require(committed.session && !committed.session->source_dirty() &&
              committed.session->dirty() &&
              committed.session->graph() == ds.graph(),
          "Explicit adoption recovers commit-before-ack window without losing "
          "visual work");
  auto unknown = saved;
  unknown.title += " third writer";
  require(!q::DocumentChangeSession::restore(checkpoint, owner, unknown,
                                             q::RecoveryMode::AdoptMatchingSave)
               .session,
          "Unknown external state never overwritten");
  auto origin = composition();
  auto cv = view(origin);
  q::CompositionChangeSession cs(*cv);
  apply(cs, origin, move(cs.graph(), 4, 0));
  auto ci = cs.begin_save();
  const auto cc = cs.checkpoint();
  auto cr = q::CompositionChangeSession::restore(cc, origin);
  require(cr.session && snapshot(*cr.session) == snapshot(cs) &&
              cr.session->graph() == cs.graph(),
          "Full Composition checkpoint roundtrip");
  auto persisted = receipt(*ci.ticket);
  auto ca = q::CompositionChangeSession::restore(
      cc, persisted, q::RecoveryMode::AdoptMatchingSave);
  require(ca.session && !ca.session->dirty() &&
              ca.session->origin().revision == persisted.revision,
          "Composition matching committed candidate adopted explicitly");
  ++persisted.revision;
  require(!q::CompositionChangeSession::restore(
               cc, persisted, q::RecoveryMode::AdoptMatchingSave)
               .session,
          "Later revision is not claimed as this interrupted save");
}
void malformed_and_bounded_checkpoints() {
  auto d = document();
  auto v = view(d);
  q::DocumentChangeSession s(*v);
  const auto original = s.checkpoint();
  auto reject = [&](q::SessionCheckpoint cp) {
    require(!q::DocumentChangeSession::restore(cp, owner, d).session,
            "Malformed checkpoint refused without session");
  };
  auto cp = original;
  cp.bytes.back() ^= 1;
  reject(cp);
  cp = original;
  cp.bytes += "extra";
  cp.sha256 = db::composition_checksum(cp.bytes);
  reject(cp);
  cp = original;
  cp.bytes[0] = '?';
  cp.sha256 = db::composition_checksum(cp.bytes);
  reject(cp);
  const auto data = q::checkpoint_detail::decode(original);
  auto x = data;
  x.layout.kind = static_cast<o::LayoutKind>(99);
  reject(q::checkpoint_detail::encode(x));
  x = data;
  x.selection.cursor = 99999;
  reject(q::checkpoint_detail::encode(x));
  x = data;
  x.frames[0].id = "foreign";
  reject(q::checkpoint_detail::encode(x));
  x = data;
  x.frames[0].frame = o::Frame{0, 0, 0, 1};
  reject(q::checkpoint_detail::encode(x));
  bool depth_refused{};
  try {
    x = data;
    x.base = std::string(65, '[') + "0" + std::string(65, ']');
    (void)q::checkpoint_detail::encode(x);
  } catch (const std::invalid_argument &) {
    depth_refused = true;
  }
  require(depth_refused,
          "Nested source parser is bounded before recursive owner codec");
  std::mt19937 random(0x5A7E01U);
  for (int i = 0; i < 100; ++i) {
    cp = original;
    cp.bytes.resize(static_cast<std::size_t>(random() % cp.bytes.size()));
    cp.sha256 = db::composition_checksum(cp.bytes);
    reject(cp);
  }
  require(!s.dirty() && !s.can_undo(),
          "Malformed recovery never changes source/session");
}
void repeated_savepoints() {
  auto d = document();
  auto v = view(d);
  q::DocumentChangeSession ds(*v);
  auto origin = composition();
  auto cv = view(origin);
  q::CompositionChangeSession cs(*cv);
  for (int i = 0; i < 20; ++i) {
    apply(ds, d, move(ds.graph(), 1, 0));
    auto di = ds.begin_save();
    d = receipt(*di.ticket);
    require(ds.acknowledge_save(*di.ticket, owner, d).accepted,
            "Generated document savepoint");
    apply(cs, origin, move(cs.graph(), 1, 0));
    auto ci = cs.begin_save();
    origin = receipt(*ci.ticket);
    require(cs.acknowledge_save(*ci.ticket, origin).accepted,
            "Generated Composition savepoint");
    require(ds.undo(ds.revision(), owner, d).accepted &&
                ds.redo(ds.revision(), owner, d).accepted && !ds.source_dirty(),
            "Generated Document undo/redo across savepoint");
    require(cs.undo(cs.revision(), origin).accepted &&
                cs.redo(cs.revision(), origin).accepted && !cs.source_dirty(),
            "Generated Composition undo/redo across savepoint");
    auto dr = q::DocumentChangeSession::restore(ds.checkpoint(), owner, d);
    auto cr = q::CompositionChangeSession::restore(cs.checkpoint(), origin);
    require(dr.session && cr.session && !dr.session->source_dirty() &&
                !cr.session->source_dirty(),
            "Generated acknowledged checkpoint restart");
  }
  require(ds.revision() == 80 && cs.revision() == 80,
          "Save acknowledgements also invalidate old session revisions");
}
} // namespace
int main() {
  try {
    document_save_history();
    reordered_storage_receipt();
    cancel_and_exact_quota();
    composition_save_and_conflicts();
    checkpoint_roundtrip_and_commit_window();
    malformed_and_bounded_checkpoints();
    repeated_savepoints();
    std::cout
        << "INDE-OBJ-01: 7 save/recovery groups passed (20+20 savepoints, 100 "
           "truncated checkpoints, CAS receipt barriers and restart modes)\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}

#include "object_change_session.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
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
void require(bool yes, const char *why) { if (!yes) throw std::runtime_error(why); }
bool code(const q::SessionResult &r, const std::string &expected) {
  return !r.accepted && !r.changed && std::any_of(r.diagnostics.begin(), r.diagnostics.end(), [&](const auto &d) { return d.code == expected; });
}
p::Document document(std::string text = "A🙂\nBé\nC\nigual\nigual") {
  p::Document d; d.id = p::new_uuid(); d.title = "Sessão sintética"; d.content = std::move(text);
  d.created_at = d.updated_at = "2026-11-20T17:00:00Z"; d.paragraphs = p::identify_paragraphs(d.content);
  return d;
}
std::unique_ptr<c::DocumentReadProjection> view(const p::Document &d) {
  auto r=c::read_document(owner,d); require(bool(r.projection), "Document capture"); return std::move(r.projection);
}
std::unique_ptr<c::CompositionReadProjection> view(const p::StoredComposition &s) {
  auto r=c::read_composition(s); require(bool(r.projection), "Composition capture"); return std::move(r.projection);
}
q::ChangeRequest move(const o::Composition &g, std::size_t from, std::optional<std::size_t> before,
                      q::ChangePurpose purpose=q::ChangePurpose::AuthorialReorder) {
  return {purpose,{o::ReorderOccurrences{g.root_layout_id,{g.layouts[0].children[from]},
          before ? std::optional(g.layouts[0].children[*before]) : std::nullopt}}};
}
q::ChangeRequest row(const o::Composition &g) {
  return {q::ChangePurpose::VisualOnly,{o::ChangeLayout{g.root_layout_id,o::LayoutKind::Row}}};
}
std::string snapshot(const q::DocumentChangeSession &s) { return db::encode_document_snapshot(s.current().document); }
std::string snapshot(const q::CompositionChangeSession &s) { return db::encode_composition(s.current()); }
void apply(q::DocumentChangeSession &s, const p::Document &origin, const q::ChangeRequest &r) {
  auto prepared=s.prepare(r); require(bool(prepared.ticket), "Prepare document ticket");
  const auto &plan=prepared.ticket->plan(); const auto expected=db::encode_document_snapshot(plan.preview().document);
  const auto graph=plan.after_graph(); const auto result=s.apply(*prepared.ticket,owner,origin);
  require(result.accepted && prepared.ticket->consumed() && snapshot(s)==expected && s.graph()==graph,"Apply exactly reviewed document candidate");
}
void apply(q::CompositionChangeSession &s, const p::StoredComposition &origin, const q::ChangeRequest &r) {
  auto prepared=s.prepare(r); require(bool(prepared.ticket), "Prepare Composition ticket");
  const auto expected=db::encode_composition(prepared.ticket->plan().preview());
  const auto graph=prepared.ticket->plan().after_graph(); const auto result=s.apply(*prepared.ticket,origin);
  require(result.accepted && prepared.ticket->consumed() && snapshot(s)==expected && s.graph()==graph,"Apply exactly reviewed Composition candidate");
}
p::StoredComposition composition() {
  p::StoredComposition s; s.project_id=owner; s.revision=4; s.updated_at="2026-11-20T17:00:00Z"; s.value.name="Composição sintética";
  auto d=document("BLOCO🙂\n"); const auto hash=db::composition_checksum(db::encode_document_snapshot(d));
  s.value.origins.emplace(hash,d); const auto b=s.value.blocks.import_document(d,0,p::utf8_character_count(d.content));
  s.value.blocks.assign_origin_snapshot(b,hash); const auto live=s.value.blocks.place(b);
  const auto old=s.value.blocks.block(b).current_revision;
  const auto pin=s.value.blocks.place(b); s.value.blocks.pin(pin,old);
  const auto ref=s.value.blocks.place(b); s.value.blocks.reference_only(ref);
  const auto fork=s.value.blocks.place(b); s.value.blocks.fork(fork,old);
  const auto hidden=s.value.blocks.place(b); s.value.blocks.set_hidden(hidden,true);
  (void)s.value.blocks.alternative(live,"Outra variante");
  s.value.blocks.revise(b,old,"APLICADO🙂\n");
  s.value.drafts[b]={s.value.blocks.block(b).current_revision,"PRIVADO PENDENTE"};
  s.sha256=db::composition_checksum(db::encode_composition(s.value)); return s;
}
void document_mixed_history() {
  auto d=document(); d.formatting={{p::DocumentTextStyle::Bold,0,2,{}}}; d.anchors={{p::new_uuid(),"Declarada",0,2}};
  auto v=view(d); q::DocumentChangeSession s(*v,{0,2}); const auto initial=snapshot(s); const auto initial_graph=s.graph(); v.reset();
  require(!s.dirty() && s.revision()==0 && !s.can_undo(),"New session owns opening capture");
  apply(s,d,row(s.graph())); const auto row_graph=s.graph();
  require(!s.source_dirty() && s.dirty(),"Visual arrangement changes only graph");
  apply(s,d,move(s.graph(),2,0)); const auto changed=snapshot(s); const auto moved=s.graph();
  apply(s,d,move(s.graph(),1,std::nullopt)); const auto second=snapshot(s); const auto second_graph=s.graph();
  require(changed!=initial && second!=changed && s.source_dirty(),"Sequential authorial plans survive reordered adapter storage");
  require(s.undo(s.revision(),owner,d).accepted && snapshot(s)==changed && s.graph()==moved,"Undo second source+graph together");
  require(s.undo(s.revision(),owner,d).accepted && snapshot(s)==initial && s.graph()==row_graph,"Undo source preserves earlier visual arrangement");
  require(s.undo(s.revision(),owner,d).accepted && !s.dirty() && s.graph()==initial_graph,"Undo visual returns exact origin");
  require(s.redo(s.revision(),owner,d).accepted && s.redo(s.revision(),owner,d).accepted &&
          s.redo(s.revision(),owner,d).accepted && snapshot(s)==second && s.graph()==second_graph,"Redo full mixed timeline");
  require(s.revision()==9 && db::encode_document_snapshot(d)==initial,"Revision monotonic, external source unchanged");
}
void tickets_scope_stale_aba_noop() {
  auto d=document(); auto v=view(d); q::DocumentChangeSession s(*v),other(*v);
  auto early=s.prepare(move(s.graph(),1,0)), first=s.prepare(row(s.graph()));
  require(early.ticket && first.ticket,"Prepare competing tickets");
  require(code(other.apply(*early.ticket,owner,d),"session_ticket_scope") && !early.ticket->consumed(),"Cross-session ticket refused");
  require(s.apply(*first.ticket,owner,d).accepted && code(s.apply(*first.ticket,owner,d),"session_ticket_consumed"),"Exactly one use");
  require(code(s.apply(*early.ticket,owner,d),"session_revision_conflict"),"Ticket stale after other apply");
  require(s.undo(s.revision(),owner,d).accepted && s.graph()==v->graph() &&
          code(s.apply(*early.ticket,owner,d),"session_revision_conflict"),"Undo cannot revive stale ticket (ABA)");
  auto noop=s.prepare({}); const auto rev=s.revision(), bytes=s.history_bytes();
  const auto r=s.apply(*noop.ticket,owner,d);
  require(r.accepted && !r.changed && s.revision()==rev && s.history_bytes()==bytes && s.can_redo(),"No-op preserves revision and redo");
  require(code(s.apply(*noop.ticket,owner,d),"session_ticket_consumed"),"No-op ticket consumed once");
  apply(s,d,move(s.graph(),1,0)); require(!s.can_redo(),"New change clears redo");
  require(code(s.undo(0,owner,d),"session_revision_conflict"),"Undo requires exact session revision");
}
void external_conflicts_are_atomic() {
  auto d=document(); auto v=view(d); q::DocumentChangeSession s(*v); auto ready=s.prepare(move(s.graph(),1,0));
  auto external=d; external.title+=" outro escritor";
  const auto before=snapshot(s); const auto graph=s.graph();
  require(code(s.apply(*ready.ticket,owner,external),"source_snapshot_changed") && snapshot(s)==before && s.graph()==graph &&
          s.revision()==0 && !s.can_undo() && !ready.ticket->consumed(),"External metadata conflict leaves all state/ticket intact");
  require(code(s.apply(*ready.ticket,p::new_uuid(),d),"source_project_conflict"),"Other project refused");
  require(s.apply(*ready.ticket,owner,d).accepted,"Failed external check does not consume ticket");
  const auto after=snapshot(s); const auto rev=s.revision();
  require(!s.undo(rev,owner,external).accepted && snapshot(s)==after && s.revision()==rev && !s.can_redo(),"Undo refuses changed external origin atomically");
}
void presentation_authority_and_frames() {
  auto d=document(); auto v=view(d); q::DocumentChangeSession s(*v);
  q::ChangeRequest spatial{q::ChangePurpose::VisualOnly,{o::ChangeLayout{s.graph().root_layout_id,o::LayoutKind::Freeform,0,o::FramePolicy::Replace,{}}}};
  auto &layout=std::get<o::ChangeLayout>(spatial.commands[0]);
  for(const auto &id:s.graph().layouts[0].children) layout.replacements.push_back({id,{2,3,4,5}});
  apply(s,d,spatial); const auto paint=s.graph().layouts[0].paint_order;
  apply(s,d,move(s.graph(),1,0));
  require(s.graph().layouts[0].paint_order==paint,"Authorial reorder preserves spatial painting");
  for(const auto &o:s.graph().occurrences) require(o.frame==o::Frame{2,3,4,5},"Frames survive authorial translation");
  apply(s,d,move(s.graph(),1,0,q::ChangePurpose::VisualOnly));
  auto refused=s.prepare(move(s.graph(),2,0));
  require(!refused.ticket && refused.diagnostics[0].code=="presentation_reading_order_diverged","Earlier visual order never implicitly promoted to authorial source");
  require(s.undo(s.revision(),owner,d).accepted,"Undo divergent visual order");
  apply(s,d,move(s.graph(),2,0));
  auto bad=s.graph(); bad.objects[0].id=p::new_uuid();
  const auto fresh=view(s.current().document);
  auto invalid=q::plan_document_change(*fresh,owner,s.current().document,{},s.current().selection,bad);
  require(!invalid.plan,"Foreign/invalid presentation graph refused");
}
void resma_and_owner_refusals() {
  auto d=document("A\nX\nY🙂\nZ");const auto leaf=p::new_uuid();
  d.resmas={{p::new_uuid(),"Grupo",leaf,2,6,{{leaf,"Ativa",{}},{p::new_uuid(),"Coberta",std::make_shared<const p::DocumentLeafContent>(p::DocumentLeafContent{"PRIVADO",{}})}}}};
  auto v=view(d);q::DocumentChangeSession s(*v);
  auto bad=s.prepare(move(s.graph(),1,0)); require(!bad.ticket && s.revision()==0 && !s.can_undo(),"Partial resma refuses before review");
  q::ChangeRequest full{q::ChangePurpose::AuthorialReorder,{o::ReorderOccurrences{s.graph().root_layout_id,
      {s.graph().layouts[0].children[1],s.graph().layouts[0].children[2]},s.graph().layouts[0].children[0]}}};
  apply(s,d,full); require(s.current().document.content=="X\nY🙂\nA\nZ","Whole resma applied");
  require(s.undo(s.revision(),owner,d).accepted && snapshot(s)==db::encode_document_snapshot(d),"Private leaf restored exactly by owner replay");
  auto marked=document("A\nB\nC");marked.anchors={{p::new_uuid(),"A+B",0,3}};auto mv=view(marked);q::DocumentChangeSession ms(*mv);
  auto refusal=ms.prepare(move(ms.graph(),1,0));require(!refusal.ticket && refusal.diagnostics[0].code=="source_text_engine_refused" && !ms.dirty(),"Owner refusal preserved in session preparation");
}
void history_limits() {
  auto d=document();auto v=view(d);q::DocumentChangeSession zero(*v,{}, {0,0});auto ticket=zero.prepare(row(zero.graph()));
  require(code(zero.apply(*ticket.ticket,owner,d),"session_history_budget") && !ticket.ticket->consumed() && !zero.dirty(),"Cannot accept an irreversible change with zero budget");
  auto noop=zero.prepare({});require(zero.apply(*noop.ticket,owner,d).accepted,"Zero budget still accepts no-op");
  q::DocumentChangeSession limited(*v,{}, {64*1024*1024,2});
  apply(limited,d,row(limited.graph())); apply(limited,d,move(limited.graph(),1,0)); apply(limited,d,move(limited.graph(),2,0));
  require(limited.undo(limited.revision(),owner,d).accepted && limited.undo(limited.revision(),owner,d).accepted &&
          code(limited.undo(limited.revision(),owner,d),"session_history_empty"),"Oldest whole steps evicted at count bound");
  bool invalid{};try{q::DocumentChangeSession bad(*v,{}, {1,65});}catch(const std::invalid_argument&){invalid=true;}require(invalid,"History count cannot exceed 64");
  q::DocumentChangeSession measured(*v);apply(measured,d,row(measured.graph()));const auto bytes=measured.history_bytes();
  q::DocumentChangeSession too_small(*v,{}, {bytes-1,64});auto r=too_small.prepare(row(too_small.graph()));
  require(code(too_small.apply(*r.ticket,owner,d),"session_history_budget"),"Exact logical byte boundary enforced");
  q::DocumentChangeSession exact(*v,{}, {bytes,64});apply(exact,d,row(exact.graph()));require(exact.history_bytes()==bytes,"Exact pair budget succeeds");
  apply(exact,d,{q::ChangePurpose::VisualOnly,{o::ChangeLayout{exact.graph().root_layout_id,o::LayoutKind::Grid,2}}});
  require(exact.undo(exact.revision(),owner,d).accepted && !exact.can_undo() && exact.history_bytes()<=bytes,"Byte cap evicts whole oldest states, retaining reversible latest pair");
}
void composition_mixed_history_and_conflicts() {
  auto origin=composition();auto v=view(origin);q::CompositionChangeSession s(*v),other(*v);const auto initial=snapshot(s);v.reset();
  apply(s,origin,row(s.graph()));auto ready=s.prepare(move(s.graph(),4,0));
  require(code(other.apply(*ready.ticket,origin),"session_ticket_scope"),"Composition ticket scope");
  require(s.apply(*ready.ticket,origin).accepted && s.source_dirty(),"Hidden occurrence source order applied");
  require(code(s.apply(*ready.ticket,origin),"session_ticket_consumed"),"Composition ticket once");
  apply(s,origin,move(s.graph(),1,std::nullopt));const auto changed=snapshot(s);const auto graph=s.graph();
  auto restored=s.current();std::vector<std::string> order;for(const auto&o:origin.value.blocks.snapshot().placements)order.push_back(o.id);
  restored.blocks.reorder(order);require(db::encode_composition(restored)==initial,"All non-order aggregate fields conserved across repeated applies");
  auto conflict=origin;++conflict.revision;const auto rev=s.revision();
  require(code(s.undo(rev,conflict),"source_composition_revision_changed") && s.revision()==rev && snapshot(s)==changed,"Revision-only external conflict blocks undo");
  require(s.undo(s.revision(),origin).accepted && s.undo(s.revision(),origin).accepted &&
          s.undo(s.revision(),origin).accepted && !s.dirty(),"Composition full mixed undo");
  require(s.redo(s.revision(),origin).accepted && s.redo(s.revision(),origin).accepted &&
          s.redo(s.revision(),origin).accepted && snapshot(s)==changed && s.graph()==graph,"Composition full mixed redo");
  require(s.origin()==view(origin)->source() && origin.revision==4 && db::encode_composition(origin.value)==initial,"Original envelope never invented/incremented");
  auto stale=s.prepare({});apply(s,origin,move(s.graph(),1,0));require(code(s.apply(*stale.ticket,origin),"session_revision_conflict"),"Composition stale ticket");
}
void composition_presentation_noops_and_budgets() {
  const auto c=composition(); const auto v=view(c); q::CompositionChangeSession s(*v);
  apply(s,c,move(s.graph(),1,0,q::ChangePurpose::VisualOnly));
  const auto refused=s.prepare(move(s.graph(),2,0));
  require(!refused.ticket && refused.diagnostics[0].code=="presentation_reading_order_diverged" && !s.source_dirty(),"Composition visual order stays separate from authorial order");
  require(s.undo(s.revision(),c).accepted,"Restore visual alignment");
  auto noop=s.prepare({});const auto rev=s.revision();
  require(s.apply(*noop.ticket,c).accepted && s.revision()==rev && s.can_redo() && noop.ticket->consumed(),"Composition no-op retains redo and consumes ticket");
  auto metadata=v->graph();metadata.objects[0].kind=o::ObjectKind::Paragraph;
  auto mismatch=q::plan_composition_change(*v,c,{},metadata);
  require(!mismatch.plan && mismatch.diagnostics[0].code=="presentation_capture_mismatch","Valid graph cannot replace captured object metadata");
  auto ready=s.prepare(row(s.graph()));auto external=c;external.value.name+=" changed";
  external.sha256=db::composition_checksum(db::encode_composition(external.value));
  require(!s.apply(*ready.ticket,external).accepted && !ready.ticket->consumed() && s.revision()==rev,"Composition payload-only external conflict is atomic");
  q::CompositionChangeSession zero(*v,{0,0});auto oversized=zero.prepare(row(zero.graph()));
  require(code(zero.apply(*oversized.ticket,c),"session_history_budget") && !zero.dirty(),"Composition refuses a change without reversible pair budget");
  bool invalid{};try{q::CompositionChangeSession bad(*v,{129*1024*1024,64});}catch(const std::invalid_argument&){invalid=true;}
  require(invalid,"History byte ceiling enforced");
}
void generated_mixed_roundtrips() {
  auto d=document();auto dv=view(d);q::DocumentChangeSession ds(*dv,{}, {64*1024*1024,64});
  auto c=composition();auto cv=view(c);q::CompositionChangeSession cs(*cv);std::mt19937 random(0x5E5510U);
  struct Snapshot { std::string doc,comp; o::Composition dg,cg; };
  std::vector<Snapshot> states{{snapshot(ds),snapshot(cs),ds.graph(),cs.graph()}};
  for(std::size_t i=0;i<40;++i){
    const auto df=static_cast<std::size_t>(random()%ds.graph().occurrences.size());
    const auto cf=static_cast<std::size_t>(random()%cs.graph().occurrences.size());
    // Move the selected occurrence to a guaranteed different position.
    const auto dt=df==0?std::optional<std::size_t>{}:std::optional<std::size_t>{0};
    const auto ct=cf==0?std::optional<std::size_t>{}:std::optional<std::size_t>{0};
    apply(ds,d,move(ds.graph(),df,dt));apply(cs,c,move(cs.graph(),cf,ct));
    states.push_back({snapshot(ds),snapshot(cs),ds.graph(),cs.graph()});
  }
  for(std::size_t i=40;i>0;--i){
    require(ds.undo(ds.revision(),owner,d).accepted && cs.undo(cs.revision(),c).accepted,"Generated undo accepted");
    const auto &expected=states[i-1];require(snapshot(ds)==expected.doc && snapshot(cs)==expected.comp && ds.graph()==expected.dg && cs.graph()==expected.cg,"Generated undo restores coordinated snapshots");
  }
  for(std::size_t i=1;i<=40;++i){
    require(ds.redo(ds.revision(),owner,d).accepted && cs.redo(cs.revision(),c).accepted,"Generated redo accepted");
    require(snapshot(ds)==states[i].doc && snapshot(cs)==states[i].comp && ds.graph()==states[i].dg && cs.graph()==states[i].cg,"Generated redo exact");
  }
  require(ds.revision()==120 && cs.revision()==120,"Generated monotonic revisions across apply/undo/redo");
}
}
int main(){
  static_assert(!std::is_move_constructible_v<q::PreparedDocumentChange>);
  static_assert(!std::is_copy_constructible_v<q::PreparedCompositionChange>);
  try{document_mixed_history();tickets_scope_stale_aba_noop();external_conflicts_are_atomic();presentation_authority_and_frames();
      resma_and_owner_refusals();history_limits();composition_mixed_history_and_conflicts();composition_presentation_noops_and_budgets();generated_mixed_roundtrips();
      std::cout<<"INDE-OBJ-01: 9 session groups passed (40+40 generated applies, 80 undo and 80 redo, origin conflicts, bounded reversible history)\n";return 0;
  }catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

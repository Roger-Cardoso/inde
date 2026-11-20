#include "object_change_session.hpp"
#include "object_session_checkpoint.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
#include "inde/project/document_state.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace inde::objects::planning {
namespace {
struct Refusal { PlanDiagnostic diagnostic; };
[[noreturn]] void refuse(std::string code, std::string reason) {
  throw Refusal{{std::move(code), "session", std::move(reason)}};
}
void require_current(bool current, const std::vector<Diagnostic> &errors) {
  if (current) return;
  if (!errors.empty()) throw Refusal{{errors.front().code, errors.front().subject_id,
      "A origem externa mudou; a sessão não aplica nem rebaseia alterações sobre ela."}};
  refuse("session_origin_conflict", "A origem externa não corresponde à captura inicial.");
}
std::size_t graph_bytes(const Composition &g) {
  std::size_t n = sizeof(g) + g.id.size() + g.root_layout_id.size();
  auto optional = [&](const std::optional<std::string> &s) { if (s) n += s->size(); };
  for (const auto &o : g.objects) { n += sizeof(o) + o.id.size(); optional(o.resource_id); optional(o.layout_id); }
  for (const auto &o : g.occurrences) n += sizeof(o) + o.id.size() + o.object_id.size();
  for (const auto &l : g.layouts) {
    n += sizeof(l) + l.id.size();
    for (const auto *v : {&l.children, &l.paint_order}) for (const auto &id : *v) n += sizeof(id) + id.size();
  }
  for (const auto &r : g.resources) n += sizeof(r) + r.id.size() + r.sha256.size() + r.media_type.size();
  return n;
}
bool same_selection(application::TextSelection a, application::TextSelection b) {
  return a.cursor == b.cursor && a.bound == b.bound;
}
struct DocumentState {
  application::DocumentEditState value;
  Composition graph;
  std::string payload;
  std::size_t source_cost() const {
    // Reserve encoded timestamp room so acknowledging UTC persistence never
    // evicts history, even at an exact logical quota boundary.
    std::size_t encoded = 2;
    for (char raw : value.document.updated_at) {
      const auto ch = static_cast<unsigned char>(raw);
      encoded += (ch == '"' || ch == '\\' || ch == '\b' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t') ? 2U : (ch < 32 ? 6U : 1U);
    }
    return payload.size() - encoded + std::max<std::size_t>(1024, encoded);
  }
  bool same(const DocumentState &b) const {
    return payload == b.payload && graph == b.graph && same_selection(value.selection, b.value.selection);
  }
};
struct CompositionState {
  project::Composition value;
  Composition graph;
  std::string payload;
  std::size_t source_cost() const { return payload.size(); }
  bool same(const CompositionState &b) const { return payload == b.payload && graph == b.graph; }
};
template<class State> class Timeline {
public:
  using Pointer = std::shared_ptr<const State>;
  std::vector<Pointer> states;
  std::size_t cursor{};
  std::uint64_t revision{};
  SessionHistoryLimits limits;
  explicit Timeline(State initial, SessionHistoryLimits l) : limits(l) {
    if (l.steps > 64 || l.bytes > 128 * 1024 * 1024)
      throw std::invalid_argument("Sessão admite até 64 passos e 128 MiB de representação histórica");
    states.push_back(std::make_shared<const State>(std::move(initial)));
  }
  const State &current() const { return *states[cursor]; }
  bool can_undo() const { return cursor > 0; }
  bool can_redo() const { return cursor + 1 < states.size(); }
  static std::size_t cost(const State &s) { return sizeof(s) + s.source_cost() + graph_bytes(s.graph); }
  std::size_t history_bytes() const {
    if (states.size() == 1) return 0;
    std::size_t n{}; for (const auto &s : states) n += cost(*s); return n;
  }
  void check_revision(std::uint64_t expected) const {
    if (expected != revision) refuse("session_revision_conflict", "A sessão mudou desde a preparação ou revisão do pedido.");
  }
  void check_increment() const {
    if (revision == std::numeric_limits<std::uint64_t>::max())
      refuse("session_revision_exhausted", "A revisão da sessão foi esgotada.");
  }
  bool commit(State next) {
    if (current().same(next)) return false;
    check_increment();
    const auto pair = cost(current()) + cost(next);
    if (!limits.steps || pair > limits.bytes)
      refuse("session_history_budget", "A alteração não cabe como unidade reversível inteira no histórico.");
    // All allocations and checks precede the only state commit (vector swap).
    auto ready = std::vector<Pointer>(states.begin(), states.begin() + static_cast<std::ptrdiff_t>(cursor + 1));
    ready.push_back(std::make_shared<const State>(std::move(next)));
    std::size_t bytes{}; for (const auto &s : ready) bytes += cost(*s);
    std::size_t drop{};
    while (ready.size() - drop > limits.steps + 1 || bytes > limits.bytes) bytes -= cost(*ready[drop++]);
    ready.erase(ready.begin(), ready.begin() + static_cast<std::ptrdiff_t>(drop));
    static_assert(std::is_nothrow_swappable_v<decltype(states)>);
    states.swap(ready); cursor = states.size() - 1; ++revision;
    return true;
  }
  template<class Verify> void replay(bool forward, Verify verify) {
    if (forward ? !can_redo() : !can_undo()) refuse("session_history_empty", "Não há passo disponível nessa direção.");
    check_increment();
    const auto target = forward ? cursor + 1 : cursor - 1;
    verify(current(), *states[target], forward); // May throw; cursor/revision intact.
    cursor = target; ++revision;
  }
};
void verify_document_replay(const DocumentState &from, const DocumentState &to, bool forward) {
  if (project::same_document_state(from.value.document, to.value.document)) {
    if (!same_selection(from.value.selection, to.value.selection))
      refuse("session_selection_mismatch", "Mudança isolada de seleção não pertence a este executor.");
    return;
  }
  const auto &before = forward ? from : to, &after = forward ? to : from;
  application::DocumentEditSession engine;
  engine.open(before.value);
  if (!engine.commit(after.value, "OBJ-01: reordenação autoral"))
    refuse("session_owner_history_mismatch", "O motor não reconheceu a mudança autoral prevista.");
  auto result = engine.undo();
  if (forward) result = engine.redo();
  if (!result || persistence::encode_document_snapshot(result->document) != to.payload ||
      !same_selection(result->selection, to.value.selection))
    refuse("session_owner_history_mismatch", "O replay proprietário não reproduziu o snapshot e a seleção esperados.");
}
bool equivalent_graph(Composition a, Composition b) {
  auto sort = [](Composition &g) {
    const auto by_id = [](const auto &x, const auto &y) { return x.id < y.id; };
    std::sort(g.objects.begin(),g.objects.end(),by_id);
    std::sort(g.occurrences.begin(),g.occurrences.end(),by_id);
    std::sort(g.resources.begin(),g.resources.end(),by_id);
  };
  sort(a); sort(b); return a == b;
}
void no_save(const std::string &pending) {
  if (!pending.empty()) refuse("session_save_pending", "Conclua ou cancele o salvamento pendente antes de editar ou percorrer o histórico.");
}
void save_identity(const std::string &owner, const std::string &session, const std::string &pending,
    const std::string &intent, bool consumed, std::uint64_t current, std::uint64_t prepared) {
  if (owner != session) refuse("session_save_scope", "Pedido de salvamento pertence a outra sessão.");
  if (consumed || pending.empty() || pending != intent) refuse("session_save_consumed", "Pedido de salvamento não está mais pendente.");
  if (current != prepared) refuse("session_revision_conflict", "A revisão do pedido de salvamento não corresponde à sessão.");
}
bool utc_stamp(const std::string &s) {
  if (s.size()!=20) return false;
  for (std::size_t i=0;i<s.size();++i) {
    char expected{};
    if(i==4||i==7)expected='-';else if(i==10)expected='T';else if(i==13||i==16)expected=':';else if(i==19)expected='Z';
    if(expected ? s[i]!=expected : (s[i]<'0'||s[i]>'9'))return false;
  }
  return true;
}
bool document_receipt(const project::Document &candidate,const project::Document &expected,const project::Document &saved) {
  if(saved.created_at!=expected.created_at || !utc_stamp(saved.updated_at))return false;
  auto normalized=candidate;normalized.updated_at=saved.updated_at;
  return project::same_document_state(normalized,saved);
}
checkpoint_detail::Data capture(bool composition, bool pending, const std::string &project_id,
    const std::string &base, const std::string &working, const Composition &graph) {
  checkpoint_detail::Data d;d.composition=composition;d.pending=pending;d.project_id=project_id;d.base=base;d.working=working;
  d.layout=graph.layouts.at(0);d.frames=graph.occurrences;
  for(const auto &o:graph.objects)d.object_order.push_back(o.id);
  return d;
}
SessionResult failure(std::uint64_t revision, const Refusal &r) { return {false, false, revision, {r.diagnostic}}; }
SessionResult failure(std::uint64_t revision, const std::exception &e) {
  return {false, false, revision, {{"session_operation_failed", "session", e.what()}}};
}
} // namespace
struct DocumentChangeSession::Impl {
  std::unique_ptr<compat::DocumentReadProjection> origin;
  std::string id = project::new_uuid(), pending_save;
  void editable() const { no_save(pending_save); }
  Timeline<DocumentState> timeline;
  Impl(std::unique_ptr<compat::DocumentReadProjection> view, application::TextSelection selection, SessionHistoryLimits l)
      : origin(std::move(view)), timeline({{origin->source_snapshot(), selection}, origin->graph(),
          persistence::encode_document_snapshot(origin->source_snapshot())}, l) {
    application::DocumentEditSession check; check.open(timeline.current().value);
  }
  void check_origin(const std::string &project_id, const project::Document &d) const {
    const auto check = origin->check_source(project_id, d); require_current(check.current, check.diagnostics);
  }
};
DocumentChangeSession::DocumentChangeSession(const compat::DocumentReadProjection &v,
    application::TextSelection selection, SessionHistoryLimits limits) {
  auto copy = compat::read_document(v.source().project_id, v.source_snapshot());
  if (!copy.projection) throw std::invalid_argument("Captura documental não admite sessão íntegra");
  impl_ = std::make_unique<Impl>(std::move(copy.projection), selection, limits);
}
DocumentChangeSession::~DocumentChangeSession() = default;
const application::DocumentEditState &DocumentChangeSession::current() const { return impl_->timeline.current().value; }
const Composition &DocumentChangeSession::graph() const { return impl_->timeline.current().graph; }
const compat::DocumentSourceStamp &DocumentChangeSession::origin() const { return impl_->origin->source(); }
std::uint64_t DocumentChangeSession::revision() const { return impl_->timeline.revision; }
bool DocumentChangeSession::source_dirty() const { return !project::same_document_state(current().document, impl_->origin->source_snapshot()); }
bool DocumentChangeSession::dirty() const { return source_dirty() || !equivalent_graph(graph(), impl_->origin->graph()); }
bool DocumentChangeSession::can_undo() const { return impl_->timeline.can_undo(); }
bool DocumentChangeSession::can_redo() const { return impl_->timeline.can_redo(); }
std::size_t DocumentChangeSession::history_bytes() const { return impl_->timeline.history_bytes(); }
PreparationResult<PreparedDocumentChange> DocumentChangeSession::prepare(const ChangeRequest &request) const {
  if (!impl_->pending_save.empty()) return {{}, {{"session_save_pending", "session", "Salvamento pendente bloqueia novas prévias."}}};
  auto view = compat::read_document(origin().project_id, current().document);
  if (!view.projection) {
    std::vector<PlanDiagnostic> errors; for (const auto &e : view.diagnostics) errors.push_back({e.code, e.subject_id, "Estado de trabalho não admite captura."});
    return {{}, std::move(errors)};
  }
  auto result = plan_document_change(*view.projection, origin().project_id, current().document, request, current().selection, graph());
  if (!result.plan) return {{}, std::move(result.diagnostics)};
  auto ticket = std::unique_ptr<PreparedDocumentChange>(new PreparedDocumentChange);
  ticket->plan_ = std::move(result.plan); ticket->session_id_ = impl_->id; ticket->revision_ = revision();
  return {std::move(ticket), {}};
}
SessionResult DocumentChangeSession::apply(PreparedDocumentChange &ticket, const std::string &project_id,
    const project::Document &external) {
  try {
    impl_->editable();
    if (ticket.session_id_ != impl_->id) refuse("session_ticket_scope", "Prévia pertence a outra sessão.");
    if (ticket.consumed_) refuse("session_ticket_consumed", "Esta prévia já foi aplicada.");
    impl_->timeline.check_revision(ticket.revision_); impl_->check_origin(project_id, external);
    const auto &p = ticket.plan();
    const auto check = p.check_source(origin().project_id, current().document); require_current(check.current, check.diagnostics);
    if (p.before_graph() != graph() || !same_selection(p.before().selection, current().selection))
      refuse("session_plan_base_mismatch", "A apresentação ou seleção não corresponde à prévia preparada.");
    DocumentState candidate{p.preview(), p.after_graph(), persistence::encode_document_snapshot(p.preview().document)};
    verify_document_replay(impl_->timeline.current(), candidate, true);
    const bool changed = impl_->timeline.commit(std::move(candidate));
    ticket.consumed_ = true; return {true, changed, revision(), {}};
  } catch (const Refusal &r) { return failure(revision(), r); }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &e) { return failure(revision(), e); }
}
SessionResult DocumentChangeSession::replay(std::uint64_t expected, const std::string &project_id,
    const project::Document &external, bool forward) {
  try {
    impl_->editable(); impl_->timeline.check_revision(expected); impl_->check_origin(project_id, external);
    impl_->timeline.replay(forward, verify_document_replay);
    return {true, true, revision(), {}};
  } catch (const Refusal &r) { return failure(revision(), r); }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &e) { return failure(revision(), e); }
}
SessionResult DocumentChangeSession::undo(std::uint64_t expected, const std::string &id, const project::Document &d) { return replay(expected, id, d, false); }
SessionResult DocumentChangeSession::redo(std::uint64_t expected, const std::string &id, const project::Document &d) { return replay(expected, id, d, true); }

struct CompositionChangeSession::Impl {
  std::unique_ptr<compat::CompositionReadProjection> origin;
  std::string id = project::new_uuid(), pending_save;
  void editable() const { no_save(pending_save); }
  Timeline<CompositionState> timeline;
  Impl(std::unique_ptr<compat::CompositionReadProjection> view, SessionHistoryLimits l)
      : origin(std::move(view)), timeline({origin->source_snapshot().value, origin->graph(),
          persistence::encode_composition(origin->source_snapshot().value)}, l) {}
  // Internal working-capture context only. Original persisted revision/time are
  // never incremented, returned as a save result or offered for persistence.
  project::StoredComposition working() const {
    auto s = origin->source_snapshot(); s.value = timeline.current().value;
    s.sha256 = persistence::composition_checksum(timeline.current().payload); return s;
  }
  void check_origin(const project::StoredComposition &s) const {
    const auto check = origin->check_source(s); require_current(check.current, check.diagnostics);
  }
};
CompositionChangeSession::CompositionChangeSession(const compat::CompositionReadProjection &v, SessionHistoryLimits limits) {
  auto copy = compat::read_composition(v.source_snapshot());
  if (!copy.projection) throw std::invalid_argument("Captura da Composição não admite sessão íntegra");
  impl_ = std::make_unique<Impl>(std::move(copy.projection), limits);
}
CompositionChangeSession::~CompositionChangeSession() = default;
const project::Composition &CompositionChangeSession::current() const { return impl_->timeline.current().value; }
const Composition &CompositionChangeSession::graph() const { return impl_->timeline.current().graph; }
const compat::CompositionSourceStamp &CompositionChangeSession::origin() const { return impl_->origin->source(); }
std::uint64_t CompositionChangeSession::revision() const { return impl_->timeline.revision; }
bool CompositionChangeSession::source_dirty() const { return impl_->timeline.current().payload != persistence::encode_composition(impl_->origin->source_snapshot().value); }
bool CompositionChangeSession::dirty() const { return source_dirty() || !equivalent_graph(graph(), impl_->origin->graph()); }
bool CompositionChangeSession::can_undo() const { return impl_->timeline.can_undo(); }
bool CompositionChangeSession::can_redo() const { return impl_->timeline.can_redo(); }
std::size_t CompositionChangeSession::history_bytes() const { return impl_->timeline.history_bytes(); }
PreparationResult<PreparedCompositionChange> CompositionChangeSession::prepare(const ChangeRequest &request) const {
  if (!impl_->pending_save.empty()) return {{}, {{"session_save_pending", "session", "Salvamento pendente bloqueia novas prévias."}}};
  auto working = impl_->working(); auto view = compat::read_composition(working);
  if (!view.projection) {
    std::vector<PlanDiagnostic> errors; for (const auto &e : view.diagnostics) errors.push_back({e.code, e.subject_id, "Estado de trabalho não admite captura."});
    return {{}, std::move(errors)};
  }
  auto result = plan_composition_change(*view.projection, working, request, graph());
  if (!result.plan) return {{}, std::move(result.diagnostics)};
  auto ticket = std::unique_ptr<PreparedCompositionChange>(new PreparedCompositionChange);
  ticket->plan_ = std::move(result.plan); ticket->session_id_ = impl_->id; ticket->revision_ = revision();
  return {std::move(ticket), {}};
}
SessionResult CompositionChangeSession::apply(PreparedCompositionChange &ticket, const project::StoredComposition &external) {
  try {
    impl_->editable();
    if (ticket.session_id_ != impl_->id) refuse("session_ticket_scope", "Prévia pertence a outra sessão.");
    if (ticket.consumed_) refuse("session_ticket_consumed", "Esta prévia já foi aplicada.");
    impl_->timeline.check_revision(ticket.revision_); impl_->check_origin(external);
    const auto &p = ticket.plan(); const auto working = impl_->working();
    const auto check = p.check_source(working); require_current(check.current, check.diagnostics);
    if (p.before_graph() != graph()) refuse("session_plan_base_mismatch", "Apresentação não corresponde à prévia preparada.");
    const bool changed = impl_->timeline.commit({p.preview(), p.after_graph(), persistence::encode_composition(p.preview())});
    ticket.consumed_ = true; return {true, changed, revision(), {}};
  } catch (const Refusal &r) { return failure(revision(), r); }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &e) { return failure(revision(), e); }
}
SessionResult CompositionChangeSession::replay(std::uint64_t expected, const project::StoredComposition &external, bool forward) {
  try {
    impl_->editable(); impl_->timeline.check_revision(expected); impl_->check_origin(external);
    impl_->timeline.replay(forward, [](const auto &, const auto &, bool) {});
    return {true, true, revision(), {}};
  } catch (const Refusal &r) { return failure(revision(), r); }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &e) { return failure(revision(), e); }
}
SessionResult CompositionChangeSession::undo(std::uint64_t expected, const project::StoredComposition &s) { return replay(expected, s, false); }
SessionResult CompositionChangeSession::redo(std::uint64_t expected, const project::StoredComposition &s) { return replay(expected, s, true); }
bool DocumentChangeSession::save_pending() const { return !impl_->pending_save.empty(); }
PreparationResult<PreparedDocumentSave> DocumentChangeSession::begin_save() {
  try {
    impl_->editable();impl_->timeline.check_increment();
    if(!source_dirty())refuse("session_source_not_dirty","A fonte não mudou; a apresentação não é gravável pelo schema autoral atual.");
    auto t=std::unique_ptr<PreparedDocumentSave>(new PreparedDocumentSave);
    t->session_id_=impl_->id;t->intent_id_=project::new_uuid();t->project_id_=origin().project_id;t->revision_=revision();
    t->expected_=impl_->origin->source_snapshot();t->candidate_=current().document;
    auto pending=t->intent_id_;impl_->pending_save.swap(pending);return {std::move(t),{}};
  }catch(const Refusal&r){return {{},{r.diagnostic}};}
}
SessionResult DocumentChangeSession::acknowledge_save(PreparedDocumentSave&t,const std::string &project_id,
    const project::Document &readback) {
  try {
    save_identity(impl_->id,t.session_id_,impl_->pending_save,t.intent_id_,t.consumed_,revision(),t.revision_);
    impl_->timeline.check_increment();
    if(project_id!=origin().project_id || !document_receipt(t.candidate_,t.expected_,readback))
      refuse("session_save_receipt_mismatch","O readback não reproduz o candidato salvo, sua identidade e timestamps permitidos.");
    auto projection=compat::read_document(project_id,readback);
    if(!projection.projection)refuse("session_save_receipt_invalid","Readback documental não admite captura íntegra.");
    auto ready=impl_->timeline.states;
    for(std::size_t i=0;i<ready.size();++i) {
      auto &state=ready[i];
      auto next=*state;next.value.document.updated_at=readback.updated_at;
      if(i==impl_->timeline.cursor)next.value.document=projection.projection->source_snapshot();
      next.payload=persistence::encode_document_snapshot(next.value.document);
      if(Timeline<DocumentState>::cost(next)>Timeline<DocumentState>::cost(*state))
        refuse("session_save_history_budget","O reconhecimento não pode ampliar a representação histórica reservada.");
      state=std::make_shared<const DocumentState>(std::move(next));
    }
    impl_->timeline.states.swap(ready);impl_->origin.swap(projection.projection);++impl_->timeline.revision;
    impl_->pending_save.clear();t.consumed_=true;return {true,true,revision(),{}};
  }catch(const Refusal&r){return failure(revision(),r);}catch(const std::bad_alloc&){throw;}catch(const std::exception&e){return failure(revision(),e);}
}
SessionResult DocumentChangeSession::cancel_save(PreparedDocumentSave&t,const std::string &project_id,const project::Document &external) {
  try {
    save_identity(impl_->id,t.session_id_,impl_->pending_save,t.intent_id_,t.consumed_,revision(),t.revision_);
    impl_->check_origin(project_id,external);impl_->pending_save.clear();t.consumed_=true;return {true,false,revision(),{}};
  }catch(const Refusal&r){return failure(revision(),r);}catch(const std::bad_alloc&){throw;}catch(const std::exception&e){return failure(revision(),e);}
}
SessionCheckpoint DocumentChangeSession::checkpoint() const {
  auto d=capture(false,save_pending(),origin().project_id,persistence::encode_document_snapshot(impl_->origin->source_snapshot()),
      impl_->timeline.current().payload,graph());d.selection=current().selection;return checkpoint_detail::encode(d);
}
SessionRecoveryResult<DocumentChangeSession> DocumentChangeSession::restore(const SessionCheckpoint &checkpoint,
    const std::string &project_id,const project::Document &external,RecoveryMode mode,SessionHistoryLimits limits) {
  try {
    auto d=checkpoint_detail::decode(checkpoint);
    if(d.composition || d.persisted_revision!=0 || !d.timestamp.empty() || d.project_id!=project_id || (mode!=RecoveryMode::ResumeUnsaved&&mode!=RecoveryMode::AdoptMatchingSave))
      refuse("session_recovery_scope","Checkpoint ou modo não corresponde ao Documento/projeto.");
    auto base=compat::read_document(project_id,persistence::decode_document_snapshot(d.base));
    auto work=compat::read_document(project_id,persistence::decode_document_snapshot(d.working));
    if(!base.projection||!work.projection||base.projection->source().document_id!=work.projection->source().document_id)
      refuse("session_recovery_source_invalid","As fontes do checkpoint não possuem identidade e estrutura compatíveis.");
    auto presentation=checkpoint_detail::presentation(d,work.projection->graph());
    auto checked=plan_document_change(*work.projection,project_id,work.projection->source_snapshot(),{},d.selection,presentation);
    if(!checked.plan)refuse("session_recovery_presentation_invalid","Apresentação/seleção não corresponde à fonte capturada.");
    std::unique_ptr<DocumentChangeSession> result;
    project::Document working=work.projection->source_snapshot();
    if(mode==RecoveryMode::ResumeUnsaved) {
      const auto check=base.projection->check_source(project_id,external);require_current(check.current,check.diagnostics);
      result=std::make_unique<DocumentChangeSession>(*base.projection,d.selection,limits);
    }else {
      if(!d.pending || !document_receipt(working,base.projection->source_snapshot(),external))
        refuse("session_recovery_commit_mismatch","A origem não corresponde ao candidato de um save interrompido.");
      auto saved=compat::read_document(project_id,external);if(!saved.projection)refuse("session_recovery_source_invalid","Readback inválido.");
      result=std::make_unique<DocumentChangeSession>(*saved.projection,d.selection,limits);working=external;
    }
    // A restart gets a new session identity and no old tickets/history. The full
    // private working snapshot/selection/presentation survives, not undo entries.
    DocumentState value{{std::move(working),d.selection},std::move(presentation),{}};
    value.payload=persistence::encode_document_snapshot(value.value.document);
    result->impl_->timeline.states[0]=std::make_shared<const DocumentState>(std::move(value));
    return {std::move(result),{}};
  }catch(const Refusal&r){return {{},{r.diagnostic}};}catch(const std::bad_alloc&){throw;}catch(const std::exception&e){return {{},{{"session_recovery_invalid","checkpoint",e.what()}}};}
}
bool CompositionChangeSession::save_pending() const { return !impl_->pending_save.empty(); }
PreparationResult<PreparedCompositionSave> CompositionChangeSession::begin_save() {
  try {
    impl_->editable();impl_->timeline.check_increment();if(!source_dirty())refuse("session_source_not_dirty","A fonte não mudou; apresentação visual não altera o agregado persistido.");
    if(origin().revision==std::numeric_limits<std::int64_t>::max())refuse("session_persisted_revision_exhausted","Revisão persistida esgotada.");
    auto t=std::unique_ptr<PreparedCompositionSave>(new PreparedCompositionSave);
    t->session_id_=impl_->id;t->intent_id_=project::new_uuid();t->revision_=revision();
    t->expected_=impl_->origin->source_snapshot();t->candidate_=current();auto pending=t->intent_id_;
    impl_->pending_save.swap(pending);return {std::move(t),{}};
  }catch(const Refusal&r){return {{},{r.diagnostic}};}
}
SessionResult CompositionChangeSession::acknowledge_save(PreparedCompositionSave&t,const project::StoredComposition &readback) {
  try {
    save_identity(impl_->id,t.session_id_,impl_->pending_save,t.intent_id_,t.consumed_,revision(),t.revision_);
    impl_->timeline.check_increment();
    if(readback.project_id!=origin().project_id || readback.revision!=t.expected_.revision+1 || !utc_stamp(readback.updated_at) ||
       persistence::encode_composition(readback.value)!=persistence::encode_composition(t.candidate_))
      refuse("session_save_receipt_mismatch","Readback não corresponde ao candidato e à revisão seguinte do CAS.");
    auto projection=compat::read_composition(readback);
    if(!projection.projection)refuse("session_save_receipt_invalid","Readback da Composição inválido.");
    impl_->origin.swap(projection.projection);++impl_->timeline.revision;impl_->pending_save.clear();t.consumed_=true;
    return {true,true,revision(),{}};
  }catch(const Refusal&r){return failure(revision(),r);}catch(const std::bad_alloc&){throw;}catch(const std::exception&e){return failure(revision(),e);}
}
SessionResult CompositionChangeSession::cancel_save(PreparedCompositionSave&t,const project::StoredComposition &external) {
  try {
    save_identity(impl_->id,t.session_id_,impl_->pending_save,t.intent_id_,t.consumed_,revision(),t.revision_);
    impl_->check_origin(external);impl_->pending_save.clear();t.consumed_=true;return {true,false,revision(),{}};
  }catch(const Refusal&r){return failure(revision(),r);}catch(const std::bad_alloc&){throw;}catch(const std::exception&e){return failure(revision(),e);}
}
SessionCheckpoint CompositionChangeSession::checkpoint() const {
  auto d=capture(true,save_pending(),origin().project_id,persistence::encode_composition(impl_->origin->source_snapshot().value),
      impl_->timeline.current().payload,graph());d.persisted_revision=origin().revision;d.timestamp=origin().updated_at;
  return checkpoint_detail::encode(d);
}
SessionRecoveryResult<CompositionChangeSession> CompositionChangeSession::restore(const SessionCheckpoint &checkpoint,
    const project::StoredComposition &external,RecoveryMode mode,SessionHistoryLimits limits) {
  try {
    auto d=checkpoint_detail::decode(checkpoint);
    if(!d.composition || d.selection.cursor!=0 || d.selection.bound!=0 || d.project_id!=external.project_id || d.persisted_revision<1 || d.timestamp.empty() ||
       (mode!=RecoveryMode::ResumeUnsaved&&mode!=RecoveryMode::AdoptMatchingSave))
      refuse("session_recovery_scope","Checkpoint ou modo não corresponde à Composição/projeto.");
    const auto hash=persistence::composition_checksum(d.base);
    project::StoredComposition baseline{persistence::decode_composition(d.base,hash),d.project_id,hash,d.timestamp,d.persisted_revision};
    auto base=compat::read_composition(baseline);
    auto working=persistence::decode_composition(d.working,persistence::composition_checksum(d.working));
    auto envelope=baseline;envelope.value=working;envelope.sha256=persistence::composition_checksum(d.working);
    auto work=compat::read_composition(envelope);
    if(!base.projection||!work.projection||base.projection->source().composition_id!=work.projection->source().composition_id)
      refuse("session_recovery_source_invalid","As fontes do checkpoint não possuem identidade e estrutura compatíveis.");
    auto presentation=checkpoint_detail::presentation(d,work.projection->graph());
    std::unique_ptr<CompositionChangeSession> result;
    if(mode==RecoveryMode::ResumeUnsaved) {
      const auto check=base.projection->check_source(external);require_current(check.current,check.diagnostics);
      result=std::make_unique<CompositionChangeSession>(*base.projection,limits);
    }else {
      if(!d.pending || d.persisted_revision==std::numeric_limits<std::int64_t>::max() || external.revision!=d.persisted_revision+1 ||
         !utc_stamp(external.updated_at) || persistence::encode_composition(external.value)!=d.working)
        refuse("session_recovery_commit_mismatch","Origem não corresponde ao candidato de um save interrompido.");
      auto saved=compat::read_composition(external);if(!saved.projection)refuse("session_recovery_source_invalid","Readback inválido.");
      result=std::make_unique<CompositionChangeSession>(*saved.projection,limits);working=external.value;
    }
    result->impl_->timeline.states[0]=std::make_shared<const CompositionState>(CompositionState{std::move(working),std::move(presentation),d.working});
    return {std::move(result),{}};
  }catch(const Refusal&r){return {{},{r.diagnostic}};}catch(const std::bad_alloc&){throw;}catch(const std::exception&e){return {{},{{"session_recovery_invalid","checkpoint",e.what()}}};}
}
} // namespace inde::objects::planning

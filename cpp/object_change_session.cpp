#include "object_change_session.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/manifest.hpp"
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
  bool same(const DocumentState &b) const {
    return payload == b.payload && graph == b.graph && same_selection(value.selection, b.value.selection);
  }
};
struct CompositionState {
  project::Composition value;
  Composition graph;
  std::string payload;
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
  static std::size_t cost(const State &s) { return sizeof(s) + s.payload.size() + graph_bytes(s.graph); }
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
  if (from.payload == to.payload) {
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
SessionResult failure(std::uint64_t revision, const Refusal &r) { return {false, false, revision, {r.diagnostic}}; }
SessionResult failure(std::uint64_t revision, const std::exception &e) {
  return {false, false, revision, {{"session_operation_failed", "session", e.what()}}};
}
} // namespace
struct DocumentChangeSession::Impl {
  std::unique_ptr<compat::DocumentReadProjection> origin;
  std::string id = project::new_uuid();
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
bool DocumentChangeSession::source_dirty() const { return impl_->timeline.current().payload != persistence::encode_document_snapshot(impl_->origin->source_snapshot()); }
bool DocumentChangeSession::dirty() const { return source_dirty() || graph() != impl_->origin->graph(); }
bool DocumentChangeSession::can_undo() const { return impl_->timeline.can_undo(); }
bool DocumentChangeSession::can_redo() const { return impl_->timeline.can_redo(); }
std::size_t DocumentChangeSession::history_bytes() const { return impl_->timeline.history_bytes(); }
PreparationResult<PreparedDocumentChange> DocumentChangeSession::prepare(const ChangeRequest &request) const {
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
    impl_->timeline.check_revision(expected); impl_->check_origin(project_id, external);
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
  std::string id = project::new_uuid();
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
bool CompositionChangeSession::dirty() const { return source_dirty() || graph() != impl_->origin->graph(); }
bool CompositionChangeSession::can_undo() const { return impl_->timeline.can_undo(); }
bool CompositionChangeSession::can_redo() const { return impl_->timeline.can_redo(); }
std::size_t CompositionChangeSession::history_bytes() const { return impl_->timeline.history_bytes(); }
PreparationResult<PreparedCompositionChange> CompositionChangeSession::prepare(const ChangeRequest &request) const {
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
    impl_->timeline.check_revision(expected); impl_->check_origin(external);
    impl_->timeline.replay(forward, [](const auto &, const auto &, bool) {});
    return {true, true, revision(), {}};
  } catch (const Refusal &r) { return failure(revision(), r); }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &e) { return failure(revision(), e); }
}
SessionResult CompositionChangeSession::undo(std::uint64_t expected, const project::StoredComposition &s) { return replay(expected, s, false); }
SessionResult CompositionChangeSession::redo(std::uint64_t expected, const project::StoredComposition &s) { return replay(expected, s, true); }
} // namespace inde::objects::planning

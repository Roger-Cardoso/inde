#include "object_change_plan.hpp"
#include "inde/application/document_reading_order.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include <algorithm>
#include <new>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace inde::objects::planning {
namespace {
struct Refusal { PlanDiagnostic diagnostic; };
[[noreturn]] void refuse(std::string code, std::string id, std::string reason) {
  throw Refusal{{std::move(code), std::move(id), std::move(reason)}};
}
std::vector<PlanDiagnostic> explain(const std::vector<Diagnostic> &errors, const std::string &reason) {
  std::vector<PlanDiagnostic> result;
  for (const auto &d : errors) result.push_back({d.code, d.subject_id, reason});
  return result;
}
void preflight(const ChangeRequest &r) {
  if (r.purpose != ChangePurpose::VisualOnly && r.purpose != ChangePurpose::AuthorialReorder)
    refuse("unknown_change_purpose", "request", "Declare visualização ou reordenação autoral explicitamente.");
  if (r.commands.size() > 64) refuse("plan_command_limit", "request", "O plano admite até 64 comandos.");
  std::size_t entries = 20000, bytes = 4 * 1024 * 1024;
  auto id = [&](const std::string &s) {
    if (!entries || s.size() > 512 || s.size() > bytes)
      refuse("plan_request_limit", "request", "IDs ou tamanho do pedido excedem o limite do plano.");
    --entries; bytes -= s.size();
  };
  auto list = [&](const auto &values) {
    if (values.size() > entries) refuse("plan_request_limit", "request", "Seleção ou atribuições excedem o limite do plano.");
    for (const auto &v : values) {
      if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) id(v);
      else id(v.occurrence_id);
    }
  };
  for (const auto &command : r.commands) {
    if (command.valueless_by_exception()) refuse("invalid_plan_command", "request", "Comando sem valor válido.");
    if (r.purpose == ChangePurpose::AuthorialReorder && !std::holds_alternative<ReorderOccurrences>(command))
      refuse("unsupported_authorial_command", "request", "Este corte só traduz reordenação no layout raiz; geometria, arranjos e transferências requerem intenção visual.");
    std::visit([&](const auto &v) {
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, ReorderOccurrences>) {
        id(v.layout_id); list(v.occurrence_ids); if (v.before) id(*v.before);
      } else if constexpr (std::is_same_v<T, TransferOccurrences>) {
        list(v.occurrence_ids); id(v.destination_layout_id); if (v.before) id(*v.before); list(v.replacements);
      } else if constexpr (std::is_same_v<T, SetFrame>) id(v.occurrence_id);
      else if constexpr (std::is_same_v<T, SetPaintOrder>) { id(v.layout_id); list(v.order); }
      else { id(v.layout_id); list(v.replacements); }
    }, command);
  }
}
Composition target_graph(const Composition &base, const ChangeRequest &r) {
  CompositionEditor copy(base, 0);
  const auto result = copy.apply(0, r.commands);
  if (!result.accepted) {
    if (result.diagnostics.empty()) refuse("graph_plan_refused", base.id, "Comando de objetos recusado.");
    const auto &d = result.diagnostics.front();
    refuse(d.code, d.subject_id, "O núcleo de objetos recusou o pedido; nenhum plano parcial foi produzido.");
  }
  return copy.current();
}
std::pair<std::string, std::string> document_move(const compat::DocumentReadProjection &base, const ReorderOccurrences &r) {
  if (r.layout_id != base.graph().root_layout_id)
    refuse("unsupported_authorial_layout", r.layout_id, "Reordenação autoral exige o layout raiz da captura.");
  std::unordered_map<std::string, const compat::ReadingGroupBinding *> groups;
  for (const auto &g : base.reading_groups()) for (const auto &id : g.occurrence_ids) groups.emplace(id, &g);
  if (r.occurrence_ids.empty()) refuse("empty_selection", base.source().document_id, "Selecione um grupo de leitura completo.");
  const auto found = groups.find(r.occurrence_ids.front());
  if (found == groups.end()) refuse("unknown_source_occurrence", r.occurrence_ids.front(), "Ocorrência não pertence à captura documental.");
  const auto &g = *found->second;
  const std::unordered_set<std::string> chosen(r.occurrence_ids.begin(), r.occurrence_ids.end());
  if (chosen.size() != r.occurrence_ids.size() || chosen.size() != g.occurrence_ids.size() ||
      std::any_of(g.occurrence_ids.begin(), g.occurrence_ids.end(), [&](const auto &id) { return !chosen.contains(id); }))
    refuse("whole_reading_group_required", g.source_block_id, "Selecione exatamente um grupo inteiro; resmas de várias linhas não serão divididas nem ampliadas automaticamente.");
  std::string destination;
  if (r.before) {
    const auto target = groups.find(*r.before);
    if (target == groups.end()) refuse("unknown_source_occurrence", *r.before, "Vizinho não pertence à captura documental.");
    if (target->second->occurrence_ids.front() != *r.before)
      refuse("destination_splits_reading_group", target->second->source_block_id, "O destino deve ser o início de outro grupo de leitura.");
    destination = target->second->source_block_id;
  }
  return {g.source_block_id, destination};
}
} // namespace
DocumentPlanResult plan_document_change(const compat::DocumentReadProjection &base,
    const std::string &project_id, const project::Document &current,
    const ChangeRequest &request, application::TextSelection selection) {
  try {
    preflight(request);
    const auto source = base.check_source(project_id, current);
    if (!source.current) return {{}, explain(source.diagnostics, "A fonte fornecida não corresponde à captura; recapture antes de planejar.")};
    const auto size = project::utf8_character_count(base.source_snapshot().content);
    if (selection.cursor > size || selection.bound > size)
      refuse("invalid_text_selection", base.source().document_id, "Seleção Unicode fora da fonte capturada.");
    if (request.purpose == ChangePurpose::AuthorialReorder && request.commands.size() > 1)
      refuse("document_authorial_batch_unsupported", base.source().document_id, "Este corte traduz um movimento de grupo por plano documental.");
    auto target = target_graph(base.graph(), request);
    auto p = std::unique_ptr<DocumentChangePlan>(new DocumentChangePlan);
    p->source_ = base.source(); p->before_graph_ = base.graph(); p->after_graph_ = std::move(target);
    p->before_ = {base.source_snapshot(), selection}; p->after_ = p->before_;
    if (request.purpose == ChangePurpose::AuthorialReorder && !request.commands.empty()) {
      const auto &r = std::get<ReorderOccurrences>(request.commands.front());
      const auto [block, destination] = document_move(base, r);
      try { p->after_ = application::move_reading_block(p->before_, block, destination); }
      catch (const std::bad_alloc &) { throw; }
      catch (const std::exception &error) { refuse("source_text_engine_refused", block, error.what()); }
      auto rebound = compat::read_document(project_id, p->after_.document);
      if (!rebound.projection) return {{}, explain(rebound.diagnostics, "O resultado textual não admite uma captura íntegra; plano recusado.")};
      if (rebound.projection->graph().layouts[0].children != p->after_graph_.layouts[0].children)
        refuse("document_order_translation_mismatch", block, "A ordem resultante do motor textual não corresponde ao pedido de objetos.");
    }
    const auto before = persistence::encode_document_snapshot(p->before_.document);
    const auto after = persistence::encode_document_snapshot(p->after_.document);
    p->preview_sha256_ = persistence::composition_checksum(after);
    p->review_ = {request.purpose, p->before_graph_ != p->after_graph_, before != after};
    return {std::move(p), {}};
  } catch (const Refusal &r) { return {{}, {r.diagnostic}}; }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &error) { return {{}, {{"document_plan_failed", base.source().document_id, error.what()}}}; }
}
CompositionPlanResult plan_composition_change(const compat::CompositionReadProjection &base,
    const project::StoredComposition &current, const ChangeRequest &request) {
  try {
    preflight(request);
    const auto source = base.check_source(current);
    if (!source.current) return {{}, explain(source.diagnostics, "A Composição fornecida mudou; recapture antes de planejar.")};
    for (const auto &command : request.commands)
      if (request.purpose == ChangePurpose::AuthorialReorder &&
          std::get<ReorderOccurrences>(command).layout_id != base.graph().root_layout_id)
        refuse("unsupported_authorial_layout", base.source().composition_id, "Reordenação autoral exige o layout raiz da captura.");
    auto target = target_graph(base.graph(), request);
    auto p = std::unique_ptr<CompositionChangePlan>(new CompositionChangePlan);
    p->source_ = base.source(); p->before_graph_ = base.graph(); p->after_graph_ = std::move(target);
    p->before_ = base.source_snapshot().value; p->after_ = p->before_;
    std::unordered_map<std::string, std::string> bindings;
    for (const auto &b : base.occurrences()) { bindings.emplace(b.occurrence_id, b.source_occurrence_id); p->source_order_.push_back(b.source_occurrence_id); }
    p->preview_order_ = p->source_order_;
    if (request.purpose == ChangePurpose::AuthorialReorder) {
      p->preview_order_.clear();
      for (const auto &id : p->after_graph_.layouts[0].children) p->preview_order_.push_back(bindings.at(id));
      p->after_.blocks.reorder(p->preview_order_);
      // Enforce the narrow contract: restoring ONLY order must reproduce every
      // byte of the source aggregate, including drafts, modes and frozen origins.
      auto restored = p->after_; restored.blocks.reorder(p->source_order_);
      if (persistence::encode_composition(restored) != persistence::encode_composition(p->before_))
        refuse("composition_non_order_change", base.source().composition_id, "O candidato alterou dados além da sequência de ocorrências.");
    }
    const auto before = persistence::encode_composition(p->before_), after = persistence::encode_composition(p->after_);
    p->preview_sha256_ = persistence::composition_checksum(after);
    p->before_reading_ = p->before_.blocks.project(p->before_.profile);
    p->after_reading_ = p->after_.blocks.project(p->after_.profile);
    p->review_ = {request.purpose, p->before_graph_ != p->after_graph_, before != after};
    return {std::move(p), {}};
  } catch (const Refusal &r) { return {{}, {r.diagnostic}}; }
    catch (const std::bad_alloc &) { throw; }
    catch (const std::exception &error) { return {{}, {{"composition_plan_failed", base.source().composition_id, error.what()}}}; }
}
compat::SourceCheck DocumentChangePlan::check_source(const std::string &project_id, const project::Document &d) const {
  if (project_id != source_.project_id) return {false, {{"source_project_conflict", "project"}}};
  if (d.id != source_.document_id) return {false, {{"source_document_conflict", "document"}}};
  auto r = compat::read_document(project_id, d);
  if (!r.projection) return {false, std::move(r.diagnostics)};
  if (r.projection->source() != source_) return {false, {{"source_snapshot_changed", d.id}}};
  return {true, {}};
}
compat::CompositionSourceCheck CompositionChangePlan::check_source(const project::StoredComposition &s) const {
  if (s.project_id != source_.project_id) return {false, {{"source_project_conflict", "project"}}};
  if (s.value.blocks.composition_id() != source_.composition_id) return {false, {{"source_composition_conflict", "composition"}}};
  auto r = compat::read_composition(s);
  if (!r.projection) return {false, std::move(r.diagnostics)};
  if (r.projection->source().revision != source_.revision) return {false, {{"source_composition_revision_changed", source_.composition_id}}};
  if (r.projection->source() != source_) return {false, {{"source_composition_snapshot_changed", source_.composition_id}}};
  return {true, {}};
}
} // namespace inde::objects::planning

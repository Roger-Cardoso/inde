#include "inde/experimental/block_layers.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include <algorithm>
#include <glib.h>
#include <set>
#include <stdexcept>
namespace inde::experimental {
LayerSnapshot slice_layers(const project::Document &source, std::size_t start, std::size_t end) {
  project::validate(source); const auto count = project::utf8_character_count(source.content);
  if (start > end || end > count) throw std::runtime_error("Faixa de extração inválida");
  auto d = source;
  d.content = {g_utf8_offset_to_pointer(source.content.c_str(), start), g_utf8_offset_to_pointer(source.content.c_str(), end)};
  d.formatting.clear(); d.anchors.clear(); d.entity_references.clear(); d.semantics = {};
  LayerSnapshot result; const bool whole = start == 0 && end == count;
  std::size_t crossed{}, global{}, relations{}, refs{};
  for (auto span : source.formatting) {
    const auto a = std::max(start, span.start_offset), b = std::min(end, span.end_offset);
    if (a < b) { span.start_offset = a - start; span.end_offset = b - start; d.formatting.push_back(span); }
  }
  std::set<std::string> anchor_ids, annotation_ids;
  for (auto anchor : source.anchors) {
    const bool point = anchor.start_offset == anchor.end_offset;
    const bool contained = anchor.start_offset >= start && anchor.end_offset <= end &&
        (!point || anchor.start_offset < end || end == count);
    if (contained) {
      anchor_ids.insert(anchor.id); anchor.start_offset -= start; anchor.end_offset -= start;
      d.anchors.push_back(anchor);
    } else if (anchor.start_offset < end && anchor.end_offset > start) ++crossed;
  }
  for (auto e : source.semantics.anchors) if (anchor_ids.contains(e.anchor_id)) {
    auto anchor = std::find_if(d.anchors.begin(), d.anchors.end(), [&](auto &a) { return a.id == e.anchor_id; });
    auto refreshed = application::SemanticAnchorEngine::evidence(d, *anchor);
    refreshed.state = e.state; refreshed.reason = e.reason; refreshed.revision = e.revision;
    refreshed.start_gravity = e.start_gravity; refreshed.end_gravity = e.end_gravity;
    d.semantics.anchors.push_back(refreshed);
  }
  for (auto annotation : source.semantics.annotations) {
    const bool is_global = annotation.scope == project::AnnotationScope::Document;
    if ((!is_global && anchor_ids.contains(annotation.anchor_id)) || (is_global && whole)) {
      annotation_ids.insert(annotation.id); d.semantics.annotations.push_back(annotation);
    } else if (is_global) ++global;
  }
  for (auto relation : source.semantics.relations) {
    if (annotation_ids.contains(relation.source_annotation_id) && annotation_ids.contains(relation.target_annotation_id)) d.semantics.relations.push_back(relation);
    else if (annotation_ids.contains(relation.source_annotation_id) || annotation_ids.contains(relation.target_annotation_id)) ++relations;
  }
  for (auto ref : source.entity_references) {
    if ((ref.anchor_id && anchor_ids.contains(*ref.anchor_id)) || (!ref.anchor_id && whole)) d.entity_references.push_back(ref);
    else if (!ref.anchor_id) ++refs;
  }
  if (crossed) result.warnings.push_back(std::to_string(crossed) + " âncora(s) atravessam o corte: continuam na origem e não foram reassociadas ao fragmento.");
  if (global) result.warnings.push_back(std::to_string(global) + " declaração(ões) global(is) pertencem ao Documento inteiro: não foram reinterpretadas como contexto deste fragmento.");
  if (relations) result.warnings.push_back(std::to_string(relations) + " relação(ões) semântica(s) atravessam o recorte e ficam na origem.");
  if (refs) result.warnings.push_back(std::to_string(refs) + " referência(s) de entidade sem faixa textual ficam na origem do recorte.");
  if (!whole) {
    if (!d.inline_commands.empty()) result.warnings.push_back("Modelos inline da origem inteira não são herdados automaticamente pelo fragmento.");
    d.inline_commands.clear();
    if (!d.perspective.empty()) result.warnings.push_back("Perspectiva documental da origem não foi atribuída ao fragmento.");
    d.perspective.clear();
  }
  // Organization belongs to source, not to the extracted body.
  d.editorial_node_id.reset(); d.group_id.reset(); d.revision_of_id.reset(); d.word_goal.reset(); d.revision_label.clear();
  project::validate(d); result.document = std::make_shared<const project::Document>(std::move(d)); return result;
}
LayerSnapshot edit_layers(const LayerSnapshot &before, const std::string &text) {
  auto result = before;
  if (!before.document) return result;
  const auto &old = before.document->content; const auto delta = application::text_delta(old, text);
  const auto start = project::utf8_character_count(old.substr(0, delta.byte_offset));
  const auto end = start + project::utf8_character_count(delta.removed);
  auto next = application::replace_selection({*before.document, {start, end}}, start, end, delta.inserted).document;
  if (next.content != text) throw std::runtime_error("A conservação de camadas divergiu do rascunho");
  const auto unresolved = std::count_if(next.semantics.anchors.begin(), next.semantics.anchors.end(),
      [](auto &e) { return e.state != project::AnchorResolution::Resolved; });
  if (unresolved) result.warnings.push_back(std::to_string(unresolved) + " âncora(s) semântica(s) precisam de revisão após editar; nenhum vínculo foi reassociado automaticamente.");
  result.document = std::make_shared<const project::Document>(std::move(next)); return result;
}
}

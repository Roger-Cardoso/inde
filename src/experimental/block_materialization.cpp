#include "inde/experimental/block_layers.hpp"
#include "inde/experimental/text_blocks.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
namespace inde::experimental {
MaterializedDocument materialize(const TextBlocksLab &lab, const TextProjection &projection, const std::string &title) {
  MaterializedDocument out; auto &d = out.document;
  d.id = project::new_uuid(); d.title = title; d.content = projection.text;
  d.created_at = d.updated_at = project::utc_now();
  out.incorporated_occurrences = std::count_if(projection.segments.begin(), projection.segments.end(), [](auto &s) { return !s.reference_only; });
  out.reference_occurrences = projection.segments.size() - out.incorporated_occurrences;
  std::map<std::pair<std::string, std::string>, project::DocumentInlineCommand> commands;
  std::set<std::pair<std::string, std::string>> conflicts;
  std::set<std::string> unanchored_entities;
  for (const auto &segment : projection.segments) {
    if (segment.reference_only) continue;
    const auto &revision = lab.version(segment.block_id, segment.revision_id);
    const auto label = lab.block(segment.block_id).label;
    for (const auto &warning : revision.layers.warnings) out.warnings.push_back(label + ": " + warning);
    if (!revision.layers.document) {
      out.warnings.push_back(label + ": versão sem snapshot de camadas; somente o texto foi incorporado."); continue;
    }
    const auto &source = *revision.layers.document;
    if (source.content != revision.text) throw std::runtime_error("Versão e snapshot de camadas incompatíveis");
    std::map<std::string, std::string> anchors, annotations;
    for (auto span : source.formatting) {
      span.start_offset += segment.start; span.end_offset += segment.start; d.formatting.push_back(span);
    }
    for (auto anchor : source.anchors) {
      const auto old = anchor.id; anchor.id = project::new_uuid(); anchors[old] = anchor.id;
      anchor.start_offset += segment.start; anchor.end_offset += segment.start; d.anchors.push_back(anchor);
    }
    for (auto evidence : source.semantics.anchors) {
      const auto old = evidence.anchor_id; evidence.anchor_id = anchors.at(old);
      auto anchor = std::find_if(d.anchors.begin(), d.anchors.end(), [&](auto &a) { return a.id == evidence.anchor_id; });
      auto refreshed = application::SemanticAnchorEngine::evidence(d, *anchor);
      refreshed.state = evidence.state; refreshed.reason = evidence.reason;
      refreshed.start_gravity = evidence.start_gravity; refreshed.end_gravity = evidence.end_gravity;
      refreshed.revision = evidence.revision; d.semantics.anchors.push_back(refreshed);
    }
    for (auto annotation : source.semantics.annotations) {
      if(annotation.type_key=="inde.editor.review-state" || annotation.type_key=="inde.editor.suggestion") {
        out.warnings.push_back(label+": decisões e sugestões editoriais pertencem à origem e não são promovidas a aprovação/proposta da montagem.");continue;
      }
      if (annotation.scope == project::AnnotationScope::Document && out.incorporated_occurrences != 1) {
        out.warnings.push_back(label + ": declaração global não foi promovida a contexto da montagem composta; nenhuma perspectiva vencedora foi escolhida."); continue;
      }
      const auto old = annotation.id; annotation.id = project::new_uuid(); annotations[old] = annotation.id;
      if (!annotation.anchor_id.empty()) annotation.anchor_id = anchors.at(annotation.anchor_id);
      for (auto &time : annotation.fictional_time) time.id = project::new_uuid();
      for (auto &date : annotation.dates) date.id = project::new_uuid();
      d.semantics.annotations.push_back(annotation);
    }
    for (auto relation : source.semantics.relations) {
      if (!annotations.contains(relation.source_annotation_id) || !annotations.contains(relation.target_annotation_id)) {
        out.warnings.push_back(label + ": relação com declaração global omitida fica somente na origem."); continue;
      }
      relation.id = project::new_uuid(); relation.source_annotation_id = annotations.at(relation.source_annotation_id);
      relation.target_annotation_id = annotations.at(relation.target_annotation_id); d.semantics.relations.push_back(relation);
    }
    for (auto ref : source.entity_references) {
      if (!ref.anchor_id && !unanchored_entities.insert(ref.entity_id).second) continue;
      ref.id = project::new_uuid(); if (ref.anchor_id) ref.anchor_id = anchors.at(*ref.anchor_id);
      d.entity_references.push_back(ref);
    }
    for (const auto &cmd : source.inline_commands) {
      const auto key = std::make_pair(cmd.prefix, cmd.keyword);
      auto existing = commands.find(key);
      if (existing != commands.end() && existing->second != cmd) conflicts.insert(key);
      else commands.emplace(key, cmd);
    }
    if (!source.perspective.empty()) {
      if (out.incorporated_occurrences == 1) d.perspective = source.perspective;
      else out.warnings.push_back(label + ": perspectiva documental textual não foi escolhida para a montagem composta.");
    }
  }
  for (auto &[key, command] : commands) {
    if (!conflicts.contains(key)) d.inline_commands.push_back(command);
    else out.warnings.push_back("Modelo inline " + key.first + key.second + " conflita entre origens: nenhum conteúdo vencedor foi escolhido.");
  }
  if (out.reference_occurrences) out.warnings.push_back("Referências sem texto permanecem na montagem experimental; o Documento independente não cria vínculos permanentes de navegação.");
  std::sort(out.warnings.begin(), out.warnings.end()); out.warnings.erase(std::unique(out.warnings.begin(), out.warnings.end()), out.warnings.end());
  project::validate(d); return out;
}
}

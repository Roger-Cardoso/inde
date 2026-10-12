#include "inde/application/editor_interaction.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <glib.h>
#include <stdexcept>
namespace inde::application {
std::string selected_text(const project::Document &d, TextRange r) {
  if (!g_utf8_validate(d.content.data(), d.content.size(), nullptr))
    throw std::runtime_error("A seleção exige texto UTF-8 válido");
  if (r.start >= r.end || r.end > project::utf8_character_count(d.content))
    throw std::runtime_error("Selecione um trecho não vazio do texto");
  const auto *a = g_utf8_offset_to_pointer(d.content.c_str(), r.start);
  const char *b = g_utf8_offset_to_pointer(d.content.c_str(), r.end);
  return std::string(a, b);
}
project::Document link_text_entity(project::Document d, TextRange r,
                                   const std::string &id,
                                   const std::string &label,
                                   std::string notes) {
  (void)selected_text(d, r);
  // Reuse an exact native occurrence; do not generate duplicates on retries.
  for (const auto &ref : d.entity_references)
    if (ref.entity_id == id && ref.anchor_id)
      for (const auto &a : d.anchors)
        if (a.id == *ref.anchor_id && a.start_offset == r.start &&
            a.end_offset == r.end)
          return d;
  SemanticAnchorEngine::inspect(d);
  for (const auto &a : d.semantics.annotations)
    if (a.type_key == "inde.semantic.entity-mention" &&
        a.status != "archived") {
      auto field = a.fields.find("target");
      if (field == a.fields.end() ||
          field->second.values != std::vector<std::string>{id})
        continue;
      auto anchor =
          std::find_if(d.anchors.begin(), d.anchors.end(),
                       [&](const auto &v) { return v.id == a.anchor_id; });
      auto evidence = std::find_if(
          d.semantics.anchors.begin(), d.semantics.anchors.end(),
          [&](const auto &v) { return v.anchor_id == a.anchor_id; });
      if (anchor != d.anchors.end() && evidence != d.semantics.anchors.end() &&
          evidence->state == project::AnchorResolution::Resolved &&
          anchor->start_offset == r.start && anchor->end_offset == r.end)
        return d;
    }
  const auto &types = project::builtin_annotation_types();
  const auto *type =
      project::find_annotation_type(types, "inde.semantic.entity-mention", 1);
  auto result = TextSemanticService::create(
      d, *type, r.start, r.end,
      {{"target", {project::SemanticFieldKind::Entity, {id}}},
       {"note", {project::SemanticFieldKind::Text, {std::move(notes)}}}});
  result.anchors.back().label = "Entidade: " + label;
  return result;
}
project::Document materialize_entity(project::Document d, TextRange r,
                                     const std::string &id,
                                     const std::string &purpose,
                                     std::string note) {
  (void)selected_text(d, r);
  const auto &types = project::builtin_annotation_types();
  const auto *type =
      project::find_annotation_type(types, "inde.semantic.materialization", 1);
  return TextSemanticService::create(
      d, *type, r.start, r.end,
      {{"target", {project::SemanticFieldKind::Entity, {id}}},
       {"purpose", {project::SemanticFieldKind::Enum, {purpose}}},
       {"note", {project::SemanticFieldKind::Text, {std::move(note)}}}});
}
std::set<std::string> entities_in_text(project::Document d, TextRange r) {
  (void)selected_text(d, r);
  SemanticAnchorEngine::inspect(d);
  std::set<std::string> ids;
  auto intersects = [&](const std::string &id) {
    auto a = std::find_if(d.anchors.begin(), d.anchors.end(),
                          [&](const auto &a) { return a.id == id; });
    return a != d.anchors.end() && a->start_offset < r.end &&
           a->end_offset > r.start;
  };
  for (const auto &ref : d.entity_references)
    if (ref.anchor_id && intersects(*ref.anchor_id))
      ids.insert(ref.entity_id);
  for (const auto &a : d.semantics.annotations) {
    auto evidence =
        std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                     [&](const auto &e) { return e.anchor_id == a.anchor_id; });
    if (a.scope != project::AnnotationScope::Interval ||
        a.status == "archived" || evidence == d.semantics.anchors.end() ||
        evidence->state != project::AnchorResolution::Resolved ||
        !intersects(a.anchor_id))
      continue;
    for (const auto &[key, f] : a.fields) {
      (void)key;
      if (f.kind == project::SemanticFieldKind::Entity)
        ids.insert(f.values.begin(), f.values.end());
    }
  }
  return ids;
}
std::optional<ProjectInlineQuery>
project_inline_query(const std::string &token) {
  if (!token.starts_with('@') || token.starts_with("@inde"))
    return {};
  const auto body = token.substr(1);
  const auto colon = body.find(':');
  if (colon == std::string::npos)
    return ProjectInlineQuery{body, {}};
  const auto kind = body.substr(0, colon);
  std::optional<std::string> key;
  if (kind == "personagem")
    key = "character";
  else if (kind == "local")
    key = "location";
  else if (kind == "acontecimento")
    key = "event";
  else if (kind == "informacao" || kind == "informação")
    key = "information";
  else if (kind != "entidade")
    return {};
  return ProjectInlineQuery{body.substr(colon + 1), key};
}
} // namespace inde::application

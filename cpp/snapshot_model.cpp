#include "snapshot_model.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include <glib.h>
namespace inde::publication {
std::string hash(const std::string &s) {
  auto *raw =
      g_compute_checksum_for_string(G_CHECKSUM_SHA256, s.data(), s.size());
  std::string out(raw);
  g_free(raw);
  return out;
}
V strings(const std::vector<std::string> &values) {
  A out;
  out.reserve(values.size());
  for (const auto &s : values)
    out.emplace_back(s);
  return out;
}
V encode_document(const project::Document &d) {
  A anchors, formatting, unsupported;
  anchors.reserve(d.anchors.size());
  formatting.reserve(d.formatting.size());
  unsupported.reserve(3);
  if (!d.entity_references.empty())
    unsupported.emplace_back("document_entity_references");
  if (!d.inline_commands.empty())
    unsupported.emplace_back("inline_commands");
  if (!d.perspective.empty())
    unsupported.emplace_back("free_perspective");
  project::validate_paragraphs(d.paragraphs, d.content);
  for (const auto &a : d.anchors)
    anchors.emplace_back(O{{"id", a.id},
                           {"label", a.label},
                           {"start", static_cast<std::int64_t>(a.start_offset)},
                           {"end", static_cast<std::int64_t>(a.end_offset)}});
  for (const auto &f : d.formatting)
    formatting.emplace_back(
        O{{"style", project::to_string(f.style)},
          {"start", static_cast<std::int64_t>(f.start_offset)},
          {"end", static_cast<std::int64_t>(f.end_offset)},
          {"value", f.value}});
  O value{{"id", d.id},
          {"title", d.title},
          {"text", d.content},
          {"text_sha256", hash(d.content)},
          {"formatting", std::move(formatting)},
          {"anchors", std::move(anchors)},
          {"unsupported_layers", std::move(unsupported)},
          {"semantics", persistence::json::parse(
                            persistence::encode_semantics(d.semantics))}};
  value.emplace("revision", hash(persistence::json::serialize(value, false)));
  return value;
}
V snapshot(const application::WorkProduction &production,
           const std::vector<project::NarrativeEntity> &entities) {
  A units, encoded_entities;
  auto notes = production.limitations;
  bool has_alternatives = false, has_profiles = false;
  units.reserve(production.units.size());
  encoded_entities.reserve(entities.size());
  for (const auto &u : production.units) {
    A documents;
    documents.reserve(u.documents.size());
    for (const auto &d : u.documents) {
      documents.emplace_back(encode_document(d));
      has_alternatives = has_alternatives || !d.resmas.empty();
      has_profiles = has_profiles || !d.visibility_profiles.empty();
    }
    units.emplace_back(
        O{{"id", u.node.id},
          {"title", u.node.title},
          {"type", project::to_string(u.node.type)},
          {"designator", u.node.designator},
          {"position", u.node.position},
          {"parent_id", u.node.parent_id ? V(*u.node.parent_id) : V(nullptr)},
          {"documents", std::move(documents)}});
  }
  for (const auto &e : entities)
    encoded_entities.emplace_back(O{{"id", e.id},
                                    {"name", e.name},
                                    {"summary", e.summary},
                                    {"type", "character"}});
  if (has_alternatives)
    notes.emplace_back("A publicação usa a redação corrente. Alternativas guardadas permanecem privadas no Projeto.");
  if (has_profiles)
    notes.emplace_back("A publicação usa o texto integral. Perfis de visibilidade do editor não excluem trechos da Edição.");
  return O{{"protocol", "inde.web.snapshot"},
           {"version", 1},
           {"source_base", "89b47f27e83d76e1ef61a46cb6c29b8c75c305c4"},
           {"source_schema", persistence::current_database_schema_version},
           {"project_id", production.project_id},
           {"structure_id", production.structure_id},
           {"prepared_at", production.prepared_at},
           {"work", O{{"id", production.work.id},
                      {"title", production.work.title},
                      {"synopsis", production.work.synopsis},
                      {"language", production.work.language}}},
           {"units", std::move(units)},
           {"entities", std::move(encoded_entities)},
           {"omissions", strings(production.omissions)},
           {"source_production_notes", strings(notes)}};
}

} // namespace inde::publication

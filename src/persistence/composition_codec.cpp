#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/json.hpp"
#include <glib.h>
#include <stdexcept>
namespace inde::persistence {
namespace {
using json::Value;
using namespace experimental;
void shape(const Value &v, std::initializer_list<const char *> keys) {
  if (v.as_object().size() != keys.size())
    throw std::runtime_error("Campos de Composição incompatíveis");
  for (auto key : keys)
    (void)v.at(key);
}
Value optional(const std::optional<std::string> &v) {
  return v ? Value(*v) : Value(nullptr);
}
std::optional<std::string> optional(const Value &v) {
  return v.is_null() ? std::nullopt : std::optional(v.as_string());
}
std::int64_t number(std::size_t n) {
  if (n > 32 * 1024 * 1024)
    throw std::runtime_error("Offset de Composição excedido");
  return static_cast<std::int64_t>(n);
}
std::size_t number(const Value &v) {
  auto n = v.as_integer();
  if (n < 0 || n > 32 * 1024 * 1024)
    throw std::runtime_error("Offset de Composição inválido");
  return static_cast<std::size_t>(n);
}
Value strings(const std::map<std::string, std::string> &map) {
  Value::Object out;
  for (const auto &[k, v] : map)
    out[k] = v;
  return out;
}
std::map<std::string, std::string> strings(const Value &v) {
  std::map<std::string, std::string> out;
  for (const auto &[k, s] : v.as_object())
    out[k] = s.as_string();
  return out;
}
void bounded(const std::string &s) {
  if (s.size() > project::composition_payload_limit ||
      s.find('\0') != std::string::npos ||
      !g_utf8_validate(s.data(), s.size(), nullptr))
    throw std::runtime_error(
        "Composição excede 2 MiB ou contém texto inválido; nada foi truncado");
  bool quoted{}, escaped{};
  int depth{};
  for (char c : s) {
    if (quoted) {
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == '"')
        quoted = false;
    } else if (c == '"')
      quoted = true;
    else if (c == '{' || c == '[') {
      if (++depth > 8)
        throw std::runtime_error("Composição contém JSON profundo demais");
    } else if (c == '}' || c == ']')
      --depth;
  }
}
void validate(const project::Composition &c) {
  if (c.name.empty() || c.name.size() > 256 ||
      c.name.find('\0') != std::string::npos ||
      !g_utf8_validate(c.name.data(), c.name.size(), nullptr))
    throw std::runtime_error("Dê um nome à Composição, até 256 bytes UTF-8");
  (void)TextBlocksLab::restore(c.blocks.snapshot());
  (void)c.blocks.project(c.profile);
  if (c.origins.size() > 256 || c.drafts.size() > 256)
    throw std::runtime_error("Limite de origens/rascunhos excedido");
  std::map<std::pair<std::string, std::string>, const project::Document *>
      sources;
  std::set<std::string> ids;
  for (const auto &[sha, d] : c.origins) {
    if (composition_checksum(encode_document_snapshot(d)) != sha)
      throw std::runtime_error("Origem congelada incompatível");
    sources[{d.id, composition_checksum(d.content)}] = &d;
    ids.insert(d.id);
  }
  for (const auto &[id, b] : c.blocks.blocks()) {
    (void)id;
    if (!ids.contains(b.origin_document_id))
      throw std::runtime_error("Origem arquivada do bloco ausente");
    if (!b.origin_snapshot_sha256 ||
        !c.origins.contains(*b.origin_snapshot_sha256) ||
        c.origins.at(*b.origin_snapshot_sha256).id != b.origin_document_id)
      throw std::runtime_error("Snapshot exato de origem do bloco ausente");
    for (const auto &r : b.revisions)
      if (r.exact_source) {
        const auto &source = *r.exact_source;
        auto found =
            sources.find({source.document_id, source.source_body_sha256});
        if (found == sources.end() ||
            source.end > project::utf8_character_count(found->second->content))
          throw std::runtime_error("Faixa não pertence à origem congelada");
        const auto &body = found->second->content;
        std::string range(g_utf8_offset_to_pointer(body.c_str(), source.start),
                          g_utf8_offset_to_pointer(body.c_str(), source.end));
        if (range != r.text)
          throw std::runtime_error("Texto diverge da origem exata declarada");
      }
  }
  for (const auto &[block, draft] : c.drafts) {
    (void)c.blocks.version(block, draft.expected_revision);
    if (draft.text.size() > project::composition_payload_limit ||
        draft.text.find('\0') != std::string::npos ||
        !g_utf8_validate(draft.text.data(), draft.text.size(), nullptr))
      throw std::runtime_error("Rascunho inválido ou longo demais");
  }
}
} // namespace
std::string composition_checksum(const std::string &s) {
  auto *h = g_compute_checksum_for_data(
      G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(s.data()), s.size());
  if (!h)
    throw std::runtime_error("Checksum indisponível");
  std::string value(h);
  g_free(h);
  return value;
}
std::string encode_composition(const project::Composition &c) {
  validate(c);
  const auto s = c.blocks.snapshot();
  Value::Array blocks, placements, visible;
  for (const auto &[id, b] : s.blocks) {
    (void)id;
    Value::Array revisions;
    for (const auto &r : b.revisions) {
      Value source(nullptr);
      Value::Array warnings;
      if (r.exact_source) {
        const auto &e = *r.exact_source;
        source = Value::Object{{"document", e.document_id},
                               {"sha256", e.source_body_sha256},
                               {"start", number(e.start)},
                               {"end", number(e.end)}};
      }
      for (const auto &w : r.layers.warnings)
        warnings.emplace_back(w);
      revisions.emplace_back(Value::Object{
          {"id", r.id},
          {"parent", r.parent_id},
          {"text", r.text},
          {"source", std::move(source)},
          {"document", encode_document_snapshot(*r.layers.document)},
          {"warnings", std::move(warnings)}});
    }
    Value fork(nullptr);
    if (b.fork_origin)
      fork = Value::Object{{"block", b.fork_origin->first},
                           {"revision", b.fork_origin->second}};
    blocks.emplace_back(
        Value::Object{{"id", b.id},
                      {"label", b.label},
                      {"origin", b.origin_document_id},
                      {"origin_snapshot", optional(b.origin_snapshot_sha256)},
                      {"current", b.current_revision},
                      {"fork", std::move(fork)},
                      {"revisions", std::move(revisions)}});
  }
  for (const auto &p : s.placements)
    placements.emplace_back(
        Value::Object{{"id", p.id},
                      {"block", p.block_id},
                      {"mode", name(p.mode)},
                      {"pinned", optional(p.pinned_revision)},
                      {"variant", optional(p.variant)},
                      {"layer", optional(p.layer)},
                      {"group", optional(p.variant_group)},
                      {"hidden", p.hidden}});
  Value::Object origins, drafts;
  for (const auto &[sha, d] : c.origins)
    origins[sha] = encode_document_snapshot(d);
  for (const auto &[id, d] : c.drafts)
    drafts[id] =
        Value::Object{{"expected", d.expected_revision}, {"text", d.text}};
  for (const auto &layer : c.profile.visible_layers)
    visible.emplace_back(layer);
  auto payload = json::serialize(
      Value::Object{
          {"version", 1},
          {"id", s.composition_id},
          {"name", c.name},
          {"blocks", std::move(blocks)},
          {"placements", std::move(placements)},
          {"variants", strings(s.variants)},
          {"layers", strings(s.layers)},
          {"variant_groups", strings(s.variant_groups)},
          {"variant_bases", strings(s.variant_bases)},
          {"profile",
           Value::Object{{"variant", optional(c.profile.variant)},
                         {"visible", std::move(visible)},
                         {"selected", strings(c.profile.selected_variants)}}},
          {"origins", std::move(origins)},
          {"drafts", std::move(drafts)}},
      false);
  bounded(payload);
  return payload;
}
project::Composition decode_composition(const std::string &payload,
                                        const std::string &hash) {
  bounded(payload);
  if (hash.size() != 64 || composition_checksum(payload) != hash)
    throw std::runtime_error(
        "Checksum da Composição inválido; evidência preservada");
  const auto v = json::parse(payload);
  shape(v,
        {"version", "id", "name", "blocks", "placements", "variants", "layers",
         "variant_groups", "variant_bases", "profile", "origins", "drafts"});
  if (v.at("version").as_integer() != 1)
    throw std::runtime_error("Versão de Composição incompatível");
  TextBlocksSnapshot s;
  s.composition_id = v.at("id").as_string();
  for (const auto &b : v.at("blocks").as_array()) {
    shape(b, {"id", "label", "origin", "origin_snapshot", "current", "fork",
              "revisions"});
    TextBlock block{b.at("id").as_string(),
                    b.at("label").as_string(),
                    b.at("origin").as_string(),
                    b.at("current").as_string(),
                    {},
                    {}};
    block.origin_snapshot_sha256 = optional(b.at("origin_snapshot"));
    if (!b.at("fork").is_null()) {
      shape(b.at("fork"), {"block", "revision"});
      block.fork_origin = std::pair(b.at("fork").at("block").as_string(),
                                    b.at("fork").at("revision").as_string());
    }
    for (const auto &r : b.at("revisions").as_array()) {
      shape(r, {"id", "parent", "text", "source", "document", "warnings"});
      BlockRevision revision{r.at("id").as_string(),
                             r.at("parent").as_string(),
                             r.at("text").as_string(),
                             {},
                             {}};
      if (!r.at("source").is_null()) {
        const auto &a = r.at("source");
        shape(a, {"document", "sha256", "start", "end"});
        revision.exact_source = SourceRange{
            a.at("document").as_string(), a.at("sha256").as_string(),
            number(a.at("start")), number(a.at("end"))};
      }
      revision.layers.document = std::make_shared<const project::Document>(
          decode_document_snapshot(r.at("document").as_string()));
      for (const auto &w : r.at("warnings").as_array())
        revision.layers.warnings.push_back(w.as_string());
      block.revisions.push_back(std::move(revision));
    }
    if (!s.blocks.emplace(block.id, std::move(block)).second)
      throw std::runtime_error("Bloco duplicado");
  }
  for (const auto &p : v.at("placements").as_array()) {
    shape(p, {"id", "block", "mode", "pinned", "variant", "layer", "group",
              "hidden"});
    const auto mode = p.at("mode").as_string();
    PlacementMode m;
    if (mode == "live")
      m = PlacementMode::Live;
    else if (mode == "pinned")
      m = PlacementMode::Pinned;
    else if (mode == "forked")
      m = PlacementMode::Forked;
    else if (mode == "reference-only")
      m = PlacementMode::ReferenceOnly;
    else
      throw std::runtime_error("Modo de ocorrência desconhecido");
    s.placements.push_back({p.at("id").as_string(), p.at("block").as_string(),
                            m, optional(p.at("pinned")),
                            optional(p.at("variant")), optional(p.at("layer")),
                            optional(p.at("group")),
                            p.at("hidden").as_boolean()});
  }
  s.variants = strings(v.at("variants"));
  s.layers = strings(v.at("layers"));
  s.variant_groups = strings(v.at("variant_groups"));
  s.variant_bases = strings(v.at("variant_bases"));
  project::Composition c;
  c.name = v.at("name").as_string();
  c.blocks = TextBlocksLab::restore(std::move(s));
  const auto &profile = v.at("profile");
  shape(profile, {"variant", "visible", "selected"});
  c.profile.variant = optional(profile.at("variant"));
  c.profile.selected_variants = strings(profile.at("selected"));
  for (const auto &l : profile.at("visible").as_array())
    if (!c.profile.visible_layers.insert(l.as_string()).second)
      throw std::runtime_error("Camada visível duplicada");
  for (const auto &[h, d] : v.at("origins").as_object())
    c.origins[h] = decode_document_snapshot(d.as_string());
  for (const auto &[id, d] : v.at("drafts").as_object()) {
    shape(d, {"expected", "text"});
    c.drafts[id] = {d.at("expected").as_string(), d.at("text").as_string()};
  }
  validate(c);
  return c;
}
} // namespace inde::persistence

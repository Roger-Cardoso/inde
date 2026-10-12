#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/json.hpp"
#include <glib.h>
#include <limits>
#include <stdexcept>
namespace inde::persistence {
namespace {
using json::Value;
using O = Value::Object;
using A = Value::Array;
void fields(const Value &v, std::size_t count) {
  if (v.as_object().size() != count)
    throw std::runtime_error(
        "Estrutura semântica incompatível; nenhuma informação descartada");
}
std::int64_t number(std::uint64_t n) {
  if (n > INT64_MAX)
    throw std::runtime_error("Número semântico excedido");
  return n;
}
std::uint64_t positive(const Value &v) {
  auto n = v.as_integer();
  if (n < 0)
    throw std::runtime_error("Número semântico negativo");
  return n;
}
std::uint32_t version(const Value &v) {
  auto n = positive(v);
  if (n == 0 || n > 100000)
    throw std::runtime_error("Versão semântica inválida");
  return n;
}
Value strings(const std::vector<std::string> &v) {
  A a;
  for (auto &s : v)
    a.emplace_back(s);
  return a;
}
std::vector<std::string> strings(const Value &v) {
  std::vector<std::string> out;
  if (v.as_array().size() > 128)
    throw std::runtime_error("Valores semânticos excedidos");
  for (auto &s : v.as_array())
    out.push_back(s.as_string());
  return out;
}
Value parse(const std::string &s) {
  if (s.size() > 16 * 1024 * 1024 ||
      !g_utf8_validate(s.data(), s.size(), nullptr))
    throw std::runtime_error("Payload semântico excedido ou UTF-8 inválido");
  bool quote = false, escape = false;
  int depth = 0;
  for (char c : s) {
    if (quote) {
      if (escape)
        escape = false;
      else if (c == '\\')
        escape = true;
      else if (c == '\"')
        quote = false;
    } else if (c == '\"')
      quote = true;
    else if (c == '{' || c == '[') {
      if (++depth > 8)
        throw std::runtime_error("JSON semântico profundo demais");
    } else if (c == '}' || c == ']')
      --depth;
  }
  return json::parse(s);
}
} // namespace
std::string encode_semantics(const project::DocumentSemantics &s) {
  A anchors, annotations, relations;
  bool has_dates = false;
  for (const auto &a : s.annotations)
    has_dates |= !a.dates.empty();
  for (auto &e : s.anchors)
    anchors.emplace_back(
        O{{"id", e.anchor_id},
          {"state", project::to_string(e.state)},
          {"start_gravity", project::to_string(e.start_gravity)},
          {"end_gravity", project::to_string(e.end_gravity)},
          {"revision", number(e.revision)},
          {"selected", e.selected_hash},
          {"before", e.before_hash},
          {"after", e.after_hash},
          {"reason", e.reason}});
  for (auto &a : s.annotations) {
    O f;
    A time;
    for (const auto &b : a.fictional_time)
      time.emplace_back(
          O{{"id", b.id},
            {"fictional_axis", b.fictional_axis_id},
            {"fictional_start_point", b.fictional_start_point_id},
            {"fictional_end_point", b.fictional_end_point_id
                                        ? Value(*b.fictional_end_point_id)
                                        : Value(nullptr)},
            {"origin", b.origin},
            {"source", b.source},
            {"notes", b.notes},
            {"created", b.created_at}});
    for (auto &[key, v] : a.fields)
      f.emplace(key, O{{"kind", project::to_string(v.kind)},
                       {"values", strings(v.values)}});
    O encoded{{"id", a.id},
              {"anchor", a.anchor_id},
              {"type", a.type_key},
              {"version", number(a.type_version)},
              {"fields", std::move(f)},
              {"status", a.status},
              {"origin", a.origin},
              {"source", a.source},
              {"notes", a.notes},
              {"opaque", a.opaque_payload},
              {"created", a.created_at},
              {"updated", a.updated_at},
              {"scope", project::to_string(a.scope)},
              {"fictional_time", std::move(time)}};
    if (has_dates) {
      A dates;
      for (const auto &b : a.dates)
        dates.emplace_back(O{{"id", b.id},
                             {"reference", b.reference_id},
                             {"role", b.role},
                             {"notes", b.notes},
                             {"created", b.created_at}});
      encoded.emplace("dates", std::move(dates));
    }
    annotations.emplace_back(std::move(encoded));
  }
  for (auto &r : s.relations)
    relations.emplace_back(O{{"id", r.id},
                             {"source", r.source_annotation_id},
                             {"target", r.target_annotation_id},
                             {"type", r.type_key},
                             {"origin", r.origin},
                             {"notes", r.notes},
                             {"created", r.created_at}});
  auto out = json::serialize(O{{"version", has_dates ? 3 : 2},
                               {"anchors", std::move(anchors)},
                               {"annotations", std::move(annotations)},
                               {"relations", std::move(relations)}},
                             false);
  (void)parse(out);
  return out;
}
project::DocumentSemantics decode_semantics(const std::string &s) {
  auto v = parse(s);
  fields(v, 4);
  const auto payload_version = v.at("version").as_integer();
  if (payload_version != 1 && payload_version != 2 && payload_version != 3)
    throw std::runtime_error("Payload semântico de versão futura");
  project::DocumentSemantics out;
  if (v.at("anchors").as_array().size() > 2000 ||
      v.at("annotations").as_array().size() > 2000 ||
      v.at("relations").as_array().size() > 4000)
    throw std::runtime_error("Limites semânticos excedidos");
  for (auto &e : v.at("anchors").as_array()) {
    fields(e, 9);
    out.anchors.push_back(
        {e.at("id").as_string(),
         project::anchor_resolution(e.at("state").as_string()),
         project::anchor_gravity(e.at("start_gravity").as_string()),
         project::anchor_gravity(e.at("end_gravity").as_string()),
         positive(e.at("revision")), e.at("selected").as_string(),
         e.at("before").as_string(), e.at("after").as_string(),
         e.at("reason").as_string()});
  }
  for (auto &a : v.at("annotations").as_array()) {
    fields(a, payload_version == 1 ? 12 : payload_version == 2 ? 14 : 15);
    project::TextAnnotation item;
    item.id = a.at("id").as_string();
    item.anchor_id = a.at("anchor").as_string();
    item.type_key = a.at("type").as_string();
    item.type_version = version(a.at("version"));
    if (a.at("fields").as_object().size() > 32)
      throw std::runtime_error("Campos semânticos excedidos");
    for (auto &[key, f] : a.at("fields").as_object()) {
      fields(f, 2);
      item.fields.emplace(
          key, project::SemanticFieldValue{
                   project::semantic_field_kind(f.at("kind").as_string()),
                   strings(f.at("values"))});
    }
    item.status = a.at("status").as_string();
    item.origin = a.at("origin").as_string();
    item.source = a.at("source").as_string();
    item.notes = a.at("notes").as_string();
    item.opaque_payload = a.at("opaque").as_string();
    item.created_at = a.at("created").as_string();
    item.updated_at = a.at("updated").as_string();
    if (payload_version >= 2) {
      item.scope = project::annotation_scope(a.at("scope").as_string());
      if (a.at("fictional_time").as_array().size() > 32)
        throw std::runtime_error("Vínculos temporais excedidos");
      for (const auto &b : a.at("fictional_time").as_array()) {
        fields(b, 8);
        item.fictional_time.push_back(
            {b.at("id").as_string(), b.at("fictional_axis").as_string(),
             b.at("fictional_start_point").as_string(),
             b.at("fictional_end_point").is_null()
                 ? std::nullopt
                 : std::optional<std::string>(
                       b.at("fictional_end_point").as_string()),
             b.at("origin").as_string(), b.at("source").as_string(),
             b.at("notes").as_string(), b.at("created").as_string()});
      }
    }
    if (payload_version >= 3) {
      if (a.at("dates").as_array().size() > 32)
        throw std::runtime_error("Datas textuais excedidas");
      for (const auto &b : a.at("dates").as_array()) {
        fields(b, 5);
        item.dates.push_back(
            {b.at("id").as_string(), b.at("reference").as_string(),
             b.at("role").as_string(), b.at("notes").as_string(),
             b.at("created").as_string()});
      }
    }
    out.annotations.push_back(std::move(item));
  }
  for (auto &r : v.at("relations").as_array()) {
    fields(r, 7);
    out.relations.push_back(
        {r.at("id").as_string(), r.at("source").as_string(),
         r.at("target").as_string(), r.at("type").as_string(),
         r.at("origin").as_string(), r.at("notes").as_string(),
         r.at("created").as_string()});
  }
  return out;
}
std::string encode_semantic_type(const project::AnnotationTypeDefinition &t) {
  project::validate(t);
  A fields;
  for (auto &f : t.fields)
    fields.emplace_back(O{{"key", f.key},
                          {"label", f.label},
                          {"kind", project::to_string(f.kind)},
                          {"required", f.required},
                          {"multiple", f.multiple},
                          {"values", strings(f.values)},
                          {"entity_types", strings(f.entity_type_keys)}});
  return json::serialize(O{{"schema", 1},
                           {"key", t.key},
                           {"label", t.label},
                           {"description", t.description},
                           {"version", number(t.version)},
                           {"system", t.system},
                           {"overlap", t.allow_overlap},
                           {"fields", std::move(fields)}},
                         false);
}
project::AnnotationTypeDefinition decode_semantic_type(const std::string &s) {
  auto v = parse(s);
  fields(v, 8);
  if (v.at("schema").as_integer() != 1)
    throw std::runtime_error("Definição semântica futura");
  project::AnnotationTypeDefinition t;
  t.key = v.at("key").as_string();
  t.label = v.at("label").as_string();
  t.description = v.at("description").as_string();
  t.version = version(v.at("version"));
  t.system = v.at("system").as_boolean();
  t.allow_overlap = v.at("overlap").as_boolean();
  if (v.at("fields").as_array().size() > 32)
    throw std::runtime_error("Campos de definição excedidos");
  for (auto &f : v.at("fields").as_array()) {
    fields(f, 7);
    t.fields.push_back({f.at("key").as_string(), f.at("label").as_string(),
                        project::semantic_field_kind(f.at("kind").as_string()),
                        f.at("required").as_boolean(),
                        f.at("multiple").as_boolean(), strings(f.at("values")),
                        strings(f.at("entity_types"))});
  }
  project::validate(t);
  return t;
}
} // namespace inde::persistence

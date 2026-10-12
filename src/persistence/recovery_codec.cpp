#include "recovery_codec.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/inline_command_codec.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include <glib.h>
#include <limits>
#include <stdexcept>

namespace inde::persistence::recovery {
namespace {
using json::Value;
std::int64_t number(std::uint64_t v) {
  if (v > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
    throw std::runtime_error("Revisão/offset de recuperação excedido");
  return static_cast<std::int64_t>(v);
}
std::size_t offset(const Value &v) {
  auto n = v.as_integer();
  if (n < 0)
    throw std::runtime_error("Offset negativo no journal");
  return static_cast<std::size_t>(n);
}
Value nullable(const std::optional<std::string> &v) {
  return v ? Value(*v) : Value(nullptr);
}
std::optional<std::string> optional(const Value &v) {
  return v.is_null() ? std::nullopt : std::optional<std::string>(v.as_string());
}
Value document(const project::Document &d) {
  Value::Array formats, anchors, refs;
  for (const auto &s : d.formatting)
    formats.emplace_back(Value::Object{{"style", project::to_string(s.style)},
                                       {"start", number(s.start_offset)},
                                       {"end", number(s.end_offset)},
                                       {"value", s.value}});
  for (const auto &a : d.anchors)
    anchors.emplace_back(Value::Object{{"id", a.id},
                                       {"label", a.label},
                                       {"start", number(a.start_offset)},
                                       {"end", number(a.end_offset)}});
  for (const auto &r : d.entity_references)
    refs.emplace_back(Value::Object{{"id", r.id},
                                    {"entity", r.entity_id},
                                    {"anchor", nullable(r.anchor_id)},
                                    {"notes", r.notes}});
  return Value::Object{
      {"id", d.id},
      {"editorial", nullable(d.editorial_node_id)},
      {"title", d.title},
      {"content", d.content},
      {"created", d.created_at},
      {"updated", d.updated_at},
      {"goal", d.word_goal ? Value(number(*d.word_goal)) : Value(nullptr)},
      {"formatting", std::move(formats)},
      {"anchors", std::move(anchors)},
      {"references", std::move(refs)},
      {"group", nullable(d.group_id)},
      {"purpose", project::to_string(d.purpose)},
      {"revision_of", nullable(d.revision_of_id)},
      {"revision_label", d.revision_label},
      {"perspective", d.perspective},
      {"semantics", encode_semantics(d.semantics)},
      {"inline_commands", encode_inline_commands(d.inline_commands)}};
}
project::Document document(const Value &v, int codec_version) {
  if (v.as_object().size() != (codec_version == 1   ? 15
                               : codec_version == 2 ? 16
                                                    : 17))
    throw std::runtime_error("Campos de Documento incompatíveis no journal");
  project::Document d;
  d.id = v.at("id").as_string();
  d.title = v.at("title").as_string();
  d.content = v.at("content").as_string();
  d.editorial_node_id = optional(v.at("editorial"));
  d.created_at = v.at("created").as_string();
  d.updated_at = v.at("updated").as_string();
  if (!v.at("goal").is_null())
    d.word_goal = offset(v.at("goal"));
  for (const auto &s : v.at("formatting").as_array()) {
    if (s.as_object().size() != (codec_version < 3 ? 3 : 4))
      throw std::runtime_error("Estilo incompatível no journal");
    d.formatting.push_back(
        {project::document_text_style_from_string(s.at("style").as_string()),
         offset(s.at("start")), offset(s.at("end")),
         codec_version < 3 ? "" : s.at("value").as_string()});
  }
  for (const auto &a : v.at("anchors").as_array()) {
    if (a.as_object().size() != 4)
      throw std::runtime_error("Âncora incompatível no journal");
    d.anchors.push_back({a.at("id").as_string(), a.at("label").as_string(),
                         offset(a.at("start")), offset(a.at("end"))});
  }
  for (const auto &r : v.at("references").as_array()) {
    if (r.as_object().size() != 4)
      throw std::runtime_error("Referência incompatível no journal");
    d.entity_references.push_back(
        {r.at("id").as_string(), r.at("entity").as_string(),
         optional(r.at("anchor")), r.at("notes").as_string()});
  }
  d.group_id = optional(v.at("group"));
  d.purpose =
      project::document_purpose_from_string(v.at("purpose").as_string());
  d.revision_of_id = optional(v.at("revision_of"));
  d.revision_label = v.at("revision_label").as_string();
  d.perspective = v.at("perspective").as_string();
  if (codec_version >= 2)
    d.semantics = decode_semantics(v.at("semantics").as_string());
  if (codec_version >= 3)
    d.inline_commands =
        decode_inline_commands(v.at("inline_commands").as_string());
  return d;
}
void bounded(const std::string &s) {
  if (s.size() > project::recovery_payload_limit ||
      !g_utf8_validate(s.data(), static_cast<gssize>(s.size()), nullptr))
    throw std::runtime_error("Journal excede limite ou contém UTF-8 inválido");
  bool quoted{}, escaped{};
  int depth{};
  for (char ch : s) {
    if (quoted) {
      if (escaped)
        escaped = false;
      else if (ch == '\\')
        escaped = true;
      else if (ch == '"')
        quoted = false;
    } else if (ch == '"')
      quoted = true;
    else if (ch == '{' || ch == '[') {
      if (++depth > 5)
        throw std::runtime_error("Journal possui JSON profundo demais");
    } else if (ch == '}' || ch == ']')
      --depth;
  }
}
} // namespace
void safe_id(const std::string &s) {
  if (s.size() != 36)
    throw std::runtime_error("Identidade de recuperação inválida");
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (s[i] != '-')
        throw std::runtime_error("Identidade de recuperação inválida");
    } else if (!g_ascii_isxdigit(s[i]))
      throw std::runtime_error("Identidade de recuperação inválida");
  }
}
void validate(const project::RecoveryRecord &r) {
  safe_id(r.project_id);
  safe_id(r.session_id);
  safe_id(r.state.document.id);
  if (r.state.document.id != r.baseline.id || r.durable_revision > r.revision)
    throw std::runtime_error("Contexto/revisão de recuperação incompatível");
  project::validate(r.baseline);
  auto draft = r.state.document;
  if (draft.title.find_first_not_of(" \t\r\n") == std::string::npos)
    draft.title = "Rascunho temporário";
  project::validate(draft);
  if (!g_utf8_validate(draft.content.data(),
                       static_cast<gssize>(draft.content.size()), nullptr) ||
      r.state.cursor > project::utf8_character_count(draft.content) ||
      r.state.bound > project::utf8_character_count(draft.content))
    throw std::runtime_error("Texto/seleção de recuperação inválidos");
  if (r.status != "pending" && r.status != "clean" && r.status != "restored" &&
      r.status != "copy-created" && r.status != "discarded")
    throw std::runtime_error("Estado de recuperação desconhecido");
  number(r.revision);
  number(r.durable_revision);
}
std::string checksum(const std::string &s) {
  auto *digest = g_compute_checksum_for_data(
      G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(s.data()), s.size());
  if (!digest)
    throw std::runtime_error("Não foi possível verificar o journal");
  std::string result(digest);
  g_free(digest);
  return result;
}
std::string encode(const project::RecoveryRecord &r) {
  validate(r);
  auto result = json::serialize(
      Value::Object{{"version", 4},
                    {"project", r.project_id},
                    {"session", r.session_id},
                    {"revision", number(r.revision)},
                    {"durable_revision", number(r.durable_revision)},
                    {"state", document(r.state.document)},
                    {"cursor", number(r.state.cursor)},
                    {"bound", number(r.state.bound)},
                    {"baseline", document(r.baseline)},
                    {"status", r.status},
                    {"updated", r.updated_at}},
      false);
  bounded(result);
  return result;
}
project::RecoveryRecord decode(const std::string &s, const std::string &hash) {
  bounded(s);
  if (hash.size() != 64 || checksum(s) != hash)
    throw std::runtime_error(
        "Checksum do journal inválido; evidência preservada");
  auto v = json::parse(s);
  auto codec_version = v.at("version").as_integer();
  if (v.as_object().size() != 11 || (codec_version != 1 && codec_version != 2 &&
                                     codec_version != 3 && codec_version != 4))
    throw std::runtime_error("Versão/campos do journal incompatíveis");
  project::RecoveryRecord r;
  r.project_id = v.at("project").as_string();
  r.session_id = v.at("session").as_string();
  r.revision = offset(v.at("revision"));
  r.durable_revision = offset(v.at("durable_revision"));
  r.state = {document(v.at("state"), codec_version), offset(v.at("cursor")),
             offset(v.at("bound"))};
  r.baseline = document(v.at("baseline"), codec_version);
  r.status = v.at("status").as_string();
  r.updated_at = v.at("updated").as_string();
  r.checksum = hash;
  validate(r);
  return r;
}
} // namespace inde::persistence::recovery

namespace inde::persistence {
std::string encode_document_snapshot(const project::Document &d) {
  project::validate(d);
  auto value = json::serialize(json::Value::Object{{"version", 1}, {"document", recovery::document(d)}}, false);
  recovery::bounded(value);
  return value;
}
project::Document decode_document_snapshot(const std::string &s) {
  recovery::bounded(s);
  auto value = json::parse(s);
  if (value.as_object().size() != 2 || value.at("version").as_integer() != 1)
    throw std::runtime_error("Versão de snapshot documental incompatível");
  auto d = recovery::document(value.at("document"), 4);
  project::validate(d);
  return d;
}
}

#include "inde/project/text_semantics.hpp"
#include "inde/project/writing.hpp"
#include <algorithm>
#include <charconv>
#include <glib.h>
#include <set>
#include <stdexcept>
#include <uuid/uuid.h>
namespace inde::project {
namespace {
void require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
void uuid(const std::string &s) {
  uuid_t u;
  require(uuid_parse(s.c_str(), u) == 0, "Identidade semântica inválida");
}
void text(const std::string &s, std::size_t max) {
  require(s.size() <= max && g_utf8_validate(s.data(), s.size(), nullptr),
          "Campo semântico excedido ou UTF-8 inválido");
}
template <class T, class K> auto ordered(std::vector<T> v, K key) {
  std::sort(v.begin(), v.end(),
            [&](const T &a, const T &b) { return key(a) < key(b); });
  return v;
}
} // namespace
void validate_semantic_key(const std::string &key) {
  require(!key.empty() && key.size() <= 128,
          "Chave semântica ausente ou excedida");
  require(std::all_of(key.begin(), key.end(),
                      [](unsigned char c) {
                        return (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '.' || c == '-' ||
                               c == '_';
                      }),
          "Chave semântica inválida");
}
bool same_semantics(const DocumentSemantics &a, const DocumentSemantics &b) {
  return ordered(a.anchors, [](auto &x) { return x.anchor_id; }) ==
             ordered(b.anchors, [](auto &x) { return x.anchor_id; }) &&
         ordered(a.annotations, [](auto &x) { return x.id; }) ==
             ordered(b.annotations, [](auto &x) { return x.id; }) &&
         ordered(a.relations, [](auto &x) { return x.id; }) ==
             ordered(b.relations, [](auto &x) { return x.id; });
}
void validate(const AnnotationTypeDefinition &t) {
  validate_semantic_key(t.key);
  require(t.key.find('.') != std::string::npos,
          "Tipo semântico precisa de namespace");
  require(t.version > 0 && t.version <= 100000, "Versão de tipo inválida");
  text(t.label, 256);
  require(!t.label.empty(), "Rótulo de tipo ausente");
  text(t.description, 4096);
  require(t.fields.size() <= 32, "Tipo excede 32 campos");
  std::set<std::string> keys;
  for (const auto &f : t.fields) {
    validate_semantic_key(f.key);
    require(keys.insert(f.key).second, "Campo duplicado no tipo");
    text(f.label, 256);
    require(!f.label.empty(), "Rótulo de campo ausente");
    (void)to_string(f.kind);
    require(f.values.size() <= 128 && f.entity_type_keys.size() <= 64,
            "Opções excedidas");
    std::set<std::string> options;
    for (const auto &v : f.values) {
      text(v, 256);
      require(!v.empty() && options.insert(v).second,
              "Opção vazia ou duplicada");
    }
    for (const auto &v : f.entity_type_keys)
      validate_semantic_key(v);
    require(f.kind == SemanticFieldKind::Enum || f.values.empty(),
            "Opções apenas em campos de enumeração");
    require(f.kind != SemanticFieldKind::Enum || !f.values.empty(),
            "Enumeração sem opções");
    require(f.kind == SemanticFieldKind::Entity || f.entity_type_keys.empty(),
            "Tipos de entidade apenas em referências");
  }
}
void validate_semantic_structure(const Document &d) {
  const auto &s = d.semantics;
  if (!s.anchors.empty())
    require(g_utf8_validate(d.content.data(), d.content.size(), nullptr),
            "Texto semântico exige UTF-8 válido");
  require(s.anchors.size() <= 2000 && s.annotations.size() <= 2000 &&
              s.relations.size() <= 4000,
          "Limite de camadas semânticas excedido");
  std::set<std::string> anchor_ids, evidence_ids, ids, relation_ids,
      temporal_ids;
  std::size_t global_perspectives{};
  for (const auto &a : d.anchors)
    anchor_ids.insert(a.id);
  for (const auto &e : s.anchors) {
    uuid(e.anchor_id);
    require(anchor_ids.contains(e.anchor_id) &&
                evidence_ids.insert(e.anchor_id).second,
            "Evidência sem âncora ou duplicada");
    (void)to_string(e.state);
    (void)to_string(e.start_gravity);
    (void)to_string(e.end_gravity);
    text(e.reason, 1024);
    for (const auto &hash : {e.selected_hash, e.before_hash, e.after_hash})
      require(hash.size() == 64 && std::all_of(hash.begin(), hash.end(),
                                               [](char c) {
                                                 return (c >= '0' &&
                                                         c <= '9') ||
                                                        (c >= 'a' && c <= 'f');
                                               }),
              "Fingerprint semântico inválido");
  }
  for (const auto &a : s.annotations) {
    uuid(a.id);
    require(ids.insert(a.id).second, "Anotação duplicada");
    (void)to_string(a.scope);
    if (a.scope == AnnotationScope::Document) {
      require(a.anchor_id.empty() &&
                  (a.type_key == "inde.semantic.perspective" ||
                   a.type_key == "inde.editor.review-state") &&
                  a.type_version == 1,
              "Escopo global reservado à perspectiva/decisão editorial v1; sem "
              "âncora "
              "artificial");
      if (a.status != "archived" && a.type_key == "inde.semantic.perspective")
        ++global_perspectives;
    } else {
      require(a.type_key != "inde.editor.review-state",
              "Decisão editorial exige escopo do Documento");
      uuid(a.anchor_id);
      require(evidence_ids.contains(a.anchor_id), "Anotação sem evidência");
    }
    require(a.fictional_time.size() <= 32,
            "Máximo de 32 vínculos temporais por anotação");
    require(a.dates.size() <= 32, "Máximo de 32 datas por anotação");
    if (a.type_key == "inde.editor.review-state" ||
        a.type_key == "inde.editor.suggestion")
      require(a.dates.empty(),
              "Decisão editorial não é contexto temporal ficcional");
    std::set<std::string> date_contexts;
    for (const auto &b : a.dates) {
      uuid(b.id);
      uuid(b.reference_id);
      require(temporal_ids.insert(b.id).second,
              "Identidade temporal duplicada");
      text(b.role, 128);
      text(b.notes, 2048);
      text(b.created_at, 128);
      require(!b.role.empty() && !b.created_at.empty(),
              "Papel/data de vínculo ausente");
      require(b.role == "scene" || b.role == "mention" || b.role == "memory" ||
                  b.role == "reference",
              "Declare explicitamente o papel temporal da data no texto");
      require(date_contexts.insert(b.reference_id + ":" + b.role).second,
              "Data/papel duplicado na anotação");
    }
    if (a.type_key == "inde.editor.review-state")
      require(a.fictional_time.empty(),
              "Decisão editorial não é declaração de tempo ficcional");
    std::set<std::string> temporal_contexts;
    for (const auto &b : a.fictional_time) {
      uuid(b.id);
      uuid(b.fictional_axis_id);
      uuid(b.fictional_start_point_id);
      if (b.fictional_end_point_id)
        uuid(*b.fictional_end_point_id);
      require(temporal_ids.insert(b.id).second, "Vínculo temporal duplicado");
      require(temporal_contexts
                  .insert(b.fictional_axis_id + ":" +
                          b.fictional_start_point_id + ":" +
                          b.fictional_end_point_id.value_or(""))
                  .second,
              "Contexto temporal duplicado na anotação");
      require(b.origin == "human" || b.origin == "imported",
              "Origem temporal inválida");
      text(b.source, 4096);
      text(b.notes, 4096);
      text(b.created_at, 128);
      require(!b.created_at.empty(), "Data de vínculo temporal ausente");
    }
    validate_semantic_key(a.type_key);
    require(a.type_version > 0 && a.type_version <= 100000,
            "Versão de anotação inválida");
    require(a.status == "active" || a.status == "archived" ||
                a.status == "incomplete",
            "Estado de anotação inválido");
    require(a.origin == "human" || a.origin == "imported",
            "Origem semântica inválida");
    text(a.source, 4096);
    text(a.notes, 4096);
    text(a.opaque_payload, 65536);
    text(a.created_at, 128);
    text(a.updated_at, 128);
    require(!a.created_at.empty() && !a.updated_at.empty(),
            "Datas semânticas ausentes");
    require(a.fields.size() <= 32, "Campos excedidos");
    for (const auto &[key, f] : a.fields) {
      validate_semantic_key(key);
      (void)to_string(f.kind);
      require(f.values.size() <= 128, "Valores excedidos");
      std::set<std::string> unique;
      for (const auto &v : f.values) {
        text(v, 4096);
        require(unique.insert(v).second, "Valor semântico duplicado");
        if (f.kind == SemanticFieldKind::Entity)
          uuid(v);
        if (f.kind == SemanticFieldKind::Boolean)
          require(v == "true" || v == "false", "Booleano inválido");
        if (f.kind == SemanticFieldKind::Integer) {
          std::int64_t n;
          auto r = std::from_chars(v.data(), v.data() + v.size(), n);
          require(r.ec == std::errc{} && r.ptr == v.data() + v.size() &&
                      std::to_string(n) == v,
                  "Inteiro não canônico");
        }
      }
    }
  }
  require(global_perspectives <= 1,
          "Já existe uma perspectiva global não arquivada; edite ou arquive "
          "explicitamente");
  for (const auto &r : s.relations) {
    uuid(r.id);
    uuid(r.source_annotation_id);
    uuid(r.target_annotation_id);
    require(ids.contains(r.source_annotation_id) &&
                relation_ids.insert(r.id).second,
            "Relação sem origem ou duplicada");
    require(r.source_annotation_id != r.target_annotation_id,
            "Autorrelacionamento semântico não permitido");
    validate_semantic_key(r.type_key);
    require(r.origin == "human" || r.origin == "imported",
            "Origem de relação inválida");
    text(r.notes, 4096);
    text(r.created_at, 128);
    require(!r.created_at.empty(), "Data de relação ausente");
  }
}
std::vector<SemanticDiagnostic>
semantic_diagnostics(const Document &d,
                     const std::vector<AnnotationTypeDefinition> &types) {
  std::vector<SemanticDiagnostic> out;
  for (const auto &a : d.semantics.annotations) {
    auto add = [&](const std::string &code, const std::string &m) {
      out.push_back({a.id, code, m, false});
    };
    auto e =
        std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                     [&](auto &v) { return v.anchor_id == a.anchor_id; });
    if (e != d.semantics.anchors.end() &&
        e->state != AnchorResolution::Resolved)
      add("anchor." + to_string(e->state), e->reason);
    if (e != d.semantics.anchors.end() &&
        e->state == AnchorResolution::Resolved) {
      auto anchor = std::find_if(d.anchors.begin(), d.anchors.end(),
                                 [&](auto &v) { return v.id == e->anchor_id; });
      if (anchor != d.anchors.end() &&
          g_utf8_validate(d.content.data(), d.content.size(), nullptr) &&
          anchor->start_offset <= anchor->end_offset &&
          anchor->end_offset <= utf8_character_count(d.content)) {
        auto digest = [&](std::size_t start, std::size_t end) {
          auto *begin = g_utf8_offset_to_pointer(d.content.c_str(), start);
          auto *last = g_utf8_offset_to_pointer(d.content.c_str(), end);
          auto *hash = g_compute_checksum_for_data(
              G_CHECKSUM_SHA256, reinterpret_cast<const guchar *>(begin),
              last - begin);
          std::string out(hash);
          g_free(hash);
          return out;
        };
        auto start = anchor->start_offset, end = anchor->end_offset;
        bool same =
            e->selected_hash == digest(start, end) &&
            e->before_hash == digest(start > 32 ? start - 32 : 0, start) &&
            e->after_hash ==
                digest(end,
                       std::min(utf8_character_count(d.content), end + 32));
        if (!same)
          out.push_back(
              {a.id, "anchor.fingerprint",
               "Fingerprint divergente; reancoragem humana necessária", true});
      }
    }
    auto *t = find_annotation_type(types, a.type_key, a.type_version);
    if (!t) {
      add("type.unavailable",
          "Tipo/versão indisponível; dados preservados sem interpretação");
      continue;
    }
    for (const auto &f : t->fields) {
      auto v = a.fields.find(f.key);
      if (v == a.fields.end() || v->second.values.empty()) {
        if (f.required)
          add("field.required", "Campo obrigatório ausente: " + f.label);
        continue;
      }
      if (v->second.kind != f.kind)
        add("field.kind", "Tipo de campo incompatível: " + f.label);
      if (!f.multiple && v->second.values.size() > 1)
        add("field.multiple",
            "Campo singular com múltiplos valores: " + f.label);
      if (f.kind == SemanticFieldKind::Enum)
        for (auto &x : v->second.values)
          if (std::find(f.values.begin(), f.values.end(), x) == f.values.end())
            add("field.enum", "Opção desconhecida: " + f.label + " = " + x);
    }
    for (const auto &[k, v] : a.fields) {
      (void)v;
      if (std::none_of(t->fields.begin(), t->fields.end(),
                       [&](auto &f) { return f.key == k; }))
        add("field.unavailable", "Campo desconhecido preservado: " + k);
    }
    if (a.type_key == "inde.semantic.perspective" && a.type_version == 1) {
      const auto focal = a.fields.find("focalization");
      const auto who = a.fields.find("viewpoint");
      const bool has_who = who != a.fields.end() && !who->second.values.empty();
      if (focal != a.fields.end() &&
          focal->second.values == std::vector<std::string>{"internal"} &&
          !has_who)
        add("perspective.viewpoint.unspecified",
            "Focalização interna sem focalizador declarado. É uma revisão "
            "opcional: nenhum personagem será atribuído automaticamente.");
    }
  }
  return out;
}
} // namespace inde::project

#include "inde/application/text_semantic_service.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/writing_service.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/project/manifest.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::application {
void WritingService::validate_semantic_document(
    const project::Document &d) const {
  persistence::SemanticStore{}.validate(require_project().path(), d);
}
void WritingService::register_semantic_type_json(const std::string &s) {
  register_semantic_type(persistence::decode_semantic_type(s));
}
std::vector<project::AnnotationTypeDefinition>
WritingService::semantic_types() const {
  return persistence::SemanticStore{}.types(require_project().path());
}
void WritingService::register_semantic_type(
    const project::AnnotationTypeDefinition &t) {
  persistence::SemanticStore{}.add_type(require_project().path(), t);
}
persistence::SemanticUsePage
WritingService::semantic_uses(const persistence::SemanticUseQuery &q) const {
  return persistence::SemanticStore{}.uses(require_project().path(), q);
}
persistence::SemanticRelationPage WritingService::semantic_relations(
    const persistence::SemanticRelationQuery &q) const {
  return persistence::SemanticStore{}.relations(require_project().path(), q);
}
namespace {
void preserve_editorial_item(const project::TextAnnotation &a) {
  if (a.type_key == "inde.editor.review-state" ||
      a.type_key == "inde.editor.suggestion")
    throw std::runtime_error("Use as decisões explícitas em Revisar; o item "
                             "editorial e seu histórico são preservados");
}
project::TextAnnotation &find(project::Document &d, const std::string &id) {
  auto a = std::find_if(d.semantics.annotations.begin(),
                        d.semantics.annotations.end(),
                        [&](auto &x) { return x.id == id; });
  if (a == d.semantics.annotations.end())
    throw std::runtime_error("Anotação não encontrada");
  return *a;
}
void check_fields(
    project::TextAnnotation &a, const project::AnnotationTypeDefinition &t,
    const std::map<std::string, project::SemanticFieldValue> &fields) {
  bool incomplete = false;
  for (auto &[key, v] : fields) {
    auto f = std::find_if(t.fields.begin(), t.fields.end(),
                          [&](auto &x) { return x.key == key; });
    if (f == t.fields.end())
      throw std::runtime_error("Novo campo não pertence à definição");
    if (v.kind != f->kind || (!f->multiple && v.values.size() > 1))
      throw std::runtime_error("Campo incompatível com o tipo semântico");
    if (v.kind == project::SemanticFieldKind::Enum)
      for (auto &s : v.values)
        if (std::find(f->values.begin(), f->values.end(), s) == f->values.end())
          throw std::runtime_error("Opção semântica inválida");
  }
  // Unknown stored fields survive; only fields understood by this definition
  // change.
  for (auto &f : t.fields) {
    a.fields.erase(f.key);
    auto v = fields.find(f.key);
    if (v != fields.end())
      a.fields.emplace(f.key, v->second);
    if (f.required && (v == fields.end() || v->second.values.empty()))
      incomplete = true;
  }
  if (a.status != "archived")
    a.status = incomplete ? "incomplete" : "active";
  a.updated_at = project::utc_now();
}
} // namespace
project::Document TextSemanticService::create(
    const project::Document &input, const project::AnnotationTypeDefinition &t,
    std::size_t start, std::size_t end,
    const std::map<std::string, project::SemanticFieldValue> &fields) {
  project::validate(t);
  auto d = input;
  project::DocumentAnchor a{project::new_uuid(), t.label, start, end};
  auto evidence = SemanticAnchorEngine::evidence(d, a);
  if (!t.allow_overlap)
    for (auto &old : d.semantics.annotations)
      if (old.status != "archived") {
        auto anchor =
            std::find_if(d.anchors.begin(), d.anchors.end(),
                         [&](auto &x) { return x.id == old.anchor_id; });
        if (anchor != d.anchors.end() && start < anchor->end_offset &&
            end > anchor->start_offset)
          throw std::runtime_error("Este tipo não permite sobreposição");
      }
  project::TextAnnotation item;
  item.id = project::new_uuid();
  item.anchor_id = a.id;
  item.type_key = t.key;
  item.type_version = t.version;
  item.created_at = project::utc_now();
  check_fields(item, t, fields);
  d.anchors.push_back(a);
  d.semantics.anchors.push_back(evidence);
  d.semantics.annotations.push_back(item);
  project::validate(d);
  return d;
}
project::Document TextSemanticService::update(
    const project::Document &input, const std::string &id,
    const std::map<std::string, project::SemanticFieldValue> &fields,
    const std::vector<project::AnnotationTypeDefinition> &types) {
  auto d = input;
  auto &a = find(d, id);
  auto *t = project::find_annotation_type(types, a.type_key, a.type_version);
  preserve_editorial_item(a);
  if (!t)
    throw std::runtime_error(
        "Tipo indisponível: edição de campos bloqueada para preservar payload");
  check_fields(a, *t, fields);
  project::validate(d);
  return d;
}
project::Document TextSemanticService::create_document_perspective(
    const project::Document &input,
    const std::map<std::string, project::SemanticFieldValue> &fields) {
  auto d = input;
  const auto types = project::builtin_annotation_types();
  const auto &type =
      *project::find_annotation_type(types, "inde.semantic.perspective", 1);
  project::TextAnnotation a;
  a.id = project::new_uuid();
  a.type_key = type.key;
  a.type_version = type.version;
  a.scope = project::AnnotationScope::Document;
  a.fields = fields;
  a.created_at = a.updated_at = project::utc_now();
  check_fields(a, type, fields);
  d.semantics.annotations.push_back(std::move(a));
  project::validate(d);
  return d;
}
project::Document TextSemanticService::bind_date(const project::Document &input,
                                                 const std::string &annotation,
                                                 const std::string &reference,
                                                 std::string role,
                                                 std::string notes) {
  auto d = input;
  auto &a = find(d, annotation);
  preserve_editorial_item(a);
  if (std::any_of(a.dates.begin(), a.dates.end(), [&](const auto &b) {
        return b.reference_id == reference && b.role == role;
      }))
    throw std::runtime_error("Data e papel já vinculados");
  a.dates.push_back({project::new_uuid(), reference, std::move(role),
                     std::move(notes), project::utc_now()});
  a.updated_at = project::utc_now();
  project::validate(d);
  return d;
}
project::Document
TextSemanticService::unbind_date(const project::Document &input,
                                 const std::string &annotation,
                                 const std::string &binding) {
  auto d = input;
  auto &a = find(d, annotation);
  preserve_editorial_item(a);
  if (!std::erase_if(a.dates, [&](const auto &b) { return b.id == binding; }))
    throw std::runtime_error("Vínculo de data ausente");
  a.updated_at = project::utc_now();
  project::validate(d);
  return d;
}
project::Document TextSemanticService::bind_fictional_time(
    const project::Document &input, const std::string &annotation,
    const std::string &axis, const std::string &point,
    std::optional<std::string> end, std::string notes) {
  auto d = input;
  auto &a = find(d, annotation);
  if (std::any_of(a.fictional_time.begin(), a.fictional_time.end(),
                  [&](auto &b) {
                    return b.fictional_axis_id == axis &&
                           b.fictional_start_point_id == point &&
                           b.fictional_end_point_id == end;
                  }))
    throw std::runtime_error("Vínculo temporal já cadastrado");
  a.fictional_time.push_back({project::new_uuid(), axis, point, std::move(end),
                              "human", "", std::move(notes),
                              project::utc_now()});
  a.updated_at = project::utc_now();
  project::validate(d);
  return d;
}
project::Document
TextSemanticService::unbind_fictional_time(const project::Document &input,
                                           const std::string &annotation,
                                           const std::string &binding) {
  auto d = input;
  auto &a = find(d, annotation);
  if (!std::erase_if(a.fictional_time,
                     [&](auto &b) { return b.id == binding; }))
    throw std::runtime_error("Vínculo temporal não encontrado");
  a.updated_at = project::utc_now();
  project::validate(d);
  return d;
}
TextSemanticService::PerspectiveContext
TextSemanticService::perspective_at(const project::Document &input,
                                    std::size_t position) {
  auto d = input;
  SemanticAnchorEngine::inspect(d);
  if (position > project::utf8_character_count(d.content))
    throw std::runtime_error("Posição fora do Documento");
  PerspectiveContext out;
  for (const auto &a : d.semantics.annotations) {
    if (a.type_key != "inde.semantic.perspective" || a.type_version != 1 ||
        a.status == "archived")
      continue;
    if (a.scope == project::AnnotationScope::Document) {
      out.document_declaration = a.id;
      out.incomplete |= a.status != "active";
    } else {
      auto k = std::find_if(d.anchors.begin(), d.anchors.end(),
                            [&](auto &v) { return v.id == a.anchor_id; });
      auto e =
          std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                       [&](auto &v) { return v.anchor_id == a.anchor_id; });
      if (k != d.anchors.end() && e != d.semantics.anchors.end() &&
          e->state == project::AnchorResolution::Resolved &&
          k->start_offset <= position && position < k->end_offset) {
        out.interval_declarations.push_back(a.id);
        out.incomplete |= a.status != "active";
      }
    }
  }
  std::sort(out.interval_declarations.begin(), out.interval_declarations.end());
  out.ambiguous = out.interval_declarations.size() > 1;
  if (!out.ambiguous && !out.incomplete)
    out.effective_declaration =
        out.interval_declarations.empty()
            ? out.document_declaration
            : std::optional<std::string>(out.interval_declarations.front());
  return out;
}
project::Document TextSemanticService::archive(const project::Document &input,
                                               const std::string &id,
                                               bool archived) {
  auto d = input;
  auto &a = find(d, id);
  preserve_editorial_item(a);
  a.status = archived ? "archived" : "incomplete";
  a.updated_at = project::utc_now();
  project::validate(d);
  return d;
}
project::Document TextSemanticService::remove(const project::Document &input,
                                              const std::string &id) {
  auto d = input;
  preserve_editorial_item(find(d, id));
  auto anchor = find(d, id).anchor_id;
  if (std::any_of(d.semantics.relations.begin(), d.semantics.relations.end(),
                  [&](auto &r) { return r.target_annotation_id == id; }))
    throw std::runtime_error("Anotação possui relações recebidas; remova-as "
                             "explicitamente ou arquive");
  std::erase_if(d.semantics.annotations, [&](auto &a) { return a.id == id; });
  std::erase_if(d.semantics.relations,
                [&](auto &r) { return r.source_annotation_id == id; });
  if (std::none_of(d.semantics.annotations.begin(),
                   d.semantics.annotations.end(),
                   [&](auto &a) { return a.anchor_id == anchor; }))
    std::erase_if(d.semantics.anchors,
                  [&](auto &e) { return e.anchor_id == anchor; });
  // Legacy anchor remains: other documentary links can still use its UUID.
  project::validate(d);
  return d;
}
project::Document TextSemanticService::reanchor(const project::Document &input,
                                                const std::string &id,
                                                std::size_t start,
                                                std::size_t end) {
  auto d = input;
  if (find(d, id).scope == project::AnnotationScope::Document)
    throw std::runtime_error("Declaração global não possui âncora; edite sua "
                             "perspectiva em vez de reancorar");
  SemanticAnchorEngine::reanchor(d, id, start, end);
  find(d, id).updated_at = project::utc_now();
  project::validate(d);
  return d;
}
project::Document TextSemanticService::relate(const project::Document &input,
                                              const std::string &source,
                                              const std::string &target,
                                              const std::string &type) {
  auto d = input;
  (void)find(d, source);
  if (type != "inde.semantic.reference" &&
      type != "inde.semantic.response-to" && type != "inde.semantic.callback")
    throw std::runtime_error("Relação semântica desconhecida");
  if (std::any_of(d.semantics.relations.begin(), d.semantics.relations.end(),
                  [&](auto &r) {
                    return r.source_annotation_id == source &&
                           r.target_annotation_id == target &&
                           r.type_key == type;
                  }))
    throw std::runtime_error("Relação já cadastrada");
  d.semantics.relations.push_back({project::new_uuid(), source, target, type,
                                   "human", "", project::utc_now()});
  project::validate(d);
  return d;
}
project::Document TextSemanticService::unrelate(const project::Document &input,
                                                const std::string &id) {
  auto d = input;
  auto count =
      std::erase_if(d.semantics.relations, [&](auto &r) { return r.id == id; });
  if (!count)
    throw std::runtime_error("Relação não encontrada");
  project::validate(d);
  return d;
}
} // namespace inde::application

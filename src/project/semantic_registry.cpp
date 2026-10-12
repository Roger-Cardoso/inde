#include "inde/project/text_semantics.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::project {
std::string to_string(AnnotationScope scope) {
  switch (scope) {
  case AnnotationScope::Interval:
    return "interval";
  case AnnotationScope::Document:
    return "document";
  }
  throw std::runtime_error("Escopo semântico inválido");
}
AnnotationScope annotation_scope(const std::string &s) {
  if (s == "interval")
    return AnnotationScope::Interval;
  if (s == "document")
    return AnnotationScope::Document;
  throw std::runtime_error("Escopo semântico desconhecido");
}
namespace {
SemanticFieldDefinition field(std::string key, std::string label,
                              SemanticFieldKind kind, bool required = false,
                              bool multiple = false,
                              std::vector<std::string> values = {},
                              std::vector<std::string> entities = {}) {
  return {std::move(key),    std::move(label),   kind, required, multiple,
          std::move(values), std::move(entities)};
}
} // namespace
const std::vector<AnnotationTypeDefinition> &builtin_annotation_types() {
  using K = SemanticFieldKind;
  static const std::vector<AnnotationTypeDefinition> values{
      {"inde.semantic.narration",
       "Narração",
       "Modo narrativo declarado pelo autor",
       1,
       true,
       true,
       {field("kind", "Modo de narração", K::Enum, false, false,
              {"action", "exposition", "summary", "transition", "backstory",
               "reflection"})}},
      {"inde.semantic.description",
       "Descrição",
       "Alvos canônicos e aspectos de descrição",
       1,
       true,
       true,
       {field("targets", "Alvos da descrição", K::Entity, false, true),
        field("aspects", "Aspectos", K::Enum, false, true,
              {"general", "appearance", "body", "clothing", "architecture",
               "weather", "condition", "movement", "action", "sensory"})}},
      {"inde.semantic.dialogue",
       "Diálogo",
       "Fala e interlocução declaradas, sem inferência",
       1,
       true,
       true,
       {field("speaker", "Falante", K::Entity, true, false, {}, {"character"}),
        field("addressees", "Interlocutores", K::Entity, false, true, {},
              {"character"}),
        field("medium", "Meio", K::Enum, false, false,
              {"spoken", "written", "telepathy"}),
        field("acts", "Atos de fala", K::Enum, false, true,
              {"statement",    "question", "command",    "request",
               "promise",      "threat",   "warning",    "apology",
               "greeting",     "farewell", "accusation", "denial",
               "confirmation", "refusal",  "suggestion", "invitation",
               "explanation",  "joke",     "insult",     "praise",
               "confession"}),
        field("interaction", "Interação", K::Enum, false, false,
              {"initiative", "response", "reply", "counter_reply", "follow_up",
               "interruption"})}},
      {"inde.semantic.thought",
       "Pensamento",
       "Pensador e forma, sem atribuição automática",
       1,
       true,
       true,
       {field("thinker", "Pensador", K::Entity, true, false, {}, {"character"}),
        field(
            "form", "Forma de pensamento", K::Enum, false, false,
            {"internal_monologue", "free_indirect", "stream_of_consciousness"}),
        field("kinds", "Tipos de pensamento", K::Enum, false, true,
              {"reflection", "memory", "imagination", "reasoning", "emotion",
               "intention", "decision"})}},
      {"inde.semantic.marked-passage",
       "Passagem marcada",
       "Categoria autoral para reencontrar trechos",
       1,
       true,
       true,
       {field("category", "Categoria", K::Enum, true, false,
              {"clue", "foreshadowing", "motif", "callback", "editorial"}),
        field("note", "Nota", K::Text)}},
      {"inde.semantic.perspective",
       "Perspectiva situada",
       "Pessoa e focalização declaradas neste intervalo; não substitui a "
       "perspectiva livre do Documento nem afirma fatos da realidade ficcional",
       1,
       true,
       true,
       {field("person", "Pessoa narrativa", K::Enum, true, false,
              {"first_person", "second_person", "third_person"}),
        field("focalization", "Focalização", K::Enum, true, false,
              {"internal", "external", "omniscient", "variable"}),
        field("viewpoint", "Focalizadores", K::Entity, false, true, {},
              {"character"}),
        field("note", "Nota de perspectiva", K::Text)}},
      {"inde.semantic.temporal-context",
       "Tempo do trecho",
       "Contexto temporal autoral; presença opcional explícita, sem inferência "
       "da menção ou da perspectiva",
       1,
       true,
       true,
       {field("participants", "Entidades presentes (declaração opcional)",
              K::Entity, false, true),
        field("exception", "Contexto excepcional", K::Enum, false, false,
              {"time_travel", "memory", "prophecy", "unreliable_narrator",
               "other"}),
        field("note", "Explicação do contexto", K::Text)}},
      {"inde.semantic.entity-mention",
       "Ocorrência de entidade",
       "Entidade canônica escolhida pelo autor para este trecho; a âncora "
       "sinaliza substituição integral",
       1,
       true,
       true,
       {field("target", "Entidade do trecho", K::Entity, true),
        field("note", "Nota do vínculo", K::Text)}},
      {"inde.semantic.materialization",
       "Materialização associativa",
       "Trecho associado explicitamente a uma entidade ou informação; não "
       "afirma datas nem cria fatos",
       1,
       true,
       true,
       {field("target", "Entidade ou informação associada", K::Entity, true),
        field("purpose", "Papel do trecho", K::Enum, true, false,
              {"materialization", "evidence", "mention", "contradiction"}),
        field("note", "Explicação da associação", K::Text)}},
      {"inde.editor.comment",
       "Comentário",
       "Nota editorial do autor sobre um trecho; não altera os fatos "
       "ficcionais",
       1,
       true,
       true,
       {field("note", "Comentário", K::Text, true),
        field("state", "Situação", K::Enum, false, false,
              {"open", "resolved"})}},
      {"inde.editor.suggestion",
       "Sugestão editorial",
       "Mudança textual proposta explicitamente; não é aplicada ao escrever",
       1,
       true,
       true,
       {field("expected", "Trecho observado", K::Text, true),
        field("replacement", "Texto proposto (vazio remove o trecho)", K::Text,
              true),
        field("note", "Motivo", K::Text),
        field("state", "Decisão", K::Enum, true, false,
              {"pending", "accepted", "rejected"})}},
      {"inde.editor.review-state",
       "Decisão editorial do Documento",
       "Registro explícito, versionado e vinculado ao conteúdo revisado",
       1,
       true,
       true,
       {field("state", "Estado", K::Enum, true, false,
              {"draft", "in_review", "changes_requested", "approved"}),
        field("sequence", "Sequência", K::Integer, true),
        field("reviewer", "Responsável", K::Text, true),
        field("note", "Parecer", K::Text),
        field("fingerprint", "Fonte revisada (SHA-256)", K::Text, true)}}};
  return values;
}
const AnnotationTypeDefinition *
find_annotation_type(const std::vector<AnnotationTypeDefinition> &types,
                     const std::string &key, std::uint32_t version) {
  const auto it = std::find_if(types.begin(), types.end(), [&](const auto &t) {
    return t.key == key && t.version == version;
  });
  return it == types.end() ? nullptr : &*it;
}
std::string to_string(SemanticFieldKind k) {
  switch (k) {
  case SemanticFieldKind::Text:
    return "text";
  case SemanticFieldKind::Enum:
    return "enum";
  case SemanticFieldKind::Integer:
    return "integer";
  case SemanticFieldKind::Boolean:
    return "boolean";
  case SemanticFieldKind::Entity:
    return "entity";
  }
  throw std::runtime_error("Tipo de campo semântico inválido");
}
SemanticFieldKind semantic_field_kind(const std::string &s) {
  for (auto k : {SemanticFieldKind::Text, SemanticFieldKind::Enum,
                 SemanticFieldKind::Integer, SemanticFieldKind::Boolean,
                 SemanticFieldKind::Entity})
    if (to_string(k) == s)
      return k;
  throw std::runtime_error("Tipo de campo semântico desconhecido");
}
std::string to_string(AnchorResolution k) {
  switch (k) {
  case AnchorResolution::Resolved:
    return "resolved";
  case AnchorResolution::Ambiguous:
    return "ambiguous";
  case AnchorResolution::Orphaned:
    return "orphaned";
  }
  throw std::runtime_error("Estado de âncora inválido");
}
AnchorResolution anchor_resolution(const std::string &s) {
  for (auto k : {AnchorResolution::Resolved, AnchorResolution::Ambiguous,
                 AnchorResolution::Orphaned})
    if (to_string(k) == s)
      return k;
  throw std::runtime_error("Estado de âncora desconhecido");
}
std::string to_string(AnchorGravity k) {
  switch (k) {
  case AnchorGravity::Left:
    return "left";
  case AnchorGravity::Right:
    return "right";
  }
  throw std::runtime_error("Gravidade inválida");
}
AnchorGravity anchor_gravity(const std::string &s) {
  if (s == "left")
    return AnchorGravity::Left;
  if (s == "right")
    return AnchorGravity::Right;
  throw std::runtime_error("Gravidade desconhecida");
}
} // namespace inde::project

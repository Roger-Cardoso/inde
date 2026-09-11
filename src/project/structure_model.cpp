#include "inde/project/structure_model.hpp"

#include <stdexcept>

namespace inde::project {
namespace {
void require_text(const std::string &value, const char *message) {
  if (value.empty())
    throw std::runtime_error(message);
}
void require_common(const std::string &id, const std::string &created_at,
                    const std::string &updated_at) {
  require_text(id, "A identidade estrutural é obrigatória");
  require_text(created_at, "A data de criação estrutural é obrigatória");
  require_text(updated_at, "A data de atualização estrutural é obrigatória");
}
} // namespace

std::string to_string(StructureLayer value) {
  return value == StructureLayer::Editorial ? "editorial" : "narrative";
}
StructureLayer structure_layer_from_string(const std::string &value) {
  if (value == "editorial")
    return StructureLayer::Editorial;
  if (value == "narrative")
    return StructureLayer::Narrative;
  throw std::runtime_error("Camada estrutural desconhecida: " + value);
}
std::string to_string(StructureCreationKind value) {
  switch (value) {
  case StructureCreationKind::Migrated:
    return "migrated";
  case StructureCreationKind::Blank:
    return "blank";
  case StructureCreationKind::Instantiated:
    return "instantiated";
  case StructureCreationKind::Duplicated:
    return "duplicated";
  case StructureCreationKind::Derived:
    return "derived";
  }
  throw std::runtime_error("Origem estrutural desconhecida");
}
StructureCreationKind
structure_creation_kind_from_string(const std::string &value) {
  if (value == "migrated")
    return StructureCreationKind::Migrated;
  if (value == "blank")
    return StructureCreationKind::Blank;
  if (value == "instantiated")
    return StructureCreationKind::Instantiated;
  if (value == "duplicated")
    return StructureCreationKind::Duplicated;
  if (value == "derived")
    return StructureCreationKind::Derived;
  throw std::runtime_error("Origem estrutural desconhecida: " + value);
}
std::string to_string(StructureModelItemKind value) {
  switch (value) {
  case StructureModelItemKind::EditorialNode:
    return "editorial-node";
  case StructureModelItemKind::NarrativeLine:
    return "narrative-line";
  case StructureModelItemKind::NarrativeUnit:
    return "narrative-unit";
  }
  throw std::runtime_error("Tipo de item de modelo desconhecido");
}
StructureModelItemKind
structure_model_item_kind_from_string(const std::string &value) {
  if (value == "editorial-node")
    return StructureModelItemKind::EditorialNode;
  if (value == "narrative-line")
    return StructureModelItemKind::NarrativeLine;
  if (value == "narrative-unit")
    return StructureModelItemKind::NarrativeUnit;
  throw std::runtime_error("Tipo de item de modelo desconhecido: " + value);
}
std::string to_string(NarrativeLinkKind value) {
  switch (value) {
  case NarrativeLinkKind::Precedes:
    return "precedes";
  case NarrativeLinkKind::Causes:
    return "causes";
  case NarrativeLinkKind::Alternative:
    return "alternative";
  case NarrativeLinkKind::Depends:
    return "depends";
  }
  throw std::runtime_error("Tipo de vínculo narrativo desconhecido");
}
NarrativeLinkKind narrative_link_kind_from_string(const std::string &value) {
  if (value == "precedes")
    return NarrativeLinkKind::Precedes;
  if (value == "causes")
    return NarrativeLinkKind::Causes;
  if (value == "alternative")
    return NarrativeLinkKind::Alternative;
  if (value == "depends")
    return NarrativeLinkKind::Depends;
  throw std::runtime_error("Tipo de vínculo narrativo desconhecido: " + value);
}

void validate(const StructureModel &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.key, "A chave do modelo é obrigatória");
  require_text(value.name, "O nome do modelo é obrigatório");
}
void validate(const StructureModelItem &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.model_id, "O modelo do item é obrigatório");
  require_text(value.title, "O título do item de modelo é obrigatório");
  if (value.designator.size() > 64)
    throw std::runtime_error("O designador do modelo excede 64 caracteres");
  if (value.parent_id && *value.parent_id == value.id)
    throw std::runtime_error("Um item de modelo não pode ser pai de si mesmo");
}
void validate(const StructureModelItemLink &value) {
  require_text(value.id, "A identidade do vínculo de modelo é obrigatória");
  require_text(value.model_id, "O modelo do vínculo é obrigatório");
  require_text(value.source_item_id, "A origem do vínculo é obrigatória");
  require_text(value.target_item_id, "O destino do vínculo é obrigatório");
  if (value.source_item_id == value.target_item_id)
    throw std::runtime_error("Um vínculo de modelo exige itens distintos");
  if (value.kind != "membership" && value.kind != "precedes" &&
      value.kind != "causes" && value.kind != "alternative" &&
      value.kind != "depends")
    throw std::runtime_error("Tipo de vínculo de modelo desconhecido");
}
void validate(const EditorialStructure &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.work_id, "A Obra da estrutura editorial é obrigatória");
  require_text(value.name, "O nome da estrutura editorial é obrigatório");
  if (value.derived_from_id && *value.derived_from_id == value.id)
    throw std::runtime_error("Uma estrutura não pode derivar de si mesma");
}
void validate(const NarrativeStructure &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.work_id, "A Obra da estrutura narrativa é obrigatória");
  require_text(value.name, "O nome da estrutura narrativa é obrigatório");
  if (value.derived_from_id && *value.derived_from_id == value.id)
    throw std::runtime_error("Uma estrutura não pode derivar de si mesma");
}
void validate(const NarrativeLine &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.structure_id, "A estrutura da linha é obrigatória");
  require_text(value.name, "O nome da linha narrativa é obrigatório");
}
void validate(const NarrativeUnit &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.structure_id, "A estrutura da unidade é obrigatória");
  require_text(value.title, "O título da unidade narrativa é obrigatório");
  if (value.designator.size() > 64)
    throw std::runtime_error("O designador narrativo excede 64 caracteres");
}
void validate(const NarrativeUnitLine &value) {
  require_text(value.unit_id, "A unidade narrativa é obrigatória");
  require_text(value.line_id, "A linha narrativa é obrigatória");
}
void validate(const NarrativeUnitEntity &value) {
  require_text(value.unit_id, "A unidade narrativa é obrigatória");
  require_text(value.entity_id, "A entidade narrativa é obrigatória");
}
void validate(const NarrativeLink &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.structure_id, "A estrutura do vínculo é obrigatória");
  require_text(value.source_unit_id, "A origem do vínculo é obrigatória");
  require_text(value.target_unit_id, "O destino do vínculo é obrigatório");
  if (value.source_unit_id == value.target_unit_id)
    throw std::runtime_error("Um vínculo narrativo exige unidades distintas");
}
void validate(const NarrativeRole &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.name, "O nome do papel narrativo é obrigatório");
}
void validate(const NarrativeRoleAssignment &value) {
  require_common(value.id, value.created_at, value.updated_at);
  require_text(value.role_id, "O papel narrativo é obrigatório");
  require_text(value.entity_id, "A entidade do papel é obrigatória");
  require_text(value.work_id, "A Obra do papel é obrigatória");
  if (value.unit_id && !value.structure_id)
    throw std::runtime_error(
        "Uma unidade exige contexto de estrutura narrativa");
}

} // namespace inde::project

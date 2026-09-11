#include "inde/project/narrative.hpp"

#include <cctype>
#include <stdexcept>
#include <uuid/uuid.h>

namespace inde::project {
namespace {

void validate_uuid(const std::string &value, const char *label) {
  uuid_t parsed{};
  if (uuid_parse(value.c_str(), parsed) != 0)
    throw std::runtime_error(std::string(label) + " possui UUID inválido");
}

void validate_dates(const std::string &created_at,
                    const std::string &updated_at, const char *label) {
  if (created_at.empty() || updated_at.empty())
    throw std::runtime_error(std::string("Datas ausentes em ") + label);
}

} // namespace

const std::vector<BuiltinEntityType> &builtin_entity_types() {
  static const std::vector<BuiltinEntityType> values{
      {"00000000-0000-4000-9000-000000000001", "character", "Personagem",
       "Pessoa ou agente individual da realidade ficcional."},
      {"00000000-0000-4000-9000-000000000002", "location", "Local",
       "Lugar persistente da realidade ficcional."},
      {"00000000-0000-4000-9000-000000000003", "event", "Acontecimento",
       "Ocorrência da realidade ficcional, distinta da cena editorial."},
      {"00000000-0000-4000-9000-000000000004", "information", "Informação",
       "Proposição ou conhecimento referenciável."},
      {"00000000-0000-4000-9000-000000000005", "objective", "Objetivo",
       "Intenção persistente de uma entidade narrativa."},
  };
  return values;
}

std::string to_string(RelationDirectionality value) {
  switch (value) {
  case RelationDirectionality::Directed:
    return "directed";
  case RelationDirectionality::Symmetric:
    return "symmetric";
  }
  throw std::runtime_error("Direcionalidade de relação desconhecida");
}

RelationDirectionality
relation_directionality_from_string(const std::string &value) {
  if (value == "directed")
    return RelationDirectionality::Directed;
  if (value == "symmetric")
    return RelationDirectionality::Symmetric;
  throw std::runtime_error("Direcionalidade de relação inválida: " + value);
}

void validate_key(const std::string &value) {
  if (value.empty() || value.size() > 64 ||
      value.front() < 'a' || value.front() > 'z')
    throw std::runtime_error(
        "A chave deve começar com letra minúscula e ter até 64 caracteres");
  for (const unsigned char character : value) {
    if ((character >= 'a' && character <= 'z') ||
        (character >= '0' && character <= '9') || character == '-')
      continue;
    throw std::runtime_error(
        "A chave aceita apenas letras minúsculas, números e hífen");
  }
}

void validate(const EntityType &value) {
  validate_uuid(value.id, "O tipo de entidade");
  validate_key(value.key);
  if (value.name.empty())
    throw std::runtime_error("O nome do tipo de entidade é obrigatório");
  validate_dates(value.created_at, value.updated_at, "tipo de entidade");
}

void validate(const NarrativeEntity &value) {
  validate_uuid(value.id, "A entidade narrativa");
  validate_uuid(value.entity_type_id, "O tipo da entidade narrativa");
  if (value.name.empty())
    throw std::runtime_error("O nome da entidade narrativa é obrigatório");
  validate_dates(value.created_at, value.updated_at, "entidade narrativa");
}

void validate(const RelationType &value) {
  validate_uuid(value.id, "O tipo de relação");
  validate_key(value.key);
  if (value.name.empty())
    throw std::runtime_error("O nome do tipo de relação é obrigatório");
  if (value.directionality == RelationDirectionality::Directed &&
      value.inverse_name.empty())
    throw std::runtime_error(
        "Uma relação direcional precisa de nome para o sentido inverso");
  validate_dates(value.created_at, value.updated_at, "tipo de relação");
}

void validate(const NarrativeRelation &value) {
  validate_uuid(value.id, "A relação narrativa");
  validate_uuid(value.relation_type_id, "O tipo da relação narrativa");
  validate_uuid(value.source_entity_id, "A origem da relação narrativa");
  validate_uuid(value.target_entity_id, "O destino da relação narrativa");
  if (value.source_entity_id == value.target_entity_id)
    throw std::runtime_error("Uma entidade não pode se relacionar consigo mesma");
  if (value.fictional_time_point_id)
    validate_uuid(*value.fictional_time_point_id,
                  "O ponto ficcional da relação narrativa");
  if (value.location_entity_id)
    validate_uuid(*value.location_entity_id,
                  "O Local da relação narrativa");
  if (value.cause_entity_id)
    validate_uuid(*value.cause_entity_id,
                  "A causa da relação narrativa");
  validate_dates(value.created_at, value.updated_at, "relação narrativa");
}

void validate(const EntityWorkScope &value) {
  validate_uuid(value.id, "O vínculo entre entidade e obra");
  validate_uuid(value.entity_id, "A entidade do vínculo com a obra");
  validate_uuid(value.work_id, "A obra do vínculo com a entidade");
  validate_dates(value.created_at, value.updated_at,
                 "vínculo entre entidade e obra");
}

void validate(const EditorialEntityReference &value) {
  validate_uuid(value.id, "A referência editorial de entidade");
  validate_uuid(value.entity_id, "A entidade da referência editorial");
  validate_uuid(value.work_id, "A obra da referência editorial");
  validate_uuid(value.editorial_node_id,
                "A unidade da referência editorial");
  if (value.purpose.empty())
    throw std::runtime_error("A finalidade da referência editorial é obrigatória");
  validate_dates(value.created_at, value.updated_at,
                 "referência editorial de entidade");
}

} // namespace inde::project

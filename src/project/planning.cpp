#include "inde/project/planning.hpp"

#include "inde/project/manifest.hpp"

#include <stdexcept>
#include <uuid/uuid.h>

namespace inde::project {
namespace {

void require_text(const std::string &value, const char *message) {
  if (value.empty())
    throw std::runtime_error(message);
}

void require_identity(const std::string &value, const char *message) {
  uuid_t parsed{};
  if (uuid_parse(value.c_str(), parsed) != 0)
    throw std::runtime_error(message);
}

void require_dates(const std::string &created_at,
                   const std::string &updated_at) {
  require_text(created_at, "Data de criação do planejamento inválida");
  require_text(updated_at, "Data de atualização do planejamento inválida");
}

} // namespace

void validate(const FictionalTimeAxis &value) {
  require_identity(value.id, "Identidade do eixo temporal inválida");
  require_text(value.name, "O eixo temporal precisa de um nome");
  require_dates(value.created_at, value.updated_at);
}

void validate(const FictionalTimePoint &value) {
  require_identity(value.id, "Identidade do ponto temporal inválida");
  require_identity(value.axis_id, "Identidade do eixo temporal inválida");
  require_text(value.label, "O ponto temporal precisa de um rótulo");
  require_dates(value.created_at, value.updated_at);
}

void validate(const EventOccurrence &value) {
  require_identity(value.id, "Identidade da ocorrência inválida");
  require_identity(value.event_entity_id,
                   "Identidade do acontecimento da ocorrência inválida");
  require_identity(value.time_point_id,
                   "Identidade do ponto temporal da ocorrência inválida");
  require_dates(value.created_at, value.updated_at);
}

void validate(const EventParticipation &value) {
  require_identity(value.id, "Identidade da participação inválida");
  require_identity(value.event_occurrence_id,
                   "Identidade da ocorrência da participação inválida");
  require_identity(value.participant_entity_id,
                   "Identidade do participante inválida");
  require_text(value.role, "A participação precisa declarar um papel");
  require_dates(value.created_at, value.updated_at);
}

void validate(const EntityPresence &value) {
  require_identity(value.id, "Identidade da presença inválida");
  require_identity(value.entity_id, "Identidade da entidade presente inválida");
  require_identity(value.location_entity_id,
                   "Identidade do local da presença inválida");
  require_identity(value.start_time_point_id,
                   "Identidade do início da presença inválida");
  if (value.end_time_point_id)
    require_identity(*value.end_time_point_id,
                     "Identidade do fim da presença inválida");
  if (value.entity_id == value.location_entity_id)
    throw std::runtime_error(
        "Uma entidade não pode estar presente em si mesma");
  require_dates(value.created_at, value.updated_at);
}

} // namespace inde::project

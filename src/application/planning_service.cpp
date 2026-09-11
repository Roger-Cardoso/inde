#include "inde/application/planning_service.hpp"

#include "inde/project/manifest.hpp"
#include "inde/project/narrative.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace inde::application {
namespace {

constexpr auto event_type_id = "00000000-0000-4000-9000-000000000003";
constexpr auto location_type_id = "00000000-0000-4000-9000-000000000002";

std::vector<std::string> normalized_ids(std::vector<std::string> ids,
                                        const char *label) {
  if (ids.size() > 32)
    throw std::runtime_error(std::string(label) +
                             " excede 32 escolhas por categoria");
  if (std::any_of(ids.begin(), ids.end(),
                  [](const auto &id) { return id.empty(); }))
    throw std::runtime_error(std::string(label) + " contém identificador vazio");
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}

std::string joined_labels(const std::vector<std::string> &ids,
                          const std::vector<project::RelationType> &types) {
  std::ostringstream output;
  constexpr std::size_t visible_limit = 3;
  for (std::size_t index = 0; index < ids.size() && index < visible_limit;
       ++index) {
    if (index != 0)
      output << ", ";
    const auto found =
        std::find_if(types.begin(), types.end(), [&](const auto &type) {
          return type.id == ids[index];
        });
    output << (found == types.end() ? "relação indisponível" : found->name);
  }
  if (ids.size() > visible_limit)
    output << " e mais " << ids.size() - visible_limit;
  return output.str();
}

void append_reason(std::string &reason, const std::string &part) {
  if (!reason.empty())
    reason += " • ";
  reason += part;
}

std::string joined_reasons(const std::string &primary,
                           const std::vector<std::string> &auxiliary) {
  std::string result = primary;
  for (const auto &reason : auxiliary)
    append_reason(result, reason);
  return result;
}

void append_unique_role(std::vector<std::string> &roles,
                        const std::string &role) {
  if (std::find(roles.begin(), roles.end(), role) == roles.end())
    roles.push_back(role);
}

std::string joined_roles(const std::vector<std::string> &roles) {
  std::ostringstream output;
  for (std::size_t index = 0; index < roles.size(); ++index) {
    if (index != 0)
      output << (index + 1 == roles.size() ? " e " : ", ");
    output << roles[index];
  }
  return output.str();
}

bool contains_search_text(const std::string &value, const std::string &search) {
  if (search.empty())
    return false;
  const auto fold_ascii = [](std::string text) {
    for (auto &character : text) {
      if (character >= 'A' && character <= 'Z')
        character = static_cast<char>(character - 'A' + 'a');
    }
    return text;
  };
  return fold_ascii(value).find(fold_ascii(search)) != std::string::npos;
}

} // namespace

const project::Project &PlanningService::require_project() const {
  const auto *value = session_.current();
  if (!value)
    throw std::runtime_error("Nenhum projeto está aberto");
  return *value;
}

project::NarrativeEntity
PlanningService::require_entity(const std::string &id) const {
  const auto value = narrative_store_.entity(require_project().path(), id);
  if (!value)
    throw std::runtime_error("Entidade narrativa não encontrada");
  return *value;
}

std::vector<project::FictionalTimeAxis> PlanningService::time_axes() const {
  return store_.time_axes(require_project().path());
}

std::vector<project::FictionalTimePoint>
PlanningService::time_points(const std::string &axis_id,
                             const persistence::PlanningQuery &query) const {
  return store_.time_points(require_project().path(), axis_id, query);
}

std::optional<project::FictionalTimePoint>
PlanningService::time_point(const std::string &id) const {
  return store_.time_point(require_project().path(), id);
}

std::vector<project::EventOccurrence> PlanningService::event_occurrences(
    const persistence::EventOccurrenceQuery &query) const {
  return store_.event_occurrences(require_project().path(), query);
}

std::optional<project::EventOccurrence>
PlanningService::event_occurrence_for_entity(
    const std::string &entity_id) const {
  return store_.event_occurrence_for_entity(require_project().path(),
                                            entity_id);
}

std::vector<project::EventParticipation> PlanningService::event_participations(
    const std::string &occurrence_id,
    const persistence::PlanningQuery &query) const {
  return store_.event_participations(require_project().path(), occurrence_id,
                                     query);
}

std::vector<project::EntityPresence>
PlanningService::presences(const persistence::PresenceQuery &query) const {
  return store_.presences(require_project().path(), query);
}

PlanningContext
PlanningService::normalize_context(const PlanningContext &input) const {
  const auto &active = require_project();
  if (input.limit == 0 || input.limit > 500)
    throw std::runtime_error("Limite do explorador deve estar entre 1 e 500");

  PlanningContext context = input;
  context.entity_type_ids = normalized_ids(
      std::move(context.entity_type_ids), "Filtro de tipos de entidade");
  context.relation_type_ids = normalized_ids(
      std::move(context.relation_type_ids), "Filtro de tipos de relação");
  const auto validate_optional_id = [](const auto &value, const char *label) {
    if (value && value->empty())
      throw std::runtime_error(std::string(label) + " contém identificador vazio");
  };
  validate_optional_id(context.work_id, "Filtro de Obra");
  validate_optional_id(context.related_entity_id, "Filtro de contraparte");
  validate_optional_id(context.fictional_axis_id, "Filtro de eixo ficcional");
  validate_optional_id(context.fictional_time_point_id,
                       "Filtro de ponto ficcional");
  validate_optional_id(context.fictional_window_start_time_point_id,
                       "Início do período ficcional");
  validate_optional_id(context.fictional_window_end_time_point_id,
                       "Fim do período ficcional");
  validate_optional_id(context.editorial_node_id, "Filtro de unidade editorial");
  validate_optional_id(context.document_id, "Filtro de Documento");
  if (context.document_id)
    context.require_document_reference = true;

  const bool has_range_start =
      context.fictional_window_start_time_point_id.has_value();
  const bool has_range_end =
      context.fictional_window_end_time_point_id.has_value();
  if (has_range_start != has_range_end)
    throw std::runtime_error(
        "O período ficcional exige ponto inicial e ponto final");
  if (context.fictional_time_point_id && has_range_start)
    throw std::runtime_error(
        "Escolha um ponto ficcional ou um período fechado, não ambos");

  if (context.fictional_time_point_id) {
    const auto point = store_.time_point(active.path(),
                                         *context.fictional_time_point_id);
    if (!point)
      throw std::runtime_error("Ponto de tempo ficcional não encontrado");
    if (context.fictional_axis_id && *context.fictional_axis_id != point->axis_id)
      throw std::runtime_error(
          "O ponto temporal não pertence ao eixo ficcional selecionado");
    context.fictional_axis_id = point->axis_id;
  }
  if (has_range_start) {
    const auto start = store_.time_point(
        active.path(), *context.fictional_window_start_time_point_id);
    const auto end = store_.time_point(
        active.path(), *context.fictional_window_end_time_point_id);
    if (!start || !end)
      throw std::runtime_error("Ponto do período ficcional não encontrado");
    if (start->axis_id != end->axis_id)
      throw std::runtime_error(
          "Os limites do período precisam pertencer ao mesmo eixo ficcional");
    if (start->ordinal > end->ordinal)
      throw std::runtime_error(
          "O início do período ficcional vem depois do fim");
    if (context.fictional_axis_id && *context.fictional_axis_id != start->axis_id)
      throw std::runtime_error(
          "O período temporal não pertence ao eixo ficcional selecionado");
    context.fictional_axis_id = start->axis_id;
  }
  if (context.fictional_axis_id &&
      !store_.time_axis(active.path(), *context.fictional_axis_id))
    throw std::runtime_error("Eixo de tempo ficcional não encontrado");

  if (context.editorial_node_id) {
    const auto node = std::find_if(
        session_.structural_nodes().begin(), session_.structural_nodes().end(),
        [&](const auto &value) { return value.id == *context.editorial_node_id; });
    if (node == session_.structural_nodes().end())
      throw std::runtime_error("Unidade editorial não encontrada");
    if (context.work_id && *context.work_id != node->work_id)
      throw std::runtime_error(
          "A unidade editorial selecionada pertence a outra Obra");
    // A unidade editorial é uma forma de apresentação; a Obra somente limita
    // o recorte factual que a contém. Guardamos ambas as noções no contexto.
    context.work_id = node->work_id;
  }
  return context;
}

PlanningExplorerSnapshot
PlanningService::explore_entities(const PlanningContext &input) const {
  const auto &active = require_project();
  const PlanningContext context = normalize_context(input);

  persistence::EntityQuery query;
  query.search = context.search;
  query.work_id = context.work_id;
  query.entity_type_ids = context.entity_type_ids;
  query.relation_type_ids = context.relation_type_ids;
  query.related_entity_id = context.related_entity_id;
  query.fictional_axis_id = context.fictional_axis_id;
  query.fictional_time_point_id = context.fictional_time_point_id;
  query.fictional_window_start_time_point_id =
      context.fictional_window_start_time_point_id;
  query.fictional_window_end_time_point_id =
      context.fictional_window_end_time_point_id;
  query.editorial_node_id = context.editorial_node_id;
  query.require_document_reference = context.require_document_reference;
  query.document_id = context.document_id;
  query.limit = context.limit;
  query.offset = context.offset;

  PlanningExplorerSnapshot snapshot;
  snapshot.context = context;
  snapshot.matching_count =
      narrative_store_.entity_count(active.path(), query);
  const auto entities = narrative_store_.entities(active.path(), query);
  snapshot.has_previous = context.offset > 0;
  snapshot.has_next =
      context.offset < snapshot.matching_count &&
      entities.size() < snapshot.matching_count - context.offset;

  const auto entity_types = narrative_store_.entity_types(active.path());
  const auto relation_types = narrative_store_.relation_types(active.path());
  const auto type_counts = narrative_store_.entity_type_facets(active.path(), query);
  const auto relation_counts =
      narrative_store_.relation_type_facets(active.path(), query);
  const auto count_for = [](const auto &counts, const std::string &id) {
    const auto found = std::find_if(counts.begin(), counts.end(),
                                    [&](const auto &value) { return value.id == id; });
    return found == counts.end() ? std::size_t{} : found->count;
  };
  for (const auto &type : entity_types) {
    snapshot.entity_type_facets.push_back(
        {type.id, type.name, count_for(type_counts, type.id),
         std::find(context.entity_type_ids.begin(), context.entity_type_ids.end(),
                   type.id) != context.entity_type_ids.end()});
  }
  for (const auto &type : relation_types) {
    snapshot.relation_type_facets.push_back(
        {type.id, type.name, count_for(relation_counts, type.id),
         std::find(context.relation_type_ids.begin(),
                   context.relation_type_ids.end(), type.id) !=
             context.relation_type_ids.end()});
  }
  const auto work = context.work_id
                        ? std::find_if(session_.catalog().works.begin(),
                                       session_.catalog().works.end(),
                                       [&](const auto &value) {
                                         return value.id == *context.work_id;
                                       })
                        : session_.catalog().works.end();
  const auto counterpart = context.related_entity_id
                               ? narrative_store_.entity(
                                     active.path(), *context.related_entity_id)
                               : std::nullopt;
  const auto axis = context.fictional_axis_id
                        ? store_.time_axis(active.path(), *context.fictional_axis_id)
                        : std::nullopt;
  const auto point = context.fictional_time_point_id
                         ? store_.time_point(active.path(),
                                             *context.fictional_time_point_id)
                         : std::nullopt;
  const auto range_start = context.fictional_window_start_time_point_id
                               ? store_.time_point(
                                     active.path(),
                                     *context.fictional_window_start_time_point_id)
                               : std::nullopt;
  const auto range_end = context.fictional_window_end_time_point_id
                             ? store_.time_point(
                                   active.path(),
                                   *context.fictional_window_end_time_point_id)
                             : std::nullopt;
  const auto editorial_node = context.editorial_node_id
                                  ? std::find_if(
                                        session_.structural_nodes().begin(),
                                        session_.structural_nodes().end(),
                                        [&](const auto &value) {
                                          return value.id == *context.editorial_node_id;
                                        })
                                  : session_.structural_nodes().end();

  // A consulta SQL decide a inclusão, mas a explicação precisa conservar cada
  // papel temporal separadamente. Assim uma mesma entidade pode dizer que é,
  // por exemplo, participante e Local de presença sem parecer uma coincidência
  // opaca na lista de resultados.
  std::unordered_map<std::string, std::vector<std::string>> temporal_roles;
  if (context.fictional_axis_id) {
    persistence::PlanningQuery point_query;
    point_query.limit = 500;
    const auto axis_points = store_.time_points(
        active.path(), *context.fictional_axis_id, point_query);
    std::unordered_map<std::string, std::int64_t> ordinals;
    for (const auto &axis_point : axis_points)
      ordinals.emplace(axis_point.id, axis_point.ordinal);
    const auto contains_ordinal = [&](std::int64_t ordinal) {
      if (point)
        return ordinal == point->ordinal;
      if (range_start && range_end)
        return ordinal >= range_start->ordinal && ordinal <= range_end->ordinal;
      return true;
    };
    persistence::EventOccurrenceQuery occurrence_query;
    occurrence_query.time_axis_id = *context.fictional_axis_id;
    occurrence_query.limit = 500;
    for (const auto &occurrence :
         store_.event_occurrences(active.path(), occurrence_query)) {
      const auto occurrence_point = ordinals.find(occurrence.time_point_id);
      if (occurrence_point == ordinals.end() ||
          !contains_ordinal(occurrence_point->second))
        continue;
      append_unique_role(temporal_roles[occurrence.event_entity_id],
                         "Acontecimento");
      persistence::PlanningQuery participation_query;
      participation_query.limit = 500;
      for (const auto &participation : store_.event_participations(
               active.path(), occurrence.id, participation_query))
        append_unique_role(temporal_roles[participation.participant_entity_id],
                           "participante");
    }
    persistence::PresenceQuery presence_query;
    presence_query.time_axis_id = *context.fictional_axis_id;
    presence_query.limit = 500;
    for (const auto &presence : store_.presences(active.path(), presence_query)) {
      const auto start = ordinals.find(presence.start_time_point_id);
      const auto end = presence.end_time_point_id
                           ? ordinals.find(*presence.end_time_point_id)
                           : ordinals.end();
      if (start == ordinals.end() ||
          (presence.end_time_point_id && end == ordinals.end()))
        continue;
      const auto ends_after_start =
          !presence.end_time_point_id || end->second >=
                                             (point ? point->ordinal
                                                    : range_start
                                                          ? range_start->ordinal
                                                          : start->second);
      const auto begins_before_end =
          start->second <= (point ? point->ordinal
                                  : range_end ? range_end->ordinal
                                              : start->second);
      if (!ends_after_start || !begins_before_end)
        continue;
      append_unique_role(temporal_roles[presence.entity_id],
                         "sujeito de presença");
      append_unique_role(temporal_roles[presence.location_entity_id],
                         "Local de presença");
    }
  }

  snapshot.items.reserve(entities.size());
  for (const auto &entity : entities) {
    std::string text_reason;
    if (!context.search.empty()) {
      auto visible_search = context.search;
      if (visible_search.size() > 48)
        visible_search = visible_search.substr(0, 45) + "...";
      const bool name_matches = contains_search_text(entity.name, context.search);
      const bool summary_matches =
          contains_search_text(entity.summary, context.search);
      text_reason = "texto “" + visible_search + "” em " +
                    (name_matches && summary_matches
                         ? "nome e resumo"
                         : name_matches ? "nome" : "resumo");
    }
    std::string type_reason;
    if (!context.entity_type_ids.empty()) {
      const auto type =
          std::find_if(entity_types.begin(), entity_types.end(),
                       [&](const auto &value) {
                         return value.id == entity.entity_type_id;
                       });
      type_reason = "tipo " +
                    (type == entity_types.end()
                         ? std::string("indisponível")
                         : type->name);
    }
    std::string work_reason;
    if (context.work_id) {
      work_reason = "faz parte de " +
                    (work == session_.catalog().works.end()
                         ? std::string("Obra indisponível")
                         : work->title);
    }
    std::string relation_reason;
    if (!context.relation_type_ids.empty())
      relation_reason =
          (context.relation_type_ids.size() == 1 ? "relação "
                                                  : "uma das relações: ") +
          joined_labels(context.relation_type_ids, relation_types);
    std::string counterpart_reason;
    if (context.related_entity_id)
      counterpart_reason =
          "relacionada a " +
          (counterpart ? counterpart->name : std::string("entidade indisponível"));
    std::string temporal_reason;
    if (context.fictional_axis_id) {
      temporal_reason = "fato no eixo ficcional " +
                        (axis ? axis->name : std::string("indisponível"));
      if (context.fictional_time_point_id)
        temporal_reason += " no ponto " +
                           (point ? point->label : std::string("indisponível"));
      else if (range_start && range_end)
        temporal_reason +=
            " entre " + range_start->label + " e " + range_end->label;
      const auto roles = temporal_roles.find(entity.id);
      if (roles != temporal_roles.end())
        temporal_reason += ": " + joined_roles(roles->second);
    }
    std::string editorial_reason;
    if (context.editorial_node_id) {
      editorial_reason =
          "apresentada em " +
          (editorial_node == session_.structural_nodes().end()
               ? std::string("unidade editorial indisponível")
               : editorial_node->title + " ou uma unidade interna");
    }
    std::string document_reason;
    if (context.document_id)
      document_reason = "referenciada no Documento selecionado";
    else if (context.require_document_reference)
      document_reason = "referenciada em ao menos um Documento";

    // A consulta livre é sempre a intenção explícita do autor. Sem ela,
    // relações (inclusive a contraparte) são o recorte mais específico; o
    // tipo escolhido é a próxima intenção explícita ("quero personagens").
    // Tempo, apresentação editorial e Obra passam a ser foco apenas quando
    // não há uma dessas perguntas mais diretas.
    // Os critérios restantes continuam íntegros, mas não competem pela
    // primeira linha do cartão.
    std::string primary_reason;
    std::vector<std::string> auxiliary_reasons;
    const auto add_auxiliary = [&auxiliary_reasons](const std::string &reason) {
      if (!reason.empty())
        auxiliary_reasons.push_back(reason);
    };
    if (!text_reason.empty()) {
      primary_reason = text_reason;
      add_auxiliary(document_reason);
      add_auxiliary(relation_reason);
      add_auxiliary(counterpart_reason);
      add_auxiliary(type_reason);
      add_auxiliary(temporal_reason);
      add_auxiliary(editorial_reason);
      add_auxiliary(work_reason);
    } else if (!document_reason.empty()) {
      primary_reason = document_reason;
      add_auxiliary(relation_reason);
      add_auxiliary(counterpart_reason);
      add_auxiliary(type_reason);
      add_auxiliary(temporal_reason);
      add_auxiliary(editorial_reason);
      add_auxiliary(work_reason);
    } else if (!relation_reason.empty()) {
      primary_reason = relation_reason;
      if (!counterpart_reason.empty())
        primary_reason += "; " + counterpart_reason;
      add_auxiliary(type_reason);
      add_auxiliary(temporal_reason);
      add_auxiliary(editorial_reason);
      add_auxiliary(work_reason);
    } else if (!counterpart_reason.empty()) {
      primary_reason = counterpart_reason;
      add_auxiliary(type_reason);
      add_auxiliary(temporal_reason);
      add_auxiliary(editorial_reason);
      add_auxiliary(work_reason);
    } else if (!type_reason.empty()) {
      primary_reason = type_reason;
      add_auxiliary(temporal_reason);
      add_auxiliary(editorial_reason);
      add_auxiliary(work_reason);
    } else if (!temporal_reason.empty()) {
      primary_reason = temporal_reason;
      add_auxiliary(editorial_reason);
      add_auxiliary(work_reason);
    } else if (!editorial_reason.empty()) {
      primary_reason = editorial_reason;
      add_auxiliary(work_reason);
    } else if (!work_reason.empty()) {
      primary_reason = work_reason;
    } else {
      primary_reason = "incluída pelo contexto Todo o Projeto";
    }
    snapshot.items.push_back(
        {entity, primary_reason, auxiliary_reasons,
         joined_reasons(primary_reason, auxiliary_reasons)});
  }
  return snapshot;
}

TemporalViewContext
PlanningService::temporal_view_context(const PlanningContext &input) const {
  const auto context = normalize_context(input);
  TemporalViewContext result;
  result.work_id = context.work_id;
  if (context.fictional_axis_id) {
    result.fictional_axis_id = *context.fictional_axis_id;
  } else {
    const auto axes = time_axes();
    const auto preferred = std::find_if(
        axes.begin(), axes.end(),
        [](const auto &axis) { return axis.is_default; });
    if (preferred != axes.end())
      result.fictional_axis_id = preferred->id;
    else if (!axes.empty())
      result.fictional_axis_id = axes.front().id;
    else
      throw std::runtime_error(
          "Crie um eixo ficcional antes de consultar o recorte temporal");
  }
  if (context.fictional_time_point_id) {
    result.window.kind = TemporalWindowKind::Point;
    result.window.start_point_id = context.fictional_time_point_id;
  } else if (context.fictional_window_start_time_point_id) {
    result.window.kind = TemporalWindowKind::ClosedRange;
    result.window.start_point_id =
        context.fictional_window_start_time_point_id;
    result.window.end_point_id = context.fictional_window_end_time_point_id;
  }
  return result;
}

TemporalViewSnapshot
PlanningService::temporal_view(const PlanningContext &context) const {
  return temporal_view(temporal_view_context(context));
}

TemporalViewSnapshot
PlanningService::temporal_view(const TemporalViewContext &context) const {
  const auto &active = require_project();
  if (context.fictional_axis_id.empty())
    throw std::runtime_error("O recorte temporal exige um eixo ficcional");
  if (context.limit == 0 || context.limit > 500)
    throw std::runtime_error("Limite do recorte temporal inválido");

  const auto axis = store_.time_axis(active.path(), context.fictional_axis_id);
  if (!axis)
    throw std::runtime_error("Eixo de tempo ficcional não encontrado");

  persistence::PlanningQuery point_query;
  point_query.limit = 500;
  auto points = store_.time_points(active.path(), axis->id, point_query);

  persistence::EventOccurrenceQuery occurrence_query;
  occurrence_query.limit = 500;
  occurrence_query.time_axis_id = axis->id;
  auto occurrences = store_.event_occurrences(active.path(), occurrence_query);

  persistence::PresenceQuery presence_query;
  presence_query.limit = 500;
  presence_query.time_axis_id = axis->id;
  auto presence_values = store_.presences(active.path(), presence_query);

  persistence::EntityQuery entity_query;
  entity_query.limit = 500;
  auto entities = narrative_store_.entities(active.path(), entity_query);

  std::vector<project::EntityWorkScope> scopes;
  if (context.work_id) {
    persistence::EntityWorkScopeQuery scope_query;
    scope_query.work_id = context.work_id;
    scope_query.limit = 500;
    scopes = narrative_store_.work_scopes(active.path(), scope_query);
  }

  const bool source_possibly_truncated =
      points.size() == point_query.limit ||
      occurrences.size() == occurrence_query.limit ||
      presence_values.size() == presence_query.limit ||
      entities.size() == entity_query.limit ||
      (context.work_id && scopes.size() == 500);
  return build_temporal_view_snapshot(context, *axis, points, occurrences,
                                      presence_values, entities, scopes,
                                      source_possibly_truncated);
}

project::FictionalTimeAxis
PlanningService::create_time_axis(std::string name, std::string description) {
  const auto now = project::utc_now();
  project::FictionalTimeAxis value{project::new_uuid(),
                                   std::move(name),
                                   std::move(description),
                                   false,
                                   now,
                                   now};
  project::validate(value);
  store_.save(require_project().path(), value);
  return value;
}

project::FictionalTimeAxis
PlanningService::update_time_axis(const project::FictionalTimeAxis &input) {
  const auto &active = require_project();
  const auto current = store_.time_axis(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Eixo de tempo ficcional não encontrado");
  auto value = input;
  value.is_default = current->is_default;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void PlanningService::delete_time_axis(const std::string &id) {
  const auto &active = require_project();
  const auto current = store_.time_axis(active.path(), id);
  if (!current)
    throw std::runtime_error("Eixo de tempo ficcional não encontrado");
  if (current->is_default)
    throw std::runtime_error(
        "O eixo de tempo ficcional principal não pode ser removido");
  store_.remove_time_axis(active.path(), id);
}

project::FictionalTimePoint
PlanningService::create_time_point(std::string axis_id, std::int64_t ordinal,
                                   std::string label, std::string description) {
  const auto &active = require_project();
  if (!store_.time_axis(active.path(), axis_id))
    throw std::runtime_error("Eixo de tempo ficcional não encontrado");
  const auto now = project::utc_now();
  project::FictionalTimePoint value{project::new_uuid(),
                                    std::move(axis_id),
                                    ordinal,
                                    std::move(label),
                                    std::move(description),
                                    now,
                                    now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::FictionalTimePoint
PlanningService::update_time_point(const project::FictionalTimePoint &input) {
  const auto &active = require_project();
  const auto current = store_.time_point(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Ponto de tempo ficcional não encontrado");
  if (!store_.time_axis(active.path(), input.axis_id))
    throw std::runtime_error("Eixo de tempo ficcional não encontrado");
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void PlanningService::delete_time_point(const std::string &id) {
  const auto &active = require_project();
  if (!store_.time_point(active.path(), id))
    throw std::runtime_error("Ponto de tempo ficcional não encontrado");
  store_.remove_time_point(active.path(), id);
}

project::EventOccurrence
PlanningService::place_event(std::string event_entity_id,
                             std::string time_point_id,
                             std::string description) {
  const auto &active = require_project();
  const auto event = require_entity(event_entity_id);
  if (event.entity_type_id != event_type_id)
    throw std::runtime_error(
        "Somente uma entidade do tipo Acontecimento pode ser situada no tempo");
  if (!store_.time_point(active.path(), time_point_id))
    throw std::runtime_error("Ponto de tempo ficcional não encontrado");
  if (store_.event_occurrence_for_entity(active.path(), event_entity_id))
    throw std::runtime_error(
        "Este acontecimento já possui uma ocorrência temporal");
  const auto now = project::utc_now();
  project::EventOccurrence value{project::new_uuid(),
                                 std::move(event_entity_id),
                                 std::move(time_point_id),
                                 std::move(description),
                                 now,
                                 now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::EventOccurrence PlanningService::update_event_occurrence(
    const project::EventOccurrence &input) {
  const auto &active = require_project();
  const auto current = store_.event_occurrence(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Ocorrência de acontecimento não encontrada");
  const auto event = require_entity(input.event_entity_id);
  if (event.entity_type_id != event_type_id)
    throw std::runtime_error("A ocorrência deve apontar para um Acontecimento");
  if (!store_.time_point(active.path(), input.time_point_id))
    throw std::runtime_error("Ponto de tempo ficcional não encontrado");
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void PlanningService::remove_event_occurrence(const std::string &id) {
  const auto &active = require_project();
  if (!store_.event_occurrence(active.path(), id))
    throw std::runtime_error("Ocorrência de acontecimento não encontrada");
  store_.remove_event_occurrence(active.path(), id);
}

project::EventParticipation
PlanningService::add_participant(std::string occurrence_id,
                                 std::string participant_entity_id,
                                 std::string role, std::string notes) {
  const auto &active = require_project();
  if (!store_.event_occurrence(active.path(), occurrence_id))
    throw std::runtime_error("Ocorrência de acontecimento não encontrada");
  const auto participant = require_entity(participant_entity_id);
  if (participant.entity_type_id == event_type_id)
    throw std::runtime_error(
        "Um Acontecimento não pode ser participante de outro");
  const auto now = project::utc_now();
  project::EventParticipation value{project::new_uuid(),
                                    std::move(occurrence_id),
                                    std::move(participant_entity_id),
                                    std::move(role),
                                    std::move(notes),
                                    now,
                                    now};
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

project::EventParticipation PlanningService::update_participation(
    const project::EventParticipation &input) {
  const auto &active = require_project();
  const auto current = store_.event_participation(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Participação em acontecimento não encontrada");
  if (!store_.event_occurrence(active.path(), input.event_occurrence_id))
    throw std::runtime_error("Ocorrência de acontecimento não encontrada");
  const auto participant = require_entity(input.participant_entity_id);
  if (participant.entity_type_id == event_type_id)
    throw std::runtime_error(
        "Um Acontecimento não pode ser participante de outro");
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  store_.save(active.path(), value);
  return value;
}

void PlanningService::remove_participation(const std::string &id) {
  const auto &active = require_project();
  if (!store_.event_participation(active.path(), id))
    throw std::runtime_error("Participação em acontecimento não encontrada");
  store_.remove_event_participation(active.path(), id);
}

void PlanningService::validate_presence_references(
    const project::EntityPresence &value) const {
  const auto &active = require_project();
  const auto entity = require_entity(value.entity_id);
  const auto location = require_entity(value.location_entity_id);
  if (location.entity_type_id != location_type_id)
    throw std::runtime_error(
        "A presença precisa apontar para uma entidade Local");
  if (entity.id == location.id)
    throw std::runtime_error(
        "Uma entidade não pode estar presente em si mesma");
  const auto start =
      store_.time_point(active.path(), value.start_time_point_id);
  if (!start)
    throw std::runtime_error("Ponto inicial da presença não encontrado");
  if (!value.end_time_point_id)
    return;
  const auto end = store_.time_point(active.path(), *value.end_time_point_id);
  if (!end)
    throw std::runtime_error("Ponto final da presença não encontrado");
  if (start->axis_id != end->axis_id)
    throw std::runtime_error(
        "Início e fim da presença devem pertencer ao mesmo eixo ficcional");
  if (start->ordinal > end->ordinal)
    throw std::runtime_error(
        "O fim da presença não pode anteceder seu início ficcional");
}

project::EntityPresence PlanningService::create_presence(
    std::string entity_id, std::string location_entity_id,
    std::string start_time_point_id,
    std::optional<std::string> end_time_point_id, std::string description) {
  const auto now = project::utc_now();
  project::EntityPresence value{project::new_uuid(),
                                std::move(entity_id),
                                std::move(location_entity_id),
                                std::move(start_time_point_id),
                                std::move(end_time_point_id),
                                std::move(description),
                                now,
                                now};
  project::validate(value);
  validate_presence_references(value);
  store_.save(require_project().path(), value);
  return value;
}

project::EntityPresence
PlanningService::update_presence(const project::EntityPresence &input) {
  const auto &active = require_project();
  const auto current = store_.presence(active.path(), input.id);
  if (!current)
    throw std::runtime_error("Presença narrativa não encontrada");
  auto value = input;
  value.created_at = current->created_at;
  value.updated_at = project::utc_now();
  project::validate(value);
  validate_presence_references(value);
  store_.save(active.path(), value);
  return value;
}

void PlanningService::remove_presence(const std::string &id) {
  const auto &active = require_project();
  if (!store_.presence(active.path(), id))
    throw std::runtime_error("Presença narrativa não encontrada");
  store_.remove_presence(active.path(), id);
}

} // namespace inde::application

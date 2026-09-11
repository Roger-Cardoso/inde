#include "inde/application/temporal_view.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace inde::application {
namespace {

using PointMap =
    std::unordered_map<std::string, const project::FictionalTimePoint *>;

const project::FictionalTimePoint &require_point(const PointMap &points,
                                                  const std::string &id,
                                                  const char *message) {
  const auto found = points.find(id);
  if (found == points.end())
    throw std::runtime_error(message);
  return *found->second;
}

std::string event_reason(TemporalWindowKind kind, bool work_limited) {
  std::string result;
  switch (kind) {
  case TemporalWindowKind::All:
    result = "Acontecimento situado no eixo selecionado";
    break;
  case TemporalWindowKind::Point:
    result = "Acontecimento situado no ponto selecionado";
    break;
  case TemporalWindowKind::ClosedRange:
    result = "Acontecimento dentro do período selecionado";
    break;
  }
  if (work_limited)
    result += " e vinculado à Obra selecionada";
  return result;
}

std::string presence_reason(TemporalWindowKind kind, bool work_limited) {
  std::string result;
  switch (kind) {
  case TemporalWindowKind::All:
    result = "Presença registrada no eixo selecionado";
    break;
  case TemporalWindowKind::Point:
    result = "Presença ativa no ponto selecionado";
    break;
  case TemporalWindowKind::ClosedRange:
    result = "Presença que intercepta o período selecionado";
    break;
  }
  if (work_limited)
    result += " e vinculada à Obra selecionada";
  return result;
}

} // namespace

const char *to_string(TemporalWindowKind value) noexcept {
  switch (value) {
  case TemporalWindowKind::All:
    return "all";
  case TemporalWindowKind::Point:
    return "point";
  case TemporalWindowKind::ClosedRange:
    return "closed_range";
  }
  return "all";
}

TemporalViewSnapshot build_temporal_view_snapshot(
    const TemporalViewContext &context,
    const project::FictionalTimeAxis &axis,
    const std::vector<project::FictionalTimePoint> &points,
    const std::vector<project::EventOccurrence> &occurrences,
    const std::vector<project::EntityPresence> &presences,
    const std::vector<project::NarrativeEntity> &entities,
    const std::vector<project::EntityWorkScope> &work_scopes,
    bool source_possibly_truncated) {
  if (context.fictional_axis_id.empty() ||
      context.fictional_axis_id != axis.id)
    throw std::runtime_error("Contexto temporal refere eixo incompatível");
  if (context.limit == 0 || context.limit > 500)
    throw std::runtime_error("Limite do recorte temporal inválido");

  PointMap point_map;
  for (const auto &point : points) {
    if (point.axis_id != axis.id)
      throw std::runtime_error("Ponto temporal pertence a outro eixo");
    if (!point_map.emplace(point.id, &point).second)
      throw std::runtime_error("Ponto temporal duplicado no recorte");
  }

  std::optional<std::int64_t> start_ordinal;
  std::optional<std::int64_t> end_ordinal;
  if (context.window.kind != TemporalWindowKind::All) {
    if (!context.window.start_point_id)
      throw std::runtime_error("O recorte temporal exige ponto inicial");
    start_ordinal =
        require_point(point_map, *context.window.start_point_id,
                      "Ponto inicial não pertence ao eixo selecionado")
            .ordinal;
  }
  if (context.window.kind == TemporalWindowKind::ClosedRange) {
    if (!context.window.end_point_id)
      throw std::runtime_error("O período temporal exige ponto final");
    end_ordinal =
        require_point(point_map, *context.window.end_point_id,
                      "Ponto final não pertence ao eixo selecionado")
            .ordinal;
    if (*start_ordinal > *end_ordinal)
      throw std::runtime_error(
          "O ponto inicial do período vem depois do ponto final");
  }

  std::unordered_set<std::string> scoped_entities;
  if (context.work_id) {
    for (const auto &scope : work_scopes) {
      if (scope.work_id == *context.work_id)
        scoped_entities.insert(scope.entity_id);
    }
  }
  const auto allows_entity = [&](const std::string &id) {
    return !context.work_id || scoped_entities.contains(id);
  };
  const auto contains_ordinal = [&](std::int64_t ordinal) {
    if (context.window.kind == TemporalWindowKind::All)
      return true;
    if (context.window.kind == TemporalWindowKind::Point)
      return ordinal == *start_ordinal;
    return ordinal >= *start_ordinal && ordinal <= *end_ordinal;
  };
  const auto intersects_window = [&](std::int64_t start,
                                     std::optional<std::int64_t> end) {
    if (context.window.kind == TemporalWindowKind::All)
      return true;
    const auto window_end = context.window.kind == TemporalWindowKind::Point
                                ? *start_ordinal
                                : *end_ordinal;
    return start <= window_end && (!end || *end >= *start_ordinal);
  };

  std::vector<project::EventOccurrence> matching_events;
  matching_events.reserve(occurrences.size());
  for (const auto &occurrence : occurrences) {
    const auto &point = require_point(point_map, occurrence.time_point_id,
                                      "Acontecimento refere ponto ausente");
    if (allows_entity(occurrence.event_entity_id) &&
        contains_ordinal(point.ordinal))
      matching_events.push_back(occurrence);
  }

  std::vector<project::EntityPresence> matching_presences;
  matching_presences.reserve(presences.size());
  for (const auto &presence : presences) {
    const auto &start = require_point(point_map, presence.start_time_point_id,
                                      "Presença refere ponto inicial ausente");
    std::optional<std::int64_t> end;
    if (presence.end_time_point_id)
      end = require_point(point_map, *presence.end_time_point_id,
                          "Presença refere ponto final ausente")
                .ordinal;
    if (allows_entity(presence.entity_id) &&
        intersects_window(start.ordinal, end))
      matching_presences.push_back(presence);
  }

  const auto point_in_context = [&](const project::FictionalTimePoint &point) {
    return contains_ordinal(point.ordinal);
  };
  std::vector<project::FictionalTimePoint> context_points;
  context_points.reserve(points.size());
  for (const auto &point : points) {
    if (point_in_context(point))
      context_points.push_back(point);
  }
  std::ranges::sort(context_points, {}, &project::FictionalTimePoint::ordinal);

  const TemporalViewCounts matching{context_points.size(),
                                     matching_events.size(),
                                     matching_presences.size()};
  const bool events_truncated = matching_events.size() > context.limit;
  const bool presences_truncated = matching_presences.size() > context.limit;

  auto complete = build_timeline_snapshot(axis, points, matching_events,
                                          matching_presences, entities,
                                          source_possibly_truncated ||
                                              events_truncated ||
                                              presences_truncated);
  complete.points.clear();
  complete.points.reserve(context_points.size());
  for (const auto &point : context_points)
    complete.points.push_back(
        {point.id, point.ordinal, point.label, point.description});

  if (complete.events.size() > context.limit)
    complete.events.resize(context.limit);
  if (complete.presences.size() > context.limit)
    complete.presences.resize(context.limit);
  if (!context.include_events)
    complete.events.clear();
  if (!context.include_presences)
    complete.presences.clear();

  const auto event_inclusion = event_reason(context.window.kind,
                                             context.work_id.has_value());
  for (auto &event : complete.events)
    event.inclusion_reason = event_inclusion;
  const auto presence_inclusion = presence_reason(context.window.kind,
                                                   context.work_id.has_value());
  for (auto &presence : complete.presences)
    presence.inclusion_reason = presence_inclusion;

  TemporalViewSnapshot result{context,
                              std::move(complete),
                              matching,
                              {},
                              {source_possibly_truncated, events_truncated,
                               presences_truncated}};
  result.visible = {result.timeline.points.size(), result.timeline.events.size(),
                    result.timeline.presences.size()};
  return result;
}

std::vector<TemporalAgendaEntry>
build_temporal_agenda(const TimelineSnapshot &snapshot) {
  if (snapshot.points.empty())
    return {};

  std::unordered_map<std::string, std::size_t> point_indexes;
  point_indexes.reserve(snapshot.points.size());
  for (std::size_t index = 0; index < snapshot.points.size(); ++index) {
    if (!point_indexes.emplace(snapshot.points[index].id, index).second)
      throw std::runtime_error("Ponto temporal duplicado na agenda");
  }

  std::vector<std::vector<TemporalAgendaEntry>> entries_by_point(
      snapshot.points.size());
  for (const auto &event : snapshot.events) {
    const auto point = point_indexes.find(event.point_id);
    if (point == point_indexes.end())
      throw std::runtime_error(
          "Acontecimento da agenda refere ponto fora do recorte");
    entries_by_point[point->second].push_back(
        {TemporalAgendaEntryKind::Event,
         event.occurrence_id,
         event.point_id,
         event.ordinal,
         "Acontecimento: " + event.entity_name,
         event.description.empty() ? event.inclusion_reason
                                   : event.description});
  }

  const auto first_ordinal = snapshot.points.front().ordinal;
  const bool single_point = snapshot.points.size() == 1;
  for (const auto &presence : snapshot.presences) {
    const auto append_presence = [&](std::size_t point_index,
                                     TemporalAgendaEntryKind kind,
                                     std::string detail) {
      const auto &point = snapshot.points[point_index];
      entries_by_point[point_index].push_back(
          {kind,
           presence.presence_id,
           point.id,
           point.ordinal,
           "Presença: " + presence.entity_name + " em " +
               presence.location_name,
           std::move(detail)});
    };

    if (single_point) {
      const auto ordinal = snapshot.points.front().ordinal;
      if (presence.start_ordinal <= ordinal &&
          (!presence.end_ordinal || *presence.end_ordinal >= ordinal))
        append_presence(0, TemporalAgendaEntryKind::PresenceActive,
                        "Presença ativa neste ponto");
      continue;
    }

    const auto start = point_indexes.find(presence.start_point_id);
    const bool same_boundary =
        presence.end_point_id &&
        *presence.end_point_id == presence.start_point_id;
    if (start != point_indexes.end()) {
      append_presence(
          start->second, TemporalAgendaEntryKind::PresenceStart,
          same_boundary ? "Início e fim registrados neste ponto"
                        : "Início registrado da presença");
    } else if (presence.start_ordinal < first_ordinal &&
               (!presence.end_ordinal ||
                *presence.end_ordinal >= first_ordinal)) {
      append_presence(0, TemporalAgendaEntryKind::PresenceActive,
                      "Presença ativa desde antes do recorte");
    }

    if (presence.end_point_id && !same_boundary) {
      const auto end = point_indexes.find(*presence.end_point_id);
      if (end != point_indexes.end())
        append_presence(end->second, TemporalAgendaEntryKind::PresenceEnd,
                        "Fim registrado da presença");
    }
  }

  std::vector<TemporalAgendaEntry> result;
  result.reserve(snapshot.points.size() + snapshot.events.size() +
                 snapshot.presences.size() * 2);
  for (std::size_t index = 0; index < snapshot.points.size(); ++index) {
    const auto &point = snapshot.points[index];
    result.push_back({TemporalAgendaEntryKind::Point,
                      point.id,
                      point.id,
                      point.ordinal,
                      std::to_string(point.ordinal) + " — " + point.label,
                      point.description});
    auto &entries = entries_by_point[index];
    std::ranges::stable_sort(entries, [](const auto &left, const auto &right) {
      if (left.kind != right.kind)
        return left.kind < right.kind;
      if (left.title != right.title)
        return left.title < right.title;
      return left.source_id < right.source_id;
    });
    result.insert(result.end(), std::make_move_iterator(entries.begin()),
                  std::make_move_iterator(entries.end()));
  }
  return result;
}

} // namespace inde::application

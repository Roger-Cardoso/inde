#include "inde/application/timeline_projection.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <unordered_map>

namespace inde::application {
namespace {

template <typename Value>
const Value &required(const std::unordered_map<std::string, const Value *> &map,
                      const std::string &id, const char *message) {
  const auto found = map.find(id);
  if (found == map.end())
    throw std::runtime_error(message);
  return *found->second;
}

std::string entity_name(
    const std::unordered_map<std::string, const project::NarrativeEntity *>
        &entities,
    const std::string &id) {
  const auto found = entities.find(id);
  return found == entities.end() ? id : found->second->name;
}

bool contains(const TimelineLayoutItem &item, double x, double y) {
  return x >= item.x && x <= item.x + item.width && y >= item.y &&
         y <= item.y + item.height;
}

std::string latin_fold(std::string value) {
  for (std::size_t index = 0; index < value.size(); ++index) {
    auto &character = value[index];
    if (character >= 'A' && character <= 'Z')
      character = static_cast<char>(character - 'A' + 'a');
    if (static_cast<unsigned char>(character) == 0xC3 &&
        index + 1 < value.size()) {
      auto &continuation = value[index + 1];
      const auto code = static_cast<unsigned char>(continuation);
      if ((code >= 0x80 && code <= 0x96) || (code >= 0x98 && code <= 0x9E))
        continuation = static_cast<char>(code + 0x20);
      ++index;
    }
  }
  return value;
}

bool matches(const std::string &needle, const std::string &first,
             const std::string &second = {}, const std::string &third = {}) {
  if (needle.empty())
    return true;
  return latin_fold(first + '\n' + second + '\n' + third).find(needle) !=
         std::string::npos;
}

} // namespace

TimelineSnapshot build_timeline_snapshot(
    const project::FictionalTimeAxis &axis,
    const std::vector<project::FictionalTimePoint> &points,
    const std::vector<project::EventOccurrence> &occurrences,
    const std::vector<project::EntityPresence> &presences,
    const std::vector<project::NarrativeEntity> &entities,
    bool possibly_truncated) {
  TimelineSnapshot result{axis, {}, {}, {}, possibly_truncated};
  std::unordered_map<std::string, const project::FictionalTimePoint *>
      point_map;
  for (const auto &point : points) {
    if (point.axis_id != axis.id)
      throw std::runtime_error("Ponto temporal pertence a outro eixo");
    if (!point_map.emplace(point.id, &point).second)
      throw std::runtime_error("Ponto temporal duplicado na projeção");
    result.points.push_back(
        {point.id, point.ordinal, point.label, point.description});
  }
  std::ranges::sort(result.points, {}, &TimelinePoint::ordinal);

  std::unordered_map<std::string, const project::NarrativeEntity *> entity_map;
  for (const auto &entity : entities)
    entity_map.emplace(entity.id, &entity);

  for (const auto &occurrence : occurrences) {
    const auto &point = required(point_map, occurrence.time_point_id,
                                 "Acontecimento refere ponto ausente");
    result.events.push_back(
        {occurrence.id, occurrence.event_entity_id,
         entity_name(entity_map, occurrence.event_entity_id), point.id,
         point.ordinal, occurrence.description, {}});
  }
  std::ranges::sort(result.events, [](const auto &left, const auto &right) {
    if (left.ordinal != right.ordinal)
      return left.ordinal < right.ordinal;
    return left.occurrence_id < right.occurrence_id;
  });

  for (const auto &presence : presences) {
    const auto &start = required(point_map, presence.start_time_point_id,
                                 "Presença refere ponto inicial ausente");
    std::optional<std::int64_t> end_ordinal;
    if (presence.end_time_point_id) {
      const auto &end = required(point_map, *presence.end_time_point_id,
                                 "Presença refere ponto final ausente");
      end_ordinal = end.ordinal;
    }
    result.presences.push_back(
        {presence.id, presence.entity_id,
         entity_name(entity_map, presence.entity_id),
         presence.location_entity_id,
         entity_name(entity_map, presence.location_entity_id),
         presence.start_time_point_id, presence.end_time_point_id,
         start.ordinal, end_ordinal, presence.description, {}});
  }
  std::ranges::sort(result.presences, [](const auto &left, const auto &right) {
    if (left.start_ordinal != right.start_ordinal)
      return left.start_ordinal < right.start_ordinal;
    return left.presence_id < right.presence_id;
  });
  return result;
}

TimelineSnapshot filter_timeline_snapshot(const TimelineSnapshot &source,
                                          const TimelineFilter &filter) {
  TimelineSnapshot result{
      source.axis, source.points, {}, {}, source.possibly_truncated};
  const auto needle = latin_fold(filter.search);
  if (filter.show_events) {
    std::ranges::copy_if(source.events, std::back_inserter(result.events),
                         [&](const auto &event) {
                           return matches(needle, event.entity_name,
                                          event.description);
                         });
  }
  if (filter.show_presences) {
    std::ranges::copy_if(source.presences, std::back_inserter(result.presences),
                         [&](const auto &presence) {
                           return matches(needle, presence.entity_name,
                                          presence.location_name,
                                          presence.description);
                         });
  }
  return result;
}

double timeline_x_for_ordinal(std::int64_t ordinal,
                              const TimelineLayout &layout) {
  const auto usable = std::max(1.0, layout.right - layout.left);
  if (layout.minimum_ordinal == layout.maximum_ordinal)
    return layout.left + usable / 2.0;
  const auto offset = static_cast<long double>(ordinal) -
                      static_cast<long double>(layout.minimum_ordinal);
  const auto extent = static_cast<long double>(layout.maximum_ordinal) -
                      static_cast<long double>(layout.minimum_ordinal);
  const auto ratio = std::clamp(offset / extent, 0.0L, 1.0L);
  return layout.left + usable * static_cast<double>(ratio);
}

double recommended_timeline_content_width(const TimelineSnapshot &snapshot,
                                          double viewport_width,
                                          double zoom) {
  constexpr double minimum_width = 640.0;
  constexpr double horizontal_margins = 160.0;
  constexpr double point_spacing = 240.0;
  constexpr double maximum_widget_width = 32760.0;
  const auto viewport = std::max(minimum_width, viewport_width);
  const auto point_extent =
      snapshot.points.size() < 2
          ? minimum_width
          : horizontal_margins +
                static_cast<double>(snapshot.points.size() - 1) * point_spacing;
  return std::min(maximum_widget_width,
                  std::max(viewport, point_extent) *
                      std::clamp(zoom, 1.0, 8.0));
}

TimelineLayout layout_timeline(const TimelineSnapshot &snapshot, double width) {
  TimelineLayout result;
  result.width = std::max(320.0, width);
  result.left = 72.0;
  result.right = result.width - 48.0;

  if (!snapshot.points.empty()) {
    result.minimum_ordinal = snapshot.points.front().ordinal;
    result.maximum_ordinal = snapshot.points.front().ordinal;
    for (const auto &point : snapshot.points) {
      result.minimum_ordinal = std::min(result.minimum_ordinal, point.ordinal);
      result.maximum_ordinal = std::max(result.maximum_ordinal, point.ordinal);
    }
  }

  std::unordered_map<std::int64_t, std::size_t> event_lanes;
  std::size_t maximum_event_lane = 0;
  for (const auto &event : snapshot.events)
    maximum_event_lane =
        std::max(maximum_event_lane, event_lanes[event.ordinal]++);

  result.axis_y = 96.0 + static_cast<double>(maximum_event_lane) * 26.0;

  for (const auto &point : snapshot.points) {
    const auto x = timeline_x_for_ordinal(point.ordinal, result);
    result.items.push_back({TimelineVisualKind::Point, point.id, x - 6.0,
                            result.axis_y - 6.0, 12.0, 12.0});
  }

  event_lanes.clear();
  for (const auto &event : snapshot.events) {
    const auto lane = event_lanes[event.ordinal]++;
    const auto x = timeline_x_for_ordinal(event.ordinal, result);
    const auto y = result.axis_y - 34.0 - static_cast<double>(lane) * 26.0;
    result.items.push_back({TimelineVisualKind::Event, event.occurrence_id,
                            x - 8.0, y - 8.0, 16.0, 16.0});
  }

  auto presence_y = result.axis_y + 48.0;
  for (const auto &presence : snapshot.presences) {
    const auto start = timeline_x_for_ordinal(presence.start_ordinal, result);
    const auto end = presence.end_ordinal
                         ? timeline_x_for_ordinal(*presence.end_ordinal, result)
                         : result.right;
    result.items.push_back({TimelineVisualKind::Presence, presence.presence_id,
                            start, presence_y, std::max(18.0, end - start),
                            18.0});
    presence_y += 32.0;
  }
  result.height = std::max(260.0, presence_y + 48.0);
  return result;
}

std::optional<TimelineLayoutItem>
timeline_hit_test(const TimelineLayout &layout, double x, double y) {
  for (auto item = layout.items.rbegin(); item != layout.items.rend(); ++item) {
    if (contains(*item, x, y))
      return *item;
  }
  return std::nullopt;
}

} // namespace inde::application

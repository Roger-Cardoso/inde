#pragma once

#include "inde/project/narrative.hpp"
#include "inde/project/planning.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace inde::application {

struct TimelinePoint {
  std::string id;
  std::int64_t ordinal{};
  std::string label;
  std::string description;
};

struct TimelineEventMarker {
  std::string occurrence_id;
  std::string entity_id;
  std::string entity_name;
  std::string point_id;
  std::int64_t ordinal{};
  std::string description;
  std::string inclusion_reason;
};

struct TimelinePresenceSpan {
  std::string presence_id;
  std::string entity_id;
  std::string entity_name;
  std::string location_entity_id;
  std::string location_name;
  std::string start_point_id;
  std::optional<std::string> end_point_id;
  std::int64_t start_ordinal{};
  std::optional<std::int64_t> end_ordinal;
  std::string description;
  std::string inclusion_reason;
};

struct TimelineSnapshot {
  project::FictionalTimeAxis axis;
  std::vector<TimelinePoint> points;
  std::vector<TimelineEventMarker> events;
  std::vector<TimelinePresenceSpan> presences;
  bool possibly_truncated{};
};

struct TimelineFilter {
  std::string search;
  bool show_events{true};
  bool show_presences{true};
};

[[nodiscard]] TimelineSnapshot build_timeline_snapshot(
    const project::FictionalTimeAxis &axis,
    const std::vector<project::FictionalTimePoint> &points,
    const std::vector<project::EventOccurrence> &occurrences,
    const std::vector<project::EntityPresence> &presences,
    const std::vector<project::NarrativeEntity> &entities,
    bool possibly_truncated = false);

[[nodiscard]] TimelineSnapshot
filter_timeline_snapshot(const TimelineSnapshot &source,
                         const TimelineFilter &filter);

enum class TimelineVisualKind { Point, Event, Presence };

struct TimelineLayoutItem {
  TimelineVisualKind kind{TimelineVisualKind::Point};
  std::string id;
  double x{};
  double y{};
  double width{};
  double height{};
};

struct TimelineLayout {
  double width{};
  double height{};
  double axis_y{};
  double left{};
  double right{};
  std::int64_t minimum_ordinal{};
  std::int64_t maximum_ordinal{};
  std::vector<TimelineLayoutItem> items;
};

[[nodiscard]] double timeline_x_for_ordinal(std::int64_t ordinal,
                                            const TimelineLayout &layout);
// Keeps dense labels usable by making horizontal scrolling the default before
// markers are compressed into an unreadable overview. The returned GTK-safe
// width is bounded even for the maximum query page.
[[nodiscard]] double recommended_timeline_content_width(
    const TimelineSnapshot &snapshot, double viewport_width, double zoom);
[[nodiscard]] TimelineLayout layout_timeline(const TimelineSnapshot &snapshot,
                                             double width);
[[nodiscard]] std::optional<TimelineLayoutItem>
timeline_hit_test(const TimelineLayout &layout, double x, double y);

} // namespace inde::application

#pragma once

#include "inde/application/timeline_projection.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace inde::application {

enum class TemporalWindowKind { All, Point, ClosedRange };

struct TemporalWindow {
  TemporalWindowKind kind{TemporalWindowKind::All};
  std::optional<std::string> start_point_id;
  std::optional<std::string> end_point_id;
};

// This is intentionally narrower than the future PlanningContext. It is a
// read-only temporal slice and does not persist a view, filter, or selection.
struct TemporalViewContext {
  std::optional<std::string> work_id;
  std::string fictional_axis_id;
  TemporalWindow window;
  bool include_events{true};
  bool include_presences{true};
  std::size_t limit{500};
};

struct TemporalViewCounts {
  std::size_t points{};
  std::size_t events{};
  std::size_t presences{};
};

struct TemporalViewTruncation {
  bool source{};
  bool events{};
  bool presences{};

  [[nodiscard]] bool any() const {
    return source || events || presences;
  }
};

struct TemporalViewSnapshot {
  TemporalViewContext context;
  TimelineSnapshot timeline;
  TemporalViewCounts matching;
  TemporalViewCounts visible;
  TemporalViewTruncation truncation;
};

enum class TemporalAgendaEntryKind {
  Point,
  Event,
  PresenceEnd,
  PresenceStart,
  PresenceActive
};

// A bounded, non-authoritative reading of a temporal snapshot. Presences are
// represented at their recorded boundaries (or once at the beginning of a
// slice they already intersect), never repeated at every visible point.
struct TemporalAgendaEntry {
  TemporalAgendaEntryKind kind{TemporalAgendaEntryKind::Point};
  std::string source_id;
  std::string point_id;
  std::int64_t ordinal{};
  std::string title;
  std::string detail;
};

[[nodiscard]] const char *to_string(TemporalWindowKind value) noexcept;

// Builds a deterministic, GTK-free read model. Inputs must belong to the
// selected axis; an explicit work limits event/presence subjects to entities
// scoped to that work. The result reports possible source truncation instead
// of silently treating a limited query as exhaustive.
[[nodiscard]] TemporalViewSnapshot build_temporal_view_snapshot(
    const TemporalViewContext &context,
    const project::FictionalTimeAxis &axis,
    const std::vector<project::FictionalTimePoint> &points,
    const std::vector<project::EventOccurrence> &occurrences,
    const std::vector<project::EntityPresence> &presences,
    const std::vector<project::NarrativeEntity> &entities,
    const std::vector<project::EntityWorkScope> &work_scopes,
    bool source_possibly_truncated = false);

[[nodiscard]] std::vector<TemporalAgendaEntry>
build_temporal_agenda(const TimelineSnapshot &snapshot);

} // namespace inde::application

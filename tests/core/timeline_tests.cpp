#include "inde/application/catalog_service.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/project_session.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/spelling_provider.hpp"
#include "inde/application/structural_service.hpp"
#include "inde/application/temporal_view.hpp"
#include "inde/application/timeline_projection.hpp"
#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_importer.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_catalog_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_structural_repository.hpp"
#include "inde/persistence/structural_repository.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

#include "core_test_cases.hpp"
#include "test_support.hpp"

namespace inde::tests {

void timeline_projection_is_deterministic_and_selectable() {
  using namespace inde;
  const project::FictionalTimeAxis axis{
      "axis", "Cronologia principal", "", true, "", ""};
  const std::vector<project::FictionalTimePoint> points{
      {"late", "axis", 20, "Depois", "", "", ""},
      {"early", "axis", -10, "Antes", "", "", ""},
      {"middle", "axis", 0, "Agora", "", "", ""}};
  const std::vector<project::NarrativeEntity> entities{
      {"event", "event-type", "A chegada", "", "", ""},
      {"character", "character-type", "Lia", "", "", ""},
      {"place", "location-type", "Estação", "", "", ""}};
  const std::vector<project::EventOccurrence> occurrences{
      {"occurrence-b", "event", "middle", "segunda marca", "", ""},
      {"occurrence-a", "event", "middle", "primeira marca", "", ""}};
  const std::vector<project::EntityPresence> presences{
      {"open-presence", "character", "place", "middle", std::nullopt,
       "permanece", "", ""},
      {"closed-presence", "character", "place", "early",
       std::optional<std::string>{"late"}, "travessia", "", ""}};

  const auto snapshot = application::build_timeline_snapshot(
      axis, points, occurrences, presences, entities, true);
  assert(snapshot.possibly_truncated);
  assert(snapshot.points.size() == 3);
  assert(snapshot.points.front().id == "early");
  assert(snapshot.points.back().id == "late");
  assert(snapshot.events.front().occurrence_id == "occurrence-a");
  assert(snapshot.events.front().entity_name == "A chegada");
  assert(snapshot.presences.front().presence_id == "closed-presence");
  assert(snapshot.presences.back().location_name == "Estação");

  const auto layout = application::layout_timeline(snapshot, 900.0);
  assert(layout.width == 900.0);
  assert(layout.minimum_ordinal == -10);
  assert(layout.maximum_ordinal == 20);
  assert(application::timeline_x_for_ordinal(-10, layout) <
         application::timeline_x_for_ordinal(0, layout));
  assert(application::timeline_x_for_ordinal(0, layout) <
         application::timeline_x_for_ordinal(20, layout));
  assert(application::recommended_timeline_content_width(snapshot, 900.0,
                                                         1.0) == 900.0);

  auto dense_snapshot = snapshot;
  for (int index = 0; index < 30; ++index)
    dense_snapshot.points.push_back(
        {"dense-" + std::to_string(index), 30 + index,
         "Ponto denso " + std::to_string(index), ""});
  const auto dense_width = application::recommended_timeline_content_width(
      dense_snapshot, 900.0, 1.0);
  assert(dense_width > 4000.0);
  assert(application::recommended_timeline_content_width(
             dense_snapshot, 900.0, 2.0) == dense_width * 2.0);

  const auto event_item = std::find_if(
      layout.items.begin(), layout.items.end(), [](const auto &item) {
        return item.kind == application::TimelineVisualKind::Event &&
               item.id == "occurrence-a";
      });
  assert(event_item != layout.items.end());
  const auto selected = application::timeline_hit_test(
      layout, event_item->x + event_item->width / 2.0,
      event_item->y + event_item->height / 2.0);
  assert(selected && selected->id == "occurrence-a");

  const auto open_item = std::find_if(
      layout.items.begin(), layout.items.end(), [](const auto &item) {
        return item.kind == application::TimelineVisualKind::Presence &&
               item.id == "open-presence";
      });
  assert(open_item != layout.items.end());
  assert(open_item->x + open_item->width == layout.right);
  assert(!application::timeline_hit_test(layout, 1.0, 1.0));

  application::TimelineFilter filter;
  filter.search = "ESTAÇÃO";
  const auto by_location =
      application::filter_timeline_snapshot(snapshot, filter);
  assert(by_location.points.size() == snapshot.points.size());
  assert(by_location.events.empty());
  assert(by_location.presences.size() == 2);
  assert(by_location.possibly_truncated);

  filter.search = "chegada";
  filter.show_presences = false;
  const auto events_only =
      application::filter_timeline_snapshot(snapshot, filter);
  assert(events_only.events.size() == 2);
  assert(events_only.presences.empty());

  filter.search.clear();
  filter.show_events = false;
  filter.show_presences = false;
  const auto axis_only =
      application::filter_timeline_snapshot(snapshot, filter);
  assert(axis_only.points.size() == 3);
  assert(axis_only.events.empty());
  assert(axis_only.presences.empty());

  auto invalid_occurrences = occurrences;
  invalid_occurrences.front().time_point_id = "missing";
  bool missing_point_rejected = false;
  try {
    static_cast<void>(application::build_timeline_snapshot(
        axis, points, invalid_occurrences, presences, entities));
  } catch (const std::exception &) {
    missing_point_rejected = true;
  }
  assert(missing_point_rejected);
}

void temporal_view_context_has_explicit_window_scope_and_limits() {
  using namespace inde;
  const project::FictionalTimeAxis axis{
      "axis", "Cronologia principal", "", true, "", ""};
  const std::vector<project::FictionalTimePoint> points{
      {"early", "axis", -10, "Antes", "", "", ""},
      {"middle", "axis", 0, "Agora", "", "", ""},
      {"late", "axis", 20, "Depois", "", "", ""}};
  const std::vector<project::NarrativeEntity> entities{
      {"event-a", "event", "A descoberta", "", "", ""},
      {"event-b", "event", "A fuga", "", "", ""},
      {"character", "character", "Lia", "", "", ""},
      {"place", "location", "Estação", "", "", ""}};
  const std::vector<project::EventOccurrence> occurrences{
      {"occurrence-early", "event-a", "early", "", "", ""},
      {"occurrence-middle", "event-a", "middle", "pista confirmada", "", ""},
      {"occurrence-late", "event-b", "late", "", "", ""}};
  const std::vector<project::EntityPresence> presences{
      {"closed", "character", "place", "early", "late", "travessia", "", ""},
      {"open", "character", "place", "middle", std::nullopt, "espera", "", ""}};
  const std::vector<project::EntityWorkScope> scopes{
      {"scope-event-a", "event-a", "work-a", "", "", ""},
      {"scope-event-b", "event-b", "work-b", "", "", ""},
      {"scope-character-a", "character", "work-a", "", "", ""},
      {"scope-character-b", "character", "work-b", "", "", ""}};

  application::TemporalViewContext point_context;
  point_context.fictional_axis_id = "axis";
  point_context.work_id = "work-a";
  point_context.window.kind = application::TemporalWindowKind::Point;
  point_context.window.start_point_id = "middle";
  const auto at_middle = application::build_temporal_view_snapshot(
      point_context, axis, points, occurrences, presences, entities, scopes);
  assert(at_middle.timeline.points.size() == 1);
  assert(at_middle.timeline.points.front().id == "middle");
  assert(at_middle.timeline.events.size() == 1);
  assert(at_middle.timeline.events.front().occurrence_id ==
         "occurrence-middle");
  assert(at_middle.timeline.presences.size() == 2);
  assert(at_middle.timeline.events.front().inclusion_reason.find("ponto") !=
         std::string::npos);
  assert(at_middle.timeline.presences.front().inclusion_reason.find("Obra") !=
         std::string::npos);
  const auto point_agenda =
      application::build_temporal_agenda(at_middle.timeline);
  assert(point_agenda.size() == 4);
  assert(std::ranges::count_if(point_agenda, [](const auto &entry) {
           return entry.kind ==
                  application::TemporalAgendaEntryKind::PresenceActive;
         }) == 2);

  application::TemporalViewContext range_context;
  range_context.fictional_axis_id = "axis";
  range_context.window.kind = application::TemporalWindowKind::ClosedRange;
  range_context.window.start_point_id = "middle";
  range_context.window.end_point_id = "late";
  const auto ranged = application::build_temporal_view_snapshot(
      range_context, axis, points, occurrences, presences, entities, scopes);
  assert(ranged.timeline.points.size() == 2);
  assert(ranged.timeline.events.size() == 2);
  assert(ranged.timeline.presences.size() == 2);
  assert(ranged.timeline.events.front().occurrence_id == "occurrence-middle");
  assert(ranged.timeline.events.back().occurrence_id == "occurrence-late");
  assert(ranged.timeline.presences.back().end_ordinal == std::nullopt);
  const auto range_agenda = application::build_temporal_agenda(ranged.timeline);
  assert(range_agenda.size() == 7);
  assert(std::ranges::count_if(range_agenda, [](const auto &entry) {
           return entry.source_id == "closed";
         }) == 2);
  assert(std::ranges::count_if(range_agenda, [](const auto &entry) {
           return entry.source_id == "open";
         }) == 1);
  assert(std::ranges::any_of(range_agenda, [](const auto &entry) {
    return entry.kind == application::TemporalAgendaEntryKind::PresenceActive &&
           entry.detail.find("antes do recorte") != std::string::npos;
  }));

  range_context.work_id = "work-a";
  const auto scoped = application::build_temporal_view_snapshot(
      range_context, axis, points, occurrences, presences, entities, scopes,
      true);
  assert(scoped.timeline.events.size() == 1);
  assert(scoped.timeline.events.front().entity_id == "event-a");
  assert(scoped.timeline.presences.size() == 2);
  assert(scoped.truncation.source);

  range_context.limit = 1;
  range_context.work_id.reset();
  const auto limited = application::build_temporal_view_snapshot(
      range_context, axis, points, occurrences, presences, entities, scopes);
  assert(limited.matching.events == 2);
  assert(limited.timeline.events.size() == 1);
  assert(limited.truncation.events);
  assert(limited.timeline.possibly_truncated);

  bool inverted_rejected = false;
  try {
    range_context.window.start_point_id = "late";
    range_context.window.end_point_id = "middle";
    static_cast<void>(application::build_temporal_view_snapshot(
        range_context, axis, points, occurrences, presences, entities, scopes));
  } catch (const std::exception &) {
    inverted_rejected = true;
  }
  assert(inverted_rejected);

  bool incompatible_point_rejected = false;
  try {
    point_context.window.start_point_id = "other-axis-point";
    static_cast<void>(application::build_temporal_view_snapshot(
        point_context, axis, points, occurrences, presences, entities, scopes));
  } catch (const std::exception &) {
    incompatible_point_rejected = true;
  }
  assert(incompatible_point_rejected);
}

} // namespace inde::tests

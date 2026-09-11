#include "inde/application/temporal_view.hpp"
#include "inde/application/timeline_projection.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <sys/resource.h>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

template <class Operation> double milliseconds(Operation operation) {
  const auto start = Clock::now();
  operation();
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}

std::string id(const char *prefix, int index) {
  return std::string(prefix) + '-' + std::to_string(index);
}

} // namespace

int main() {
  constexpr int item_count = 500;
  const inde::project::FictionalTimeAxis axis{
      "axis", "Cronologia densa", "", true, "", ""};
  std::vector<inde::project::FictionalTimePoint> points;
  std::vector<inde::project::EventOccurrence> occurrences;
  std::vector<inde::project::EntityPresence> presences;
  std::vector<inde::project::NarrativeEntity> entities;
  std::vector<inde::project::EntityWorkScope> scopes;
  points.reserve(item_count);
  occurrences.reserve(item_count);
  presences.reserve(item_count);
  entities.reserve(item_count);
  scopes.reserve(item_count);

  for (int index = 0; index < item_count; ++index) {
    points.push_back({id("point", index), "axis", index * 10,
                      "Marco temporal " + std::to_string(index), "", "", ""});
    entities.push_back({id("entity", index), "type",
                        (index < item_count / 2 ? "Personagem " : "Local ") +
                            std::to_string(index),
                        "", "", ""});
    scopes.push_back({id("scope", index), id("entity", index), "work", "",
                      "", ""});
  }
  for (int index = 0; index < item_count; ++index) {
    occurrences.push_back(
        {id("occurrence", index), id("entity", index % (item_count / 2)),
         id("point", index), "Acontecimento representativo", "", ""});
    const auto end = index + 5 < item_count
                         ? std::optional<std::string>{id("point", index + 5)}
                         : std::nullopt;
    presences.push_back(
        {id("presence", index), id("entity", index % (item_count / 2)),
         id("entity", item_count / 2 + index % (item_count / 2)),
         id("point", index), end, "Presença representativa", "", ""});
  }

  inde::application::TimelineSnapshot snapshot;
  const auto projection_ms = milliseconds([&] {
    snapshot = inde::application::build_timeline_snapshot(
        axis, points, occurrences, presences, entities, true);
  });

  inde::application::TemporalViewContext context;
  context.fictional_axis_id = axis.id;
  context.work_id = "work";
  context.limit = item_count;
  inde::application::TemporalViewSnapshot temporal_snapshot;
  const auto temporal_composition_ms = milliseconds([&] {
    temporal_snapshot = inde::application::build_temporal_view_snapshot(
        context, axis, points, occurrences, presences, entities, scopes, true);
  });

  std::uint64_t checksum{};
  constexpr int layout_iterations = 100;
  const auto layouts_ms = milliseconds([&] {
    for (int index = 0; index < layout_iterations; ++index) {
      const auto layout =
          inde::application::layout_timeline(temporal_snapshot.timeline,
                                             1600.0 + index);
      checksum += layout.items.size();
    }
  });

  inde::application::TimelineFilter filter;
  filter.search = "Personagem 24";
  constexpr int filter_iterations = 100;
  const auto filters_ms = milliseconds([&] {
    for (int index = 0; index < filter_iterations; ++index) {
      const auto filtered =
          inde::application::filter_timeline_snapshot(temporal_snapshot.timeline,
                                                      filter);
      checksum += filtered.events.size() + filtered.presences.size();
    }
  });

  std::vector<inde::application::TemporalAgendaEntry> agenda;
  const auto agenda_ms = milliseconds([&] {
    agenda =
        inde::application::build_temporal_agenda(temporal_snapshot.timeline);
  });
  const auto agenda_limit = temporal_snapshot.timeline.points.size() +
                            temporal_snapshot.timeline.events.size() +
                            temporal_snapshot.timeline.presences.size() * 2;
  if (agenda.size() > agenda_limit)
    return 2;
  checksum += agenda.size();
  const auto content_width =
      inde::application::recommended_timeline_content_width(
          temporal_snapshot.timeline, 1600.0, 1.0);

  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  std::cout << "points=" << snapshot.points.size()
            << " events=" << snapshot.events.size()
            << " presences=" << snapshot.presences.size() << '\n'
            << "projection_ms=" << projection_ms << '\n'
            << "temporal_composition_ms=" << temporal_composition_ms << '\n'
            << "layout_iterations=" << layout_iterations << '\n'
            << "layouts_total_ms=" << layouts_ms << '\n'
            << "layout_average_ms=" << layouts_ms / layout_iterations << '\n'
            << "filter_iterations=" << filter_iterations << '\n'
            << "filters_total_ms=" << filters_ms << '\n'
            << "filter_average_ms=" << filters_ms / filter_iterations << '\n'
            << "agenda_entries=" << agenda.size() << '\n'
            << "agenda_ms=" << agenda_ms << '\n'
            << "recommended_content_width=" << content_width << '\n'
            << "max_rss_kib=" << usage.ru_maxrss << '\n'
            << "checksum=" << checksum << '\n';
}

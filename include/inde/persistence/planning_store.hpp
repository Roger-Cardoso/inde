#pragma once

#include "inde/project/planning.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace inde::persistence {

struct PlanningQuery {
  std::string search;
  std::size_t limit{100};
  std::size_t offset{};
};

struct EventOccurrenceQuery : PlanningQuery {
  std::optional<std::string> time_axis_id;
};

struct PresenceQuery : PlanningQuery {
  std::optional<std::string> entity_id;
  std::optional<std::string> location_entity_id;
  std::optional<std::string> time_axis_id;
};

class PlanningStore {
public:
  virtual ~PlanningStore() = default;

  virtual void initialize(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::vector<project::FictionalTimeAxis>
  time_axes(const std::filesystem::path &project_path) const = 0;
  [[nodiscard]] virtual std::optional<project::FictionalTimeAxis>
  time_axis(const std::filesystem::path &project_path,
            const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::FictionalTimePoint>
  time_points(const std::filesystem::path &project_path,
              const std::string &axis_id, const PlanningQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::FictionalTimePoint>
  time_point(const std::filesystem::path &project_path,
             const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::EventOccurrence>
  event_occurrences(const std::filesystem::path &project_path,
                    const EventOccurrenceQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::EventOccurrence>
  event_occurrence(const std::filesystem::path &project_path,
                   const std::string &id) const = 0;
  [[nodiscard]] virtual std::optional<project::EventOccurrence>
  event_occurrence_for_entity(const std::filesystem::path &project_path,
                              const std::string &event_entity_id) const = 0;
  [[nodiscard]] virtual std::vector<project::EventParticipation>
  event_participations(const std::filesystem::path &project_path,
                       const std::string &event_occurrence_id,
                       const PlanningQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::EventParticipation>
  event_participation(const std::filesystem::path &project_path,
                      const std::string &id) const = 0;
  [[nodiscard]] virtual std::vector<project::EntityPresence>
  presences(const std::filesystem::path &project_path,
            const PresenceQuery &query) const = 0;
  [[nodiscard]] virtual std::optional<project::EntityPresence>
  presence(const std::filesystem::path &project_path,
           const std::string &id) const = 0;

  virtual void save(const std::filesystem::path &project_path,
                    const project::FictionalTimeAxis &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::FictionalTimePoint &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::EventOccurrence &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::EventParticipation &value) const = 0;
  virtual void save(const std::filesystem::path &project_path,
                    const project::EntityPresence &value) const = 0;
  virtual void remove_time_axis(const std::filesystem::path &project_path,
                                const std::string &id) const = 0;
  virtual void remove_time_point(const std::filesystem::path &project_path,
                                 const std::string &id) const = 0;
  virtual void
  remove_event_occurrence(const std::filesystem::path &project_path,
                          const std::string &id) const = 0;
  virtual void
  remove_event_participation(const std::filesystem::path &project_path,
                             const std::string &id) const = 0;
  virtual void remove_presence(const std::filesystem::path &project_path,
                               const std::string &id) const = 0;
};

} // namespace inde::persistence

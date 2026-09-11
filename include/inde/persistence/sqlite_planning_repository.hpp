#pragma once

#include "inde/persistence/planning_store.hpp"

namespace inde::persistence {

class SqlitePlanningRepository final : public PlanningStore {
public:
  void initialize(const std::filesystem::path &project_path) const override;
  [[nodiscard]] std::vector<project::FictionalTimeAxis>
  time_axes(const std::filesystem::path &project_path) const override;
  [[nodiscard]] std::optional<project::FictionalTimeAxis>
  time_axis(const std::filesystem::path &, const std::string &) const override;
  [[nodiscard]] std::vector<project::FictionalTimePoint>
  time_points(const std::filesystem::path &, const std::string &,
              const PlanningQuery &) const override;
  [[nodiscard]] std::optional<project::FictionalTimePoint>
  time_point(const std::filesystem::path &, const std::string &) const override;
  [[nodiscard]] std::vector<project::EventOccurrence>
  event_occurrences(const std::filesystem::path &,
                    const EventOccurrenceQuery &) const override;
  [[nodiscard]] std::optional<project::EventOccurrence>
  event_occurrence(const std::filesystem::path &,
                   const std::string &) const override;
  [[nodiscard]] std::optional<project::EventOccurrence>
  event_occurrence_for_entity(const std::filesystem::path &,
                              const std::string &) const override;
  [[nodiscard]] std::vector<project::EventParticipation>
  event_participations(const std::filesystem::path &, const std::string &,
                       const PlanningQuery &) const override;
  [[nodiscard]] std::optional<project::EventParticipation>
  event_participation(const std::filesystem::path &,
                      const std::string &) const override;
  [[nodiscard]] std::vector<project::EntityPresence>
  presences(const std::filesystem::path &,
            const PresenceQuery &) const override;
  [[nodiscard]] std::optional<project::EntityPresence>
  presence(const std::filesystem::path &, const std::string &) const override;

  void save(const std::filesystem::path &,
            const project::FictionalTimeAxis &) const override;
  void save(const std::filesystem::path &,
            const project::FictionalTimePoint &) const override;
  void save(const std::filesystem::path &,
            const project::EventOccurrence &) const override;
  void save(const std::filesystem::path &,
            const project::EventParticipation &) const override;
  void save(const std::filesystem::path &,
            const project::EntityPresence &) const override;
  void remove_time_axis(const std::filesystem::path &,
                        const std::string &) const override;
  void remove_time_point(const std::filesystem::path &,
                         const std::string &) const override;
  void remove_event_occurrence(const std::filesystem::path &,
                               const std::string &) const override;
  void remove_event_participation(const std::filesystem::path &,
                                  const std::string &) const override;
  void remove_presence(const std::filesystem::path &,
                       const std::string &) const override;
};

} // namespace inde::persistence

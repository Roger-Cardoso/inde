#pragma once

#include "inde/application/project_session.hpp"
#include "inde/application/planning_context.hpp"
#include "inde/application/temporal_view.hpp"
#include "inde/persistence/narrative_store.hpp"
#include "inde/persistence/planning_store.hpp"

#include <vector>

namespace inde::application {

class PlanningService {
public:
  PlanningService(ProjectSession &session, persistence::PlanningStore &store,
                  persistence::NarrativeStore &narrative_store)
      : session_(session), store_(store), narrative_store_(narrative_store) {}

  [[nodiscard]] std::vector<project::FictionalTimeAxis> time_axes() const;
  [[nodiscard]] std::vector<project::FictionalTimePoint>
  time_points(const std::string &axis_id,
              const persistence::PlanningQuery &query = {}) const;
  [[nodiscard]] std::optional<project::FictionalTimePoint>
  time_point(const std::string &id) const;
  [[nodiscard]] std::vector<project::EventOccurrence>
  event_occurrences(const persistence::EventOccurrenceQuery &query = {}) const;
  [[nodiscard]] std::optional<project::EventOccurrence>
  event_occurrence_for_entity(const std::string &entity_id) const;
  [[nodiscard]] std::vector<project::EventParticipation>
  event_participations(const std::string &occurrence_id,
                       const persistence::PlanningQuery &query = {}) const;
  [[nodiscard]] std::vector<project::EntityPresence>
  presences(const persistence::PresenceQuery &query = {}) const;
  [[nodiscard]] PlanningExplorerSnapshot
  explore_entities(const PlanningContext &context) const;
  // Deriva a projeção temporal do mesmo contexto usado pelo explorador. Esta
  // conversão é independente de GTK e não persiste uma vista.
  [[nodiscard]] TemporalViewContext
  temporal_view_context(const PlanningContext &context) const;
  [[nodiscard]] TemporalViewSnapshot
  temporal_view(const PlanningContext &context) const;
  [[nodiscard]] TemporalViewSnapshot
  temporal_view(const TemporalViewContext &context) const;

  [[nodiscard]] project::FictionalTimeAxis
  create_time_axis(std::string name, std::string description = {});
  [[nodiscard]] project::FictionalTimeAxis
  update_time_axis(const project::FictionalTimeAxis &input);
  void delete_time_axis(const std::string &id);

  [[nodiscard]] project::FictionalTimePoint
  create_time_point(std::string axis_id, std::int64_t ordinal,
                    std::string label, std::string description = {});
  [[nodiscard]] project::FictionalTimePoint
  update_time_point(const project::FictionalTimePoint &input);
  void delete_time_point(const std::string &id);

  [[nodiscard]] project::EventOccurrence
  place_event(std::string event_entity_id, std::string time_point_id,
              std::string description = {});
  [[nodiscard]] project::EventOccurrence
  update_event_occurrence(const project::EventOccurrence &input);
  void remove_event_occurrence(const std::string &id);

  [[nodiscard]] project::EventParticipation
  add_participant(std::string occurrence_id, std::string participant_entity_id,
                  std::string role, std::string notes = {});
  [[nodiscard]] project::EventParticipation
  update_participation(const project::EventParticipation &input);
  void remove_participation(const std::string &id);

  [[nodiscard]] project::EntityPresence
  create_presence(std::string entity_id, std::string location_entity_id,
                  std::string start_time_point_id,
                  std::optional<std::string> end_time_point_id = std::nullopt,
                  std::string description = {});
  [[nodiscard]] project::EntityPresence
  update_presence(const project::EntityPresence &input);
  void remove_presence(const std::string &id);

private:
  [[nodiscard]] const project::Project &require_project() const;
  [[nodiscard]] project::NarrativeEntity
  require_entity(const std::string &id) const;
  [[nodiscard]] PlanningContext
  normalize_context(const PlanningContext &input) const;
  void validate_presence_references(const project::EntityPresence &value) const;

  ProjectSession &session_;
  persistence::PlanningStore &store_;
  persistence::NarrativeStore &narrative_store_;
};

} // namespace inde::application

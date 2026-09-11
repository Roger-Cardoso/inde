#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace inde::project {

struct FictionalTimeAxis {
  std::string id;
  std::string name;
  std::string description;
  bool is_default{};
  std::string created_at;
  std::string updated_at;
};

struct FictionalTimePoint {
  std::string id;
  std::string axis_id;
  std::int64_t ordinal{};
  std::string label;
  std::string description;
  std::string created_at;
  std::string updated_at;
};

struct EventOccurrence {
  std::string id;
  std::string event_entity_id;
  std::string time_point_id;
  std::string description;
  std::string created_at;
  std::string updated_at;
};

struct EventParticipation {
  std::string id;
  std::string event_occurrence_id;
  std::string participant_entity_id;
  std::string role;
  std::string notes;
  std::string created_at;
  std::string updated_at;
};

struct EntityPresence {
  std::string id;
  std::string entity_id;
  std::string location_entity_id;
  std::string start_time_point_id;
  std::optional<std::string> end_time_point_id;
  std::string description;
  std::string created_at;
  std::string updated_at;
};

void validate(const FictionalTimeAxis &value);
void validate(const FictionalTimePoint &value);
void validate(const EventOccurrence &value);
void validate(const EventParticipation &value);
void validate(const EntityPresence &value);

} // namespace inde::project

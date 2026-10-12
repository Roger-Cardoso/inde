#pragma once
#include "inde/application/project_session.hpp"
#include "inde/project/planning.hpp"
#include "inde/project/worldbuilding.hpp"
#include <map>
#include <optional>
namespace inde::project {
struct Document;
}
namespace inde::application {
struct TextDateUse {
  std::string id, document_id, annotation_id, title, role, token;
  bool operator==(const TextDateUse &) const = default;
};
struct TemporalContinuityNotice {
  std::string annotation_id, entity_id, reference_id, source_id, message;
  bool exceptional{};
};
struct CalendarEvent {
  std::string point_id, name;
  std::int64_t ordinal{};
};
struct CalendarMonthView {
  struct ReferenceHit {
    project::TemporalReference reference;
    std::vector<int> days;
  };
  std::vector<project::FictionalTimePoint> points;
  std::vector<CalendarEvent> events;
  std::vector<ReferenceHit> references;
  std::vector<project::TemporalReference> unpositioned;
  std::map<int, std::string> cuts;
  std::map<int, std::string> regimes;
  std::int64_t matching_points{}, matching_events{};
};
class WorldbuildingService {
public:
  explicit WorldbuildingService(ProjectSession &s) : session_(s) {}
  std::vector<project::EntityAlias> aliases(const std::string &entity_id) const;
  project::EntityAlias
  save_alias(project::EntityAlias,
             std::optional<project::EntityAlias> expected = {}) const;
  void remove_alias(const project::EntityAlias &expected) const;
  std::vector<project::FictionalCalendar> calendars() const;
  project::FictionalCalendar calendar(const std::string &id) const;
  project::FictionalCalendar
  create_calendar(project::FictionalCalendar,
                  std::optional<std::string> parent = {}) const;
  std::optional<std::string> calendar_parent(const std::string &id) const;
  std::vector<project::CalendarAdoption>
  adoptions(const std::string &axis) const;
  project::CalendarAdoption
  adopt_calendar(project::CalendarAdoption, const project::AxisCalendar &base,
                 const std::vector<project::CalendarAdoption> &expected) const;
  void remove_last_adoption(const project::AxisCalendar &base,
                            const std::vector<project::CalendarAdoption> &expected) const;
  std::vector<std::string>
  parallel_dates(const project::FictionalTimePoint &) const;
  std::vector<project::TemporalReference>
  temporal_references(const std::string &axis) const;
  project::TemporalReference temporal_reference(const std::string &id) const;
  std::vector<TextDateUse> text_date_uses(const std::string &reference) const;
  std::vector<TemporalContinuityNotice>
      continuity_notices(project::Document) const;
  project::TemporalReference save_temporal_reference(
      project::TemporalReference,
      std::optional<project::TemporalReference> expected = {},
      const std::vector<project::TemporalAttachment> &expected_links = {},
      const std::vector<TextDateUse> &expected_text = {}) const;
  void
  remove_temporal_reference(const project::TemporalReference &expected) const;
  std::vector<project::TemporalTarget>
  temporal_targets(const std::string &kind, const std::string &query) const;
  project::TemporalTarget temporal_target(const std::string &kind,
                                          const std::string &id) const;
  std::vector<project::TemporalAttachment>
  temporal_attachments(const std::string &reference) const;
  std::vector<project::TemporalReference>
  target_dates(const project::TemporalTarget &) const;
  project::TemporalAttachment
  attach_date(project::TemporalAttachment,
              const project::TemporalReference &expected,
              const project::TemporalTarget &target) const;
  project::TemporalAttachment create_attached_date(project::TemporalReference,
      const project::FictionalCalendar &, project::TemporalAttachment,
      const project::TemporalTarget &) const;
  void
  remove_date_attachment(const project::TemporalAttachment &expected) const;
  std::optional<project::AxisCalendar>
  binding(const std::string &axis_id) const;
  std::optional<project::AxisCalendar>
  assign_calendar(const std::string &axis_id,
                  std::optional<std::string> calendar_id, std::int64_t epoch,
                  const std::optional<project::AxisCalendar> &expected) const;
  std::string point_date(const project::FictionalTimePoint &) const;
  std::map<std::string, std::string>
  point_dates(const std::vector<project::FictionalTimePoint> &) const;
  project::FictionalTimePoint
  create_dated_point(const project::AxisCalendar &expected,
                     project::CalendarDate, std::string label,
                     std::string description = {}) const;
  CalendarMonthView month_view(const project::AxisCalendar &expected,
                               std::int64_t year, int month,
                               const std::string &regime = "all") const;

private:
  const project::Project &owner() const;
  ProjectSession &session_;
};
} // namespace inde::application

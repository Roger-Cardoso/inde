#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace inde::project {
struct EntityAlias {
  std::string id, entity_id, name, kind, note, updated_at, token;
  bool operator==(const EntityAlias &) const = default;
};
struct CalendarMonth {
  std::string name;
  int days{};
  bool operator==(const CalendarMonth &) const = default;
};
struct TimeUnit {
  std::string singular, plural, symbol;
  int parts{1}; // number of this unit in its parent, from day downwards.
  bool operator==(const TimeUnit &) const = default;
};
struct FictionalCalendar {
  std::string id, name;
  std::vector<CalendarMonth> months;
  std::vector<std::string> weekdays;
  int leap_cycle{}, leap_phase{}, leap_month{1};
  std::string engine{"custom"};
  std::vector<TimeUnit> clock{{"hora", "horas", "h", 24},
                              {"minuto", "minutos", "min", 60},
                              {"segundo", "segundos", "s", 60}};
  std::string
      rule_revision{}; // pinned computational rules, never upgraded silently.
  bool operator==(const FictionalCalendar &) const = default;
};
struct CalendarDate {
  std::int64_t year{};
  int month{1}, day{1};
  bool operator==(const CalendarDate &) const = default;
};
struct AxisCalendar {
  std::string axis_id, calendar_id;
  std::int64_t epoch_ordinal{};
  std::string token;
  bool operator==(const AxisCalendar &) const = default;
};
struct CalendarAdoption {
  std::string id, axis_id, calendar_id;
  std::int64_t from_ordinal{}, epoch_ordinal{};
  std::string
      mode; // retire or coexist; scoped to this explicitly measured axis.
  bool operator==(const CalendarAdoption &) const = default;
};
struct TemporalBoundary {
  std::int64_t year{};
  std::optional<int> month, day;
  std::vector<int> clock;
  bool operator==(const TemporalBoundary &) const = default;
};
struct TemporalReference {
  std::string id, axis_id, calendar_id, label;
  std::int64_t year{};
  std::optional<int> month, day;
  std::vector<int> clock; // empty = unknown time, never midnight.
  std::string note, token;
  std::string qualifier{
      "exact"}; // exact, before, after, between, approximate, season
  std::optional<TemporalBoundary> upper;
  std::string uncertainty;
  bool operator==(const TemporalReference &) const = default;
};
struct TemporalTarget {
  std::string kind, id, label, context, token;
  bool operator==(const TemporalTarget &) const = default;
};
struct TextDateCreation {
  TemporalReference reference;
  FictionalCalendar calendar; // snapshot reviewed by the author
};
struct TemporalAttachment {
  std::string id, reference_id, kind, target_id, role, note, token;
  std::string meaning{
      "unspecified"}; // author-chosen formal meaning, never guessed from role
  bool operator==(const TemporalAttachment &) const = default;
};
void validate(const TemporalAttachment &);
struct TemporalCoordinate {
  std::int64_t day{}, numerator{}, denominator{1};
};
int compare_temporal_coordinates(TemporalCoordinate, TemporalCoordinate);
struct TemporalWindow {
  std::optional<long double> earliest, latest; // days; upper endpoint exclusive
  bool comparable{};
  std::optional<TemporalCoordinate> lower_bound, upper_bound;
};
TemporalWindow temporal_window(const FictionalCalendar &,
                               const TemporalReference &, std::int64_t epoch);
// NFC/casefold, without removing accents or guessing synonyms.
std::string alias_key(const std::string &);
std::string trim_world_text(const std::string &);
void validate(const EntityAlias &);
void validate(const FictionalCalendar &);
int days_in_month(const FictionalCalendar &, std::int64_t year, int month);
std::int64_t calendar_ordinal(const FictionalCalendar &, CalendarDate,
                              std::int64_t epoch = 0);
CalendarDate calendar_date(const FictionalCalendar &, std::int64_t ordinal,
                           std::int64_t epoch = 0);
std::string format_calendar_date(const FictionalCalendar &, CalendarDate);
int calendar_weekday(const FictionalCalendar &, CalendarDate);
FictionalCalendar calendar_template(const std::string &engine);
std::vector<std::string> calendar_template_engines();
std::string calendar_rule_description(const FictionalCalendar &);
std::vector<CalendarMonth> calendar_months(const FictionalCalendar &,
                                           std::int64_t year);
int calendar_first_day(const FictionalCalendar &);
// Proleptic terrestrial calendars only; no automatic historical cutover.
std::int64_t terrestrial_day(const FictionalCalendar &, CalendarDate);
CalendarDate terrestrial_date(const FictionalCalendar &, std::int64_t day);
void validate(const FictionalCalendar &, const TemporalReference &);
std::string format_temporal_reference(const FictionalCalendar &,
                                      const TemporalReference &);
std::int64_t clock_ticks(const FictionalCalendar &, const std::vector<int> &);
} // namespace inde::project

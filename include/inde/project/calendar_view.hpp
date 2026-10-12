#pragma once
#include "inde/project/worldbuilding.hpp"
#include <utility>
namespace inde::project {
struct CalendarCell {
  CalendarDate date;
  int column{}, row{}, weekday{-1};
};
struct CalendarGrid {
  std::int64_t year{};
  int month{}, columns{};
  bool weekly{};
  std::string title;
  std::vector<CalendarCell> cells;
};
// Pure presentation: never selects a date or writes to a Project.
CalendarGrid calendar_grid(const FictionalCalendar &, std::int64_t, int);
std::pair<std::int64_t, std::int64_t>
calendar_year_limits(const FictionalCalendar &);
CalendarDate calendar_adjacent_month(const FictionalCalendar &, CalendarDate,
                                     int);
TemporalBoundary calendar_direct_boundary(const FictionalCalendar &, const std::string &);
} // namespace inde::project

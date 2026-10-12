#pragma once
#include "inde/project/worldbuilding.hpp"
namespace inde::project::calendar_engines {
bool icu(const FictionalCalendar &);
std::string revision();
std::vector<CalendarMonth> months(const FictionalCalendar &, std::int64_t);
std::int64_t day(const FictionalCalendar &, CalendarDate);
CalendarDate date(const FictionalCalendar &, std::int64_t);
FictionalCalendar model(const std::string &);
void validate_rules(const FictionalCalendar &);
std::string maya_cycles(CalendarDate);
} // namespace inde::project::calendar_engines

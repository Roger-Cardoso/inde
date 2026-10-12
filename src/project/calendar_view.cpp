#include "inde/project/calendar_view.hpp"
#include <charconv>
#include <stdexcept>
namespace inde::project {
TemporalBoundary calendar_direct_boundary(const FictionalCalendar &c,
                                          const std::string &input) {
  std::vector<std::int64_t> values;
  std::size_t begin = 0;
  for (;;) {
    auto end = input.find(';', begin);
    auto text = trim_world_text(
        input.substr(begin, end == std::string::npos ? end : end - begin));
    std::int64_t n{};
    auto [p, e] = std::from_chars(text.data(), text.data() + text.size(), n);
    if (text.empty() || e != std::errc{} || p != text.data() + text.size() ||
        values.size() == 3)
      throw std::runtime_error("Use ano; período; dia, ou apenas ano/período. "
                               "Não há interpretação automática.");
    values.push_back(n);
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
  TemporalReference r;
  r.label = "Prévia";
  r.year = values[0];
  if (values.size() > 1) {
    if (values[1] < 1 || values[1] > 32)
      throw std::runtime_error("Período inválido");
    r.month = static_cast<int>(values[1]);
  }
  if (values.size() > 2) {
    if (values[2] < 0 || values[2] > 60)
      throw std::runtime_error("Dia inválido");
    r.day = static_cast<int>(values[2]);
  }
  validate(c, r);
  return {r.year, r.month, r.day, {}};
}
std::pair<std::int64_t, std::int64_t>
calendar_year_limits(const FictionalCalendar &c) {
  validate(c);
  if (c.engine == "chinese")
    return {1901, 2100};
  if (c.engine == "hebrew" || c.engine == "ethiopic" ||
      c.engine == "islamic-civil")
    return {1, 9999};
  return {-1000000, 1000000};
}
CalendarGrid calendar_grid(const FictionalCalendar &c, std::int64_t y, int m) {
  auto months = calendar_months(c, y);
  if (m < 1 || m > static_cast<int>(months.size()))
    throw std::runtime_error("Período inexistente neste ano");
  CalendarGrid out;
  out.year = y;
  out.month = m;
  // Wide weeks use sequential presentation, retaining each actual weekday.
  out.weekly = c.weekdays.size() >= 3 && c.weekdays.size() <= 8;
  out.columns = out.weekly ? static_cast<int>(c.weekdays.size()) : 5;
  out.title = months[m - 1].name;
  const int first = calendar_first_day(c);
  const int offset = out.weekly ? calendar_weekday(c, {y, m, first}) : 0;
  for (int n = 0; n < months[m - 1].days; ++n) {
    CalendarDate date{y, m, first + n};
    out.cells.push_back({date, (offset + n) % out.columns,
                         (offset + n) / out.columns,
                         calendar_weekday(c, date)});
  }
  return out;
}
CalendarDate calendar_adjacent_month(const FictionalCalendar &c, CalendarDate d,
                                     int delta) {
  if (delta != -1 && delta != 1)
    throw std::runtime_error("Navegação mensal inválida");
  (void)calendar_grid(c, d.year, d.month);
  d.day = calendar_first_day(c);
  d.month += delta;
  if (d.month < 1) {
    --d.year;
    d.month = static_cast<int>(calendar_months(c, d.year).size());
  } else if (d.month > static_cast<int>(calendar_months(c, d.year).size())) {
    ++d.year;
    d.month = 1;
  }
  (void)calendar_grid(c, d.year, d.month);
  return d;
}
} // namespace inde::project

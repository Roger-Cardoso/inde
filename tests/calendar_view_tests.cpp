#include "inde/project/calendar_view.hpp"
#include <cassert>
#include <iostream>
using namespace inde::project;
template <class F> void refuses(F f) {
  bool denied = false;
  try {
    f();
  } catch (const std::exception &) {
    denied = true;
  }
  assert(denied);
}
int main() {
  std::size_t cells = 0;
  for (const auto &engine : calendar_template_engines()) {
    const auto c = calendar_template(engine);
    auto years = engine == "chinese"
                     ? std::vector<std::int64_t>{1901, 2023, 2024, 2100}
                 : engine == "hebrew"
                     ? std::vector<std::int64_t>{1, 5783, 5784, 9999}
                     : std::vector<std::int64_t>{1, 3, 2015, 2016, 9999};
    for (auto y : years) {
      auto months = calendar_months(c, y);
      for (std::size_t i = 0; i < months.size(); ++i) {
        auto grid = calendar_grid(c, y, static_cast<int>(i + 1));
        assert(grid.cells.size() == static_cast<std::size_t>(months[i].days));
        assert(grid.title == months[i].name);
        for (std::size_t n = 0; n < grid.cells.size(); ++n) {
          const auto &cell = grid.cells[n];
          assert(cell.date.day == calendar_first_day(c) + static_cast<int>(n));
          assert(cell.column >= 0 && cell.column < grid.columns &&
                 cell.row >= 0);
          assert(calendar_date(c, calendar_ordinal(c, cell.date)) == cell.date);
          if (grid.weekly)
            assert(cell.column == cell.weekday);
          ++cells;
        }
        if (i + 1 < months.size())
          assert(calendar_adjacent_month(
                     c, {y, static_cast<int>(i + 1), calendar_first_day(c)}, 1)
                     .month == static_cast<int>(i + 2));
      }
    }
    auto [lo, hi] = calendar_year_limits(c);
    refuses([&] {
      calendar_adjacent_month(c, {lo, 1, calendar_first_day(c)}, -1);
    });
    refuses([&] {
      calendar_adjacent_month(c,
                              {hi,
                               static_cast<int>(calendar_months(c, hi).size()),
                               calendar_first_day(c)},
                              1);
    });
    refuses([&] { calendar_grid(c, years.front(), 0); });
  }
  auto custom = calendar_template("gregorian");
  custom.engine = "custom";
  custom.leap_cycle = 0;
  custom.months = {{"Luz", 60}, {"Sombra", 1}};
  for (int week : {0, 1, 5, 8, 16}) {
    custom.weekdays.clear();
    for (int i = 0; i < week; ++i)
      custom.weekdays.push_back("Dia " + std::to_string(i));
    for (auto year : {-1000000, -1, 0, 1, 1000000}) {
      auto grid = calendar_grid(custom, year, 1);
      assert(grid.cells.size() == 60);
      assert(grid.weekly == (week >= 3 && week <= 8));
      for (const auto &cell : grid.cells)
        assert(cell.weekday ==
               (week ? calendar_weekday(custom, cell.date) : -1));
    }
  }
  auto chinese = calendar_template("chinese");
  assert(calendar_grid(chinese, 2023, 3).title == "Mês lunar 2 (intercalar)");
  assert(calendar_adjacent_month(chinese, {2023, 13, 1}, 1).year == 2024);
  auto maya = calendar_template("maya-haab");
  assert(calendar_grid(maya, 0, 19).cells.front().date.day == 0);
  assert(calendar_grid(maya, 0, 19).cells.size() == 5);
  auto gregorian = calendar_template("gregorian");
  assert(calendar_direct_boundary(gregorian, "-5").year == -5);
  assert(!calendar_direct_boundary(gregorian, "2024; 2").day);
  assert(calendar_direct_boundary(gregorian, "2024;2;29").day == 29);
  assert(calendar_direct_boundary(maya, "0;19;0").day == 0);
  refuses([&] { calendar_direct_boundary(gregorian, "2023;2;29"); });
  refuses([&] { calendar_direct_boundary(gregorian, "2024;2;29;12"); });
  refuses([&] { calendar_direct_boundary(chinese, "1900;1;1"); });
  refuses([&] { calendar_direct_boundary(gregorian, "amanhã"); });
  assert(calendar_grid(gregorian, 2024, 2).cells.size() == 29);
  std::cout << "PASS calendar projection: " << cells
            << " cells, eight engines, custom weeks and limits\n";
}

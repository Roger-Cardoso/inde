#include "inde/persistence/calendar_navigation_store.hpp"
#include "inde/project/calendar_view.hpp"
#include <fstream>
#include <glib.h>
#include <sstream>
namespace inde::persistence {
std::optional<project::CalendarDate> CalendarNavigationStore::load(
    const project::FictionalCalendar &c) const noexcept {
  try {
    if (!g_uuid_string_is_valid(c.id.c_str()))
      return {};
    auto p = path_ / c.id;
    std::error_code error;
    if (!std::filesystem::is_regular_file(p, error) ||
        std::filesystem::file_size(p, error) > 128 || error)
      return {};
    std::ifstream input(p);
    project::CalendarDate d;
    std::string extra;
    if (!(input >> d.year >> d.month) || (input >> extra))
      return {};
    d.day = project::calendar_first_day(c);
    (void)project::calendar_grid(c, d.year, d.month);
    return d;
  } catch (...) {
    return {};
  }
}
void CalendarNavigationStore::remember(const project::FictionalCalendar &c,
                                       project::CalendarDate d) const noexcept {
  try {
    if (!g_uuid_string_is_valid(c.id.c_str()))
      return;
    (void)project::calendar_grid(c, d.year, d.month);
    std::error_code error;
    std::filesystem::create_directories(path_, error);
    if (error)
      return;
    auto payload =
        std::to_string(d.year) + " " + std::to_string(d.month) + "\n";
    GError *failure{};
    g_file_set_contents_full((path_ / c.id).c_str(), payload.data(),
                             payload.size(), G_FILE_SET_CONTENTS_CONSISTENT,
                             0600, &failure);
    if (failure)
      g_error_free(failure);
  } catch (...) {
  } // preference failure cannot block writing
}
} // namespace inde::persistence

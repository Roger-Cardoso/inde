#pragma once
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/project/worldbuilding.hpp"
#include <filesystem>
namespace inde::persistence {
// Disposable UI preference. Never a selected date or an authorial fact.
class CalendarNavigationStore {
public:
  explicit CalendarNavigationStore(
      std::filesystem::path path =
          RecentProjectsStore::default_path().parent_path() /
          "calendar-navigation")
      : path_(std::move(path)) {}
  std::optional<project::CalendarDate>
  load(const project::FictionalCalendar &) const noexcept;
  void remember(const project::FictionalCalendar &,
                project::CalendarDate) const noexcept;

private:
  std::filesystem::path path_;
};
} // namespace inde::persistence

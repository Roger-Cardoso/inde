#pragma once
#include "inde/application/project_service.hpp"
#include "inde/ui/incremental_selector.hpp"
#include <gtkmm.h>
namespace inde::ui {
void show_temporal_context(Gtk::Window &, application::ProjectService &,
                           const std::string &kind, const std::string &id,
                           sigc::slot<void()> changed = {});
Gtk::Button *append_temporal_context(Gtk::Box &, Gtk::Window &,
                                     application::ProjectService &,
                                     const std::string &kind,
                                     const std::string &id);
void append_time_point_calendar(IncrementalSelector &,
                                application::ProjectService &);
} // namespace inde::ui

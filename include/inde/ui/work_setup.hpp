#pragma once
#include "inde/application/project_service.hpp"
#include <gtkmm.h>
#include <functional>
namespace inde::ui {
// A reusable, user-confirmed context setup; cancel/read-only never seeds content.
void show_work_setup(Gtk::Window &, application::ProjectService &,
                     std::function<void(const std::string &)> completed);
}

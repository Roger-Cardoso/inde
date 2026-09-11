#include "inde/ui/main_window.hpp"

#include <gtkmm/application.h>
#include <filesystem>
#include <optional>

int main(int argc, char* argv[]) {
    auto application = Gtk::Application::create("io.github.inde", Gio::Application::Flags::NON_UNIQUE);
    Gtk::Window::set_default_icon_name("inde");
    application->set_accel_for_action("win.new-project", "<Primary>n");
    application->set_accel_for_action("win.open-project", "<Primary>o");
    application->set_accel_for_action("win.save-project", "<Primary>s");
    application->set_accel_for_action("win.save-project-as", "<Primary><Shift>s");
    application->set_accel_for_action("win.close-project", "<Primary>w");
    application->set_accel_for_action("win.writing-undo", "<Primary>z");
    application->set_accel_for_action("win.writing-redo", "<Primary><Shift>z");
    application->set_accel_for_action("win.writing-bold", "<Primary>b");
    application->set_accel_for_action("win.writing-italic", "<Primary>i");
    application->set_accel_for_action("win.writing-underline", "<Primary>u");
    application->set_accel_for_action("win.proofread-current", "F7");
    std::optional<std::filesystem::path> initial_project;
    if (argc > 1) initial_project = std::filesystem::path(argv[1]);
    const int application_argc = initial_project ? 1 : argc;
    return application->make_window_and_run<inde::ui::MainWindow>(application_argc, argv, initial_project);
}

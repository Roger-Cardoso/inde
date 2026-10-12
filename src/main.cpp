#include "inde/ui/main_window.hpp"
#include "ui/rendering_policy.hpp"
#include "ui/theme_compatibility.hpp"

#include <gtkmm/application.h>
#include <filesystem>
#include <optional>

int main(int argc, char* argv[]) {
    inde::ui::configure_gtk_theme_base();
    inde::ui::configure_rendering_backend();
    auto application = Gtk::Application::create("io.github.inde", Gio::Application::Flags::NON_UNIQUE);
    Gtk::Window::set_default_icon_name("inde");
    // Routing belongs to the owning window and its focused context. Global
    // win.writing-* accelerators used to edit a hidden document behind forms.
    std::optional<std::filesystem::path> initial_project;
    if (argc > 1) initial_project = std::filesystem::path(argv[1]);
    const int application_argc = initial_project ? 1 : argc;
    return application->make_window_and_run<inde::ui::MainWindow>(application_argc, argv, initial_project);
}

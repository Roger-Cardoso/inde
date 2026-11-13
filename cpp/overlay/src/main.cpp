#include "inde/ui/main_window.hpp"
#include "ui/rendering_policy.hpp"
#include "ui/theme_compatibility.hpp"

#include <gtkmm/application.h>
#include "main_window_smoke.hpp"
#include <filesystem>
#include <optional>

int main(int argc, char* argv[]) {
    inde::ui::configure_gtk_theme_base();
    inde::ui::configure_rendering_backend();
    auto application = Gtk::Application::create("io.github.inde.publicacao.laboratorio", Gio::Application::Flags::NON_UNIQUE);
    Gtk::Window::set_default_icon_name("inde");
    if(argc==3 && std::string(argv[1])=="--integracao-revisao-smoke")return run_review_integration_smoke(application,argv[2]);
    if(argc==3 && std::string(argv[1])=="--comic")return run_comic_workspace(application,argv[2]);
    if(argc==3 && (std::string(argv[1])=="--importacao" || std::string(argv[1])=="--importacao-smoke"))return run_import_review(application,argv[2],std::string(argv[1])=="--importacao-smoke");
    if(argc==4 && std::string(argv[1])=="--comic01-smoke")return run_comic_window_smoke(application,argv[2],argv[3]);
    if(argc==4 && std::string(argv[1])=="--pub03b-smoke")return run_imported_window_smoke(application,argv[2],argv[3]);
    if(argc==4 && std::string(argv[1])=="--pub02b-smoke")return run_catalog_window_smoke(application,argv[2],argv[3]);
    if(argc==4 && std::string(argv[1])=="--pub02-smoke")return run_main_window_smoke(application,argv[2],argv[3]);
    // Routing belongs to the owning window and its focused context. Global
    // win.writing-* accelerators used to edit a hidden document behind forms.
    std::optional<std::filesystem::path> initial_project;
    if (argc > 1) initial_project = std::filesystem::path(argv[1]);
    const int application_argc = initial_project ? 1 : argc;
    return application->make_window_and_run<inde::ui::MainWindow>(application_argc, argv, initial_project);
}

#include "inde/experimental/blocks_lab_window.hpp"
#include "inde/ui/application_theme.hpp"
#include "../src/ui/rendering_policy.hpp"
#include "../src/ui/theme_compatibility.hpp"
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2) { std::cerr << "Usage: inde_text_blocks_lab prepared-copy.inde\n"; return 2; }
  try {
    const std::filesystem::path copy(argv[1]);
    inde::ui::configure_gtk_theme_base();
    inde::ui::configure_rendering_backend();
    auto app = Gtk::Application::create("io.github.inde.blocks.lab", Gio::Application::Flags::NON_UNIQUE);
    app->signal_startup().connect([] { inde::ui::install_application_theme(); });
    return app->make_window_and_run<inde::experimental::BlocksLabWindow>(1, argv, copy);
  } catch (const std::exception &e) { std::cerr << "FAIL laboratory: " << e.what() << '\n'; return 1; }
}

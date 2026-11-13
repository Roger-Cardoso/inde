#include "inde/ui/main_window.hpp"
#include "main_window_smoke.hpp"
#include <iostream>
int run_comic_workspace(const Glib::RefPtr<Gtk::Application> &app,
                        const std::filesystem::path &project) {
  std::unique_ptr<inde::ui::MainWindow> window;
  int result = 0;
  app->signal_activate().connect([&] {
    try {
      window = std::make_unique<inde::ui::MainWindow>(project);
      app->add_window(*window);
      window->present();
      Glib::signal_idle().connect_once([&] {
        try {
          auto action = window->lookup_action("show-comic");
          if (action)
            action->activate();
        } catch (const std::exception &e) {
          std::cerr << e.what() << '\n';
        }
      });
    } catch (const std::exception &e) {
      std::cerr << e.what() << '\n';
      result = 1;
      app->quit();
    }
  });
  app->run();
  window.reset();
  return result;
}

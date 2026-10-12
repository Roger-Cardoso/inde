// Opt-in regression of the real MainWindow, with public actions and widgets.
// Run only on a disposable fixture copy. No private access or UI replacement.
#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/main_window.hpp"
#include "ui/widget_probe.hpp"

#include <algorithm>
#include <gdk/x11/gdkx.h>
#include <glibmm/spawn.h>
#include <iostream>

using namespace inde::test::ui;

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "Usage: ui_shell_smoke DISPOSABLE_FIXTURE EXPECTED_ENTITIES "
                 "menu|initial|full|physical|seeded|lifecycle --allow-fixture-writes\n";
    return 2;
  }
  const std::filesystem::path fixture = argv[1];
  auto expected = std::stoul(argv[2]);
  const std::string mode = argv[3];
  if ((mode != "menu" && mode != "initial" && mode != "full" &&
      mode != "physical" && mode != "seeded" && mode != "lifecycle") ||
      std::string(argv[4]) != "--allow-fixture-writes")
    return 2;
  int result = 1, step = 0;
  inde::ui::configure_rendering_backend();
  const bool full = mode == "full" || mode == "seeded" || mode == "lifecycle";
  std::optional<std::string> seeded_id;
  if (mode == "seeded") {
    inde::application::ProjectService seed;
    seed.open(fixture);
    require(seed.narrative().entity_count() == 0 && expected == 0,
            "Seed mode requires an empty disposable project");
    seeded_id = seed.narrative().create_entity(
        "00000000-0000-4000-9000-000000000001", "QA persistido — memória 🖋").id;
    seed.close();
    ++expected;
    std::cout << "CHECK: entity persisted before MainWindow construction" << std::endl;
  }
  inde::ui::MainWindow *window = nullptr;
  auto app = Gtk::Application::create("io.github.inde.shell-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  sigc::connection timer;
  app->signal_activate().connect([&] {
    window = new inde::ui::MainWindow(fixture);
    app->add_window(*window);
    window->present();
    timer = Glib::signal_timeout().connect(
        [&] {
          try {
            if (step >= (full ? 16 : 12)) {
              result = 0;
              std::cout << "PASS: shell regression " << mode << std::endl;
              window->close();
              return false;
            }
            auto *planning = find<inde::ui::PlanningWorkspace>(
                *window, [](auto &) { return true; });
            require(planning, "Planning workspace missing");
            if (step == 0) {
              auto *renderer =
                  gtk_native_get_renderer(GTK_NATIVE(window->gobj()));
              require(renderer, "Renderer not realized");
              std::cout << "CHECK: active renderer="
                        << G_OBJECT_TYPE_NAME(renderer) << std::endl;
              if (const auto *choice = g_getenv("GSK_RENDERER");
                  choice && std::string(choice) == "cairo")
                require(GSK_IS_CAIRO_RENDERER(renderer),
                        "Cairo request not honored");
            }
            auto *menu = find<Gtk::MenuButton>(*window, [](auto &b) {
              return b.get_tooltip_text() == "Menu do aplicativo";
            });
            require(menu && menu->get_popover(), "Application menu missing");
            if (full && step == 13) {
              gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                         "win.close-project", nullptr);
              std::cout << "CHECK: project closed" << std::endl;
            } else if (full && step == 14) {
              click(*window, fixture.stem().string());
              std::cout << "CHECK: project reopened through recent-project card"
                        << std::endl;
            } else if (mode == "initial" ||
                       (full && (step == 0 || step == 15))) {
              gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                         "win.show-planning", nullptr);
              click(*planning, "Personagens e mundo");
              auto &explorer =
                  *pages(*planning, "explorer").get_child_by_name("explorer");
              require(count(explorer,
                            [](auto &w) {
                              return w.has_css_class("entity-card");
                            }) == std::min(expected, std::size_t{100}),
                      "First activation did not load entity cards");
              require(find<Gtk::Label>(explorer,
                                       [&](auto &l) {
                                         return l.get_text().raw().starts_with(
                                             std::to_string(expected) +
                                             (expected == 1
                                                  ? " resultado • página "
                                                  : " resultados • página "));
                                       }),
                      "First activation did not explain result count");
              std::cout << "PASS: initial explorer without clearing filters"
                        << std::endl;
              if (mode == "lifecycle") {
                result = 0;
                std::cout << "PASS: shell regression lifecycle with pending idle"
                          << std::endl;
                window->close();
                return false;
              }
              if (mode == "initial") {
                step = 12;
                return true;
              }
            } else if ((step - (full ? 1 : 0)) % 2 == 0) {
              std::cout << "CHECK: application menu popup " << step / 2 + 1
                        << std::endl;
              if (mode == "physical") {
                auto surface = window->get_surface();
                require(GDK_IS_X11_SURFACE(surface->gobj()),
                        "X11 required for pointer regression");
                graphene_rect_t bounds{};
                require(gtk_widget_compute_bounds(GTK_WIDGET(menu->gobj()),
                                                  GTK_WIDGET(window->gobj()),
                                                  &bounds),
                        "Menu bounds unavailable");
                const auto xid = gdk_x11_surface_get_xid(surface->gobj());
                Glib::spawn_async(
                    {},
                    {"xdotool", "mousemove", "--window", std::to_string(xid),
                     std::to_string(static_cast<int>(bounds.origin.x +
                                                     bounds.size.width / 2)),
                     std::to_string(static_cast<int>(bounds.origin.y +
                                                     bounds.size.height / 2)),
                     "click", "1"},
                    Glib::SpawnFlags::SEARCH_PATH);
              } else
                menu->popup();
            } else {
              require(menu->get_popover()->get_mapped(), "Menu did not map");
              menu->popdown();
              std::cout << "PASS: application menu mapped/closed" << std::endl;
            }
            ++step;
            return true;
          } catch (const std::exception &e) {
            std::cerr << "FAIL shell: " << e.what() << std::endl;
            window->close();
            return false;
          }
        },
        500);
  });
  app->run(1, argv);
  timer.disconnect();
  delete window;
  if (result == 0 && seeded_id) {
    inde::application::ProjectService cleanup;
    cleanup.open(fixture);
    cleanup.narrative().delete_entity(*seeded_id);
    cleanup.close();
    std::cout << "CHECK: seeded fixture restored to zero entities" << std::endl;
  }
  return result;
}

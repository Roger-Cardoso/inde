#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "ui/widget_probe.hpp"
#include <cmath>
#include <gdk/x11/gdkx.h>
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
// Real X11 button events on the disposable Xvfb display: cover the distinction
// between selecting a row, activating it, and clicking its child checkbox.
bool mouse_click(Gtk::Widget &widget, Gtk::Window &window, int repeat = 1) {
  if (!widget.get_mapped() || widget.get_width() <= 0 || widget.get_height() <= 0)
    return false;
  graphene_point_t origin{static_cast<float>(widget.get_width() / 2),
                          static_cast<float>(widget.get_height() / 2)},
      point{};
  require(widget.get_mapped() &&
              gtk_widget_compute_point(widget.gobj(), GTK_WIDGET(window.gobj()), &origin,
                                       &point),
          "Click target is not allocated");
  auto *picked = gtk_widget_pick(GTK_WIDGET(window.gobj()), point.x, point.y,
                                 GTK_PICK_DEFAULT);
  if (!picked || (picked != widget.gobj() &&
                  !gtk_widget_is_ancestor(picked, widget.gobj())))
    return false; // Wait for a frame allocation, never click stale coordinates.
  const auto surface = window.get_surface();
  require(surface && GDK_IS_X11_SURFACE(surface->gobj()),
          "Mouse QA requires isolated X11");
  const auto xid = std::to_string(gdk_x11_surface_get_xid(surface->gobj()));
  const auto x = std::to_string(static_cast<int>(std::round(point.x))),
             y = std::to_string(static_cast<int>(std::round(point.y)));
  const auto repeats = std::to_string(repeat);
  const char *command[] = {
      "xdotool", "mousemove", "--window", xid.c_str(),     x.c_str(),
      y.c_str(), "click",     "--repeat", repeats.c_str(), "--delay",
      "100",     "1",         nullptr};
  int status{};
  GError *error{};
  const bool spawned = g_spawn_sync(nullptr, const_cast<char **>(command),
                                    nullptr, G_SPAWN_SEARCH_PATH, nullptr,
                                    nullptr, nullptr, nullptr, &status, &error);
  const std::string message =
      error ? error->message : "X11 event injection failed";
  if (error)
    g_error_free(error);
  require(spawned && status == 0, message);
  std::cout << "MOUSE target=" << G_OBJECT_TYPE_NAME(widget.gobj())
            << " x=" << x << " y=" << y << std::endl;
  return true;
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::filesystem::path fixture = argv[1];
  const bool restart = std::string(argv[2]) == "--verify-restart";
  if (!restart && std::string(argv[2]) != "--allow-fixture-writes")
    return 2;
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.access-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::MainWindow *window{};
  int phase = 0, result = 1;
  bool double_click_sent = false;
  int comparison_click_stage = 0;
  std::string work, source, copy, third, unit, line;
  auto read = [&](auto f) {
    application::ProjectService s;
    s.open(fixture);
    f(s);
  };
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(fixture);
    app->add_window(*window);
    window->present();
    Glib::signal_timeout().connect(
        [&] {
          try {
            auto *planning = find<ui::PlanningWorkspace>(
                *window, [](auto &) { return true; });
            auto *builder = find<ui::NarrativeStructureWorkspace>(
                *window, [](auto &) { return true; });
            require(planning && builder, "Planning workspace absent");
            auto modal = [&]() -> Gtk::Box & {
              auto *body = find<Gtk::Box>(*window, [](auto &b) {
                return b.get_mapped() && b.has_css_class("modal-card");
              });
              require(body, "Modal missing");
              return *body;
            };
            auto field = [&](const std::string &name) -> Gtk::Entry & {
              auto *label = find<Gtk::Label>(
                  modal(), [&](auto &v) { return v.get_text().raw() == name; });
              require(label, "Field absent: " + name);
              auto *entry =
                  dynamic_cast<Gtk::Entry *>(label->get_mnemonic_widget());
              require(entry, "Field type mismatch");
              return *entry;
            };
            auto row = [&](const std::string &name) -> Gtk::ListBoxRow & {
              auto *item = find<Gtk::ListBoxRow>(*builder, [&](auto &v) {
                return find<Gtk::Label>(v, [&](auto &l) {
                         return l.get_text().raw() == name;
                       }) != nullptr;
              });
              require(item, "Structure missing: " + name);
              return *item;
            };
            auto menu = [&](const std::string &name,
                            const std::string &action) {
              auto *button = find<Gtk::MenuButton>(row(name), [&](auto &b) {
                return b.get_tooltip_text().raw() == "Ações do plano: " + name;
              });
              require(button, "Ellipsis missing");
              button->popup();
              click(*button->get_popover(), action);
            };
            auto restore = [&](const std::string &page) {
              auto view = builder->capture_view().as_object();
              view["work"] = work;
              view["structure"] = source;
              view["unit"] =
                  unit.empty() ? ui::ViewState{} : ui::ViewState{unit};
              view["line"] =
                  line.empty() ? ui::ViewState{} : ui::ViewState{line};
              view["page"] = page;
              view["order"] = "narrative";
              view["detail_section"] = "units";
              builder->restore_view(view);
            };
            if (restart) {
              if (phase == 0) {
                auto view = builder->capture_view();
                require(view.at("page").as_string() == "detail" &&
                            view.at("detail_section").as_string() == "links",
                        "Stable detail section not restored");
                require(count(*builder,
                              [](auto &v) {
                                return dynamic_cast<Gtk::Paned *>(&v) !=
                                       nullptr;
                              }) == 0,
                        "Draggable narrative layout returned");
                std::cout << "SNAPSHOT: access-restart" << std::endl;
              } else {
                std::cout << "PASS temporal access restart" << std::endl;
                result = 0;
                window->close();
                return false;
              }
              ++phase;
              return true;
            }
            switch (phase) {
            case 0:
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 "win.show-planning", nullptr),
                      "Planning unavailable");
              click(*planning, "Plano narrativo");
              require(count(*builder,
                            [](auto &v) {
                              return dynamic_cast<Gtk::Paned *>(&v) != nullptr;
                            }) == 0,
                      "Narrative split pane remains");
              std::cout << "SNAPSHOT: access-start" << std::endl;
              break;
            case 1: {
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 "win.show-catalog", nullptr),
                      "Catalog unavailable");
              auto *catalog = find<ui::EditorialWorkspace>(
                  *window, [](auto &) { return true; });
              require(catalog, "Catalog missing");
              click(*catalog, "Nova obra");
              break;
            }
            case 2:
              click(modal(), "Cancelar");
              break;
            case 3:
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 "win.show-planning", nullptr),
                      "Planning unavailable");
              click(*planning, "Plano narrativo");
              read([&](auto &s) {
                if (s.catalog().works.empty())
                  click(*builder, "Começar minha Obra…");
                else {
                  require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                     "win.show-catalog",
                                                     nullptr),
                          "Catalog unavailable");
                  auto *catalog = find<ui::EditorialWorkspace>(
                      *window, [](auto &) { return true; });
                  require(catalog, "Catalog missing");
                  click(*catalog, "Nova obra");
                }
              });
              break;
            case 4: {
              require(
                  find<Gtk::Label>(
                      modal(), [](auto &l) { return l.get_text() == "Obra"; }),
                  "Contextual setup unavailable");
              break;
            }
            case 5:
              field("Obra").set_text("  ");
              click(modal(), "Criar Obra");
              break;
            case 6:
              require(field("Obra").get_text() == "  ",
                      "Validation destroyed input");
              field("Obra").set_text("QA Access Obra");
              field("Nome do novo ").set_text("QA Access Universo");
              std::cout << "SNAPSHOT: access-work-setup" << std::endl;
              break;
            case 7:
              click(modal(), "Criar Obra");
              break;
            case 8:
              read([&](auto &s) {
                for (auto &w : s.catalog().works)
                  if (w.title == "QA Access Obra")
                    work = w.id;
                require(!work.empty(), "Work not created");
                source = s.structures()
                             .create_narrative(work, "QA Access Original")
                             .id;
                line = s.structures()
                           .create_line(source, "Linha de descoberta", "")
                           .id;
                unit =
                    s.structures()
                        .create_unit(source, "", "Consequência antes da causa",
                                     "", "", "")
                        .id;
                auto second =
                    s.structures()
                        .create_unit(source, "", "Causa revelada depois", "",
                                     "", "")
                        .id;
                s.structures().add_unit_to_line(unit, line);
                static_cast<void>(s.structures().create_link(
                    source, unit, second, project::NarrativeLinkKind::Causes,
                    "Não impor cronologia"));
                third = s.structures()
                            .create_narrative(work, "QA Access Terceira")
                            .id;
              });
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 "win.show-planning", nullptr),
                      "Planning unavailable");
              click(*planning, "Plano narrativo");
              restore("library");
              break;
            case 9: {
              auto &item = row("QA Access Original");
              auto *list = dynamic_cast<Gtk::ListBox *>(item.get_parent());
              require(list && !list->get_activate_on_single_click(),
                      "Single click opens structure");
              if (!mouse_click(item, *window))
                return true;
              std::cout << "SNAPSHOT: access-library" << std::endl;
              break;
            }
            case 10:
              require(pages(*builder, "library").get_visible_child_name() ==
                          "library",
                      "Left click opened detail");
              require(dynamic_cast<Gtk::ListBox *>(
                          row("QA Access Original").get_parent())
                              ->get_selected_row() ==
                          &row("QA Access Original"),
                      "Left click did not select");
              menu("QA Access Original", "Duplicar");
              break;
            case 11: {
              auto *e = find<Gtk::Entry>(modal(), [](auto &) { return true; });
              require(e, "Duplicate title absent");
              e->set_text("QA Access Cópia");
              auto *shared = find<Gtk::CheckButton>(
                  modal(), [](auto &) { return true; });
              require(shared, "Sharing acknowledgement missing");
              shared->set_active(true);
              click(modal(), "Duplicar");
              break;
            }
            case 12:
              read([&](auto &s) {
                for (auto &v : s.structures().narrative_structures(work))
                  if (v.name == "QA Access Cópia")
                    copy = v.id;
                require(!copy.empty(), "Duplicate missing");
              });
              menu("QA Access Original", "Comparar com…");
              break;
            case 13: {
              const std::string name = comparison_click_stage < 2
                                           ? "QA Access Cópia"
                                           : "QA Access Terceira";
                auto *check = find<Gtk::CheckButton>(
                    row(name), [](auto &) { return true; });
                require(check && check->get_visible(),
                        "Comparison checkbox absent");
              if (comparison_click_stage % 2 == 0)
                row(name).grab_focus(); // Scroll the target into the viewport.
              else if (!mouse_click(row(name), *window))
                return true;
              if (++comparison_click_stage < 4)
                return true; // Allocate scrolling before injecting mouse events.
              require(pages(*builder, "library").get_visible_child_name() ==
                          "library",
                      "Target selection opened details");
              std::cout << "SNAPSHOT: access-comparison-targets" << std::endl;
              break;
            }
            case 14:
              for (auto name : {"QA Access Cópia", "QA Access Terceira"})
                require(
                    find<Gtk::CheckButton>(row(name),
                                           [](auto &) { return true; })
                        ->get_active(),
                    "Row click did not mark comparison target exactly once");
              click(*builder, "Comparar");
              break;
            case 15: {
              require(pages(*builder, "comparison").get_visible_child_name() ==
                          "comparison",
                      "Multi comparison absent");
              auto *choice = find<Gtk::ComboBoxText>(*builder, [&](auto &c) {
                return c.get_visible() && c.get_active_id().raw() == copy;
              });
              require(choice && choice->set_active_id(third),
                      "Second comparison target absent");
              std::cout << "SNAPSHOT: access-comparison" << std::endl;
              break;
            }
            case 16:
              restore("library");
              break;
            case 17:
              if (!double_click_sent) {
                if (!mouse_click(row("QA Access Original"), *window, 2))
                  return true;
                double_click_sent = true;
                return true; // Let GTK process events on the allocated library.
              }
              require(pages(*builder, "detail").get_visible_child_name() ==
                          "detail",
                      "Double click did not open structure");
              require(pages(*builder, "units").get_visible_child_name() ==
                          "units",
                      "Units default absent");
              std::cout << "SNAPSHOT: access-units" << std::endl;
              break;
            case 18:
              pages(*builder, "lines").set_visible_child("lines");
              break;
            case 19:
              std::cout << "SNAPSHOT: access-lines" << std::endl;
              break;
            case 20:
              pages(*builder, "links").set_visible_child("links");
              break;
            case 21:
              std::cout << "SNAPSHOT: access-links" << std::endl;
              click(*builder, "Novo vínculo");
              break;
            case 22:
              require(find<Gtk::Label>(modal(),
                                       [](auto &l) {
                                         return l.get_text() ==
                                                "Parte de origem";
                                       }),
                      "Link source requires hidden selection");
              click(modal(), "Cancelar");
              break;
            case 23:
              restore("library");
              menu("QA Access Original", "Salvar como modelo de planejamento");
              break;
            case 24: {
              auto *e = find<Gtk::Entry>(modal(), [](auto &) { return true; });
              require(e, "Model title absent");
              e->set_text("QA Access Modelo");
              click(modal(), "Salvar modelo");
              break;
            }
            case 25: {
              auto *b = find<Gtk::MenuButton>(*builder, [](auto &v) {
                return v.get_label() == "Modelos de planejamento";
              });
              require(b && !b->get_popover()->get_visible(),
                      "Models permanently visible");
              b->popup();
              std::cout << "SNAPSHOT: access-models" << std::endl;
              break;
            }
            case 26: {
              auto *b = find<Gtk::MenuButton>(*builder, [](auto &v) {
                return v.get_label() == "Modelos de planejamento";
              });
              b->popdown();
              restore("detail");
              pages(*builder, "links").set_visible_child("links");
              window->set_default_size(900, 650);
              // Xvfb has no window manager: resize the real X11 surface, as a
              // user resize would, instead of only changing GTK's default.
              const auto xid = std::to_string(gdk_x11_surface_get_xid(
                  window->get_surface()->gobj()));
              const char *command[] = {"xdotool", "windowsize", xid.c_str(),
                                       "900", "650", nullptr};
              int status{};
              require(g_spawn_sync(nullptr, const_cast<char **>(command), nullptr,
                                   G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr,
                                   nullptr, &status, nullptr) && status == 0,
                      "Native resize failed");
              break;
            }
            case 27:
              {
                int minimum{}, natural{};
                gtk_widget_measure(GTK_WIDGET(window->gobj()), GTK_ORIENTATION_HORIZONTAL,
                                   -1, &minimum, &natural, nullptr, nullptr);
                std::cout << "MEASURE compact width=" << window->get_width()
                          << " minimum=" << minimum << " natural=" << natural << std::endl;
                require(minimum <= 920, "Narrative minimum exceeds compact width");
              }
              require(window->get_width() <= 920,
                      "Narrative page imposes excessive minimum width");
              std::cout << "SNAPSHOT: access-compact" << std::endl;
              break;
            case 28:
              read([&](auto &s) {
                require(s.catalog().works.size() >= 1 &&
                            s.structures().narrative_structures(work).size() ==
                                4,
                        "Unexpected mutation");
              });
              result = 0;
              std::cout << "PASS temporal access" << std::endl;
              window->close();
              return false;
            }
            ++phase;
          } catch (const std::exception &e) {
            std::cerr << "FAIL access phase " << phase << ": " << e.what()
                      << std::endl;
            window->close();
            return false;
          }
          return true;
        },
        900);
  });
  app->run(1, argv);
  delete window;
  return result;
}

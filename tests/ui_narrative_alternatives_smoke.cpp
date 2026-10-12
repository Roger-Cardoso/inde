#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const bool restart = std::string(argv[2]) == "--verify-restart";
  if (!restart && std::string(argv[2]) != "--allow-fixture-writes")
    return 2;
  const std::filesystem::path fixture = argv[1];
  std::string work, structure, source_a, copy, copy_a, copy_line;
  {
    application::ProjectService s;
    s.open(fixture);
    if (!restart) {
      work = s.create_work(
                  s.create_intellectual_property("QA Alternatives", "", "").id,
                  "QA Alternatives Obra", "", "", "pt-BR", "Planejamento")
                 .id;
      structure =
          s.structures().create_narrative(work, "QA Alternatives Original").id;
      auto line = s.structures().create_line(structure, "Investigação",
                                             "Pergunta aberta");
      auto a = s.structures().create_unit(
               structure, "", "Consequência primeiro", "", "Intenção", ""),
           b = s.structures().create_unit(structure, "", "Causa depois", "", "",
                                          "");
      source_a = a.id;
      s.structures().add_unit_to_line(a.id, line.id);
      s.structures().add_unit_to_line(b.id, line.id);
      s.builder().queue_unit(s.builder().snapshot(structure), b.id, true);
      s.builder().queue_unit(s.builder().snapshot(structure), a.id, true);
      const auto l = s.structures().create_link(
          structure, a.id, b.id, project::NarrativeLinkKind::Precedes,
          "Ordem de revelação declarada");
      s.builder().set_dependency_scope(s.builder().snapshot(structure), l.id,
                                       "narrative");
      const auto doc = s.builder().create_document(
          s.builder().snapshot(structure), a.id, "Texto compartilhado QA");
      auto d = *s.writing().document(doc);
      d.content = "Texto preservado — Água 🙂";
      d=s.writing().update_document(d);
    } else {
      auto view = ui::parse_view_state(
                      ui::ui_preference(ui::project_preference_group(
                                            s.current()->manifest().project_id),
                                        "context-v1"))
                      .at("planning")
                      .at("builder");
      work = view.at("work").as_string();
      copy = view.at("structure").as_string();
      copy_a = view.at("unit").as_string();
      copy_line = view.at("line").as_string();
      require(view.at("order").as_string() == "line", "Restart line lens lost");
      require(!s.builder().compare_to_origin(copy).differences.empty(),
              "Restart provenance differences lost");
    }
  }
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.alternatives-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::MainWindow *window{};
  sigc::connection tick;
  std::function<void()> after_snapshot;
  int phase{}, result = 1;
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(fixture);
    app->add_window(*window);
    window->present();
    tick = Glib::signal_timeout().connect(
        [&] {
          try {
            if (after_snapshot) {
              auto action = std::move(after_snapshot);
              after_snapshot = {};
              action();
            }
            if (!window->get_mapped())
              return true;
            auto *planning = find<ui::PlanningWorkspace>(
                *window, [](auto &) { return true; });
            auto *builder = find<ui::NarrativeStructureWorkspace>(
                *window, [](auto &) { return true; });
            require(planning && builder, "Workspace missing");
            auto modal = [&]() -> Gtk::Box & {
              auto *v = find<Gtk::Box>(*window, [](auto &b) {
                return b.get_mapped() && b.has_css_class("modal-card");
              });
              require(v, "Modal missing");
              return *v;
            };
            auto defer_modal_click = [&](std::string label) {
              after_snapshot = [window, label = std::move(label)] {
                auto *v = find<Gtk::Box>(*window, [](auto &b) {
                  return b.get_mapped() && b.has_css_class("modal-card");
                });
                require(v, "Deferred modal missing");
                click(*v, label);
              };
            };
            auto restore = [&](std::string id, std::string unit,
                               std::string line, std::string page,
                               std::string order) {
              auto view = builder->capture_view().as_object();
              view["work"] = work;
              view["structure"] = id;
              view["unit"] =
                  unit.empty() ? ui::ViewState{} : ui::ViewState{unit};
              view["line"] =
                  line.empty() ? ui::ViewState{} : ui::ViewState{line};
              view["page"] = page;
              view["order"] = order;
              view["scroll"] = ui::scroll_state(0);
              builder->restore_view(view);
            };
            auto select = [&](const std::string &name) {
              auto *row = find<Gtk::ListBoxRow>(*builder, [&](auto &r) {
                return find<Gtk::Label>(r, [&](auto &l) {
                         return l.get_text().raw() == name;
                       }) != nullptr;
              });
              require(row, "Structure row missing: " + name);
              auto *list = dynamic_cast<Gtk::ListBox *>(row->get_parent());
              require(list, "List missing");
              list->select_row(*row);
            };
            auto menu = [&](const std::string &name, const std::string &action) {
              auto *button=find<Gtk::MenuButton>(*builder,[&](auto &b) { return b.get_tooltip_text().raw()=="Ações do plano: "+name; });
              require(button,"Structure actions missing");
              button->popup();click(*button->get_popover(),action);
            };
            auto read = [&](auto f) {
              application::ProjectService s;
              s.open(fixture);
              f(s);
            };
            if (restart) {
              if (phase == 0) {
                auto v = builder->capture_view();
                require(v.at("structure").as_string() == copy &&
                            v.at("unit").as_string() == copy_a &&
                            v.at("order").as_string() == "line",
                        "Restart selection/lens lost");
                std::cout << "SNAPSHOT: alternatives-restart" << std::endl;
              } else {
                result = 0;
                std::cout << "PASS temporal alternatives restart" << std::endl;
                window->close();
                return false;
              }
            } else
              switch (phase) {
              case 0:
                require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                   "win.show-planning",
                                                   nullptr),
                        "Planning unavailable");
                click(*planning, "Plano narrativo");
                restore(structure, "", "", "library", "narrative");
                break;
              case 1:
                select("QA Alternatives Original");
                break;
              case 2:
                menu("QA Alternatives Original", "Criar alternativa");
                break;
              case 3: {
                auto *entry =
                    find<Gtk::Entry>(modal(), [](auto &) { return true; });
                require(entry, "Name field missing");
                entry->set_text("QA Alternatives Copiada");
                click(modal(), "Criar alternativa");
                break;
              }
              case 4: {
                auto *entry =
                    find<Gtk::Entry>(modal(), [](auto &) { return true; });
                require(entry && entry->get_text() == "QA Alternatives Copiada",
                        "Validation lost name");
                require(find<Gtk::Label>(modal(),
                                         [](auto &l) {
                                           return l.get_text().raw().find(
                                                      "pode afetar ambas") !=
                                                  std::string::npos;
                                         }),
                        "Validation lost sharing policy");
                auto *check = find<Gtk::CheckButton>(
                    modal(), [](auto &) { return true; });
                require(check, "Sharing confirmation missing");
                check->set_active(true);
                std::cout << "SNAPSHOT: alternatives-derivation-policy"
                          << std::endl;
                defer_modal_click("Criar alternativa");
                break;
              }
              case 5:
                read([&](auto &s) {
                  for (auto &v : s.structures().narrative_structures(work))
                    if (v.name == "QA Alternatives Copiada")
                      copy = v.id;
                  require(!copy.empty(), "Derivation missing");
                  auto target = s.builder().snapshot(copy);
                  copy_a = target.units.front().id;
                  copy_line = target.lines.front().id;
                  require(
                      s.builder().compare(structure, copy).differences.empty(),
                      "Copied context differs");
                  require(
                      target.realizations.size() == 1 &&
                          s.writing()
                                  .document(
                                      target.realizations.front().document_id)
                                  ->content == "Texto preservado — Água 🙂",
                      "Shared prose lost");
                });
                select("QA Alternatives Copiada");
                break;
              case 6:
                menu("QA Alternatives Copiada", "Comparar com…");
                click(*builder, "Origem registrada…");
                break;
              case 7:
                click(modal(), "Consultar diferenças");
                break;
              case 8:
                require(
                    pages(*builder, "comparison").get_visible_child_name() ==
                        "comparison",
                    "Comparison page missing");
                std::cout << "SNAPSHOT: alternatives-identical" << std::endl;
                break;
              case 9:
                click(*builder, "← Alternativas");
                restore(copy, copy_a, copy_line, "unit", "narrative");
                break;
              case 10: {
                auto *e = find<Gtk::Expander>(*builder, [](auto &v) {
                  return v.get_label() == "Ordens e trabalho";
                });
                require(e, "Orders missing");
                e->set_expanded(true);
                break;
              }
              case 11:
                click(*builder, "Mover depois");
                break;
              case 12:
                std::cout << "SNAPSHOT: alternatives-impact-warning"
                          << std::endl;
                defer_modal_click("Cancelar");
                break;
              case 13:
                read([&](auto &s) {
                  require(s.builder().snapshot(copy).units.front().id == copy_a,
                          "Cancelled movement wrote data");
                });
                click(*builder, "Mover depois");
                break;
              case 14:
                click(modal(), "Confirmar movimento");
                break;
              case 15:
                read([&](auto &s) {
                  require(
                      s.builder().snapshot(copy).units.front().id != copy_a &&
                          s.builder().snapshot(structure).units.front().id ==
                              source_a,
                      "Orders leaked between alternatives");
                });
                click(*builder, "Planos");
                select("QA Alternatives Copiada");
                break;
              case 16:
                menu("QA Alternatives Copiada", "Comparar com…");
                click(*builder, "Origem registrada…");
                break;
              case 17:
                click(modal(), "Consultar diferenças");
                break;
              case 18:
                std::cout << "SNAPSHOT: alternatives-montage-differences"
                          << std::endl;
                after_snapshot = [builder] {
                  click(*builder, "Abrir parte no destino");
                };
                break;
              case 19: {
                restore(copy, copy_a, copy_line, "unit", "line");
                auto *e = find<Gtk::Expander>(*builder, [](auto &v) {
                  return v.get_label() == "Ordens e trabalho";
                });
                require(e, "Line orders missing");
                e->set_expanded(true);
                break;
              }
              case 20:
                click(*builder, "Mover depois");
                break;
              case 21:
                std::cout << "SNAPSHOT: alternatives-independent-line"
                          << std::endl;
                defer_modal_click("Confirmar movimento");
                break;
              case 22:
                read([&](auto &s) {
                  const auto delta = s.builder().compare_to_origin(copy);
                  require(std::any_of(delta.differences.begin(),
                                      delta.differences.end(),
                                      [](auto &d) {
                                        return d.dimension ==
                                               "Linhas e posições próprias";
                                      }),
                          "Line order not changed independently");
                });
                break;
              case 23:
                std::cout << "SNAPSHOT: alternatives-line-return" << std::endl;
                break;
              case 24:
                result = 0;
                std::cout << "PASS temporal alternatives production flow"
                          << std::endl;
                window->close();
                return false;
              }
            ++phase;
            return true;
          } catch (const std::exception &e) {
            std::cerr << "FAIL alternatives phase=" << phase << ": " << e.what()
                      << std::endl;
            window->hide();
            return false;
          }
        },
        800);
  });
  auto code = app->run();
  tick.disconnect();
  delete window;
  return code ? code : result;
}

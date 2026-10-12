#include "../src/ui/rendering_policy.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/project_operations.hpp"
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
  application::ProjectService observed;
  ui::configure_rendering_backend();
  observed.open(argv[1]);
  auto &world = observed.worldbuilding();
  std::string calendar, axis, document, reference, point;
  if (!restart) {
    auto c = project::calendar_template("gregorian");
    c.name = "QA Auxiliar — calendário em testes";
    c = world.create_calendar(c);
    calendar = c.id;
    auto ax =
        observed.planning().create_time_axis("QA Auxiliar — eixo de testes");
    axis = ax.id;
    world.assign_calendar(axis, calendar, 0, {});
    point = observed.planning()
                .create_time_point(axis, 42, "QA Ponto preservado")
                .id;
    project::TemporalReference r;
    r.axis_id = axis;
    r.calendar_id = calendar;
    r.label = "QA Data da cena";
    r.year = 2000;
    r.month = 1;
    r.day = 2;
    r = world.save_temporal_reference(r);
    reference = r.id;
    for (int i = 0; i < 13; ++i) {
      auto other = r;
      other.id.clear();
      other.label = "QA Data extra " + std::to_string(i);
      (void)world.save_temporal_reference(other);
    }
    auto d =
        observed.writing().create_document("QA Auxiliar — texto preservado");
    document = d.id;
    for (int i = 0; i < 70; ++i)
      d.content +=
          "Uma linha de escrita para testar o retorno ao mesmo ponto.\n";
    auto type =
        *project::find_annotation_type(project::builtin_annotation_types(),
                                       "inde.semantic.temporal-context", 1);
    d = application::TextSemanticService::create(d, type, 100, 110, {});
    d = application::TextSemanticService::bind_date(
        d, d.semantics.annotations.front().id, reference, "scene");
    (void)observed.writing().update_document(d);
  } else {
    for (const auto &c : world.calendars())
      if (c.name == "QA Auxiliar — calendário em testes")
        calendar = c.id;
    for (const auto &a : observed.planning().time_axes())
      if (a.name == "QA Auxiliar — eixo de testes")
        axis = a.id;
    persistence::DocumentQuery q;
    q.search = "QA Auxiliar — texto preservado";
    for (const auto &d : observed.writing().documents(q))
      if (d.title == q.search)
        document = d.id;
    require(!document.empty() && !calendar.empty() && !axis.empty(),
            "Restart fixture missing");
    require(!world.binding(axis) && world.adoptions(axis).empty(),
            "Axis detach/history not persisted");
    require(world.temporal_references(axis).size() == 14,
            "References lost on detach");
    require(observed.writing()
                    .document(document)
                    ->semantics.annotations.front()
                    .dates.size() == 1,
            "Text date lost");
  }
  int result = 1, phase = 0;
  sigc::connection timer;
  ui::MainWindow *window{};
  std::optional<ui::ViewState> writing_origin;
  // Parallel normal/sanitized runs must not forward activation over D-Bus to
  // another fixture's window. This flag belongs to the driver, not production.
  auto app = Gtk::Application::create("org.inde.removal.assistant.qa",
                                      Gio::Application::Flags::NON_UNIQUE);
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(std::filesystem::path(argv[1]));
    app->add_window(*window);
    window->present();
    timer = Glib::signal_timeout().connect(
        [&] {
          try {
            if (!window->get_mapped())
              return true;
            auto *writing = find<ui::WritingWorkspace>(
                *window, [](auto &) { return true; });
            auto *planning = find<ui::PlanningWorkspace>(
                *window, [](auto &) { return true; });
            require(writing && planning, "Production workspaces missing");
            auto modal = [&]() -> Gtk::Box & {
              auto *box = find<Gtk::Box>(*window, [](auto &b) {
                return b.has_css_class("modal-card") && b.get_mapped();
              });
              require(box, "Modal missing");
              return *box;
            };
            auto check = [&](Gtk::Widget &root, const std::string &name) {
              auto *c = find<Gtk::CheckButton>(
                  root, [&](auto &b) { return b.get_label().raw() == name; });
              require(c && c->get_sensitive(), "Checkbox missing: " + name);
              c->set_active(true);
            };
            if (restart) {
              result = 0;
              std::cout << "PASS temporal removal assistant restart\n";
              window->close();
              return false;
            }
            switch (phase) {
            case 0:
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 "win.show-writing", nullptr),
                      "Writing action missing");
              writing->reveal_document(document);
              writing->reveal_usage_offset(600);
              break;
            case 1:
              writing_origin = writing->capture_view();
              ui::confirm_project_removal(*window, observed,
                                          project::RemovalKind::CalendarSystem,
                                          calendar, [] {});
              break;
            case 2: {
              auto *ack = find<Gtk::CheckButton>(modal(), [](auto &v) {
                return v.get_label() == "Conferi os efeitos desta operação";
              });
              require(ack && !ack->get_sensitive(),
                      "Blocked deletion is enabled");
              require(find<Gtk::Label>(
                          modal(),
                          [&](auto &v) {
                            return v.get_text().find(
                                       "QA Auxiliar — eixo de testes") !=
                                   Glib::ustring::npos;
                          }),
                      "Named axis missing");
              std::cout << "SNAPSHOT: assistant-named-usages" << std::endl;
              break;
            }
            case 3:
              click(modal(), "Próximos usos");
              break;
            case 4: {
              auto *search =
                  find<Gtk::SearchEntry>(modal(), [](auto &) { return true; });
              require(search, "Usage filter missing");
              search->set_text("eixo de testes");
              break;
            }
            case 5:
              click(modal(), "Abrir local do uso");
              break;
            case 6: {
              auto *exp = find<Gtk::Expander>(*planning, [](auto &e) {
                return e.get_label() == "Desvincular calendário da linha do tempo…";
              });
              require(exp, "Sole calendar detach unavailable");
              exp->set_expanded(true);
              require(world.calendars().size() != 1 ||
                          !find<Gtk::Button>(*planning,
                                             [](auto &b) {
                                               return b.get_mapped() &&
                                                      b.get_label() ==
                                                          "Confirmar adoção";
                                             }),
                      "Sole-calendar configuration exposes an empty successor "
                      "form");
              std::cout << "SNAPSHOT: assistant-detach-sole-calendar"
                        << std::endl;
              break;
            }
            case 7:
              check(*planning,
                    "Entendi a mudança de exibição; preservar fatos e datas");
              click(*planning, "Desvincular calendário");
              break;
            case 8:
              require(!world.binding(axis) &&
                          world.temporal_references(axis).size() == 14,
                      "Detach removed references or failed");
              require(observed.planning().time_point(point)->ordinal == 42,
                      "Detach changed point ordinal");
              click(*window, "Voltar à exclusão");
              break;
            case 9:
              require(writing->current_document_id() == document,
                      "Return lost writing document");
              require(writing->capture_view().at("cursor").as_integer() ==
                          writing_origin->at("cursor").as_integer(),
                      "Return lost writing cursor");
              require(!find<Gtk::Label>(modal(),
                                        [](auto &v) {
                                          return v.get_text().find(
                                                     "calendário de origem") !=
                                                 Glib::ustring::npos;
                                        }),
                      "Stale binding remains in assistant");
              std::cout << "SNAPSHOT: assistant-after-detach" << std::endl;
              break;
            case 10:
              click(modal(), "Cancelar / fechar");
              break;
            case 11: {
              auto p = observed.removal_impact(
                  project::RemovalKind::CalendarSystem, calendar);
              auto it =
                  std::find_if(p.uses.begin(), p.uses.end(), [](const auto &u) {
                    return u.table == "semantic_date_bindings";
                  });
              require(it != p.uses.end() && it->text_offset == 100,
                      "Text source/offset missing");
              window->navigate_removal_use(p, *it, [] {});
              break;
            }
            case 12:
              require(writing->current_document_id() == document,
                      "Wrong document opened");
              require(writing->capture_view().at("cursor").as_integer() == 100,
                      "Wrong text offset");
              std::cout << "SNAPSHOT: assistant-exact-text-use" << std::endl;
              break;
            case 13:
              click(modal(), "Voltar às camadas");
              break;
            case 14:
              click(*window, "Voltar à exclusão");
              break;
            case 15:
              click(modal(), "Cancelar / fechar");
              break;
            case 16:
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 "win.show-planning", nullptr),
                      "Planning action missing");
              planning->reveal_time_point(point);
              break;
            case 17:
              require(find<Gtk::Label>(*planning,
                                       [](auto &v) {
                                         return v.get_mapped() &&
                                                v.get_text().find(
                                                    "Sem calendário · pontos "
                                                    "em ordem relativa") !=
                                                    Glib::ustring::npos;
                                       }),
                      "Integrated calendar summary missing");
              require(find<Gtk::Button>(*planning,
                                        [](auto &v) {
                                          return v.get_mapped() &&
                                                 v.get_label() ==
                                                     "Sistemas e datas…";
                                        }),
                      "Temporal library entry missing in chronology");
              std::cout << "SNAPSHOT: assistant-integrated-fictional-time"
                        << std::endl;
              break;
            case 18: {
              auto base = *world.assign_calendar(axis, calendar, 0, {});
              auto c = project::calendar_template("julian");
              c.name = "QA Auxiliar — calendário sucessor";
              c = world.create_calendar(c);
              (void)world.adopt_calendar({{}, axis, c.id, 10, 0, "coexist"},
                                         base, {});
              click(*planning, "Configurar calendário…");
              break;
            }
            case 19: {
              auto *exp = find<Gtk::Expander>(*planning, [](auto &e) {
                return e.get_label() == "Rever a última adoção…";
              });
              require(exp, "Adoption cleanup missing");
              exp->set_expanded(true);
              std::cout << "SNAPSHOT: assistant-adoption-cleanup" << std::endl;
              break;
            }
            case 20:
              check(*planning,
                    "Entendi a mudança de exibição; preservar fatos e datas");
              click(*planning, "Remover última adoção");
              break;
            case 21: {
              require(world.adoptions(axis).empty(), "Adoption removal failed");
              auto *exp = find<Gtk::Expander>(*planning, [](auto &e) {
                return e.get_label() == "Desvincular calendário da linha do tempo…";
              });
              require(exp, "Cleanup did not unlock origin detach");
              exp->set_expanded(true);
              check(*planning,
                    "Entendi a mudança de exibição; preservar fatos e datas");
              click(*planning, "Desvincular calendário");
              break;
            }
            case 22:
              require(!world.binding(axis) &&
                          world.temporal_references(axis).size() == 14,
                      "Final preserved data mismatch");
              require(observed.writing()
                              .document(document)
                              ->semantics.annotations.front()
                              .dates.size() == 1,
                      "Text lost its date");
              result = 0;
              std::cout
                  << "PASS temporal removal assistant: blocked named uses, "
                     "pagination/filter, location, explicit detach, preserved "
                     "dates/ordinals/text, exact text tool, return cursor, "
                     "integrated chronology, adoption cleanup\n";
              window->close();
              return false;
            }
            ++phase;
            return true;
          } catch (const std::exception &e) {
            std::cerr << "FAIL assistant phase=" << phase << ": " << e.what()
                      << std::endl;
            window->close();
            return false;
          }
        },
        1800);
  });
  char *args[] = {argv[0], nullptr};
  app->run(1, args);
  timer.disconnect();
  delete window;
  return result;
}

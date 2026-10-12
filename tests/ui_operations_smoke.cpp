#include "../src/ui/rendering_policy.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/project_operations.hpp"
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
  application::ProjectService observed;
  observed.open(argv[1]);
  const auto prefs =
      ui::project_preference_group(observed.current()->manifest().project_id);
  std::string id, deletion;
  project::Document expected;
  if (!restart) {
    auto d = observed.writing().create_document("QA — Documento reversível");
    d.content = "Água 🙂 encontra memória.\nSegundo parágrafo.";
    d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}}};
    expected = observed.writing().update_document(d);
    id = d.id;
    for (int i = 0; i < 28; ++i)
      static_cast<void>(observed.writing().create_document_group(
          "QA histórico " + std::to_string(i), ""));
    ui::set_ui_preference(prefs, "qa-operation-document", id);
  } else {
    id = ui::ui_preference(prefs, "qa-operation-document");
    deletion = ui::ui_preference(prefs, "qa-deletion-operation");
    require(observed.writing().document(id).has_value(),
            "Restored Document missing after restart");
    require(!observed.reversal_impact(deletion).allowed,
            "Reverted operation became reversible again");
  }
  auto app = Gtk::Application::create("io.github.inde.operations-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::configure_rendering_backend();
  ui::MainWindow *window{};
  int step{}, result = 1;
  sigc::connection timer;
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(argv[1]);
    window->set_default_size(1000, 720);
    app->add_window(*window);
    window->present();
    timer = Glib::signal_timeout().connect(
        [&] {
          try {
            std::cout << "STEP: " << step << std::endl;
            auto *writing = find<ui::WritingWorkspace>(
                *window, [](auto &) { return true; });
            require(writing, "Writing workspace missing");
            auto action = [&](const char *n) {
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()), n,
                                                 nullptr),
                      n);
            };
            auto modal = [&]() {
              auto *p = find<Gtk::Box>(*window, [](auto &b) {
                return b.has_css_class("modal-card") && b.get_mapped();
              });
              require(p, "Modal missing");
              return p;
            };
            auto ack = [&]() {
              auto *p = find<Gtk::CheckButton>(*modal(), [](auto &c) {
                return c.get_label() == "Conferi os efeitos desta operação";
              });
              require(p, "Acknowledgment missing");
              return p;
            };
            auto choose_deletion = [&]() {
              auto *p = find<Gtk::Button>(*modal(), [](auto &b) {
                return find<Gtk::Label>(b, [](auto &l) {
                         return l.get_text().find("Remover Documento") !=
                                Glib::ustring::npos;
                       }) != nullptr;
              });
              require(p, "Deletion journal row missing");
              g_signal_emit_by_name(p->gobj(), "clicked");
            };
            auto keyboard_cycle = [&] {
              auto *layer = find<Gtk::Box>(*window, [](auto &b) {
                return b.has_css_class("modal-scrim") && b.get_mapped();
              });
              require(layer, "Modal keyboard layer missing");
              auto *controllers =
                  gtk_widget_observe_controllers(GTK_WIDGET(layer->gobj()));
              GObject *key{};
              for (guint i = 0; i < g_list_model_get_n_items(controllers);
                   ++i) {
                auto *item = G_OBJECT(g_list_model_get_item(controllers, i));
                if (GTK_IS_EVENT_CONTROLLER_KEY(item)) {
                  key = item;
                  break;
                }
                g_object_unref(item);
              }
              g_object_unref(controllers);
              require(key, "Modal keyboard controller missing");
              for (int i = 0; i < 10; ++i) {
                auto *before = window->get_focus();
                gboolean handled{};
                g_signal_emit_by_name(key, "key-pressed", GDK_KEY_Tab, 0,
                                      static_cast<GdkModifierType>(0),
                                      &handled);
                auto *after = window->get_focus();
                require(handled && after && after->get_mapped() &&
                            after != before,
                        "Tab stuck on a hidden/inactive history page");
              }
              g_object_unref(key);
            };
            if (restart) {
              if (step == 0)
                action("win.project-operations");
              else if (step == 1) {
                choose_deletion();
                require(!ack()->get_active() && !ack()->get_sensitive(),
                        "Reverted operation should be blocked");
                std::cout << "SNAPSHOT: operations-restart-blocked"
                          << std::endl;
              } else {
                result = 0;
                std::cout << "PASS operations actual restart and no inherited "
                             "approval"
                          << std::endl;
                window->close();
                return false;
              }
            } else if (step == 0) {
              action("win.show-writing");
              writing->reveal_document(id);
            } else if (step == 1) {
              click(*writing, "Excluir");
            } else if (step == 2) {
              auto *b = find<Gtk::Button>(*modal(), [](auto &b) {
                return b.get_label() == "Excluir e registrar";
              });
              require(b && !b->get_sensitive() && !ack()->get_active(),
                      "Removal not gated");
              std::cout << "SNAPSHOT: operations-removal-light" << std::endl;
            } else if (step == 3) {
              click(*modal(), "Cancelar / fechar");
              require(project::same_document_state(
                          *observed.writing().document(id), expected),
                      "Cancel altered source");
            } else if (step == 4) {
              click(*writing, "Excluir");
              ack()->set_active(true);
              expected.content += " Alteração concorrente preservada.";
              expected = observed.writing().update_document(expected);
              click(*modal(), "Excluir e registrar");
              require(observed.writing().document(id).has_value() &&
                          !ack()->get_active(),
                      "Stale preview committed");
              require(find<Gtk::Label>(*modal(),
                                       [](auto &l) {
                                         return l.get_text().find(
                                                    "Não confirmado:") !=
                                                Glib::ustring::npos;
                                       }),
                      "Conflict unexplained");
            } else if (step == 5) {
              click(*modal(), "Atualizar prévia");
              require(!ack()->get_active(), "Refresh carried approval");
              ack()->set_active(true);
              click(*modal(), "Excluir e registrar");
              require(!observed.writing().document(id),
                      "Confirmed removal missing");
              deletion = observed.project_operations().front().id;
              ui::set_ui_preference(prefs, "qa-deletion-operation", deletion);
            } else if (step == 6) {
              action("win.project-operations");
            } else if (step == 7) {
              choose_deletion();
              require(!ack()->get_active(), "Automatic reversal approval");
              std::cout << "SNAPSHOT: operations-reversal-light" << std::endl;
            } else if (step == 8) {
              action("win.theme-dark");
            } else if (step == 9) {
              keyboard_cycle();
              require(window->get_width() <= 1020,
                      "History forced wide window");
              std::cout << "SNAPSHOT: operations-reversal-dark" << std::endl;
            } else if (step == 10) {
              ack()->set_active(true);
              click(*modal(), "Reverter operação do Projeto");
              require(project::same_document_state(
                          *observed.writing().document(id), expected),
                      "Restoration incomplete");
            } else if (step == 11) {
              click(*modal(), "Voltar ao histórico");
              click(*modal(), "Próximas");
              click(*modal(), "Anteriores");
              std::cout << "SNAPSHOT: operations-history-dark" << std::endl;
            } else if (step == 12) {
              click(*modal(), "Fechar");
              writing->reveal_document(id);
              auto *t = find<Gtk::TextView>(*writing, [](auto &t) {
                return t.get_editable() && t.get_mapped();
              });
              require(t &&
                          t->get_buffer()->get_text().raw() == expected.content,
                      "Editor retained pre-reversal source");
              auto end = t->get_buffer()->end();
              t->get_buffer()->insert(end, " Nova escrita.");
            } else if (step == 13) {
              writing->flush_changes();
              require(observed.writing().document(id)->content ==
                          expected.content + " Nova escrita.",
                      "Autosave replayed stale source");
              require(observed.project_operations().front().compensation,
                      "Text autosave entered Project journal");
              expected = *observed.writing().document(id);
              static_cast<void>(observed.writing().create_editorial_revision(
                  expected, "Fonte protegida"));
            } else if (step == 14) {
              click(*writing, "Excluir");
              require(!ack()->get_sensitive(), "Revision source not protected");
              std::cout << "SNAPSHOT: operations-source-protected" << std::endl;
            } else if (step == 15) {
              click(*modal(), "Cancelar / fechar");
            } else {
              result = 0;
              std::cout << "PASS operations cancel, stale preview, explicit "
                           "approval, complete restoration, pagination, "
                           "themes, fresh editor baseline, pure-text exclusion "
                           "and revision source guard"
                        << std::endl;
              window->close();
              return false;
            }
            ++step;
            return true;
          } catch (const std::exception &e) {
            std::cerr << "FAIL step=" << step << ": " << e.what() << std::endl;
            window->close();
            return false;
          }
        },
        1300);
  });
  char *application_argv[] = {argv[0], nullptr};
  app->run(1, application_argv);
  timer.disconnect();
  delete window;
  return result;
}

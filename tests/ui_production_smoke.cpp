#include "../src/ui/rendering_policy.hpp"
#include "inde/persistence/json.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/saved_workspaces.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "ui/widget_probe.hpp"
#include <fstream>
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
  const auto destination = fixture.parent_path() / "production-output";
  if (!restart)
    std::filesystem::create_directory(destination);
  std::string work, node, first, second, project_id, original;
  {
    application::ProjectService seed;
    seed.open(fixture);
    project_id = seed.current()->manifest().project_id;
    if (!restart) {
      const auto ip = seed.create_intellectual_property(
                              "QA-Produção-" + project::new_uuid(), "", "")
                          .id;
      work = seed.create_work(ip, "Obra de produção", "", "", "pt-BR",
                              "Planejamento")
                 .id;
      for (int i = 0; i < 32; ++i) {
        const auto n =
            seed.create_structural_node(work, std::nullopt,
                                        project::StructuralNodeType::Chapter,
                                        "Unidade " + std::to_string(i + 1))
                .id;
        if (i == 0)
          node = n;
      }
      auto d = seed.writing().create_document("Primeiro texto", node);
      d.content = "Água 🙂 encontra memória.\r\nTexto de produção.";
      d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}}};
      d = seed.writing().update_document(d);
      first = d.id;
      original = d.content;
      d = seed.writing().create_document("Segundo texto", node);
      d.content = "O segundo deve aparecer primeiro na saída escolhida.";
      second = seed.writing().update_document(d).id;
    } else {
      const auto contexts = ui::saved_workspaces(project_id);
      require(contexts.size() == 1, "Production workspace not saved");
      const auto v = ui::parse_view_state(contexts[0].state)
                         .at("editorial")
                         .at("production");
      work = v.at("work").as_string();
      const auto &c = *v.at("choices").as_object().begin();
      node = c.first;
      second = c.second.at("documents").as_array()[0].as_string();
      first = c.second.at("documents").as_array()[1].as_string();
      original = seed.writing().document(first)->content;
    }
    seed.close();
  }
  auto app = Gtk::Application::create("io.github.inde.production-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::configure_rendering_backend();
  ui::MainWindow *window{};
  int step{}, result = 1, waiting{};
  sigc::connection timer;
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(fixture);
    window->set_default_size(1000, 720);
    app->add_window(*window);
    window->present();
    timer = Glib::signal_timeout().connect(
        [&] {
          try {
            auto *manuscript = find<ui::EditorialStructureWorkspace>(
                *window, [](auto &) { return true; });
            auto *production =
                find<ui::ProductionPage>(*window, [](auto &) { return true; });
            require(manuscript && production, "Missing production views");
            auto action = [&](const char *n) {
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()), n,
                                                 nullptr),
                      n);
            };
            auto modal = [&] {
              auto *d = find<Gtk::Box>(*window, [](auto &b) {
                return b.has_css_class("modal-card") && b.get_mapped();
              });
              require(d, "Missing modal");
              return d;
            };
            auto choose = [&] {
              auto *b = find<Gtk::Button>(*production, [&](auto &b) {
                return b.get_label() == "Escolher textos" &&
                       b.get_tooltip_text().raw() == node;
              });
              require(b, "Unit picker missing");
              g_signal_emit_by_name(b->gobj(), "clicked");
            };
            auto add = [&](const std::string &id) {
              auto *c = find<Gtk::ComboBoxText>(*modal(),
                                                [](auto &) { return true; });
              require(c && c->set_active_id(id), "Document choice missing");
              click(*modal(), "Adicionar texto");
            };
            auto check_selection = [&] {
              const auto v = production->capture_view();
              const auto &ds =
                  v.at("choices").at(node).at("documents").as_array();
              require(ds.size() == 2 && ds[0].as_string() == second &&
                          ds[1].as_string() == first,
                      "Explicit text order lost");
            };
            auto check_preview = [&] {
              auto *t = find<Gtk::TextView>(*production, [](auto &v) {
                return v.get_mapped() && !v.get_editable();
              });
              require(t, "Missing readonly preview");
              const auto s = t->get_buffer()->get_text().raw();
              require(s.find("O segundo") < s.find("Água"),
                      "Preview ignored explicit order");
              require(s.find(original) != std::string::npos,
                      "Preview changed exact source text");
            };
            if (restart) {
              if (step == 0) {
                action("win.show-editorial");
                require(
                    pages(*manuscript, "production").get_visible_child_name() ==
                        "production",
                    "Production page lost after restart");
                check_selection();
                click(*production, "Preparar prévia");
              } else if (step == 1) {
                check_preview();
                require(find<Gtk::CheckButton>(
                            *production,
                            [](auto &c) {
                              return c.get_label() == "Conferi a seleção e as "
                                                      "perdas desta saída" &&
                                     !c.get_active();
                            }),
                        "Approval was inferred across restart");
                std::cout << "SNAPSHOT: production-restart" << std::endl;
              } else {
                result = 0;
                std::cout
                    << "PASS production selection and context actual restart"
                    << std::endl;
                window->close();
                return false;
              }
            } else if (step == 0) {
              action("win.show-editorial");
              manuscript->reveal_work(work);
              click(*manuscript, "Produzir obra…");
            } else if (step == 1) {
              require(window->get_width() <= 1020,
                      "Production forced wide window");
              require(
                  production->capture_view().at("choices").as_object().empty(),
                  "Automatic document selection");
              click(*production, "Próximas unidades");
            } else if (step == 2) {
              require(production->capture_view().at("offset").as_integer() ==
                          30,
                      "Unit paging failed");
              click(*production, "Unidades anteriores");
              choose();
              add(first);
            } else if (step == 3) {
              click(*modal(), "Cancelar");
              require(
                  production->capture_view().at("choices").as_object().empty(),
                  "Cancelled choice was saved");
              choose();
              add(first);
              add(second);
            } else if (step == 4) {
              auto *up = find<Gtk::Button>(*modal(), [](auto &b) {
                return b.get_label() == "↑" && b.get_sensitive();
              });
              require(up, "Reorder missing");
              g_signal_emit_by_name(up->gobj(), "clicked");
            } else if (step == 5) {
              click(*modal(), "Confirmar seleção");
              check_selection();
              std::cout << "SNAPSHOT: production-selection" << std::endl;
            } else if (step == 6) {
              click(*production, "Preparar prévia");
            } else if (step == 7) {
              check_preview();
              auto *button = find<Gtk::Button>(*production, [](auto &b) {
                return b.get_label() == "Escolher destino e exportar";
              });
              require(button && !button->get_sensitive(),
                      "Loss acknowledgment not required");
              std::cout << "SNAPSHOT: production-preview-light" << std::endl;
            } else if (step == 8) {
              action("win.theme-dark");
            } else if (step == 9) {
              check_preview();
              std::cout << "SNAPSHOT: production-preview-dark" << std::endl;
            } else if (step == 10) {
              auto *c = find<Gtk::CheckButton>(*production, [](auto &b) {
                return b.get_label() ==
                       "Conferi a seleção e as perdas desta saída";
              });
              require(c, "Missing acknowledgement");
              c->set_active(true);
              click(*production, "Escolher destino e exportar");
            } else if (step == 11) {
              // Exercise the real native folder picker, no private export
              // hooks.
              auto *windows = gtk_window_get_toplevels();
              bool found{};
              for (guint i = 0; i < g_list_model_get_n_items(windows); ++i) {
                auto *item = g_list_model_get_item(windows, i);
                if (GTK_IS_FILE_CHOOSER(item) && GTK_IS_DIALOG(item)) {
                  gtk_dialog_response(GTK_DIALOG(item), GTK_RESPONSE_CANCEL);
                  found = true;
                }
                g_object_unref(item);
              }
              require(found, "Native folder chooser not observable");
            } else if (step == 12) {
              click(*production, "Escolher destino e exportar");
            } else if (step == 13 || step == 14) {
              auto *windows = gtk_window_get_toplevels();
              bool found{};
              for (guint i = 0; i < g_list_model_get_n_items(windows); ++i) {
                auto *item = g_list_model_get_item(windows, i);
                if (GTK_IS_FILE_CHOOSER(item) && GTK_IS_DIALOG(item)) {
                  if (step == 13) {
                    auto *folder = g_file_new_for_path(destination.c_str());
                    GError *error{};
                    const bool ok = gtk_file_chooser_set_current_folder(
                        GTK_FILE_CHOOSER(item), folder, &error);
                    g_object_unref(folder);
                    if (error)
                      g_error_free(error);
                    require(ok, "Native folder selection failed");
                  } else
                    gtk_dialog_response(GTK_DIALOG(item), GTK_RESPONSE_ACCEPT);
                  found = true;
                }
                g_object_unref(item);
              }
              require(found, "Native accept dialog not observable");
            } else if (step == 15) {
              auto *done = find<Gtk::Label>(*production, [](auto &l) {
                return l.get_text().raw().starts_with(
                    "Produção gravada e conferida:");
              });
              if (!done && ++waiting < 20)
                return true;
              require(done, "Production worker did not confirm readback");
              std::size_t completed{};
              for (const auto &folder :
                   std::filesystem::directory_iterator(destination)) {
                const auto p = folder.path() / "relatorio.json";
                if (!std::filesystem::exists(p))
                  continue;
                std::ifstream in(p);
                const auto report = persistence::json::parse(
                    std::string((std::istreambuf_iterator<char>(in)), {}));
                require(report.at("complete").as_boolean() &&
                            report.at("readback_verified").as_boolean(),
                        "Invalid production seal");
                require(
                    std::filesystem::exists(folder.path() / "obra.pdf") &&
                        std::filesystem::exists(folder.path() / "obra.docx") &&
                        std::filesystem::exists(folder.path() / "obra.epub"),
                    "Professional output absent");
                ++completed;
              }
              require(completed == 1,
                      "Duplicate/missing export after native chooser");
              application::ProjectService probe;
              probe.open(fixture);
              require(probe.writing().document(first)->content == original,
                      "Production mutated authorial text");
              probe.close();
              std::cout << "SNAPSHOT: production-complete" << std::endl;
            } else if (step == 16) {
              action("win.saved-workspaces");
              auto *e = find<Gtk::Entry>(*modal(), [](auto &e) {
                return e.get_placeholder_text() == "Nome do novo espaço";
              });
              require(e, "Missing workspace name");
              e->set_text("Produção da obra");
              click(*modal(), "Salvar este espaço de trabalho");
            } else if (step == 17) {
              click(*modal(), "Fechar");
              action("win.show-writing");
            } else if (step == 18) {
              action("win.show-editorial");
              check_selection();
              click(*production, "Preparar prévia");
            } else if (step == 19) {
              check_preview();
              action("win.close-project");
              click(*window, fixture.stem().string());
            } else if (step == 20) {
              require(
                  pages(*manuscript, "production").get_visible_child_name() ==
                      "production",
                  "Production page lost on reopen");
              check_selection();
              click(*production, "Preparar prévia");
            } else if (step == 21) {
              check_preview();
              result = 0;
              std::cout
                  << "PASS production paging, explicit order, cancel, preview, "
                     "themes, native destination cancel/accept, worker "
                     "readback, workspace and reopen"
                  << std::endl;
              window->close();
              return false;
            }
            ++step;
            return true;
          } catch (const std::exception &e) {
            std::cerr << "FAIL step=" << step << " " << e.what() << std::endl;
            window->close();
            return false;
          }
        },
        1200);
  });
  char *args[] = {argv[0], nullptr};
  app->run(1, args);
  timer.disconnect();
  delete window;
  return result;
}

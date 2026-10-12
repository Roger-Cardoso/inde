#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/application_theme.hpp"
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
  std::string expected_text = "Texto salvo do construtor — Água 🙂";
  for(int i=0;i<400;++i)expected_text += "\nEscrita longa — Água 🙂, linha " + std::to_string(i);
  std::string work, structure, unit, doc, project_id;
  {
    application::ProjectService seed;
    seed.open(fixture);
    project_id = seed.current()->manifest().project_id;
    if (!restart) {
      auto ip = seed.create_intellectual_property("QA Builder", "", "").id;
      work = seed.create_work(ip, "QA Builder Obra", "", "", "pt-BR",
                              "Planejamento")
                 .id;
      structure =
          seed.structures().create_narrative(work, "QA Builder Montagem").id;
      auto line = seed.structures().create_line(structure, "Investigação",
                                                "Uma linha, outra ordem");
      unit = seed.structures()
                 .create_unit(structure, "", "QA Builder Unidade",
                              "Resultado antes da causa",
                              "Mostrar consequência", "Lia")
                 .id;
      seed.structures().add_unit_to_line(unit, line.id);
      (void)seed.structures().create_unit(structure, "", "Causa depois", "", "",
                                          "");
    } else {
      auto view = ui::parse_view_state(ui::ui_preference(
          ui::project_preference_group(project_id), "context-v1"));
      const auto builder = view.at("planning").at("builder");
      work = builder.at("work").as_string();
      structure = builder.at("structure").as_string();
      unit = builder.at("unit").as_string();
      auto state = seed.builder().snapshot(structure);
      require(state.realizations.size() == 1, "Restart realization lost");
      doc = state.realizations.front().document_id;
      require(seed.writing().document(doc)->content == expected_text,
              "Restart text lost");
      require(state.contexts.front().writing_position.has_value(),
              "Restart queue lost");
    }
  }
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.builder-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::MainWindow *window{};
  sigc::connection tick;
  int phase{}, result = 1;
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(fixture);
    app->add_window(*window);
    window->present();
    tick = Glib::signal_timeout().connect(
        [&] {
          try {
            if (!window->get_mapped())
              return true;
            auto *planning = find<ui::PlanningWorkspace>(
                *window, [](auto &) { return true; });
            auto *builder = find<ui::NarrativeStructureWorkspace>(
                *window, [](auto &) { return true; });
            auto *writing = find<ui::WritingWorkspace>(
                *window, [](auto &) { return true; });
            require(planning && builder && writing,
                    "Production workspace missing");
            auto action = [&](const char *name) {
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 name, nullptr),
                      name);
            };
            auto modal = [&]() -> Gtk::Box & {
              auto *card = find<Gtk::Box>(*window, [](auto &v) {
                return v.has_css_class("modal-card") && v.get_mapped();
              });
              require(card, "Modal missing");
              return *card;
            };
            auto expand = [&](const std::string &name) {
              auto *e = find<Gtk::Expander>(
                  *builder, [&](auto &v) { return v.get_label().raw() == name; });
              require(e, "Section missing: " + name);
              e->set_expanded(true);
            };
            auto field = [&](const std::string &name) -> Gtk::Entry & {
              auto *l = find<Gtk::Label>(modal(), [&](auto &v) {
                    return v.get_text().raw() == name && v.get_mnemonic_widget();
              });
              require(l, "Field missing: " + name);
              auto *e = dynamic_cast<Gtk::Entry *>(l->get_mnemonic_widget());
              require(e, "Entry missing");
              return *e;
            };
            auto state = [&] { return builder->capture_view(); };
            if (restart) {
              if (phase == 0) {
                require(planning->capture_view().at("page").as_string() ==
                            "narrative",
                        "Restart planning page lost");
                require(state().at("unit").as_string() == unit &&
                            state().at("page").as_string() == "unit" &&
                            state().at("order").as_string() == "writing",
                        "Restart builder selection/order lost");
                std::cout << "SNAPSHOT: builder-restart" << std::endl;
              } else {
                result = 0;
                std::cout << "PASS temporal builder restart" << std::endl;
                window->close();
                return false;
              }
            } else
              switch (phase) {
              case 0: {
                action("win.show-planning");
                click(*planning, "Plano narrativo");
                auto v = state().as_object();
                v["work"] = work;
                v["structure"] = structure;
                v["unit"] = unit;
                v["line"] = ui::ViewState{};
                v["page"] = "unit";
                v["order"] = "narrative";
                v["scroll"] = ui::scroll_state(0);
                builder->restore_view(v);
                break;
              }
              case 1: {
                require(state().at("unit").as_string() == unit,
                        "Unit selection not restored");
                std::cout << "SNAPSHOT: builder-simple" << std::endl;
                click(*builder, "Editar parte…");
                break;
              }
              case 2:
                field("Título").set_text("");
                click(modal(), "Salvar parte");
                break;
              case 3: {
                require(field("Função desta parte").get_text() ==
                            "Mostrar consequência",
                        "Validation discarded input");
                field("Título").set_text("QA Builder Unidade revista");
                click(modal(), "Salvar parte");
                break;
              }
              case 4:
                expand("Ordens e trabalho");
                break;
              case 5:
                click(*builder, "Adicionar à ordem de escrita");
                break;
              case 6:
                expand("Ordens e trabalho");
                break;
              case 7:
                click(*builder, "Trocar ordem");
                break;
              case 8: {
                require(state().at("order").as_string() == "writing",
                        "Named queue lens missing");
                click(*builder, "Escrever…");
                break;
              }
              case 9:
                field("Título do Documento").set_text("QA Builder Texto");
                std::cout << "SNAPSHOT: builder-create-document" << std::endl;
                click(modal(), "Criar e escrever");
                break;
              case 10: {
                doc = writing->current_document_id();
                require(!doc.empty(), "No linked Document opened");
                auto *editor = find<Gtk::TextView>(
                    *writing, [](auto &v) { return v.get_editable(); });
                require(editor, "Editor missing");
            editor->get_buffer()->set_text(expected_text);
                auto *back = find<Gtk::Button>(*window, [](auto &v) {
                  return v.get_label() == "Voltar ao planejamento";
                });
                require(back && back->get_mapped(),
                        "Return button not visible");
                std::cout << "SNAPSHOT: builder-writing" << std::endl;
                break;
              }
              case 11:
                click(*window, "Voltar ao planejamento");
                break;
              case 12: {
                require(state().at("unit").as_string() == unit &&
                            state().at("order").as_string() == "writing" &&
                            state().at("page").as_string() == "unit",
                        "Round-trip lost planning context");
                application::ProjectService check;
                check.open(fixture);
            require(check.writing().document(doc)->content == expected_text,
                        "Return did not save text");
                require(!check.writing().document(doc)->editorial_node_id,
                        "Builder changed publication order");
                require(check.builder().snapshot(structure).units.front().id ==
                            unit,
                        "Queue changed montage");
                std::cout << "SNAPSHOT: builder-return" << std::endl;
                expand("Documentos que desenvolvem esta parte");
                break;
              }
              case 13: {
                std::cout << "SNAPSHOT: builder-realization" << std::endl;
                auto *scroll = find<Gtk::ScrolledWindow>(*builder, [](auto &v) {
                  return find<Gtk::Expander>(v, [](auto &e) {
                           return e.get_label() ==
                                  "Documentos que desenvolvem esta parte";
                         }) != nullptr;
                });
                require(scroll, "Builder scroller missing");
                scroll->get_vadjustment()->set_value(
                    scroll->get_vadjustment()->get_upper());
                break;
              }
              case 14: {
                std::cout << "SNAPSHOT: builder-scrolled" << std::endl;
                // Deliberately change workspace and refresh before checking
                // restoration.
                action("win.show-writing");
                action("win.show-planning");
                break;
              }
              case 15: {
                require(state().at("unit").as_string() == unit &&
                            state().at("page").as_string() == "unit",
                        "Workspace refresh lost unit");
                result = 0;
                std::cout << "PASS temporal builder production flow"
                          << std::endl;
                window->close();
                return false;
              }
              }
            ++phase;
            return true;
          } catch (const std::exception &e) {
            std::cerr << "FAIL builder phase=" << phase << ": " << e.what()
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

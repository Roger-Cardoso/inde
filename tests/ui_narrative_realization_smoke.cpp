#include "../src/ui/rendering_policy.hpp"
#include "../src/ui/theme_compatibility.hpp"
#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/main_window.hpp"
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
  std::string work, structure, a, b, c, doc, composition, occurrence, anchor,
      independent;
  std::size_t start{}, end{};
  {
    application::ProjectService s;
    s.open(fixture);
    if (!restart) {
      work = s.create_work(
                  s.create_intellectual_property("QA Realização", "", "").id,
                  "QA Realização Obra", "", "", "pt-BR", "Planejamento")
                 .id;
      structure =
          s.structures().create_narrative(work, "QA Realização Montagem").id;
      a = s.structures()
              .create_unit(structure, "", "A — consequência", "", "", "")
              .id;
      b = s.structures().create_unit(structure, "", "B — causa", "", "", "").id;
      c = s.structures()
              .create_unit(structure, "", "C — montagem", "", "", "")
              .id;
      auto d = s.writing().create_document("QA Realização longa");
      for (int i = 0; i < 500; ++i)
        d.content += "Linha " + std::to_string(i) + " — trabalho autoral.\n";
      start = g_utf8_strlen(d.content.c_str(), -1);
      d.content += "Água 🙂";
      end = start + 6;
      for (int i = 0; i < 120; ++i)
        d.content += "\nDepois " + std::to_string(i);
      d = s.writing().update_document(d);
      doc = d.id;
      s.builder().attach_range(s.builder().snapshot(structure), a, d, start,
                               end, "Alvo longo");
      d = *s.writing().document(doc);
      anchor = d.anchors.front().id;
      auto co = s.compositions().create("QA Realização Composição");
      occurrence = s.compositions().import(
          co.value, d, 0, project::utf8_character_count(d.content));
      co = s.compositions().save(co);
      composition = co.value.blocks.composition_id();
      s.builder().attach_occurrence(s.builder().snapshot(structure), c, co,
                                    occurrence);
      auto link = s.structures().create_link(
          structure, a, b, project::NarrativeLinkKind::Precedes,
          "Montagem declarada");
      s.builder().set_dependency_scope(s.builder().snapshot(structure), link.id,
                                       "narrative");
      s.builder().move_unit(s.builder().snapshot(structure), b,
                            application::BuilderOrder::Narrative, -1);
    } else {
      for (auto &w : s.catalog().works)
        if (w.title == "QA Realização Obra")
          work = w.id;
      for (auto &v : s.structures().narrative_structures(work))
        if (v.name == "QA Realização Montagem")
          structure = v.id;
      auto snap = s.builder().snapshot(structure);
      for (auto &u : snap.units) {
        if (u.title.starts_with("A —"))
          a = u.id;
        if (u.title.starts_with("B —"))
          b = u.id;
        if (u.title.starts_with("C —"))
          c = u.id;
      }
      require(snap.realizations.size() == 4, "Restart targets lost");
      require(std::any_of(snap.realizations.begin(), snap.realizations.end(),
                          [](auto &r) { return !r.provenance.empty(); }),
              "Restart independence provenance lost");
      require(std::any_of(snap.realizations.begin(), snap.realizations.end(),
                          [](auto &r) { return r.review_state == "diverged"; }),
              "Restart review decision lost");
      const auto notices = s.builder().continuity(snap);
      require(std::any_of(notices.begin(), notices.end(),
                          [](const auto &n) {
                            return n.exception_note ==
                                   "Não linearidade deliberada";
                          }),
              "Restart author exception lost");
    }
  }
  ui::configure_gtk_theme_base();
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.realization-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::MainWindow *window{};
  sigc::connection timer;
  int phase{}, result = 1, wait{}, modal_wait{}, scroll_wait{};
  ui::ViewState writing_origin, composition_origin;
  std::function<void()> deferred;
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(fixture);
    window->set_default_size(1000, 800);
    app->add_window(*window);
    window->present();
    timer = Glib::signal_timeout().connect(
        [&] {
          try {
            if (!window->get_mapped())
              return true;
            if (wait) {
              --wait;
              return true;
            }
            if (deferred) {
              auto f = std::move(deferred);
              deferred = {};
              f();
              return true; // let the modal host settle/re-present between
                           // responses
            }
            auto *planning = find<ui::PlanningWorkspace>(
                *window, [](auto &) { return true; });
            auto *builder = find<ui::NarrativeStructureWorkspace>(
                *window, [](auto &) { return true; });
            auto *writing = find<ui::WritingWorkspace>(
                *window, [](auto &) { return true; });
            auto *comp =
                find<ui::CompositionPage>(*window, [](auto &) { return true; });
            require(planning && builder && writing && comp,
                    "Production workspaces missing");
            auto action = [&](const char *name) {
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 name, nullptr),
                      name);
            };
            auto modal = [&]() -> Gtk::Box & {
              auto *m = find<Gtk::Box>(*window, [](auto &v) {
                return v.get_mapped() && v.has_css_class("modal-card");
              });
              require(m, "Modal missing");
              return *m;
            };
            auto later = [&](std::string name) {
              deferred = [window, name] {
                auto *m = find<Gtk::Box>(*window, [](auto &v) {
                  return v.get_mapped() && v.has_css_class("modal-card");
                });
                require(m, "Deferred modal missing");
                click(*m, name);
              };
            };
            auto selector = [&](const std::string &placeholder)
                -> ui::IncrementalSelector & {
              auto *p = find<ui::IncrementalSelector>(modal(), [&](auto &p) {
                return find<Gtk::SearchEntry>(p, [&](auto &e) {
                  return e.get_placeholder_text().raw() == placeholder;
                });
              });
              require(p, "Selector missing: " + placeholder);
              return *p;
            };
            auto unit_view = [&](const std::string &unit) {
              auto v = builder->capture_view().as_object();
              v["work"] = work;
              v["structure"] = structure;
              v["unit"] = unit;
              v["line"] = ui::ViewState{};
              v["page"] = "unit";
              v["order"] = "narrative";
              v["scroll"] = ui::scroll_state(0);
              builder->restore_view(v);
              auto *e = find<Gtk::Expander>(*builder, [](auto &e) {
                return e.get_label().raw() ==
                       "Documentos que desenvolvem esta parte";
              });
              require(e, "Realizations section missing");
              e->set_expanded(true);
            };
            auto open_target = [&] {
              auto *v = find<Gtk::Button>(*builder, [](auto &v) {
                return v.get_label().raw().starts_with("Abrir escrita:");
              });
              require(v && v->get_sensitive(), "Open target unavailable");
              g_signal_emit_by_name(v->gobj(), "clicked");
            };
            auto use_id = [&](const std::string &unit) {
              application::ProjectService s;
              s.open(fixture);
              for (const auto &r : s.builder().snapshot(structure).realizations)
                if (r.unit_id == unit)
                  return r.id;
              throw std::runtime_error("Source realization absent");
            };
            auto menu_target = [&](const std::string &name) {
              auto *m = find<Gtk::MenuButton>(*builder, [](auto &m) {
                return m.get_tooltip_text().raw() ==
                       "Ações deste texto associado";
              });
              require(m, "Realization menu missing");
              m->popup();
              click(*m->get_popover(), name);
            };
            auto editor = [&]() -> Gtk::TextView & {
              auto *v = find<Gtk::TextView>(*writing, [&](auto &v) {
                return v.get_mapped() && v.get_editable() &&
                       !v.is_ancestor(*comp);
              });
              require(v, "Document editor missing");
              return *v;
            };
            auto selected = [&] {
              Gtk::TextIter x, y;
              auto buf = editor().get_buffer();
              require(buf->get_selection_bounds(x, y), "Selection lost");
              return buf->get_text(x, y).raw();
            };
            if (restart) {
              if (phase == 0) {
                require(planning->capture_view().at("page").as_string() ==
                            "narrative",
                        "Restart page lost");
                require(builder->capture_view().at("unit").as_string() == b,
                        "Restart unit lost");
                std::cout << "SNAPSHOT: realization-restart" << std::endl;
                wait = 5;
              } else {
                result = 0;
                std::cout << "PASS temporal realization restart" << std::endl;
                window->close();
                return false;
              }
            } else
              switch (phase) {
              case 0:
                action("win.show-planning");
                click(*planning, "Plano narrativo");
                unit_view(a);
                wait = 4;
                break;
              case 1:
                std::cout << "SNAPSHOT: realization-unit" << std::endl;
                wait = 5;
                break;
              case 2:
                open_target();
                wait = 8;
                break;
              case 3:
                require(writing->current_document_id() == doc &&
                            selected() == "Água 🙂",
                        "Anchor navigation was not exact");
                writing_origin = writing->capture_view();
                if (ui::view_scroll(writing_origin.at("scroll")) <= 1000 &&
                    ++scroll_wait < 25)
                  return true; // bounded layout wait, keep the exact scroll assertion
                require(ui::view_scroll(writing_origin.at("scroll")) > 1000,
                        "Long target did not scroll");
                std::cout << "SNAPSHOT: realization-long-selection"
                          << std::endl;
                wait = 5;
                break;
              case 4: {
                // The origin is the position at departure, after the initial
                // anchor navigation/layout and screenshot have settled.
                writing_origin = writing->capture_view();
                auto *m = find<Gtk::MenuButton>(*writing, [](auto &m) {
                  return m.get_label().raw() == "Ferramentas";
                });
                require(m, "Tools missing");
                m->popup();
                click(*m->get_popover(), "Planejamento deste texto…");
                break;
              }
              case 5:
                selector("Pesquisar parte ou plano")
                    .set_selected(use_id(a), "A");
                later("Abrir parte no planejamento");
                break;
              case 6:
                require(builder->capture_view().at("unit").as_string() == a,
                        "Source unit wrong");
                click(*window, "Voltar à escrita");
                wait = 14;
                break;
              case 7: {
                require(selected() == "Água 🙂", "Return selection collapsed");
                const auto now = writing->capture_view();
                if (ui::view_offset(now.at("cursor")) !=
                        ui::view_offset(writing_origin.at("cursor")) ||
                    std::abs(ui::view_scroll(now.at("scroll")) -
                             ui::view_scroll(writing_origin.at("scroll"))) >= 5)
                  std::cerr
                      << "RETURN origin cursor="
                      << ui::view_offset(writing_origin.at("cursor"))
                      << " scroll="
                      << ui::view_scroll(writing_origin.at("scroll"))
                      << " current cursor=" << ui::view_offset(now.at("cursor"))
                      << " scroll=" << ui::view_scroll(now.at("scroll"))
                      << std::endl;
                require(ui::view_offset(now.at("cursor")) ==
                                ui::view_offset(writing_origin.at("cursor")) &&
                            std::abs(ui::view_scroll(now.at("scroll")) -
                                     ui::view_scroll(
                                         writing_origin.at("scroll"))) < 5,
                        "Return position changed");
                auto *p = find<Gtk::Popover>(editor(), [](auto &p) {
                  return p.has_css_class("context-menu");
                });
                require(p, "Context menu missing");
                p->popup();
                wait = 2;
                break;
              }
              case 8:
                click(editor(), "Associar seleção a parte da narrativa…");
                break;
              case 9: {
                auto choose = [&](const std::string &id) {
                  auto *co = find<Gtk::ComboBoxText>(
                      modal(), [&](auto &co) { return combo_has_id(co, id); });
                  require(co && co->set_active_id(id), "Scope choice missing");
                };
                choose(work);
                choose(structure);
                selector("Pesquisar parte nesta estrutura")
                    .set_selected(b, "B");
                later("Associar trecho");
                break;
              }
              case 10: {
                writing->flush_changes();
                application::ProjectService s;
                s.open(fixture);
                require(s.builder().snapshot(structure).realizations.size() ==
                            3,
                        "GUI association not committed");
                action("win.show-planning");
                unit_view(b);
                menu_target("Conferir texto e planejamento…");
                break;
              }
              case 11: {
                auto *cb = find<Gtk::CheckButton>(modal(), [](auto &v) {
                  return v.get_label().raw() == "O texto difere do plano por escolha minha";
                });
                require(cb, "Divergence control missing");
                cb->set_active(true);
                auto *label = find<Gtk::Label>(modal(), [](auto &v) {
                  return v.get_text().raw() == "Nota da conferência" &&
                         v.get_mnemonic_widget();
                });
                require(label, "Review note missing");
                auto *note =
                    dynamic_cast<Gtk::Entry *>(label->get_mnemonic_widget());
                require(note, "Review note field missing");
                note->set_text("Divergência intencional para teste autoral");
                std::cout << "SNAPSHOT: realization-review" << std::endl;
                wait = 5;
                later("Registrar conferência");
                break;
              }
              case 12:
                unit_view(b);
                menu_target("Conferir texto e planejamento…");
                break;
              case 13:
                later("Criar texto independente");
                break;
              case 14: {
                if (!find<Gtk::Box>(*window, [](const auto &v) {
                      return v.get_mapped() && v.has_css_class("modal-card");
                    })) {
                  require(++modal_wait < 25, "Validation form did not return");
                  return true; // bounded wait for idle re-presentation
                }
                modal_wait = 0;
                auto *cb = find<Gtk::CheckButton>(modal(), [](auto &v) {
                  return v.get_label().raw().starts_with("Se separar:");
                });
                require(cb && !cb->get_active(),
                        "Independence was not explicit");
                application::ProjectService s;
                s.open(fixture);
                require(s.builder().snapshot(structure).realizations.size() ==
                            3,
                        "Unconfirmed copy changed targets");
                cb->set_active(true);
                std::cout << "SNAPSHOT: realization-independence" << std::endl;
                wait = 5;
                later("Criar texto independente");
                break;
              }
              case 15: {
                unit_view(b);
                application::ProjectService s;
                s.open(fixture);
                for (auto &r : s.builder().snapshot(structure).realizations)
                  if (r.unit_id == b) {
                    independent = r.document_id;
                    require(!r.provenance.empty() && r.document_id != doc,
                            "Copy not independent");
                  }
                require(s.writing().document(independent)->content ==
                            s.writing().document(doc)->content,
                        "Copy lost source text");
                click(*builder, "Associar trecho ou uso de bloco…");
                break;
              }
              case 16: {
                auto *m = find<Gtk::ComboBoxText>(modal(), [](auto &m) {
                  return combo_has_id(m, "occurrence");
                });
                require(m && m->set_active_id("occurrence"),
                        "Mode unavailable");
                selector("Pesquisar montagem de texto")
                    .set_selected(composition, "Composição");
                selector("Pesquisar uso do bloco na montagem")
                    .set_selected(occurrence, "Quando acontece");
                later("Vincular alvo");
                break;
              }
              case 17:
                unit_view(c);
                open_target();
                wait = 6;
                break;
              case 18: {
                require(comp->capture_view().at("occurrence").as_string() ==
                            occurrence,
                        "Composition target wrong");
                auto *ed = find<Gtk::TextView>(*comp, [](auto &v) {
                  return v.get_mapped() && v.get_editable();
                });
                require(ed, "Occurrence editor missing");
                auto buf = ed->get_buffer();
                ed->grab_focus();
                buf->select_range(buf->get_iter_at_offset(end),
                                  buf->get_iter_at_offset(start));
                auto at = buf->get_iter_at_offset(start);
                ed->scroll_to(at, 0.2);
                wait = 8;
                break;
              }
              case 19:
                composition_origin = comp->capture_view();
                require(ui::view_scroll(composition_origin.at("scroll")) > 1000,
                        "Occurrence scroll absent");
                comp->signal_narrative_uses().emit();
                break;
              case 20:
                selector("Pesquisar parte ou plano")
                    .set_selected(use_id(c), "C");
                later("Abrir parte no planejamento");
                break;
              case 21:
                require(builder->capture_view().at("unit").as_string() == c,
                        "Occurrence source wrong");
                click(*window, "Voltar à escrita");
                wait = 14;
                break;
              case 22:
                require(comp->capture_view().at("occurrence").as_string() ==
                            occurrence,
                        "Occurrence return lost");
                require(
                    ui::view_offset(comp->capture_view().at("cursor")) ==
                            ui::view_offset(composition_origin.at("cursor")) &&
                        ui::view_offset(comp->capture_view().at("bound")) ==
                            ui::view_offset(composition_origin.at("bound")) &&
                        std::abs(
                            ui::view_scroll(comp->capture_view().at("scroll")) -
                            ui::view_scroll(composition_origin.at("scroll"))) <
                            5,
                    "Occurrence selection/scroll return lost");
                action("win.show-planning");
                unit_view(b);
                click(*builder, "Continuidade…");
                wait = 4;
                break;
              case 23:
                require(
                    pages(*builder, "continuity").get_visible_child_name() ==
                        "continuity",
                    "Continuity page missing");
                std::cout << "SNAPSHOT: realization-continuity" << std::endl;
                wait = 5;
                break;
              case 24:
                click(*builder, "Registrar escolha intencional…");
                break;
              case 25: {
                auto *note =
                    find<Gtk::Entry>(modal(), [](auto &) { return true; });
                require(note, "Exception note missing");
                note->set_text("Não linearidade deliberada");
                std::cout << "SNAPSHOT: realization-exception" << std::endl;
                wait = 5;
                later("Registrar decisão");
                break;
              }
              case 26: {
                application::ProjectService s;
                s.open(fixture);
                const auto notices =
                    s.builder().continuity(s.builder().snapshot(structure));
                require(std::any_of(notices.begin(), notices.end(),
                                    [](const auto &n) {
                                      return n.exception_note ==
                                             "Não linearidade deliberada";
                                    }),
                        "Author exception not committed from UI");
                break;
              }
              case 27:
                click(*builder, "← Planejamento");
                unit_view(b);
                wait = 3;
                break;
              case 28:
                menu_target("Conferir texto e planejamento…");
                wait = 2;
                break;
              default:
                // Deliberately close before a rejected validation's idle retry.
                // Neither a copy nor a stale Window callback may survive.
                click(modal(), "Criar texto independente");
                result = 0;
                std::cout
                    << "PASS temporal realization production UI: exact Unicode "
                       "anchor, long selection and scroll round-trip, GUI "
                       "association, author divergence, explicit independence, "
                       "occurrence selection and return, continuity and author "
                       "exception"
                    << std::endl;
                window->close();
                return false;
              }
            ++phase;
            return true;
          } catch (const std::exception &e) {
            std::cerr << "FAIL realization UI phase=" << phase << ": "
                      << e.what() << std::endl;
            window->close();
            return false;
          }
        },
        120);
  });
  const auto code = app->run();
  timer.disconnect();
  delete window;
  return code ? code : result;
}

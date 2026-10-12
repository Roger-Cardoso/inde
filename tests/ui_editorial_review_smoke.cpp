#include "../src/ui/rendering_policy.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/saved_workspaces.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "ui/widget_probe.hpp"
#include <cmath>
#include <iostream>
using namespace inde::test::ui;
using namespace inde;
void check_diff_contrast(Gtk::TextView &view) {
  const auto luminance = [](const Gdk::RGBA &c) {
    const auto channel = [](double v) {
      return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.get_red()) + 0.7152 * channel(c.get_green()) +
           0.0722 * channel(c.get_blue());
  };
  for (const auto *name : {"review-removed", "review-added"}) {
    const auto tag = view.get_buffer()->get_tag_table()->lookup(name);
    require(bool(tag), "Missing difference tag");
    const auto fg = luminance(tag->property_foreground_rgba().get_value()),
               bg = luminance(tag->property_background_rgba().get_value());
    const auto ratio = (std::max(fg, bg) + 0.05) / (std::min(fg, bg) + 0.05);
    require(ratio >= 4.5,
            "Diff text contrast below 4.5: " + std::to_string(ratio));
  }
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const bool restart = std::string(argv[2]) == "--verify-restart";
  if (!restart && std::string(argv[2]) != "--allow-fixture-writes")
    return 2;
  const std::filesystem::path fixture = argv[1];
  std::string doc, revision, project_id, original, title;
  std::size_t before_count{};
  {
    application::ProjectService seed;
    seed.open(fixture);
    project_id = seed.current()->manifest().project_id;
    if (!restart) {
      auto d =
          seed.writing().create_document("QA-Revisão-" + project::new_uuid());
      title = d.title;
      d.content = "Água 🙂 retorna.\nA ponte espera.\r\nFim";
      d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}}};
      d.perspective = "Mara";
      auto types = seed.writing().semantic_types();
      const auto *t =
          project::find_annotation_type(types, "inde.editor.comment", 1);
      require(t, "Missing comment type");
      using K = project::SemanticFieldKind;
      d = application::TextSemanticService::create(
          d, *t, 0, 4,
          {{"note", {K::Text, {"Examinar ritmo"}}},
           {"state", {K::Enum, {"open"}}}});
      d = seed.writing().update_document(d);
      doc = d.id;
      original = d.content;
      before_count = seed.writing().document_count();
    } else {
      const auto saved = ui::parse_view_state(ui::ui_preference(
          ui::project_preference_group(project_id), "context-v1"));
      require(saved.at("workspace").as_string() == "review" &&
                  saved.at("review").at("page").as_string() == "comparison",
              "Review continuity not saved");
      doc = saved.at("review").at("left").as_string();
      revision = saved.at("review").at("right").as_string();
      require(seed.writing().document(revision)->revision_of_id == doc,
              "Revision provenance lost");
      require(
          application::editorial_state(*seed.writing().document(doc)).outdated,
          "Outdated decision lost");
    }
    seed.close();
  }
  auto app = Gtk::Application::create("io.github.inde.editorial-review-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::configure_rendering_backend();
  ui::MainWindow *window{};
  int step{}, result = 1;
  sigc::connection tick;
  app->signal_activate().connect([&] {
    window = new ui::MainWindow(fixture);
    window->set_default_size(1000, 720);
    app->add_window(*window);
    window->present();
    tick = Glib::signal_timeout().connect(
        [&] {
          try {
            auto *writing = find<ui::WritingWorkspace>(
                *window, [](auto &) { return true; });
            auto *review =
                find<ui::ReviewWorkspace>(*window, [](auto &) { return true; });
            require(writing && review, "Missing production workspaces");
            auto *editor = find<Gtk::TextView>(
                *writing, [](auto &v) { return v.get_editable(); });
            require(editor, "Missing writer");
            auto action = [&](const char *name) {
              require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),
                                                 name, nullptr),
                      name);
            };
            auto modal = [&]() {
              auto *d = find<Gtk::Box>(*window, [](auto &v) {
                return v.has_css_class("modal-card") && v.get_mapped();
              });
              require(d, "Missing modal");
              return d;
            };
            auto persisted = [&](const std::string &id) {
              application::ProjectService read;
              read.open(fixture);
              auto d = read.writing().document(id);
              require(d.has_value(), "Missing persisted Document");
              return *d;
            };
            auto entry = [&](const std::string &placeholder,
                             const std::string &value) {
              auto *e = find<Gtk::Entry>(*modal(), [&](auto &v) {
                return v.get_placeholder_text().raw() == placeholder;
              });
              require(e, "Entry missing: " + placeholder);
              e->set_text(value);
            };
            auto more = [&](const std::string &name) {
              auto *m = find<Gtk::MenuButton>(*review, [](auto &v) {
                return v.get_label() == "Documento" && v.get_mapped();
              });
              require(m, "Missing Document menu");
              m->popup();
              click(*m->get_popover(), name);
            };
            if (restart) {
              if (step == 0) {
                require(
                    review->get_mapped() &&
                        pages(*review, "comparison").get_visible_child_name() ==
                            "comparison",
                    "Comparison not restored after process restart");
                require(
                    review->capture_view().at("differences_only").as_boolean(),
                    "Comparison filter not restored");
                auto *v = find<Gtk::TextView>(
                    *review, [](auto &v) { return v.get_mapped(); });
                require(v && !v->get_editable() &&
                            v->get_buffer()->get_text().raw().find(
                                "Outra linha") != std::string::npos,
                        "Readonly diff missing after restart");
                require(ui::saved_workspaces(project_id).size() == 1,
                        "Named review workspace lost");
                std::cout << "SNAPSHOT: review-restart" << std::endl;
                action("win.show-writing");
              } else if (step == 1) {
                require(writing->current_document_id() == revision,
                        "Revision writer context lost");
                require(editor->get_buffer()->get_text().raw() ==
                            persisted(revision).content,
                        "Revision body not restored");
                action("win.show-review");
              } else if (step == 2) {
                const auto state = review->capture_view();
                auto missing = state.as_object();
                missing["document"] = "missing";
                missing["left"] = "missing";
                missing["right"] = "missing";
                review->restore_view(missing);
                require(pages(*review, "queue").get_visible_child_name() ==
                            "queue",
                        "Missing comparison did not fall back safely");
                review->restore_view(state);
              } else {
                result = 0;
                std::cout << "PASS actual review process restart, "
                             "query/detail/comparison and fallback"
                          << std::endl;
                window->close();
                return false;
              }
            } else if (step == 0) {
              action("win.show-writing");
              writing->reveal_document(doc);
            } else if (step == 1) {
              auto b = editor->get_buffer();
              b->select_range(b->get_iter_at_offset(4),
                              b->get_iter_at_offset(0));
              action("win.writing-suggest");
            } else if (step == 2) {
              auto *v = find<Gtk::TextView>(
                  *modal(), [](auto &v) { return v.get_editable(); });
              require(v, "Missing proposal editor");
              v->get_buffer()->set_text("Memória");
              entry("Motivo da sugestão (opcional)", "Ritmo mais preciso");
              click(*modal(), "Guardar sugestão");
            } else if (step == 3) {
              require(persisted(doc).content == original &&
                          application::review_counts(persisted(doc))
                                  .pending_suggestions == 1,
                      "Proposal wrote body or was not saved");
              action("win.show-review");
            } else if (step == 4) {
              click(*review, title);
            } else if (step == 5) {
              require(pages(*review, "detail").get_visible_child_name() ==
                          "detail",
                      "No review detail");
              click(*review, "Resolver comentário");
            } else if (step == 6) {
              require(
                  application::review_counts(persisted(doc)).open_comments == 0,
                  "Resolve not durable");
              click(*review, "Reabrir comentário");
            } else if (step == 7) {
              require(
                  application::review_counts(persisted(doc)).open_comments == 1,
                  "Reopen not durable");
              click(*review, "Resolver comentário");
              click(*review, "Examinar e aplicar…");
              std::cout << "SNAPSHOT: suggestion-preview" << std::endl;
            } else if (step == 8) {
              click(*modal(), "Cancelar");
              require(persisted(doc).content == original,
                      "Cancel altered body");
              click(*review, "Examinar e aplicar…");
            } else if (step == 9) {
              click(*modal(), "Aplicar ao trecho");
            } else if (step == 10) {
              require(persisted(doc).content.find("Memória") == 0,
                      "Suggestion not applied");
              require(editor->get_buffer()->get_text().raw() ==
                          persisted(doc).content,
                      "Hidden writer has stale authority");
              click(*review, "Abrir texto");
            } else if (step == 11) {
              action("win.writing-undo");
              writing->flush_changes();
              require(persisted(doc).content == original &&
                          application::review_counts(persisted(doc))
                                  .pending_suggestions == 1,
                      "Undo did not restore text and proposal together");
            } else if (step == 12) {
              action("win.writing-redo");
              writing->flush_changes();
              require(persisted(doc).content.find("Memória") == 0 &&
                          application::review_counts(persisted(doc))
                                  .pending_suggestions == 0,
                      "Redo failed");
              action("win.show-review");
            } else if (step == 13) {
              more("Registrar estado editorial…");
            } else if (step == 14) {
              auto *s = find<Gtk::ComboBoxText>(*modal(),
                                                [](auto &) { return true; });
              require(s, "Missing status choices");
              s->set_active_id("approved");
              entry("Responsável pela decisão", "Editora QA");
              entry("Parecer ou orientação (opcional)",
                    "Passe aprovado neste estado");
              click(*modal(), "Registrar decisão");
            } else if (step == 15) {
              auto state = application::editorial_state(persisted(doc));
              require(state.state == "approved" && !state.outdated,
                      "Approval not current/durable");
              std::cout << "SNAPSHOT: review-approved" << std::endl;
            } else if (step == 16) {
              click(*review, "Abrir texto");
              auto b = editor->get_buffer();
              b->insert(b->end(), "!");
              writing->flush_changes();
            } else if (step == 17) {
              action("win.show-review");
            } else if (step == 18) {
              require(application::editorial_state(persisted(doc)).outdated,
                      "Approval survived changed body");
              require(find<Gtk::Label>(*review,
                                       [](auto &l) {
                                         return l.get_text().raw().find(
                                                    "decisão desatualizada") !=
                                                std::string::npos;
                                       }),
                      "Outdated approval not visible");
              more("Criar revisão independente…");
            } else if (step == 19) {
              click(*modal(), "Cancelar");
              application::ProjectService read;
              read.open(fixture);
              require(read.writing().document_count() == before_count,
                      "Cancel created revision");
              more("Criar revisão independente…");
            } else if (step == 20) {
              entry("Identificação da revisão", "Passe alternativo");
              click(*modal(), "Criar revisão");
            } else if (step == 21) {
              revision = review->capture_view().at("document").as_string();
              const auto r = persisted(revision);
              require(r.revision_of_id == doc &&
                          r.semantics.annotations.empty() && r.anchors.empty(),
                      "Revision fabricated bindings");
              require(r.content == persisted(doc).content && revision != doc,
                      "Revision source changed");
              click(*review, "Abrir texto");
            } else if (step == 22) {
              auto b = editor->get_buffer();
              b->insert(b->end(), "\nOutra linha da revisão 🙂\n");
              writing->flush_changes();
              action("win.show-review");
            } else if (step == 23) {
              click(*review, "Comparar versões…");
            } else if (step == 24) {
              click(*modal(), "Comparar");
            } else if (step == 25) {
              auto *v = find<Gtk::TextView>(
                  *review, [](auto &v) { return v.get_mapped(); });
              require(v && !v->get_editable() &&
                          v->get_buffer()->get_text().raw().find(
                              "Outra linha") != std::string::npos,
                      "Missing readonly diff");
              auto *c = find<Gtk::CheckButton>(*review, [](auto &v) {
                return v.get_label() == "Somente diferenças";
              });
              require(c, "Missing difference filter");
              c->set_active(true);
              click(*review, "Próxima diferença");
              action("win.theme-dark");
            } else if (step == 26) {
              auto *v = find<Gtk::TextView>(
                  *review, [](auto &v) { return v.get_mapped(); });
              require(v, "Missing comparison");
              check_diff_contrast(*v);
              require(window->get_width() <= 1010,
                      "Review forced an oversized window");
              std::cout << "SNAPSHOT: comparison-dark" << std::endl;
            } else if (step == 27) {
              action("win.theme-light");
            } else if (step == 28) {
              auto *v = find<Gtk::TextView>(
                  *review, [](auto &v) { return v.get_mapped(); });
              require(v, "Missing comparison");
              check_diff_contrast(*v);
              std::cout << "SNAPSHOT: comparison-light" << std::endl;
            } else if (step == 29) {
              action("win.saved-workspaces");
            } else if (step == 30) {
              entry("Nome do novo espaço", "Revisão da editora");
              click(*modal(), "Salvar este espaço de trabalho");
            } else if (step == 31) {
              click(*modal(), "Fechar");
              result = 0;
              std::cout << "PASS real editor/review transactions, "
                           "cancellation, durable states, undo/redo, "
                           "independent revision and readonly comparison"
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
  tick.disconnect();
  delete window;
  return result;
}

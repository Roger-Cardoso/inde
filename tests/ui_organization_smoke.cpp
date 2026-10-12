// Exercises the real production shell on a disposable copy, without private access.
#include "inde/ui/main_window.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/project/manifest.hpp"
#include "ui/widget_probe.hpp"
#include "../src/ui/rendering_policy.hpp"
#include <algorithm>
#include <iostream>

using namespace inde::test::ui;
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes") return 2;
  const std::filesystem::path fixture = argv[1];
  std::string work_a, work_b, node, doc, prefix, original;
  {
    inde::application::ProjectService seed; seed.open(fixture);
    prefix = "QA-Organização-" + inde::project::new_uuid();
    const auto ip = seed.create_intellectual_property(prefix, "", "").id;
    work_a = seed.create_work(ip, "Caderno de organização", "", "", "pt-BR", "Planejamento").id;
    work_b = seed.create_work(ip, "Outra obra da verificação — um título autoral deliberadamente muito longo que não deve ampliar a janela nem desaparecer do modelo", "", "", "pt-BR", "Planejamento").id;
    node = seed.create_structural_node(work_a, std::nullopt,
        inde::project::StructuralNodeType::Chapter, "Capítulo de organização").id;
    (void)seed.create_structural_node(work_a, node,
        inde::project::StructuralNodeType::Scene, "Cena interna preservada");
    auto document = seed.writing().create_document("Texto da unidade — organização", node);
    original = "Água, memória e organização.\nUm documento relacionado sem alterações implícitas.";
    document.content = original;
    doc = seed.writing().update_document(document).id;
    const auto types = seed.narrative().entity_types();
    const auto type = std::find_if(types.begin(), types.end(), [](const auto &t) { return t.key == "character"; });
    require(type != types.end(), "Missing character type");
    for (int i = 0; i < 108; ++i)
      (void)seed.narrative().create_entity(type->id, prefix + " " + std::to_string(i), "Fixture isolada");
    seed.close();
  }
  auto app = Gtk::Application::create("io.github.inde.organization-smoke", Gio::Application::Flags::NON_UNIQUE);
  inde::ui::configure_rendering_backend();
  inde::ui::MainWindow *window{};
  sigc::connection tick;
  int step = 0, result = 1;
  bool geometry_hold = false;
  app->signal_activate().connect([&] {
    window = new inde::ui::MainWindow(fixture); app->add_window(*window); window->present();
    tick = Glib::signal_timeout().connect([&] {
      try {
        auto *manuscript = find<inde::ui::EditorialStructureWorkspace>(*window, [](auto &) { return true; });
        auto *planning = find<inde::ui::PlanningWorkspace>(*window, [](auto &) { return true; });
        auto *writing = find<inde::ui::WritingWorkspace>(*window, [](auto &) { return true; });
        require(manuscript && planning && writing, "Missing production workspace");
        auto *structure = find<inde::ui::StructuralEditor>(*manuscript, [](auto &) { return true; });
        auto *work = find<Gtk::ComboBoxText>(*manuscript, [](auto &c) { return c.get_tooltip_text() == "Obra do manuscrito"; });
        require(structure && work, "Missing manuscript controls");
        auto action = [&](const char *name) {
          require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()), name, nullptr), name);
        };
        auto search = [&]() -> Gtk::Entry & {
          auto *entry = find<Gtk::Entry>(*planning, [](auto &e) { return e.get_placeholder_text() == "Pesquisar nome, alias ou resumo"; });
          require(entry, "Missing entity search"); return *entry;
        };
        if (step == 0) {
          action("win.show-editorial");
          require(pages(*manuscript, "editor").get_visible_child_name() == "editor", "Manuscript opens model management instead of content");
          require(work->set_active_id(work_a), "Work absent");
          require(count(*structure, [](auto &w) { return w.has_css_class("tool-section") && w.get_mapped(); }) == 0,
                  "Permanent structure management sections");
          std::cout << "SNAPSHOT: manuscript-content" << std::endl;
        } else if (step == 1) {
          manuscript->reveal_node(node);
          require(pages(*structure, "detail").get_visible_child_name() == "detail", "Editorial source not visible");
          require(count(*structure, [](auto &w) { return dynamic_cast<Gtk::Expander *>(&w) && w.get_mapped(); }) == 2,
                  "Related entities are not progressive");
          std::cout << "SNAPSHOT: manuscript-unit" << std::endl;
        } else if (step == 2) {
          auto *button = find<Gtk::Button>(*structure, [](auto &b) {
            return b.get_child() && find<Gtk::Label>(*b.get_child(), [](auto &l) { return l.get_text() == "Texto da unidade — organização"; });
          });
          require(button && button->get_mapped(), "Linked document unreachable");
          graphene_rect_t bounds;
          auto *scroll = find<Gtk::ScrolledWindow>(*structure, [](auto &s) {
            return s.get_mapped() && s.get_child() && find<Gtk::Label>(*s.get_child(), [](auto &l) { return l.get_text() == "Documentos desta unidade"; });
          });
          require(scroll && gtk_widget_compute_bounds(GTK_WIDGET(button->gobj()), GTK_WIDGET(scroll->gobj()), &bounds), "Missing document geometry");
          require(bounds.origin.y >= 0 && bounds.origin.y + bounds.size.height <= scroll->get_height(),
                  "Empty cover pushes linked document outside the viewport");
          g_signal_emit_by_name(button->gobj(), "clicked");
          require(writing->current_document_id() == doc, "Wrong document opened");
          action("win.show-editorial");
          require(work->get_active_id().raw() == work_a, "Return changes work");
          require(pages(*structure, "detail").get_visible_child_name() == "detail", "Return loses node context");
        } else if (step == 3) {
          manuscript->refresh();
          require(pages(*structure, "detail").get_visible_child_name() == "detail", "Refresh loses node context");
          require(work->set_active_id(work_b), "Second work absent");
          require(pages(*structure, "detail").get_visible_child_name() == "structure", "Work switch retains stale node");
          require(work->set_active_id(work_a), "First work absent");
          click(*manuscript, "Modelos e alternativas");
          require(pages(*manuscript, "library").get_visible_child_name() == "library", "Management inaccessible");
          auto &views = pages(*manuscript, "alternatives");
          require(views.get_visible_child_name() == "alternatives", "Model management obscures alternatives");
          auto *list = find<Gtk::ListBox>(*views.get_child_by_name("alternatives"), [](auto &) { return true; });
          require(list && list->get_row_at_index(0), "No active structure");
          list->select_row(*list->get_row_at_index(0));
          manuscript->refresh();
          require(list->get_selected_row(), "Refresh loses selected alternative");
          std::cout << "SNAPSHOT: manuscript-alternatives" << std::endl;
        } else if (step == 4) {
          auto &views = pages(*manuscript, "alternatives");
          views.set_visible_child("models");
          require(!views.get_child_by_name("alternatives")->get_mapped(), "Two permanent library panels");
          auto *list = find<Gtk::ListBox>(*views.get_child_by_name("models"), [](auto &) { return true; });
          require(list && list->get_row_at_index(0), "Missing reusable models");
          list->select_row(*list->get_row_at_index(0));
          manuscript->refresh(); require(list->get_selected_row(), "Refresh loses selected model");
          std::cout << "SNAPSHOT: manuscript-models" << std::endl;
        } else if (step == 5) {
          manuscript->reveal_node(node);
          require(pages(*manuscript, "editor").get_visible_child_name() == "editor", "Source hidden by model management");
          require(find<Gtk::Label>(*manuscript, [](auto &l) { return l.get_text() == "Manuscrito" && l.get_mapped(); }), "Source navigation retains wrong page heading");
          click(*structure, "← Voltar à estrutura");
        } else if (step == 6) {
          auto *options = find<Gtk::MenuButton>(*structure, [](auto &b) { return b.get_label() == "Organizar"; });
          require(options, "Missing progressive structure management"); options->popup();
          std::cout << "SNAPSHOT: manuscript-options" << std::endl;
        } else if (step == 7) {
          auto *options = find<Gtk::MenuButton>(*structure, [](auto &b) { return b.get_label() == "Organizar"; });
          require(options->get_popover()->get_width() < 600, "Management popover too wide");
          require(find<Gtk::Button>(*options->get_popover(), [](auto &b) { return b.get_label() == "Nova categoria"; }), "Lost type management");
          options->popdown(); action("win.show-planning");
          require(pages(*planning, "explorer").get_visible_child_name() == "explorer", "Planning opens model management instead of materials");
          require(search().get_mapped(), "Search hidden behind filters");
          require(count(*planning, [](auto &w) { return w.has_css_class("tool-section") && w.get_mapped(); }) == 0,
                  "Permanent entity management sections");
          search().set_text(prefix);
          std::cout << "SNAPSHOT: planning-materials" << std::endl;
        } else if (step == 8) {
          click(*planning, "Próxima");
          require(find<Gtk::Label>(*planning, [](auto &l) { return l.get_text().raw().starts_with("108 resultados • página 2 de 2"); }), "Pagination lost");
          auto *card = find<Gtk::Button>(*planning, [](auto &b) { return b.has_css_class("entity-card") && b.get_mapped(); });
          require(card, "No second-page entity"); g_signal_emit_by_name(card->gobj(), "clicked");
          require(pages(*planning, "detail").get_visible_child_name() == "detail", "Entity did not open");
          action("win.show-writing"); action("win.show-planning");
          require(pages(*planning, "detail").get_visible_child_name() == "detail", "Workspace return loses entity");
        } else if (step == 9) {
          click(*planning, "← Voltar aos resultados");
          require(search().get_text().raw() == prefix, "Back lost search");
          require(find<Gtk::Label>(*planning, [](auto &l) { return l.get_text().raw().starts_with("108 resultados • página 2 de 2"); }), "Back lost page");
          click(*planning, "Filtros"); click(*planning, "Limpar contexto");
          require(search().get_text().empty(), "Clear did not reset search");
          click(*planning, "← Voltar");
          require(search().get_text().raw() == prefix, "History does not restore full query");
          require(find<Gtk::Label>(*planning, [](auto &l) { return l.get_text().raw().starts_with("108 resultados • página 2 de 2"); }), "History does not restore offset");
          std::cout << "SNAPSHOT: planning-restored" << std::endl;
        } else if (step == 10) {
          click(*planning, "Avançar →"); require(search().get_text().empty(), "Forward does not restore cleared query");
          auto *areas = find<Gtk::MenuButton>(*planning, [](auto &b) { return b.get_tooltip_text() == "Escolher área de planejamento"; });
          require(areas && areas->get_mapped(), "Missing compact planning navigation");
          areas->popup(); std::cout << "SNAPSHOT: planning-areas" << std::endl;
        } else if (step == 11) {
          auto *choice = find<Gtk::Button>(*planning, [](auto &b) { return b.get_label() == "Explorar relações"; });
          require(choice && choice->get_mapped(), "Planning area unavailable in menu");
          click(*planning, "Explorar relações");
        } else if (step == 12) {
          auto *options = find<Gtk::MenuButton>(*planning, [](auto &b) { return b.get_tooltip_text() == "Gerenciar tipos de relação"; });
          require(options && options->get_mapped(), "Relation management inaccessible");
          options->popup(); std::cout << "SNAPSHOT: planning-relation-types" << std::endl;
        } else if (step == 13) {
          auto *options = find<Gtk::MenuButton>(*planning, [](auto &b) { return b.get_tooltip_text() == "Gerenciar tipos de relação"; });
          require(options->get_popover()->get_mapped() && options->get_popover()->get_width() < 600, "Relation management too wide");
          options->popdown();
          action("win.theme-dark"); action("win.show-editorial");
          std::cout << "SNAPSHOT: manuscript-dark" << std::endl;
        } else if (step == 14) {
          action("win.show-writing"); writing->reveal_document(doc);
          auto *editor = find<Gtk::TextView>(*writing, [](auto &v) { return v.get_editable(); });
          require(editor && editor->get_buffer()->get_text().raw() == original, "Navigation changed authorial text");
          action("win.close-project");
          click(*window, fixture.stem().string()); action("win.show-editorial");
          require(pages(*manuscript, "editor").get_visible_child_name() == "editor", "New project carries stale management page");
        } else if (step == 15) {
          require(work->set_active_id(work_b), "Long-title work missing after reopen");
          app->hold(); geometry_hold = true;
          window->hide(); window->unmaximize(); window->set_default_size(940, 660); window->present();
        } else if (step == 16) {
          std::cout << "GEOMETRY: actual=" << window->get_width() << "x" << window->get_height()
                    << " maximized=" << window->is_maximized() << std::endl;
          require(window->get_width() <= 980 && window->get_height() <= 720,
                  "Organization forces oversized geometry on a smaller window: " +
                  std::to_string(window->get_width()) + "x" + std::to_string(window->get_height()));
          std::cout << "SNAPSHOT: manuscript-compact" << std::endl;
        } else {
          result = 0; std::cout << "PASS: organization, progressive management, sources, history and lifecycle" << std::endl;
          if (geometry_hold) { app->release(); geometry_hold = false; }
          window->close(); return false;
        }
        ++step; return true;
      } catch (const std::exception &error) {
        if (geometry_hold) { app->release(); geometry_hold = false; }
        std::cerr << "FAIL step " << step << ": " << error.what() << std::endl;
        window->close(); return false;
      }
    }, 1200);
  });
  app->run(1, argv); tick.disconnect(); delete window; return result;
}

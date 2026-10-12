#include "inde/ui/main_window.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "../src/ui/rendering_policy.hpp"
#include "ui/widget_probe.hpp"
#include <cmath>
#include <iostream>
#include <tuple>
#include <gdk/x11/gdkx.h>
#include <glibmm/spawn.h>

using namespace inde::test::ui;
namespace {
double luminance(const Gdk::RGBA &color) {
  auto channel = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
  return 0.2126 * channel(color.get_red()) + 0.7152 * channel(color.get_green()) + 0.0722 * channel(color.get_blue());
}
void contrast(Gtk::Widget &widget) {
  Gdk::RGBA background;
  require(widget.get_style_context()->lookup_color(dynamic_cast<Gtk::TextView *>(&widget) ? "inde_paper" : "inde_surface", background), "Missing background token");
  const auto foreground = widget.get_style_context()->get_color();
  const auto a = luminance(foreground), b = luminance(background);
  const auto ratio = (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
  require(ratio >= 4.5, "Text contrast below 4.5: " + std::to_string(ratio));
}
void palette(Gtk::Widget &widget) {
  for (const auto &[foreground, background, minimum] : {
      std::tuple{"inde_fg", "inde_surface", 4.5}, std::tuple{"inde_muted", "inde_surface", 4.5},
      std::tuple{"inde_on_accent", "inde_accent", 4.5}, std::tuple{"inde_accent_fg", "inde_accent_soft", 4.5},
      std::tuple{"inde_danger", "inde_danger_bg", 4.5}, std::tuple{"inde_success", "inde_success_bg", 4.5},
      std::tuple{"inde_control_border", "inde_paper", 3.0}}) {
    Gdk::RGBA fg, bg;
    require(widget.get_style_context()->lookup_color(foreground, fg) && widget.get_style_context()->lookup_color(background, bg), "Missing palette token");
    const auto a = luminance(fg), b = luminance(bg);
    const auto ratio = (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
    require(ratio >= minimum, std::string("Insufficient palette contrast: ") + foreground);
  }
}
}
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes") return 2;
  const std::filesystem::path fixture = argv[1];
  std::string docid, original;
  {
    inde::application::ProjectService seed; seed.open(fixture);
    auto d = seed.writing().create_document("Caderno de revisão — reforma");
    d.content = "Água e memória 🙂\n\nLia chegou à estação vazia.\nNa plataforma, encontrou uma chave.\nNinguém soube explicar quem a deixara ali.";
    const auto types = seed.writing().semantic_types();
    const auto *comment = inde::project::find_annotation_type(types, "inde.editor.comment", 1);
    require(comment, "Comment type missing");
    d = inde::application::TextSemanticService::create(d, *comment, 0, 4,
        {{"note", {inde::project::SemanticFieldKind::Text, {"Verificar a imagem de abertura."}}}});
    d = seed.writing().update_document(d); docid = d.id; original = d.content;
    auto resolved = seed.writing().create_document("Comentário resolvido — não é pendência");
    resolved.content = "Uma tarefa concluída.";
    resolved = inde::application::TextSemanticService::create(resolved, *comment, 0, 3,
        {{"note", {inde::project::SemanticFieldKind::Text, {"Concluído."}}},
         {"state", {inde::project::SemanticFieldKind::Enum, {"resolved"}}}});
    (void)seed.writing().update_document(resolved);
    seed.close();
  }
  auto app = Gtk::Application::create("io.github.inde.reform-smoke", Gio::Application::Flags::NON_UNIQUE);
  app->set_accel_for_action("win.toggle-focus", "<Primary><Shift>f");
  inde::ui::configure_rendering_backend();
  inde::ui::MainWindow *window{};
  int step = 0, result = 1, snapshot_wait = 0;
  sigc::connection tick;
  app->signal_activate().connect([&] {
    window = new inde::ui::MainWindow(fixture); app->add_window(*window); window->present();
    tick = Glib::signal_timeout().connect([&] {
      try {
        auto *writing = find<inde::ui::WritingWorkspace>(*window, [](auto &) { return true; });
        require(writing, "Missing production writing workspace");
        const auto action = [&](const char *name) {
          require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()), name, nullptr), std::string("Unavailable action: ") + name);
        };
        if (step == 0) {
          require(pages(*window, "writing").get_visible_child_name() == "writing", "New project must open in Writing");
          {
            inde::ui::OverlayDialog modal("QA — contexto modal", *window, true);
            modal.add_button("Fechar", Gtk::ResponseType::CLOSE); modal.present();
            action("win.toggle-focus");
            auto *focus = find<Gtk::ToggleButton>(*window, [](auto &b) { return b.get_label() == "Foco"; });
            require(focus && !focus->get_active(), "Focus action interrupted a modal");
            focus->set_active(true);
            require(!focus->get_active(), "Focus button interrupted a modal");
            modal.hide();
          }
          writing->reveal_document(docid); action("win.theme-light");
          std::cout << "SNAPSHOT: light-editor" << std::endl;
        } else if (step == 1) {
          auto *editor = find<Gtk::TextView>(*writing, [](auto &v) { return v.get_editable(); });
          require(editor && editor->get_buffer()->get_text().raw() == original, "Opening changed Unicode text");
          contrast(*editor); palette(*window);
          const auto toolbar = find<Gtk::Box>(*writing, [](auto &b) { return b.has_css_class("writing-toolbar"); });
          require(toolbar && toolbar->get_mapped(), "Toolbar not mapped");
          require(count(*writing, [](auto &w) { return w.has_css_class("toolbar-surface") && w.get_mapped(); }) == 1,
                  "Editor exposes multiple permanent toolbars");
          auto *format = find<Gtk::MenuButton>(*writing, [](auto &b) { return b.get_label() == "Formatar"; });
          require(format, "Missing formatting entry point"); format->popup();
          std::cout << "SNAPSHOT: light-format" << std::endl;
        } else if (step == 2) {
          auto *format = find<Gtk::MenuButton>(*writing, [](auto &b) { return b.get_label() == "Formatar"; });
          require(format->get_popover()->get_mapped(), "Formatting did not open");
          require(format->get_popover()->get_width() < 700, "Formatting popover too wide");
          format->popdown();
          find<Gtk::TextView>(*writing, [](auto &v) { return v.get_editable(); })->grab_focus();
          const auto xid = std::to_string(gdk_x11_surface_get_xid(window->get_surface()->gobj()));
          Glib::spawn_async({}, {"xdotool", "windowfocus", "--sync", xid, "key", "--window", xid,
              "ctrl+shift+f"}, Glib::SpawnFlags::SEARCH_PATH);
          std::cout << "SNAPSHOT: light-focus" << std::endl;
        } else if (step == 3) {
          auto *navigation = find<Gtk::Box>(*window, [](auto &b) { return b.has_css_class("workspace-switcher"); });
          require(navigation && !navigation->get_mapped(), "Focus keeps task navigation visible");
          require(!find<Gtk::MenuButton>(*writing, [](auto &b) { return b.get_label() == "Formatar"; })->get_mapped(), "Focus keeps formatting visible");
          action("win.toggle-focus"); action("win.theme-dark");
          std::cout << "SNAPSHOT: dark-editor" << std::endl;
        } else if (step == 4) {
          require(inde::ui::application_theme_is_dark(), "Dark theme not active");
          contrast(*find<Gtk::TextView>(*writing, [](auto &v) { return v.get_editable(); })); palette(*window);
          action("win.theme-system");
          auto settings = Gtk::Settings::get_default();
          settings->property_gtk_application_prefer_dark_theme() = true;
          require(inde::ui::application_theme_is_dark(), "System GTK dark preference ignored");
          settings->property_gtk_application_prefer_dark_theme() = false;
          require(!inde::ui::application_theme_is_dark(), "System GTK light preference ignored");
          action("win.theme-dark");
          action("win.show-catalog"); std::cout << "SNAPSHOT: dark-catalog" << std::endl;
        } else if (step == 5) {
          auto *card = find<Gtk::Button>(*window, [](auto &b) { return b.has_css_class("catalog-card") && b.get_mapped(); });
          if (card) {
            auto *label = find<Gtk::Label>(*card, [](auto &l) { return !l.has_css_class("dim-label"); });
            require(label, "Card without label"); contrast(*label);
          }
          action("win.theme-light"); std::cout << "SNAPSHOT: light-catalog" << std::endl;
        } else if (step == 6) {
          action("win.show-planning"); click(*window, "Personagens e mundo");
          std::cout << "SNAPSHOT: light-entities" << std::endl;
        } else if (step == 7) {
          auto *card = find<Gtk::Button>(*window, [](auto &b) { return b.has_css_class("entity-card") && b.get_mapped(); });
          if (card) contrast(*find<Gtk::Label>(*card, [](auto &l) { return !l.has_css_class("dim-label"); }));
          action("win.theme-dark"); std::cout << "SNAPSHOT: dark-entities" << std::endl;
        } else if (step == 8) {
          action("win.show-review"); std::cout << "SNAPSHOT: dark-review" << std::endl;
        } else if (step == 9) {
          auto *review = find<inde::ui::ReviewWorkspace>(*window, [](auto &) { return true; });
          require(review, "Missing review workspace");
          require(find<Gtk::Button>(*review, [](auto &b) { return b.get_label() == "Caderno de revisão — reforma"; }),
                  "Review queue lost a real comment");
          require(!find<Gtk::Button>(*review, [](auto &b) { return b.get_label() == "Comentário resolvido — não é pendência"; }),
                  "Resolved comment presented as pending");
          action("win.theme-light"); std::cout << "SNAPSHOT: light-review" << std::endl;
        } else if (step == 10) {
          action("win.show-editorial"); std::cout << "SNAPSHOT: light-manuscript" << std::endl;
        } else if (step == 11) {
          action("win.show-graphs"); std::cout << "SNAPSHOT: light-timeline" << std::endl;
        } else if (step == 12) {
          action("win.show-cartography"); std::cout << "SNAPSHOT: light-map" << std::endl;
        } else if (step == 13) {
          action("win.theme-dark"); std::cout << "SNAPSHOT: dark-map" << std::endl;
        } else if (step == 14) {
          action("win.show-writing"); writing->reveal_document(docid);
          action("win.save-project");
          require(writing->current_document_id() == docid, "Save changes current document");
          action("win.close-project");
          click(*window, fixture.stem().string());
          require(writing->current_document_id() == docid, "Reopen did not resume document");
          require(inde::ui::ui_preference("appearance", "theme") == "dark", "Theme not persisted");
          std::cout << "SNAPSHOT: resumed-editor" << std::endl;
        } else if (step == 15) {
          auto *editor = find<Gtk::TextView>(*writing, [](auto &v) { return v.get_editable(); });
          auto *controllers = gtk_widget_observe_controllers(GTK_WIDGET(editor->gobj()));
          bool opened = false;
          for (guint i = 0; i < g_list_model_get_n_items(controllers); ++i) {
            auto *controller = G_OBJECT(g_list_model_get_item(controllers, i));
            if (GTK_IS_EVENT_CONTROLLER_KEY(controller)) {
              gboolean handled = false;
              g_signal_emit_by_name(controller, "key-pressed", GDK_KEY_F10, 0u, GDK_SHIFT_MASK, &handled);
              opened = opened || handled;
            }
            g_object_unref(controller);
          }
          g_object_unref(controllers);
          require(opened, "Context menu unavailable by keyboard");
          std::cout << "SNAPSHOT: dark-context-menu" << std::endl;
        } else {
          if (++snapshot_wait < 4) return true;
          auto *menu = find<Gtk::Popover>(*writing, [](auto &p) { return p.has_css_class("context-menu") && p.get_mapped(); });
          require(menu, "Keyboard context menu did not map");
          auto *label = find<Gtk::Label>(*menu, [](auto &l) { return l.get_text() == "Comentar seleção…"; });
          require(label, "Context menu not localized"); contrast(*label); menu->popdown();
          require(find<Gtk::TextView>(*writing, [](auto &v) { return v.get_editable(); })->get_buffer()->get_text().raw() == original,
                  "Theme/focus/navigation altered text");
          result = 0; std::cout << "PASS: reform, contrast, focus, review, resume, Unicode and project lifecycle" << std::endl;
          window->close(); return false;
        }
        ++step; return true;
      } catch (const std::exception &e) {
        std::cerr << "FAIL step " << step << ": " << e.what() << std::endl;
        window->close(); return false;
      }
    }, 650);
  });
  app->run(1, argv); tick.disconnect(); delete window; return result;
}

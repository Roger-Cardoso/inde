#include "inde/ui/application_theme.hpp"
#include "../src/ui/theme_compatibility.hpp"
#include "../src/ui/rendering_policy.hpp"
#include "ui/theme_probe.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
void capture(Gtk::Widget &widget, const std::filesystem::path &path) {
  auto *snapshot = gtk_snapshot_new();
  auto *paintable = gtk_widget_paintable_new(widget.gobj());
  gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot),
                        widget.get_width(), widget.get_height());
  g_object_unref(paintable);
  auto *node = gtk_snapshot_free_to_node(snapshot);
  require(node, "No allocated theme render node");
  auto surface = Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32,
                                             widget.get_width(), widget.get_height());
  auto context = Cairo::Context::create(surface);
  gsk_render_node_draw(node, context->cobj());
  gsk_render_node_unref(node);
  surface->write_to_png(path.string());
}
}
int main(int argc, char **argv) {
  try {
    require(argc == 2 || argc == 3, "Usage: theme-smoke --inspect-desktop | render frame-directory");
    const bool inspect = std::string(argv[1]) == "--inspect-desktop";
    require(inspect || argc == 3, "Frame directory required");
    inde::ui::configure_gtk_theme_base();
    inde::ui::configure_rendering_backend();
    auto app = Gtk::Application::create("io.github.inde.theme-compatibility-qa",
                                       Gio::Application::Flags::NON_UNIQUE);
    app->register_application();
    const auto settings = Gtk::Settings::get_default();
    require(bool(settings), "No GTK settings");
    const auto inherited = settings->property_gtk_theme_name().get_value();
    const auto font = settings->property_gtk_font_name().get_value();
    const auto icons = settings->property_gtk_icon_theme_name().get_value();
    const auto prefer_dark = settings->property_gtk_application_prefer_dark_theme().get_value();
    inde::ui::install_application_theme();
    require(inde::ui::application_theme() == inde::ui::ThemeMode::System,
            "Probe needs a clean INDE configuration");
    require(inde::ui::application_theme_is_dark() ==
              (prefer_dark || inde::ui::theme_name_is_dark(inherited.raw())),
            "System palette did not follow inherited desktop preference");
    std::cout << "GTK " << gtk_get_major_version() << '.' << gtk_get_minor_version()
              << '.' << gtk_get_micro_version() << "; inherited-theme=" << inherited
              << "; base=" << g_getenv("GTK_THEME") << "; font=" << font
              << "; icons=" << icons << "; system-dark="
              << inde::ui::application_theme_is_dark() << '\n';
    if (inspect) {
      std::cout << "PASS: desktop settings and System palette, no visible window\n";
      return 0;
    }
    const std::filesystem::path frames(argv[2]);
    std::filesystem::create_directories(frames);
    Gtk::Window window;
    window.add_css_class("inde-window");
    window.set_title("INDE — compatibilidade de tema");
    window.set_default_size(720, 400);
    Gtk::Box root(Gtk::Orientation::VERTICAL, 16);
    root.add_css_class("content-card");
    root.set_margin(20);
    Gtk::Label title("Base GTK compatível; paleta do INDE");
    title.add_css_class("page-title");
    title.set_xalign(0);
    Gtk::Label description("O CSS externo do desktop não controla a paleta.\nFontes, ícones e a preferência de modo continuam disponíveis.");
    description.set_xalign(0); description.set_wrap(true);
    Gtk::Entry entry; entry.set_text("Seleção e legibilidade preservadas");
    Gtk::CheckButton check("Opção selecionada"); check.set_active(true);
    Gtk::Box actions(Gtk::Orientation::HORIZONTAL, 12);
    Gtk::Button primary("Ação principal"), secondary("Cancelar");
    primary.add_css_class("suggested-action");
    actions.append(primary); actions.append(secondary);
    root.append(title); root.append(description); root.append(entry);
    root.append(check); root.append(actions); window.set_child(root);
    app->add_window(window); window.present();
    int stage = 0, result = 1;
    auto timer = Glib::signal_timeout().connect([&] {
      try {
        if (!stage) {
          require(inherited == settings->property_gtk_theme_name().get_value(),
                  "Compatible base overwrote inherited theme setting");
          inde::ui::set_application_theme(inde::ui::ThemeMode::Light);
        } else if (stage == 1) {
          inde::test::ui::require_surface_background(root);
          require(!inde::ui::application_theme_is_dark(), "Light ignored");
          capture(root, frames / "light.png");
          inde::ui::set_application_theme(inde::ui::ThemeMode::Dark);
        } else if (stage == 2) {
          inde::test::ui::require_surface_background(root);
          require(inde::ui::application_theme_is_dark(), "Dark ignored");
          capture(root, frames / "dark.png");
          // These setters change only this process, never desktop XSettings.
          settings->property_gtk_theme_name() = "Orchis-Dark-Compact";
          settings->property_gtk_application_prefer_dark_theme() = false;
          inde::ui::set_application_theme(inde::ui::ThemeMode::System);
        } else if (stage == 3) {
          require(inde::ui::application_theme_is_dark(), "Mixed-case Dark ignored");
          settings->property_gtk_theme_name() = "Adwaita";
        } else if (stage == 4) {
          require(!inde::ui::application_theme_is_dark(), "System light change ignored");
          settings->property_gtk_application_prefer_dark_theme() = true;
        } else if (stage == 5) {
          require(inde::ui::application_theme_is_dark(), "System dark preference ignored");
          inde::ui::set_application_theme(inde::ui::ThemeMode::Light);
          settings->property_gtk_theme_name() = "Orchis-Dark-Compact";
        } else if (stage == 6) {
          require(!inde::ui::application_theme_is_dark(), "Explicit Light followed desktop Dark");
          inde::ui::set_application_theme(inde::ui::ThemeMode::Dark);
          settings->property_gtk_theme_name() = "Adwaita";
        } else {
          require(inde::ui::application_theme_is_dark(), "Explicit Dark followed desktop Light");
          require(font == settings->property_gtk_font_name().get_value(), "Font changed");
          require(icons == settings->property_gtk_icon_theme_name().get_value(), "Icons changed");
          require(std::string(g_getenv("GTK_THEME")).starts_with("Adwaita"),
                  "Theme property changes replaced compatible base");
          result = 0; window.close();
          std::cout << "PASS: native light/dark pixels, System changes, explicit palette modes, font/icons preserved\n";
          return false;
        }
        ++stage; return true;
      } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        window.close(); return false;
      }
    }, 350);
    app->run(); timer.disconnect(); return result;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n'; return 1;
  }
}

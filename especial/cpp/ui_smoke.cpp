#include "publication_ui.hpp"
#include <cairomm/cairomm.h>
#include <fstream>
#include <gsk/gsk.h>
#include <gtk/gtk.h>
#include <iostream>
namespace fs = std::filesystem;
#include "ui_test_driver.hpp"
void capture(Gtk::Widget &widget, const fs::path &path) {
  auto *snapshot = gtk_snapshot_new();
  auto *paintable = gtk_widget_paintable_new(GTK_WIDGET(widget.gobj()));
  gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot), widget.get_width(),
                         widget.get_height());
  g_object_unref(paintable);
  auto *node = gtk_snapshot_free_to_node(snapshot);
  if (!node)
    throw std::runtime_error("Snapshot GTK vazio");
  auto surface = Cairo::ImageSurface::create(
      Cairo::Surface::Format::ARGB32, widget.get_width(), widget.get_height());
  auto ctx = Cairo::Context::create(surface);
  gsk_render_node_draw(node, ctx->cobj());
  gsk_render_node_unref(node);
  surface->write_to_png(path.string());
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const fs::path project = argv[1], picture = argv[2];
  setenv("XDG_CONFIG_HOME", (project.parent_path() / "perfil-ui").c_str(), 1);
  auto app = Gtk::Application::create("io.github.inde.publicacao.smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  inde::application::ProjectService source;
  source.open(project);
  inde::publication::Service service(source);
  Gtk::Window window;
  window.set_title("INDE Publicação · smoke GTK");
  window.set_default_size(1000, 760);
  inde::ui::PublicationPage page(service);
  window.set_child(page);
  int result = 1, stage = 0, ticks = 0;
  app->signal_activate().connect([&] {
    app->add_window(window);
    window.present();
    Glib::signal_timeout().connect(
        [&] {
          try {
            if (++ticks > 120)
              throw std::runtime_error("Timeout GTK");
            if (stage == 0) {
              page.refresh();
              inde::ui::PublicationPageProbe::site(page);
              inde::ui::PublicationPageProbe::edition(page);
              inde::ui::PublicationPageProbe::recover(page);
              inde::ui::PublicationPageProbe::generate(page);
              stage = 1;
              return true;
            }
            if (stage == 1) {
              if (!inde::ui::PublicationPageProbe::done(page))
                return true;
              capture(window, picture);
              inde::ui::PublicationPageProbe::back(page);
              stage = 2;
              return true;
            }
            result = 0;
            std::cout << "GTK: formulários, gravação, seleção explícita, "
                         "bloqueios, geração assíncrona e reabertura OK\n";
            window.hide();
            app->quit();
            return false;
          } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
            window.hide();
            app->quit();
            return false;
          }
        },
        250);
  });
  app->run(1, argv);
  window.unset_child();
  return result;
}

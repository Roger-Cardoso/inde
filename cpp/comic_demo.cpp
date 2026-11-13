#include "comic_service.hpp"
#include "inde/project/manifest.hpp"
#include <cairo.h>
#include <fstream>
#include <glib.h>
#include <iostream>
using namespace inde;
int main(int argc, char **argv) {
  try {
    if (argc != 2 && argc != 3)
      throw std::runtime_error(
          "Uso: inde_comic_demo PROJETO.inde [RECURSOS_PARA_PREPARAR]");
    application::ProjectService source;
    source.open(argv[1]);
    if (source.catalog().works.empty())
      throw std::runtime_error("Crie uma Obra primeiro");
    publication::Service pub(source);
    comic::Service comics(pub);
    if (!comics.books(true).empty()) {
      std::cout << "Demo comic já existente; preservada.\n";
      if (argc == 3) {
        const auto books = comics.books();
        const auto sites = pub.sites();
        if (books.empty() || sites.empty())
          throw std::runtime_error("Restaure um comic e um Site antes de preparar");
        const auto book = books.front();
        const auto site = sites.front();
        auto prepared = comics.prepare(book.id, site.id, book.revision,
                                       site.revision, argv[2]);
        std::cout << comics.materialize(prepared).string() << '\n';
      }
      return 0;
    }
    if (pub.sites().empty()) {
      publication::Site site;
      site.name = "Biblioteca das passagens";
      site.author = "Autoria de demonstração";
      site.description =
          "Um projeto autoral com escrita e composição de comics";
      site.theme = "verde";
      static_cast<void>(pub.create_site(site));
    }
    auto b = comic::example(source.catalog().works.front().id);
    for (int i = 0; i < 3; i++) {
      auto *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 900, 560);
      auto *cr = cairo_create(surface);
      auto *gradient = cairo_pattern_create_linear(0, 0, 0, 560);
      cairo_pattern_add_color_stop_rgb(gradient, 0, .10, .22, .28);
      cairo_pattern_add_color_stop_rgb(gradient, 1, .025, .07, .11);
      cairo_set_source(cr, gradient);
      cairo_paint(cr);
      cairo_pattern_destroy(gradient);
      cairo_set_source_rgb(cr, .28, .52, .58);
      cairo_arc(cr, 710, 120, 44, 0, 6.2832);
      cairo_fill(cr);
      cairo_set_source_rgb(cr, .07, .14, .19);
      cairo_move_to(cr, 0, 390);
      cairo_line_to(cr, 210, 230);
      cairo_line_to(cr, 370, 370);
      cairo_line_to(cr, 540, 190);
      cairo_line_to(cr, 900, 430);
      cairo_line_to(cr, 900, 560);
      cairo_line_to(cr, 0, 560);
      cairo_close_path(cr);
      cairo_fill(cr);
      cairo_set_source_rgb(cr, .46, .58, .57);
      cairo_rectangle(cr, 610, 160, 30, 270);
      cairo_rectangle(cr, 720, 160, 30, 270);
      cairo_fill(cr);
      cairo_set_line_width(cr, 30);
      cairo_arc(cr, 680, 160, 55, 3.14159, 6.2832);
      cairo_stroke(cr);
      cairo_set_source_rgba(cr, .95, .81, .50, .26 + i * .13);
      cairo_move_to(cr, 640, 170);
      cairo_line_to(cr, 720, 170);
      cairo_line_to(cr, 840, 560);
      cairo_line_to(cr, 500, 560);
      cairo_close_path(cr);
      cairo_fill(cr);
      cairo_set_source_rgb(cr, .90, .67, .39);
      cairo_arc(cr, 350, 390, 12, 0, 6.2832);
      cairo_fill(cr);
      cairo_move_to(cr, 350, 405);
      cairo_line_to(cr, 322, 470);
      cairo_line_to(cr, 378, 470);
      cairo_close_path(cr);
      cairo_fill(cr);
      cairo_set_source_rgb(cr, .02, .05, .08);
      cairo_rectangle(cr, 337, 470, 9, 35);
      cairo_rectangle(cr, 355, 470, 9, 35);
      cairo_fill(cr);
      cairo_destroy(cr);
      std::string raw;
      const auto status = cairo_surface_write_to_png_stream(
          surface,
          +[](void *closure, const unsigned char *data, unsigned int length) {
            static_cast<std::string *>(closure)->append(
                reinterpret_cast<const char *>(data), length);
            return CAIRO_STATUS_SUCCESS;
          },
          &raw);
      cairo_surface_destroy(surface);
      if (status != CAIRO_STATUS_SUCCESS)
        throw std::runtime_error("Falha na arte sintética");
      comic::Asset a;
      a.id = project::new_uuid();
      a.name = "portal-demo.png";
      a.mime = "image/png";
      auto *encoded = g_base64_encode(
          reinterpret_cast<const guchar *>(raw.data()), raw.size());
      a.data = encoded;
      g_free(encoded);
      b.assets[a.id] = a;
      b.panels[i].image = a.id;
      b.panels[i].alt = "Ilustração sintética: viajante diante de um portal "
                        "luminoso nas montanhas.";
    }
    b = comics.save(b, 0);
    std::cout << "Comic criado pelos serviços C++ na Obra nativa.\n";
    if (argc == 3) {
      const auto site = pub.sites().front();
      const auto prepared =
          comics.prepare(b.id, site.id, b.revision, site.revision, argv[2]);
      std::cout << comics.materialize(prepared).string() << '\n';
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

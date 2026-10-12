#include "production_support.hpp"
#include <cairo-pdf.h>
#include <memory>
#include <pango/pangocairo.h>
namespace inde::application::production_detail {
namespace {
struct PdfBuffer {
  std::string bytes;
  bool failed{};
};
cairo_status_t append(void *closure, const unsigned char *data,
                      unsigned length) noexcept {
  auto &b = *static_cast<PdfBuffer *>(closure);
  try {
    if (b.bytes.size() + length > 128 * 1024 * 1024)
      throw std::runtime_error("PDF grande");
    b.bytes.append(reinterpret_cast<const char *>(data), length);
    return CAIRO_STATUS_SUCCESS;
  } catch (...) {
    b.failed = true;
    return CAIRO_STATUS_WRITE_ERROR;
  }
}
struct Layout {
  PangoLayout *p;
  ~Layout() { g_object_unref(p); }
};
struct Font {
  PangoFontDescription *p;
  ~Font() { pango_font_description_free(p); }
};
} // namespace
std::string make_pdf(const WorkProduction &w, std::stop_token stop) {
  PdfBuffer buffer;
  std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface(
      cairo_pdf_surface_create_for_stream(append, &buffer, 612, 792),
      cairo_surface_destroy);
  std::unique_ptr<cairo_t, decltype(&cairo_destroy)> cr(
      cairo_create(surface.get()), cairo_destroy);
  cairo_pdf_surface_set_metadata(surface.get(), CAIRO_PDF_METADATA_TITLE,
                                 w.work.title.c_str());
  cairo_pdf_surface_set_metadata(surface.get(), CAIRO_PDF_METADATA_AUTHOR,
                                 w.options.author.c_str());
  cairo_pdf_surface_set_metadata(surface.get(), CAIRO_PDF_METADATA_CREATOR,
                                 "INDE Work Production");
  cairo_pdf_surface_set_metadata(surface.get(), CAIRO_PDF_METADATA_CREATE_DATE,
                                 w.prepared_at.c_str());
  const bool manuscript = w.options.profile == ProductionProfile::Manuscript;
  double y = 72;
  int page = 1;
  auto footer = [&] {
    Layout l{pango_cairo_create_layout(cr.get())};
    pango_cairo_context_set_resolution(pango_layout_get_context(l.p), 72);
    Font f{pango_font_description_from_string("Serif 10")};
    pango_layout_set_font_description(l.p, f.p);
    pango_layout_set_width(l.p, 468 * PANGO_SCALE);
    pango_layout_set_alignment(l.p, PANGO_ALIGN_RIGHT);
    const auto s = std::to_string(page);
    pango_layout_set_text(l.p, s.c_str(), -1);
    cairo_move_to(cr.get(), 72, 744);
    pango_cairo_show_layout(cr.get(), l.p);
  };
  auto next = [&] {
    cancelled(stop);
    footer();
    cairo_show_page(cr.get());
    ++page;
    y = 72;
  };
  auto paragraph = [&](const std::string &markup, int size, bool heading,
                       bool cover = false) {
    cancelled(stop);
    Layout l{pango_cairo_create_layout(cr.get())};
    pango_cairo_context_set_resolution(pango_layout_get_context(l.p), 72);
    Font f{pango_font_description_from_string(
        ("Serif " + std::to_string(size)).c_str())};
    pango_layout_set_font_description(l.p, f.p);
    pango_layout_set_width(l.p, 468 * PANGO_SCALE);
    pango_layout_set_wrap(l.p, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_markup(l.p, markup.c_str(),
                            static_cast<int>(markup.size()));
    if (pango_layout_get_unknown_glyphs_count(l.p))
      throw std::runtime_error(
          "Fonte local não representa todos os caracteres do PDF; selecione "
          "outros formatos ou instale a fonte ausente");
    const double spacing = heading || cover ? 1.25 : manuscript ? 2.0 : 1.5;
    int block_width, block_height;
    pango_layout_get_size(l.p, &block_width, &block_height);
    (void)block_width;
    const double block =
        block_height / static_cast<double>(PANGO_SCALE) * spacing + 6;
    // Keep ordinary short paragraphs together. Long paragraphs can still
    // flow across pages; no arbitrary character/line truncation is performed.
    if (!heading && block <= 648 && y > 72 && y + block > 720)
      next();
    if (heading) {
      y += 12;
      int width, height;
      pango_layout_get_size(l.p, &width, &height);
      (void)width;
      if (y + height / static_cast<double>(PANGO_SCALE) + 36 > 720 && y > 72)
        next();
    }
    std::unique_ptr<PangoLayoutIter, decltype(&pango_layout_iter_free)> iter(
        pango_layout_get_iter(l.p), pango_layout_iter_free);
    do {
      cancelled(stop);
      PangoRectangle logical;
      pango_layout_iter_get_line_extents(iter.get(), nullptr, &logical);
      const double height = logical.height / static_cast<double>(PANGO_SCALE),
                   advance = height * spacing;
      if (y + height > 720)
        next();
      const double ascent =
          (pango_layout_iter_get_baseline(iter.get()) - logical.y) /
          static_cast<double>(PANGO_SCALE);
      cairo_move_to(cr.get(), 72, y + ascent);
      pango_cairo_show_layout_line(
          cr.get(), pango_layout_iter_get_line_readonly(iter.get()));
      y += advance;
    } while (pango_layout_iter_next_line(iter.get()));
    y += heading ? 8 : 6;
  };
  paragraph(xml(w.work.title), 24, true, true);
  if (!w.work.subtitle.empty())
    paragraph(xml(w.work.subtitle), 16, false, true);
  if (!w.options.author.empty())
    paragraph(xml(w.options.author), 12, false, true);
  next();
  for (const auto &u : w.units) {
    cancelled(stop);
    if (u.heading)
      paragraph("<b>" +
                    xml(u.node.designator +
                        (u.node.designator.empty() ? "" : " ") + u.node.title) +
                    "</b>",
                u.depth ? 14 : 16, true);
    for (const auto &d : u.documents)
      for (const auto &p : paragraphs(d, stop)) {
        std::string markup;
        for (const auto &r : p.runs)
          markup += (r.bold ? "<b>" : "") + std::string(r.italic ? "<i>" : "") +
                    (r.underline ? "<u>" : "") + (r.strike ? "<s>" : "") +
                    xml(r.text) + (r.strike ? "</s>" : "") +
                    (r.underline ? "</u>" : "") + (r.italic ? "</i>" : "") +
                    (r.bold ? "</b>" : "");
        paragraph(markup, 12, false);
      }
  }
  footer();
  cairo_show_page(cr.get());
  cairo_surface_finish(surface.get());
  if (buffer.failed || cairo_status(cr.get()) != CAIRO_STATUS_SUCCESS ||
      cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
    throw std::runtime_error("PDF não foi concluído pelo renderizador");
  return buffer.bytes;
}
} // namespace inde::application::production_detail

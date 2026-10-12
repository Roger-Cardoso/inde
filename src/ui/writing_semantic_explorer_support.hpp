#pragma once
#include "writing_semantic_ui_support.hpp"

namespace inde::ui::semantic_ui {
inline std::string clipped(const std::string &text, std::size_t limit = 180) {
  const Glib::ustring value(text);
  auto result = value.substr(0, limit).raw();
  std::replace(result.begin(), result.end(), '\n', ' ');
  std::replace(result.begin(), result.end(), '\r', ' ');
  return result + (value.size() > limit ? "…" : "");
}
inline std::string status_label(const std::string &status) {
  return status == "active"       ? "Ativa"
         : status == "incomplete" ? "Incompleta"
         : status == "archived"   ? "Arquivada"
                                  : status;
}
inline const project::SemanticAnchorEvidence *
evidence(const project::Document &d, const project::TextAnnotation &a) {
  auto i = std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                        [&](auto &e) { return e.anchor_id == a.anchor_id; });
  return i == d.semantics.anchors.end() ? nullptr : &*i;
}
inline std::string anchor_key(const project::Document &d,
                              const project::TextAnnotation &a) {
  if (a.scope == project::AnnotationScope::Document)
    return "document";
  auto *e = evidence(d, a);
  return e ? project::to_string(e->state) : "unavailable";
}
inline std::string anchor_label(const std::string &key) {
  return key == "document" ? "Para todo o documento — sem trecho específico"
         : key == "resolved"  ? "Trecho localizado"
         : key == "ambiguous" ? "Há mais de um trecho possível"
         : key == "orphaned"  ? "Trecho removido"
                              : "Trecho indisponível";
}
inline std::string excerpt(const project::Document &d,
                           const project::TextAnnotation &a) {
  if (a.scope == project::AnnotationScope::Document)
    return "Declaração para todo o documento; o conteúdo não é marcado como um intervalo.";
  const auto *e = evidence(d, a);
  if (!e || e->state != project::AnchorResolution::Resolved)
    return anchor_label(anchor_key(d, a)) +
           " — escolha novamente o trecho";
  auto i = std::find_if(d.anchors.begin(), d.anchors.end(),
                        [&](auto &k) { return k.id == a.anchor_id; });
  const Glib::ustring text(d.content);
  if (i == d.anchors.end() || i->end_offset > text.size() ||
      i->start_offset > i->end_offset)
    return "Trecho indisponível";
  return clipped(
      text.substr(i->start_offset, i->end_offset - i->start_offset).raw());
}
inline void compact_label(Gtk::Label &l) {
  l.set_xalign(0);
  l.set_wrap(true);
  l.set_lines(3);
  l.set_ellipsize(Pango::EllipsizeMode::END);
  l.set_max_width_chars(34);
}
} // namespace inde::ui::semantic_ui

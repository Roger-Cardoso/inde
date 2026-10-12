#pragma once
#include "writing_semantic_explorer_support.hpp"

namespace inde::ui::semantic_ui {
inline std::string relation_label(const std::string &key) {
  return key == "inde.semantic.reference" ? "Referência explícita"
      : key == "inde.semantic.response-to" ? "Resposta a"
      : key == "inde.semantic.callback" ? "Retomada" : key;
}
inline const project::TextAnnotation &annotation(const project::Document &d,
                                               const std::string &id) {
  auto it = std::find_if(d.semantics.annotations.begin(), d.semantics.annotations.end(),
                        [&](const auto &a) { return a.id == id; });
  if (it == d.semantics.annotations.end())
    throw std::runtime_error("Anotação indisponível; atualize as relações");
  return *it;
}
inline std::string role_label(const project::TextAnnotation &a,
    const std::vector<project::AnnotationTypeDefinition> &types) {
  const auto *t = project::find_annotation_type(types, a.type_key, a.type_version);
  return t ? author::annotation_label(*t) : a.type_key + " (tipo indisponível)";
}
inline void endpoint_label(Gtk::Box &box, const std::string &title,
    const project::Document &d, const project::TextAnnotation &a,
    const std::vector<project::AnnotationTypeDefinition> &types) {
  label(box, title + ": " + clipped(d.title, 100) + " • " + role_label(a, types))
      ->add_css_class("heading");
  label(box, status_label(a.status) + " • " + anchor_label(anchor_key(d, a)));
  label(box, excerpt(d, a));
  auto *identity = label(box, "Documento: " + d.id + "\nAnotação: " + a.id);
  identity->set_selectable(true);
}
} // namespace inde::ui::semantic_ui

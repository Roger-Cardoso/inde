#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "writing_semantic_ui_support.hpp"
#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
namespace inde::ui {
using namespace semantic_ui;
void WritingWorkspace::show_semantic_uses(persistence::SemanticUseQuery query) {
  if (!current_document_)
    return;
  semantic_project_scope_ = !query.document_id.has_value();
  semantic_use_filter_ = std::move(query);
  show_semantic_layers(semantic_use_filter_.offset);
}
void WritingWorkspace::render_semantic_uses(
    Gtk::Box &box, persistence::SemanticUseQuery query) {
  const auto page = service_.writing().semantic_uses(query);
  semantic_offset_ = query.offset;
  label(box, "Consulta de anotações persistidas — " +
                 std::to_string(page.total) +
                 " resultado(s). Alterações ainda não salvas não aparecem.")
      ->add_css_class("context-summary");
  auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
  scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
  scroll->set_vexpand(true);
  auto *cards = Gtk::make_managed<Gtk::FlowBox>();
  cards->set_selection_mode(Gtk::SelectionMode::NONE);
  cards->set_min_children_per_line(1);
  cards->set_max_children_per_line(4);
  cards->set_homogeneous(true);
  cards->set_column_spacing(12);
  cards->set_row_spacing(12);
  cards->set_valign(Gtk::Align::START);
  const auto types = service_.writing().semantic_types();
  for (const auto &use : page.rows) {
    auto *card = Gtk::make_managed<Gtk::Button>();
    card->add_css_class("entity-card");
    card->set_size_request(270, 140);
    auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    body->set_margin(12);
    auto *title = label(*body, use.document_title);
    title->set_ellipsize(Pango::EllipsizeMode::END);
    title->add_css_class("heading");
    std::string role = use.type_key;
    for (const auto &t : types)
      if (t.key == use.type_key) {
        role = author::annotation_label(t);
        break;
      }
    if (use.scope == project::AnnotationScope::Document && use.type_key=="inde.semantic.perspective")
      role = "Ponto de vista do documento";
    label(*body, role + " • " + use.status)->add_css_class("dim-label");
    label(*body,
          use.category.empty() ? "Sem categoria" : value_label(use.category));
    label(*body, use.scope == project::AnnotationScope::Document
                     ? "Declaração para todo o documento"
                     : "Abrir marcação e examinar o trecho");
    card->set_child(*body);
    card->set_tooltip_text("Abrir detalhe da anotação\n" + use.annotation_id);
    card->signal_clicked().connect([this, use] {
      reveal_document(use.document_id);
      if (current_document_ && current_document_->id == use.document_id)
        show_semantic_annotation(use.annotation_id);
    });
    cards->append(*card);
  }
  if (page.rows.empty()) {
    auto *empty = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    empty->set_margin(24);
    empty->add_css_class("content-card");
    label(*empty, "Nenhuma camada corresponde aos filtros do Projeto");
    label(*empty, "Ajuste tipo de marcação, categoria, elemento da história ou Obra. Nenhuma marcação foi removida.");
    scroll->set_child(*empty);
  } else
    scroll->set_child(*cards);
  box.append(*scroll);
  auto *pagination =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  pagination->set_halign(Gtk::Align::END);
  auto *previous = Gtk::make_managed<Gtk::Button>("Página anterior de camadas");
  auto *next = Gtk::make_managed<Gtk::Button>("Próxima página de camadas");
  previous->set_sensitive(query.offset > 0);
  next->set_sensitive(query.offset + page.rows.size() < page.total);
  previous->signal_clicked().connect([this, query] {
    show_semantic_layers(
        query.offset >= query.limit ? query.offset - query.limit : 0);
  });
  next->signal_clicked().connect(
      [this, query] { show_semantic_layers(query.offset + query.limit); });
  pagination->append(*previous);
  pagination->append(*next);
  box.append(*pagination);
}
} // namespace inde::ui

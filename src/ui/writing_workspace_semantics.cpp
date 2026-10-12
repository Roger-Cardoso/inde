#include "inde/ui/author_diagnostics.hpp"
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
void WritingWorkspace::build_semantic_ui() {
  contextual_toolbar_.add_css_class("toolbar-surface");
  contextual_toolbar_.append(anchors_button_);
  contextual_toolbar_.append(entities_button_);
  contextual_toolbar_.append(semantic_button_);
  contextual_toolbar_.append(semantic_lens_button_);
  contextual_toolbar_.append(semantic_mark_button_);
  contextual_toolbar_.append(comment_button_);
  contextual_toolbar_.append(statistics_button_);
  contextual_toolbar_.append(goal_button_);
  contextual_toolbar_.append(repeated_button_);
  contextual_toolbar_.append(long_paragraphs_button_);
  contextual_toolbar_.append(inline_button_);
  semantic_button_.set_label("Marcações do texto");
  semantic_mark_button_.set_label("Marcar Seleção");
  semantic_page_.set_hexpand(true);
  semantic_page_.set_vexpand(true);
  semantic_page_.add_css_class("semantic-explorer");
  page_stack_.set_hhomogeneous(false);
  page_stack_.set_vhomogeneous(false);
  page_stack_.add(semantic_page_, "semantic-layers");
  semantic_button_.set_tooltip_text(
      "Marcações do texto, campos e relações explícitas. Selecione um trecho "
      "para criar uma marcação.");
  semantic_button_.signal_clicked().connect([this] { show_semantic_layers(); });
  semantic_mark_button_.set_tooltip_text(
      "Criar uma anotação sobre o trecho selecionado, sem atribuição automática");
  semantic_mark_button_.signal_clicked().connect([this] { edit_semantic_fields(); });
  semantic_lens_button_.set_tooltip_text(
      "Fundos coloridos atenuados, sem mudar as letras. Passe o ponteiro "
      "sobre o trecho para identificar as marcações e suas informações.");
  editor_.set_has_tooltip(true);
  editor_.signal_query_tooltip().connect(
      sigc::mem_fun(*this, &WritingWorkspace::semantic_tooltip), false);
  semantic_lens_button_.signal_toggled().connect(
      [this] { refresh_semantic_lenses(); });
}
void WritingWorkspace::semantic_before_edit(std::size_t start, std::size_t end,
                                            const std::string &replacement) {
  if (!current_document_ || current_document_->semantics.anchors.empty())
    return;
  try {
    auto d = capture_editor_state();
    application::SemanticAnchorEngine::edit(d, start, end, replacement);
    current_document_->semantics = std::move(d.semantics);
  } catch (const std::exception &e) {
    save_error_ = std::string("Falha nos trechos das marcações: ") + author::diagnostic(e.what());
    signal_status_message_.emit(save_error_);
  }
}
void WritingWorkspace::apply_semantic_document(project::Document d,
                                               const std::string &name) {
  service_.writing().validate_semantic_document(d);
  auto state = capture_edit_state();
  state.document = std::move(d);
  apply_edit_transaction(std::move(state), name);
  refresh_semantic_lenses();
}
void WritingWorkspace::refresh_semantic_lenses() {
  // Buffer notifications also fire while reset/restore has temporarily
  // detached anchor marks. Inspect only the completed editor state.
  if (loading_editor_ || restoring_undo_) return;
  auto buffer = editor_.get_buffer();
  for (auto &[k, t] : semantic_tags_) {
    (void)k;
    buffer->remove_tag(t, buffer->begin(), buffer->end());
  }
  if (!current_document_ || !semantic_lens_button_.get_active())
    return;
  auto d = capture_editor_state();
  application::SemanticAnchorEngine::inspect(d);
  for (auto &a : d.semantics.annotations) {
    if (a.status == "archived")
      continue;
    auto e =
        std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                     [&](auto &x) { return x.anchor_id == a.anchor_id; });
    if (e == d.semantics.anchors.end() ||
        e->state != project::AnchorResolution::Resolved)
      continue;
    auto k = std::find_if(d.anchors.begin(), d.anchors.end(),
                          [&](auto &x) { return x.id == a.anchor_id; });
    if (k == d.anchors.end() || k->start_offset == k->end_offset)
      continue;
    auto key = a.type_key;
    auto &tag = semantic_tags_[key];
    if (!tag) {
      tag = buffer->create_tag("inde-semantic-" + key);
      // Author formatting and spelling diagnostics keep visual precedence.
      tag->set_priority(0);
      tag->property_underline_set() = false;
      // Translucent, non-additive backgrounds retain the underlying paper and
      // glyph contrast. GTK tag priority chooses one paint on overlaps; the
      // tooltip identifies every role rather than silently choosing meaning.
      tag->property_background_rgba() = Gdk::RGBA(
          key == "inde.semantic.dialogue"         ? "rgba(60,155,220,0.28)"
          : key == "inde.semantic.thought"        ? "rgba(175,115,215,0.28)"
          : key == "inde.semantic.description"    ? "rgba(90,175,110,0.28)"
          : key == "inde.semantic.marked-passage" ? "rgba(215,160,55,0.28)"
          : key == "inde.semantic.perspective"    ? "rgba(220,100,130,0.28)"
                                                  : "rgba(135,155,180,0.28)");
      tag->property_background_set() = true;
      tag->property_foreground_set() = false;
    }
    buffer->apply_tag(tag, buffer->get_iter_at_offset(k->start_offset),
                      buffer->get_iter_at_offset(k->end_offset));
  }
}

bool WritingWorkspace::semantic_tooltip(
    int x, int y, bool keyboard, const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
  if (!current_document_ || !semantic_lens_button_.get_active()) return false;
  try {
    auto buffer = editor_.get_buffer();
    auto it = buffer->get_iter_at_mark(buffer->get_insert());
    if (!keyboard) {
      int bx{}, by{};
      editor_.window_to_buffer_coords(Gtk::TextWindowType::WIDGET, x, y, bx, by);
      if (!editor_.get_iter_at_location(it, bx, by)) return false;
    }
    const auto pos = static_cast<std::size_t>(it.get_offset());
    const auto types = service_.writing().semantic_types();
    std::string text;
    auto d = capture_editor_state();
    application::SemanticAnchorEngine::inspect(d);
    for (const auto &a : d.semantics.annotations) {
      if (a.status == "archived") continue;
      auto k = std::find_if(d.anchors.begin(), d.anchors.end(),
                            [&](auto &v) { return v.id == a.anchor_id; });
      auto e = std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                            [&](auto &v) { return v.anchor_id == a.anchor_id; });
      if (k == d.anchors.end() || e == d.semantics.anchors.end() ||
          e->state != project::AnchorResolution::Resolved ||
          pos < k->start_offset || pos >= k->end_offset) continue;
      const auto *t = project::find_annotation_type(types, a.type_key, a.type_version);
      if (!text.empty()) text += "\n\n";
      text += (t ? author::annotation_label(*t) : a.type_key) + " — " +
              (a.status == "incomplete" ? "campos pendentes" : "ativa");
      // Type declarations supply useful field labels; entities stay canonical
      // and enum labels never reinterpret free-text author content.
      bool detail = false;
      if (t) for (const auto &f : t->fields) {
        auto v = a.fields.find(f.key);
        if (v == a.fields.end() || v->second.values.empty()) continue;
        text += "\n" + author::field_label(*t, f) + ": ";
        const auto &first = v->second.values.front();
        const auto detail_text = v->second.kind == project::SemanticFieldKind::Entity
                    ? entity_label(first)
                    : v->second.kind == project::SemanticFieldKind::Enum
                          ? value_label(first) : first;
        text += Glib::ustring(detail_text).substr(0, 120).raw();
        if (v->second.values.size() > 1)
          text += " (+" + std::to_string(v->second.values.size()-1) + ")";
        detail = true;
        if (a.type_key != "inde.semantic.perspective" || f.key == "viewpoint")
          break;
      }
      if (!detail) text += "\nTrecho: " + std::to_string(k->end_offset-k->start_offset) + " caracteres";
      for(std::size_t n=0;n<std::min<std::size_t>(2,a.dates.size());++n) {
        const auto &b=a.dates[n];const auto r=service_.worldbuilding().temporal_reference(b.reference_id);
        text+="\nTempo ("+value_label(b.role)+"): "+project::format_temporal_reference(service_.worldbuilding().calendar(r.calendar_id),r);
      }
      if(a.dates.size()>2)text+="\nOutras datas: consulte o detalhe da anotação.";
      if (text.size() > 4000) { text += "\nOutras marcações: consulte Marcações do texto."; break; }
    }
    if (text.empty()) return false;
    tooltip->set_text(text);
    return true;
  } catch (const std::exception &) { return false; }
}
} // namespace inde::ui

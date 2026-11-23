#include "inde/ui/text_coordinates.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/project/document_alternatives.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include <algorithm>
#include <glibmm/main.h>
#include <stdexcept>
namespace inde::ui {
bool WritingWorkspace::paragraph_before_edit(std::size_t start, std::size_t end, const std::string &text) {
  if (!current_document_) return true;
  try {
    // Prepare evidence against the old representation, but publish it only
    // after every structural guard succeeds. A refused edit changes nothing.
    auto semantics = current_document_->semantics;
    if (!semantics.anchors.empty()) {
      auto before = capture_editor_state();
      application::SemanticAnchorEngine::edit(before,start,end,text);
      semantics = std::move(before.semantics);
    }
    auto paragraphs=project::edit_paragraphs(current_document_->paragraphs,start,end,text);
    auto resmas=project::edit_alternatives(current_document_->resmas,
        editor_.get_buffer()->get_char_count(),start,end,project::utf8_character_count(text),focused_resma_id_);
    current_document_->paragraphs=std::move(paragraphs);
    if(project::canonical_paragraphs(current_document_->paragraphs))
      current_document_->content=project::render_paragraphs(current_document_->paragraphs);
    current_document_->resmas=std::move(resmas);
    current_document_->semantics=std::move(semantics);
    return true;
  } catch (const std::exception &e) {
    signal_status_message_.emit(std::string("Edição não aplicada: ") + e.what());
    return false;
  }
}
void WritingWorkspace::show_document_paragraphs(std::size_t offset) {
  auto *owner = owner_window();
  if (!owner || !current_document_) return;
  try {
    flush_changes();
    const auto observed = capture_edit_state();
    const auto &structure = observed.document.paragraphs;
    auto *d = new OverlayDialog("Parágrafos do documento", *owner, true);
    d->set_secondary_text(structure.enabled
      ? "Identidades estruturais, sem interpretação narrativa. Escolher um parágrafo seleciona seu texto no editor normal. Trechos manuais continuam em Trechos marcados."
      : "Este Documento ainda não tem identidades de parágrafo. Identificar conserva texto, formatação e trechos marcados; não cria alternativas nem marcações semânticas.");
    if (!structure.enabled) {
      d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
      d->add_button("Identificar parágrafos", Gtk::ResponseType::ACCEPT);
      d->signal_response().connect(sigc::track_obj([this, d, observed](int r) mutable {
        if (r != Gtk::ResponseType::ACCEPT) return;
        try {
          auto next = observed;
          next.document.paragraphs = project::identify_paragraphs(next.document.content);
          apply_edit_transaction(std::move(next), "Identificar parágrafos do Documento");
          Glib::signal_idle().connect_once(sigc::track_obj([this] { show_document_paragraphs(); }, *this));
        } catch (const std::exception &e) {
          d->keep_open_on_response(); signal_status_message_.emit(e.what());
        }
      }, *this));
    } else {
      offset = std::min(offset / 50 * 50, (structure.rows.size() - 1) / 50 * 50);
      auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
      scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
      scroll->set_min_content_height(180);
      scroll->set_max_content_height(320);
      auto *rows = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
      const auto buffer = editor_.get_buffer();
      for (std::size_t i = offset; i < std::min(offset + 50, structure.rows.size()); ++i) {
        const auto &p = structure.rows[i];
        const auto excerpt = buffer->get_text(buffer->get_iter_at_offset(gtk_text_offset(p.start)),
            buffer->get_iter_at_offset(gtk_text_offset(std::min(p.end, p.start + 72)))).raw();
        auto *button = Gtk::make_managed<Gtk::Button>(std::to_string(i + 1) + ". " + (excerpt.empty() ? "(parágrafo vazio)" : excerpt));
        set_accessible_label(*button, "Selecionar parágrafo " + std::to_string(i + 1));
        button->set_tooltip_text("Identidade: " + p.id);
        button->signal_clicked().connect(sigc::track_obj([this, d, p, id=observed.document.id] {
          if (!current_document_ || current_document_->id != id) return;
          auto buffer = editor_.get_buffer();
          buffer->select_range(buffer->get_iter_at_offset(gtk_text_offset(p.start)), buffer->get_iter_at_offset(gtk_text_offset(p.end)));
          d->hide();
          Glib::signal_idle().connect_once([d] { delete d; });
          focus_common_sheet();
        }, *this));
        rows->append(*button);
      }
      scroll->set_child(*rows); d->get_content_area()->append(*scroll);
      auto *summary = Gtk::make_managed<Gtk::Label>(std::to_string(structure.rows.size()) + " parágrafos · " + std::to_string(structure.retired.size()) + " identidades retiradas · página " + std::to_string(offset / 50 + 1));
      d->get_content_area()->append(*summary);
      if (structure.rows.size() > 50) {
        auto *bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
        for (int direction : {-1, 1}) {
          auto *button = Gtk::make_managed<Gtk::Button>(direction < 0 ? "Página anterior" : "Próxima página");
          button->set_sensitive(direction < 0 ? offset > 0 : offset + 50 < structure.rows.size());
          button->signal_clicked().connect(sigc::track_obj([this, d, offset, direction] {
            d->hide();
            Glib::signal_idle().connect_once(sigc::track_obj([this, d, offset, direction] {
              delete d; show_document_paragraphs(direction < 0 ? offset - 50 : offset + 50);
            }, *this));
          }, *this));
          bar->append(*button);
        }
        d->get_content_area()->append(*bar);
      }
      d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    }
    d->present();
  } catch (const std::exception &e) { show_error("Não foi possível consultar os parágrafos", e); }
}
}

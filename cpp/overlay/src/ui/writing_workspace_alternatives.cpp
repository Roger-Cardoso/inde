#include "inde/ui/text_coordinates.hpp"
#include "inde/application/document_alternatives.hpp"
#include "inde/project/document_alternatives.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <algorithm>
#include <glibmm/main.h>
#include <stdexcept>
namespace inde::ui {
void WritingWorkspace::build_alternatives_ui() {
  auto *create = Gtk::make_managed<Gtk::Button>("Criar alternativa do trecho");
  create->signal_clicked().connect([this] {
    document_popover_.popdown();
    create_text_alternative();
  });
  document_menu_.append(*create);
  auto *all = Gtk::make_managed<Gtk::Button>("Folhas e alternativas…");
  all->signal_clicked().connect([this] {
    document_popover_.popdown();
    show_document_alternatives();
  });
  document_menu_.append(*all);
  alternative_bar_.set_halign(Gtk::Align::END);
  current_leaf_.set_max_width_chars(24);
  current_leaf_.set_ellipsize(Pango::EllipsizeMode::END);
  alternative_bar_.append(previous_leaf_);
  alternative_bar_.append(current_leaf_);
  alternative_bar_.append(next_leaf_);
  alternative_bar_.append(manage_leaves_);
  set_accessible_label(previous_leaf_, "Folha anterior");
  set_accessible_label(next_leaf_, "Próxima folha");
  previous_leaf_.set_tooltip_text("Mostrar folha anterior desta resma");
  next_leaf_.set_tooltip_text("Mostrar próxima folha desta resma");
  previous_leaf_.signal_clicked().connect([this] { step_text_leaf(-1); });
  next_leaf_.signal_clicked().connect([this] { step_text_leaf(1); });
  manage_leaves_.signal_clicked().connect([this] {
    if (!focused_resma_id_.empty())
      show_resma(focused_resma_id_);
  });
  alternative_bar_.set_visible(false);
  // Reuse the header height: entering/leaving a resma must not push the
  // continuous text up and down or cover a line with floating controls.
  editor_toolbar_.append(alternative_bar_);
}
void WritingWorkspace::refresh_alternative_context() {
  if (loading_editor_ || restoring_undo_)
    return;
  if (!current_document_) {
    alternative_bar_.set_visible(false);
    focused_resma_id_.clear();
    return;
  }
  auto buffer = editor_.get_buffer();
  const auto p = buffer->get_iter_at_mark(buffer->get_insert()).get_offset();
  focused_resma_id_ =
      project::resma_at(current_document_->resmas, p, focused_resma_id_);
  const auto r = std::find_if(
      current_document_->resmas.begin(), current_document_->resmas.end(),
      [&](const auto &x) { return x.id == focused_resma_id_; });
  alternative_bar_.set_visible(r != current_document_->resmas.end());
  if (r == current_document_->resmas.end())
    return;
  const auto l = std::find_if(r->leaves.begin(), r->leaves.end(),
                              [&](const auto &x) { return x.id == r->top_id; });
  const auto n = static_cast<std::size_t>(l - r->leaves.begin());
  current_leaf_.set_text(l->label + " · " + std::to_string(n + 1) + "/" +
                         std::to_string(r->leaves.size()) +
                         (r->start == r->end ? " · trecho vazio" : ""));
  current_leaf_.set_tooltip_text("Resma: " + r->id +
                                 "\nFolha no topo: " + l->id);
  previous_leaf_.set_sensitive(n > 0);
  next_leaf_.set_sensitive(n + 1 < r->leaves.size());
}
void WritingWorkspace::create_text_alternative() {
  if (!current_document_)
    return;
  try {
    flush_changes();
    auto before = capture_edit_state();
    const auto a = std::min(before.selection.cursor, before.selection.bound),
               b = std::max(before.selection.cursor, before.selection.bound);
    auto start = a, end = b;
    const auto id =
        project::resma_at(before.document.resmas, a, focused_resma_id_);
    if (!id.empty()) {
      const auto &r = *std::find_if(before.document.resmas.begin(),
                                    before.document.resmas.end(),
                                    [&](const auto &x) { return x.id == id; });
      if (a != b && (a != r.start || b != r.end))
        throw std::runtime_error(
            "A seleção cruza parte de uma resma. Use a resma inteira ou "
            "selecione um trecho fora dela; resmas não são aninhadas.");
    } else if (a == b) {
      auto x = editor_.get_buffer()->get_iter_at_offset(gtk_text_offset(a));
      x.set_line_offset(0);
      auto y = x;
      y.forward_to_line_end();
      start = x.get_offset();
      end = y.get_offset();
    }
    if (id.empty()) {
      const auto target = require_visible_interval(start, end);
      start = target.start; end = target.end;
    }
    auto after = application::create_alternative(before, start, end, id);
    const auto chosen = id.empty() ? after.document.resmas.back().id : id;
    if (apply_edit_transaction(std::move(after), "Criar folha alternativa")) {
      focused_resma_id_ = chosen;
      refresh_alternative_context();
      focus_common_sheet();
      signal_status_message_.emit(
          "Folha anterior conservada. Escreva a alternativa no mesmo "
          "Documento; as setas alternam a folha no topo.");
    }
  } catch (const std::exception &e) {
    show_error("Não foi possível criar alternativa", e);
  }
}
void WritingWorkspace::select_text_leaf(const std::string &resma,
                                        const std::string &leaf) {
  if (!current_document_)
    return;
  try {
    // Callers may pass IDs owned by current_document_; flushing can replace it.
    const auto resma_id = resma, leaf_id = leaf;
    flush_changes();
    auto next = application::select_document_leaf(capture_edit_state(),
                                                  resma_id, leaf_id);
    apply_edit_transaction(std::move(next), "Alternar folha no topo");
    focused_resma_id_ = resma_id;
    refresh_alternative_context();
    focus_common_sheet();
  } catch (const std::exception &e) {
    show_error("Não foi possível alternar folha", e);
  }
}
void WritingWorkspace::step_text_leaf(int direction) {
  if (!current_document_)
    return;
  const auto r = std::find_if(
      current_document_->resmas.begin(), current_document_->resmas.end(),
      [&](const auto &x) { return x.id == focused_resma_id_; });
  if (r == current_document_->resmas.end())
    return;
  const auto l = std::find_if(r->leaves.begin(), r->leaves.end(),
                              [&](const auto &x) { return x.id == r->top_id; });
  const auto n = static_cast<int>(l - r->leaves.begin()) + direction;
  if (n >= 0 && n < static_cast<int>(r->leaves.size()))
    select_text_leaf(r->id, r->leaves[n].id);
}
void WritingWorkspace::show_resma(const std::string &id) {
  if (!current_document_ || !owner_window())
    return;
  try {
    const auto resma_id = id;
    flush_changes();
    const auto observed = capture_edit_state();
    const auto at = std::find_if(
        observed.document.resmas.begin(), observed.document.resmas.end(),
        [&](const auto &r) { return r.id == resma_id; });
    if (at == observed.document.resmas.end())
      throw std::runtime_error("Resma não encontrada");
    const auto r = *at;
    auto *d = new OverlayDialog("Folhas deste trecho", *owner_window(), true);
    d->set_secondary_text(
        "Só a folha no topo participa da leitura. Apagar seu texto não apaga "
        "as outras folhas. Trocar não é desfazer o histórico. "
        "Marcações/vínculos dentro do trecho impedem a troca neste corte.");
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll->set_min_content_height(120);
    scroll->set_max_content_height(280);
    auto *rows = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    std::string current_label;
    for (const auto &l : r.leaves) {
      if (l.id == r.top_id)
        current_label = l.label;
      auto *button = Gtk::make_managed<Gtk::Button>(
          (l.id == r.top_id ? "No topo: " : "Usar: ") + l.label);
      button->set_sensitive(l.id != r.top_id);
      button->set_tooltip_text(l.label+"\nFolha: " + l.id);
      if(auto *label=dynamic_cast<Gtk::Label *>(button->get_child())) {
        label->set_max_width_chars(48);label->set_ellipsize(Pango::EllipsizeMode::END);
      }
      button->signal_clicked().connect(sigc::track_obj(
          [this, d, resma = r.id, leaf = l.id,
           document = observed.document.id] {
            d->hide();
            Glib::signal_idle().connect_once([d] { delete d; });
            if (current_document_ && current_document_->id == document)
              select_text_leaf(resma, leaf);
          },
          *this));
      rows->append(*button);
    }
    scroll->set_child(*rows);
    d->get_content_area()->append(*scroll);
    auto *name = Gtk::make_managed<Gtk::Entry>();
    name->set_text(current_label);
    name->set_placeholder_text("Nome da folha no topo");
    set_accessible_label(*name, "Nome da folha no topo");
    d->get_content_area()->append(*name);
    d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    d->add_button("Renomear folha", Gtk::ResponseType::APPLY);
    d->add_button("Nova alternativa", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect(sigc::track_obj(
        [this, d, name, r, document = observed.document.id](int response) {
          if (!current_document_ || current_document_->id != document)
            return;
          try {
            if (response == Gtk::ResponseType::APPLY) {
              flush_changes();
              auto next = application::rename_document_leaf(
                  capture_edit_state(), r.id, r.top_id, name->get_text());
              apply_edit_transaction(std::move(next), "Renomear folha");
              refresh_alternative_context();
            } else if (response == Gtk::ResponseType::ACCEPT) {
              flush_changes();
              auto next = application::create_alternative(capture_edit_state(),
                                                          r.start, r.end, r.id);
              apply_edit_transaction(std::move(next),
                                     "Criar folha alternativa");
              focused_resma_id_ = r.id;
              refresh_alternative_context();
              focus_common_sheet();
            }
          } catch (const std::exception &e) {
            d->keep_open_on_response();
            signal_status_message_.emit(e.what());
          }
        },
        *this));
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar as folhas", e);
  }
}
void WritingWorkspace::show_document_alternatives(std::size_t offset) {
  if (!current_document_ || !owner_window())
    return;
  try {
    flush_changes();
    const auto observed = capture_edit_state();
    auto *d = new OverlayDialog("Folhas e alternativas", *owner_window(), true);
    d->set_secondary_text(
        "Criar alternativa conserva a seleção atual; sem seleção, conserva o "
        "parágrafo no cursor. Uma folha vazia abre no mesmo editor. Resmas "
        "vazias continuam listadas; nenhuma é destruída por apagar texto.");
    const auto &all = observed.document.resmas;
    if (!all.empty())
      offset = std::min(offset / 25 * 25, (all.size() - 1) / 25 * 25);
    else
      offset = 0;
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroll->set_min_content_height(120);
    scroll->set_max_content_height(300);
    auto *rows = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    for (std::size_t i = offset; i < std::min(offset + 25, all.size()); ++i) {
      const auto r = all[i];
      auto *button = Gtk::make_managed<Gtk::Button>(
          r.label + " · " + std::to_string(r.leaves.size()) + " folhas" +
          (r.start == r.end ? " · vazio" : ""));
      button->signal_clicked().connect(sigc::track_obj(
          [this, d, r, document = observed.document.id] {
            d->hide();
            Glib::signal_idle().connect_once([d] { delete d; });
            if (!current_document_ || current_document_->id != document)
              return;
            focused_resma_id_ = r.id;
            auto b = editor_.get_buffer();
            b->select_range(b->get_iter_at_offset(gtk_text_offset(r.start)),
                            b->get_iter_at_offset(gtk_text_offset(r.end)));
            refresh_alternative_context();
            show_resma(r.id);
          },
          *this));
      rows->append(*button);
    }
    scroll->set_child(*rows);
    d->get_content_area()->append(*scroll);
    if (all.size() > 25) {
      auto *bar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
      for (int direction : {-1, 1}) {
        auto *button = Gtk::make_managed<Gtk::Button>(
            direction < 0 ? "Página anterior" : "Próxima página");
        button->set_sensitive(direction < 0 ? offset > 0
                                            : offset + 25 < all.size());
        button->signal_clicked().connect(sigc::track_obj(
            [this, d, offset, direction] {
              d->hide();
              Glib::signal_idle().connect_once(sigc::track_obj(
                  [this, d, offset, direction] {
                    delete d;
                    show_document_alternatives(direction < 0 ? offset - 25
                                                             : offset + 25);
                  },
                  *this));
            },
            *this));
        bar->append(*button);
      }
      d->get_content_area()->append(*bar);
    }
    d->add_button("Fechar", Gtk::ResponseType::CLOSE);
    d->add_button("Criar alternativa do trecho", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect(sigc::track_obj(
        [this](int r) {
          if (r == Gtk::ResponseType::ACCEPT)
            Glib::signal_idle().connect_once(
                sigc::track_obj([this] { create_text_alternative(); }, *this));
        },
        *this));
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar alternativas", e);
  }
}
} // namespace inde::ui

#include "inde/ui/text_coordinates.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <algorithm>
#include <stdexcept>
namespace inde::ui {
void WritingWorkspace::reset_editor_visibility() {
  visibility_idle_.disconnect();
  visibility_before_.reset();
  visibility_edits_.clear();
  active_visibility_id_.clear();
  editor_projection_ = {};
  if (hidden_text_tag_) {
    const auto b = editor_.get_buffer();
    b->remove_tag(hidden_text_tag_, b->begin(), b->end());
  }
  pending_edit_button_.set_visible(false);
  show_all_button_.set_visible(false);
  visibility_button_.set_label("Visão: integral");
}
void WritingWorkspace::show_all_editor_text() {
  try {
    require_completed_composition();
    flush_visibility_edits();
    commit_undo_group();
    active_visibility_id_.clear();
    invalidate_clipboard();
    refresh_editor_visibility();
    schedule_find();
    editor_.grab_focus();
  } catch (const std::exception &e) {
    show_error("Não foi possível mudar a visibilidade", e);
  }
}
void WritingWorkspace::refresh_editor_visibility() {
  if (refreshing_visibility_ || !hidden_text_tag_)
    return;
  refreshing_visibility_ = true;
  try {
    const auto b = editor_.get_buffer();
    b->remove_tag(hidden_text_tag_, b->begin(), b->end());
    editor_projection_ = {};
    if (current_document_ && !active_visibility_id_.empty()) {
      auto p =
          std::find_if(current_document_->visibility_profiles.begin(),
                       current_document_->visibility_profiles.end(),
                       [&](auto &x) { return x.id == active_visibility_id_; });
      if (p == current_document_->visibility_profiles.end())
        active_visibility_id_.clear();
      else {
        editor_projection_ = application::project_visibility(
            capture_editor_state(), active_visibility_id_);
        std::size_t start = 0;
        for (const auto &r : editor_projection_.spans) {
          if (start < r.source_start)
            b->apply_tag(hidden_text_tag_, b->get_iter_at_offset(gtk_text_offset(start)),
                         b->get_iter_at_offset(gtk_text_offset(r.source_start)));
          start = r.source_end;
        }
        if (start < std::size_t(b->get_char_count()))
          b->apply_tag(hidden_text_tag_, b->get_iter_at_offset(gtk_text_offset(start)),
                       b->end());
        const auto cursor = b->get_iter_at_mark(b->get_insert()).get_offset();
        if (!b->get_has_selection() && !editor_projection_.spans.empty() &&
            std::none_of(editor_projection_.spans.begin(),
                         editor_projection_.spans.end(), [&](auto &s) {
                           return s.editable &&
                                  s.source_start <= std::size_t(cursor) &&
                                  std::size_t(cursor) <= s.source_end;
                         })) {
          std::optional<std::size_t> at;
          for (const auto &s : editor_projection_.spans)
            if (s.editable) {
              at = s.source_end;
              if (s.source_start >= std::size_t(cursor)) {
                at = s.source_start;
                break;
              }
            }
          if (at)
            b->place_cursor(b->get_iter_at_offset(gtk_text_offset(*at)));
        }
        const Glib::ustring name(p->name);
        const auto short_name=name.size()>28?name.substr(0,28)+"…":name;
        const bool review=editor_projection_.text.empty() && !editor_projection_.diagnostics.empty();
        visibility_button_.set_label("Visão: " + short_name + (review?" — revisar":""));
        if(review)visibility_button_.add_css_class("warning");else visibility_button_.remove_css_class("warning");
        visibility_button_.set_tooltip_text(
            p->name+"\n"+std::to_string(
                project::utf8_character_count(editor_projection_.text)) +
            " de " + std::to_string(b->get_char_count()) +
            " caracteres exibidos. Omitidos conservados; este perfil não "
            "escolhe saída editorial." +
            (editor_projection_.diagnostics.empty()
                 ? ""
                 : " Há critérios sem origem confiável; reveja o perfil."));
      }
    }
    if (active_visibility_id_.empty()) {
      visibility_button_.set_label("Visão: integral");
      visibility_button_.remove_css_class("warning");
      visibility_button_.set_tooltip_text("A visibilidade não altera a origem nem escolhe a saída editorial. Aplicar um perfil conserva a mesma escrita.");
    }
    show_all_button_.set_visible(!active_visibility_id_.empty());
    if(object_editor_ && objects_can_leave() && !changing_specialization_ && !committing_object_document_ && document_surfaces_.get_visible_child_name()=="objects")flush_document_objects();
    refreshing_visibility_ = false;
    refresh_reading_blocks();
  } catch (...) {
    refreshing_visibility_ = false;
    throw;
  }
}
std::vector<application::TextRange>
WritingWorkspace::visible_ranges(std::size_t a, std::size_t b,
                                 bool editable) const {
  if (active_visibility_id_.empty())
    return a < b ? std::vector<application::TextRange>{{a, b}}
                 : std::vector<application::TextRange>{};
  return application::visibility_source_ranges(editor_projection_, a, b,
                                               editable);
}
application::TextRange
WritingWorkspace::require_visible_interval(std::size_t a, std::size_t b) const {
  if (active_visibility_id_.empty())
    return {a, b};
  if (a == b) {
    for (const auto &s : editor_projection_.spans)
      if (s.editable && s.source_start <= a && a <= s.source_end)
        return {a, b};
    throw std::runtime_error("O cursor não está num trecho visível editável. "
                             "Posicione-o no texto exibido.");
  }
  auto ranges = visible_ranges(a, b, true);
  if (ranges.size() != 1)
    throw std::runtime_error(
        "Esta ação cria um vínculo único. Selecione um trecho visível "
        "contínuo; partes separadas não serão unidas através de texto oculto.");
  return ranges.front();
}
bool WritingWorkspace::intercept_visibility_edit(std::size_t a, std::size_t b,
                                                 const std::string &text) {
  if (active_visibility_id_.empty() || applying_visibility_edits_)
    return false;
  try {
    invalidate_clipboard();
    if (text.size() > application::clipboard_limit ||
        text.find('\0') != std::string::npos)
      throw std::runtime_error("Entrada excedida ou inválida");
    if (!visibility_before_) {
      visibility_before_ = capture_edit_state();
    }
    const auto start =
                   application::visibility_view_offset(editor_projection_, a),
               end = application::visibility_view_offset(editor_projection_, b);
    if (!visibility_edits_.empty() && start == end && !text.empty() &&
        visibility_edits_.back().start == start &&
        visibility_edits_.back().end == end &&
        !visibility_edits_.back().text.empty())
      visibility_edits_.back().text += text;
    else
      visibility_edits_.push_back({start, end, text});
    if (!ime_active_) {
      flush_visibility_edits();
      return true;
    }
    if (!visibility_idle_.connected())
      visibility_idle_ = Glib::signal_timeout().connect(
          [this] {
            if (ime_active_)
              return true;
            try {
              flush_visibility_edits();
            } catch (const std::exception &e) {
              signal_status_message_.emit(e.what());
            }
            return false;
          },
          1);
  } catch (const std::exception &e) {
    signal_status_message_.emit(std::string("Entrada não aplicada: ") +
                                e.what());
  }
  return true; // Do not let GTK delete omitted source characters.
}
void WritingWorkspace::flush_visibility_edits() {
  if (applying_visibility_edits_ || visibility_edits_.empty())
    return;
  require_completed_composition();
  visibility_idle_.disconnect();
  applying_visibility_edits_ = true;
  try {
    if (!visibility_before_ ||
        !project::same_document_state(capture_editor_state(),
                                      visibility_before_->document, false))
      throw std::runtime_error("A origem mudou antes da aplicação da entrada");
    auto after = application::edit_visibility(
        *visibility_before_, editor_projection_, visibility_edits_);
    (void)application::project_visibility(
        after.document, active_visibility_id_); // Preflight presentation bounds
                                                // before replacing the buffer.
    // Native typing joins the existing typing group, rather than adding one
    // undo command per key (or separating deletion from its replacement).
    restore_edit_state(after);
    mark_dirty();
    visibility_edits_.clear();
    visibility_before_.reset();
    pending_edit_button_.set_visible(false);
    applying_visibility_edits_ = false;
  } catch (const std::exception &e) {
    bool recovered = false;
    try {
      if (visibility_before_) {
        restore_edit_state(*visibility_before_);
        recovered = true;
      }
    } catch (...) {
    }
    applying_visibility_edits_ = false;
    pending_edit_button_.set_visible(true);
    throw std::runtime_error(
        std::string(e.what()) +
        ". Tentativa conservada em Rever edição não aplicada; " +
        (recovered ? "origem restaurada."
                   : "confira a origem antes de continuar."));
  }
}
void WritingWorkspace::review_pending_visibility_edit() {
  if (!owner_window() || visibility_edits_.empty())
    return;
  auto *d = new OverlayDialog("Entrada não aplicada", *owner_window(), true);
  d->set_secondary_text(
      "A origem não foi substituída. Copie a entrada abaixo antes de descartar "
      "esta tentativa; depois ajuste a visibilidade na mesma área de escrita.");
  auto *text = Gtk::make_managed<Gtk::TextView>();
  text->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
  text->set_size_request(-1, 180);
  std::string draft;
  for (const auto &e : visibility_edits_)
    if (!e.text.empty())
      draft += e.text;
  text->get_buffer()->set_text(draft);
  d->get_content_area()->append(*text);
  d->add_button("Manter tentativa", Gtk::ResponseType::CANCEL);
  d->add_button("Descartar tentativa não aplicada", Gtk::ResponseType::REJECT);
  d->signal_response().connect(sigc::track_obj(
      [this](int r) {
        if (r == Gtk::ResponseType::REJECT) {
          visibility_edits_.clear();
          visibility_before_.reset();
          pending_edit_button_.set_visible(false);
        }
      },
      *this));
  d->present();
}
std::vector<application::ReadingBlock>
WritingWorkspace::visible_reading_blocks() const {
  if (!current_document_)
    return {};
  auto blocks = application::reading_blocks(*current_document_);
  if (active_visibility_id_.empty())
    return blocks;
  std::vector<application::ReadingBlock> out;
  for (auto b : blocks) {
    auto r = visible_ranges(b.start, b.end);
    if (!r.empty()) {
      b.start = r.front().start;
      b.end = r.back().end;
      out.push_back(std::move(b));
    } else if (b.start == b.end) {
      auto first=std::lower_bound(editor_projection_.spans.begin(),editor_projection_.spans.end(),b.start,
        [](const auto &s,auto point){return s.source_end<point;});
      for(auto it=first;it!=editor_projection_.spans.end() && it->source_start<=b.start;++it)
        if(it->editable && b.end<=it->source_end){out.push_back(std::move(b));break;}
    }
  }
  return out;
}
} // namespace inde::ui

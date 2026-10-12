#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/accessibility.hpp"
#include "editor_diagnostics.hpp"
#include <algorithm>
#include <stdexcept>

namespace inde::ui {
void WritingWorkspace::build_search_ui() {
  find_button_.set_tooltip_text("Localizar no Documento (Ctrl+F); substituir: Ctrl+H");
  formatting_toolbar_.append(find_button_);
  find_button_.signal_clicked().connect([this] { open_find(false); });
  find_query_.set_placeholder_text("Localizar no Documento");
  find_replacement_.set_placeholder_text("Substituir por (vazio apaga)");
  for (auto *entry : {&find_query_, &find_replacement_}) {
    entry->set_hexpand(true);
    entry->set_width_chars(12);
    entry->set_input_purpose(Gtk::InputPurpose::TERMINAL);
  }
  set_accessible_label(find_query_, "Localizar no Documento");
  set_accessible_label(find_replacement_, "Substituir por");
  find_row_.append(find_query_);
  find_row_.append(find_previous_);
  find_row_.append(find_next_);
  find_row_.append(find_replace_mode_);
  find_row_.append(find_close_);
  replace_row_.append(find_replacement_);
  replace_row_.append(replace_one_);
  replace_row_.append(replace_all_);
  find_panel_.append(find_row_);
  find_panel_.append(find_case_);
  find_panel_.append(find_word_);
  find_panel_.append(replace_row_);
  find_status_.set_xalign(0);
  find_status_.set_ellipsize(Pango::EllipsizeMode::END);
  find_panel_.append(find_status_);
  find_revealer_.set_child(find_panel_);
  find_revealer_.set_transition_type(Gtk::RevealerTransitionType::SLIDE_DOWN);
  editor_page_.append(find_revealer_);
  find_query_.signal_changed().connect([this] { schedule_find(); });
  find_case_.signal_toggled().connect([this] { schedule_find(); });
  find_word_.signal_toggled().connect([this] { schedule_find(); });
  find_query_.signal_activate().connect([this] { navigate_find(false); });
  find_replacement_.signal_activate().connect([this] { replace_find(false); });
  find_previous_.signal_clicked().connect([this] { navigate_find(true); });
  find_next_.signal_clicked().connect([this] { navigate_find(false); });
  find_close_.signal_clicked().connect(sigc::mem_fun(*this, &WritingWorkspace::close_find));
  find_replace_mode_.signal_clicked().connect([this] { open_find(true); });
  replace_one_.signal_clicked().connect([this] { replace_find(false); });
  replace_all_.signal_clicked().connect([this] { replace_find(true); });
  auto keys = Gtk::EventControllerKey::create();
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect([this](guint key, guint, Gdk::ModifierType modifiers) {
    if (ime_active_) return false;
    if (!current_document_ || page_stack_.get_visible_child_name() != "editor") return false;
    const auto mask=static_cast<unsigned>(modifiers)&(GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK);
    if (mask == GDK_CONTROL_MASK) {
      // Ctrl+Shift+F belongs to the shell's Focus action, not local Find.
      if ((modifiers & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType{}) return false;
      if (key == GDK_KEY_f || key == GDK_KEY_F) { open_find(false); return true; }
      if (key == GDK_KEY_h || key == GDK_KEY_H) { open_find(true); return true; }
    }
    if (!find_revealer_.get_reveal_child()) return false;
    if (key == GDK_KEY_Escape && mask==0) { close_find(); return true; }
    if (key == GDK_KEY_F3 && (mask==0 || mask==GDK_SHIFT_MASK)) {
      navigate_find((modifiers & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType{});
      return true;
    }
    return false;
  }, false);
  // Local scope: shortcuts cannot edit behind a modal or in another workspace.
  add_controller(keys);
  invalidate_find();
}

void WritingWorkspace::open_find(bool replace) {
  if (ime_active_) { signal_status_message_.emit("Conclua a composição IME antes de abrir a busca."); return; }
  if (!get_mapped() || !current_document_ || page_stack_.get_visible_child_name() != "editor") return;
  find_revealer_.set_reveal_child(true);
  replace_row_.set_visible(replace);
  find_replace_mode_.set_visible(!replace);
  auto buffer = editor_.get_buffer();
  Gtk::TextBuffer::iterator start, end;
  if (buffer->get_selection_bounds(start, end) && end.get_offset() - start.get_offset() <= 512) {
    const auto selected = buffer->get_text(start, end).raw();
    if (selected.find('\n') == std::string::npos) find_query_.set_text(selected);
  }
  find_query_.grab_focus();
  find_query_.select_region(0, -1);
  schedule_find();
}
void WritingWorkspace::close_find() {
  find_revealer_.set_reveal_child(false);
  invalidate_find();
  if (current_document_ && get_mapped() && page_stack_.get_visible_child_name() == "editor")
    editor_.grab_focus();
}
void WritingWorkspace::invalidate_find() {
  find_debounce_.disconnect();
  find_delivery_.disconnect();
  find_analysis_.invalidate();
  find_result_ = {};
  find_index_ = 0;
  for (auto *button : {&find_previous_, &find_next_, &replace_one_, &replace_all_})
    button->set_sensitive(false);
}
void WritingWorkspace::schedule_find() {
  invalidate_find();
  if (!current_document_ || !find_revealer_.get_reveal_child()) return;
  if (find_query_.get_text().empty()) {
    find_status_.set_text("Digite um texto para localizar; busca literal, sem expressão regular.");
    return;
  }
  find_status_.set_text("Pesquisando…");
  find_debounce_ = Glib::signal_timeout().connect([this] { request_find(); return false; }, 150);
}
void WritingWorkspace::request_find() {
  if (!current_document_ || !find_revealer_.get_reveal_child()) return;
  EditorPhaseTimer measurement("find_snapshot_submit");
  application::TextSearchRequest query{find_query_.get_text().raw(), find_case_.get_active(), find_word_.get_active()};
  find_analysis_.submit({current_document_->id, editor_.get_buffer()->get_text().raw(), {}, query});
  find_delivery_ = Glib::signal_timeout().connect([this] {
    if (auto result = find_analysis_.take_latest()) {
      editor_diagnostic("find_worker", result->elapsed);
      if (!current_document_ || result->document_id != current_document_->id || !find_revealer_.get_reveal_child()) return false;
      if (!result->search) { find_status_.set_text("Busca indisponível. Reabra a busca para tentar novamente."); return false; }
      find_result_ = std::move(*result->search);
      const bool found = !find_result_.matches.empty();
      find_next_.set_sensitive(found); find_previous_.set_sensitive(found);
      replace_one_.set_sensitive(found); replace_all_.set_sensitive(found && !find_result_.truncated);
      if (!found) { find_status_.set_text("Nenhuma ocorrência no Documento."); return false; }
      const auto cursor = static_cast<std::size_t>(editor_.get_buffer()->get_iter_at_mark(editor_.get_buffer()->get_insert()).get_offset());
      const auto it = std::lower_bound(find_result_.matches.begin(), find_result_.matches.end(), cursor,
          [](const auto &match, auto offset) { return match.start < offset; });
      select_find_match(it == find_result_.matches.end() ? 0 : static_cast<std::size_t>(it - find_result_.matches.begin()), false);
      return false;
    }
    return find_analysis_.busy();
  }, 30);
}
void WritingWorkspace::select_find_match(std::size_t index, bool focus) {
  if (index >= find_result_.matches.size()) return;
  find_index_ = index;
  const auto match = find_result_.matches[index];
  auto buffer = editor_.get_buffer();
  buffer->select_range(buffer->get_iter_at_offset(static_cast<int>(match.end)),
                       buffer->get_iter_at_offset(static_cast<int>(match.start)));
  editor_.scroll_to(buffer->get_insert());
  find_status_.set_text(std::to_string(index + 1) + " de " + std::to_string(find_result_.matches.size()) +
      (find_result_.truncated ? " ocorrências — limite atingido; refine a busca. Substituir tudo desativado." : " ocorrências"));
  if (focus) editor_.grab_focus();
}
void WritingWorkspace::navigate_find(bool previous) {
  if (find_result_.matches.empty()) return;
  select_find_match(previous ? (find_index_ + find_result_.matches.size() - 1) % find_result_.matches.size()
                             : (find_index_ + 1) % find_result_.matches.size(), false);
}
void WritingWorkspace::replace_find(bool all) {
  if (!current_document_ || find_result_.matches.empty()) return;
  try {
    auto plan = find_result_;
    if (!all) {
      const auto match = plan.matches.at(find_index_);
      Gtk::TextBuffer::iterator start, end;
      if (!editor_.get_buffer()->get_selection_bounds(start, end) ||
          static_cast<std::size_t>(start.get_offset()) != match.start || static_cast<std::size_t>(end.get_offset()) != match.end) {
        select_find_match(find_index_, false);
        find_status_.set_text("Ocorrência selecionada. Clique novamente para substituí-la.");
        return;
      }
      plan.matches = {match}; plan.truncated = false;
    }
    const auto before = capture_edit_state();
    const auto after = application::replace_text(before, plan, find_replacement_.get_text().raw());
    if (after.document.content == before.document.content) {
      find_status_.set_text("A substituição não altera o Documento."); return;
    }
    apply_edit_transaction(after, all ? "Substituir tudo" : "Substituir ocorrência");
    signal_status_message_.emit(all ? "Substituição concluída em uma operação reversível" : "Ocorrência substituída");
  } catch (const std::exception &error) { show_error("Não foi possível substituir no Documento", error); }
}
bool WritingWorkspace::apply_edit_transaction(application::DocumentEditState after, const std::string &label) {
  require_completed_composition();
  EditorPhaseTimer measurement("compound_edit_gtk");
  commit_undo_group();
  const auto before = capture_edit_state();
  bool changed = false;
  try {
    restore_edit_state(after);
    // GTK can canonicalize adjacent spans; history records the actual UI state.
    changed = edit_session_.commit(capture_edit_state(), label);
  } catch (...) {
    restore_edit_state(before);
    throw;
  }
  if (changed) { ++edit_revision_; save_error_.clear(); schedule_autosave(); schedule_recovery(); }
  update_editor_status(); update_undo_actions();
  return changed;
}
} // namespace inde::ui

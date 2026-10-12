#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/writing_workspace.hpp"

#include "inde/ui/accessibility.hpp"
#include "inde/ui/context_menu.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "writing_workspace_util.hpp"
#include "editor_diagnostics.hpp"

namespace inde::ui {

project::Document WritingWorkspace::capture_editor_state() {
  if (!current_document_)
    throw std::runtime_error("Nenhum Documento está aberto");
  auto value = *current_document_;
  value.title = title_entry_.get_text();
  value.content = editor_.get_buffer()->get_text();
  const auto placement = placement_.get_active_id().raw();
  value.editorial_node_id = placement.empty() || placement == "none"
                                ? std::nullopt
                                : std::optional<std::string>{placement};
  value.word_goal = goal_enabled_.get_active()
                        ? std::optional<std::size_t>{static_cast<std::size_t>(
                              word_goal_.get_value_as_int())}
                        : std::nullopt;
  value.formatting = capture_formatting();
  value.anchors = capture_anchors();
  value.entity_references = entity_references_;
  return value;
}

application::DocumentEditState WritingWorkspace::capture_edit_state() {
  const auto buffer = editor_.get_buffer();
  return {capture_editor_state(),
          {static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_insert()).get_offset()),
           static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset())}};
}

void WritingWorkspace::restore_edit_state(const application::DocumentEditState &state) {
  restore_editor_state(state.document, true);
  const auto buffer = editor_.get_buffer();
  restoring_undo_ = true;
  buffer->select_range(buffer->get_iter_at_offset(static_cast<int>(state.selection.cursor)),
                       buffer->get_iter_at_offset(static_cast<int>(state.selection.bound)));
  restoring_undo_ = false;
}

void WritingWorkspace::restore_editor_state(const project::Document &state, bool incremental) {
  EditorPhaseTimer measurement("restore_document_gtk");
  invalidate_text_analysis();
  loading_editor_ = true;
  restoring_undo_ = true;
  current_document_ = state;
  title_entry_.set_text(state.title);
  auto buffer = editor_.get_buffer();
  if (incremental) {
    EditorPhaseTimer delta_timer("restore_incremental_gtk");
    const auto before = buffer->get_text().raw();
    const auto delta = application::text_delta(before, state.content);
    const auto start = static_cast<int>(g_utf8_pointer_to_offset(before.data(), before.data() + delta.byte_offset));
    if (!delta.removed.empty()) {
      auto a = buffer->get_iter_at_offset(start);
      auto b = buffer->get_iter_at_offset(start + static_cast<int>(project::utf8_character_count(delta.removed)));
      buffer->erase(a, b);
    }
    if (!delta.inserted.empty()) buffer->insert(buffer->get_iter_at_offset(start), delta.inserted);
  } else buffer->set_text(state.content);
  if (!placement_.set_active_id(
          state.editorial_node_id ? *state.editorial_node_id : "none"))
    placement_.set_active_id("none");
  const auto placement = placement_.get_active_id().raw();
  open_placement_button_.set_sensitive(!placement.empty() &&
                                       placement != "none");
  goal_enabled_.set_active(state.word_goal.has_value());
  word_goal_.set_value(
      static_cast<double>(state.word_goal.value_or(std::size_t{1000})));
  restore_formatting(state.formatting);
  restore_anchor_marks(state.anchors);
  entity_references_ = state.entity_references;
  selected_anchor_id_.reset();
  selected_entity_reference_id_.reset();
  restoring_undo_ = false;
  loading_editor_ = false;
  refresh_anchor_list();
  refresh_semantic_lenses();
  refresh_entity_references();
  refresh_text_tools();
  schedule_find();
}

void WritingWorkspace::mark_dirty() {
  if (loading_editor_ || restoring_undo_ || !current_document_)
    return;
  edit_session_.note_mutation();
  ++edit_revision_;
  save_error_.clear();
  invalidate_text_analysis();
  // Qualquer mutação nova cria um ramo de edição e invalida o refazer antes
  // mesmo de o grupo de digitação ser consolidado.
  update_editor_status();
  undo_group_connection_.disconnect();
  undo_group_connection_ = Glib::signal_timeout().connect(
      [this] {
        try { commit_undo_group(); }
        catch (const std::exception &error) {
          show_error("Não foi possível consolidar a edição reversível", error);
        }
        return false;
      },
      650);
  analysis_connection_.disconnect();
  analysis_connection_ = Glib::signal_timeout().connect(
      [this] {
        refresh_text_tools();
        refresh_anchor_list();
        return false;
      },
      260);
  update_undo_actions();
  schedule_autosave();
  schedule_recovery();
  schedule_find();
}

void WritingWorkspace::schedule_autosave() {
  autosave_connection_.disconnect();
  autosave_connection_ = Glib::signal_timeout().connect(
      [this] {
        EditorPhaseTimer measurement("autosave_callback");
        try { queue_autosave(); }
        catch (const std::exception &error) {
          save_error_ = author::diagnostic(error.what()); update_editor_status();
          signal_status_message_.emit("Autosave falhou; alterações permanecem nesta sessão. Use Salvar para tentar novamente.");
        }
        return false;
      },
      900);
}

void WritingWorkspace::commit_undo_group() {
  if (ime_active_) return;
  EditorPhaseTimer measurement("undo_group");
  const bool group_pending = undo_group_connection_.connected();
  undo_group_connection_.disconnect();
  if ((!group_pending && !edit_session_.pending()) || !current_document_)
    return;
  edit_session_.commit(capture_edit_state(), ime_commit_pending_ ? "Composição IME" : "Digitação");
  update_editor_status();
}

void WritingWorkspace::undo() try {
  require_completed_composition();
  if (!current_document_ || page_stack_.get_visible_child_name() != "editor")
    return;
  commit_undo_group();
  const auto target = edit_session_.undo();
  if (!target) return;
  ++edit_revision_; save_error_.clear();
  restore_edit_state(*target);
  update_editor_status();
  update_undo_actions();
  schedule_autosave();
  schedule_recovery();
} catch (const std::exception &error) {
  show_error("Não foi possível desfazer", error);
}

void WritingWorkspace::redo() try {
  require_completed_composition();
  if (!current_document_ || page_stack_.get_visible_child_name() != "editor" ||
      !edit_session_.can_redo())
    return;
  undo_group_connection_.disconnect();
  const auto target = edit_session_.redo();
  if (!target) return;
  ++edit_revision_; save_error_.clear();
  restore_edit_state(*target);
  update_editor_status();
  update_undo_actions();
  schedule_autosave();
  schedule_recovery();
} catch (const std::exception &error) {
  show_error("Não foi possível refazer", error);
}

void WritingWorkspace::update_undo_actions() {
  undo_button_.set_sensitive(edit_session_.can_undo());
  redo_button_.set_sensitive(edit_session_.can_redo());
}

void WritingWorkspace::update_editor_status() {
  const bool dirty = edit_session_.dirty();
  const bool saving = save_worker_.busy();
  editor_status_.set_text(
      !save_error_.empty() ? "Falha ao salvar — alterações preservadas nesta sessão" :
      saving ? "Salvamento em andamento — edição liberada" :
      dirty ? "Alterações pendentes — salvamento automático" : "Salvo");
  save_button_.set_sensitive(dirty || saving || !save_error_.empty());
  update_undo_actions();
  update_recovery_status();
}

Gtk::Window *WritingWorkspace::owner_window() {
  return dynamic_cast<Gtk::Window *>(get_root());
}

void WritingWorkspace::show_error(const Glib::ustring &title,
                                  const std::exception &error) {
  auto *owner = owner_window();
  if (!owner)
    return;
  auto *dialog =
      new OverlayDialog(*owner, author::diagnostic(error.what()), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->show();
}

std::string WritingWorkspace::placement_label(
    const std::optional<std::string> &node_id) const {
  if (!node_id)
    return "Documento livre";
  const auto found = placement_labels_.find(*node_id);
  return found == placement_labels_.end() ? "Colocação editorial indisponível"
                                          : found->second;
}

std::string WritingWorkspace::entity_label(const std::string &entity_id) const {
  const auto entity = service_.narrative().entity(entity_id);
  return entity ? entity->name : "Elemento da história indisponível";
}

std::string
WritingWorkspace::search_match_label(project::DocumentSearchMatch match) {
  switch (match) {
  case project::DocumentSearchMatch::Name:
    return "Nome";
  case project::DocumentSearchMatch::Content:
    return "Conteúdo";
  case project::DocumentSearchMatch::NameAndContent:
    return "Nome e conteúdo";
  case project::DocumentSearchMatch::None:
    return "";
  }
  return "";
}

} // namespace inde::ui

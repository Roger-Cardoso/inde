#include "document_recovery_transport.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "object_native_ui.hpp"
#include <stdexcept>
namespace inde::ui {
bool WritingWorkspace::poll_project_flush() {
  if (file_flush_stage_ == 0) {
    if (!objects_can_leave())
      throw std::runtime_error(
          "Aplique ou cancele a edição aberta antes de confirmar o projeto.");
    file_flush_stage_ = 1;
    common_sheet_idle_.disconnect();
    recovery_debounce_.disconnect();
    recovery_periodic_.disconnect();
    flush_visibility_edits();
    composition_page_.flush();
    require_completed_composition();
    autosave_connection_.disconnect();
    if (!current_document_) {
      file_flush_stage_ = 3;
      return true;
    }
    queue_recovery(true);
    file_flush_stage_ = 1;
    return false;
  }
  if (file_flush_stage_ == 1) {
    if (recovery_worker_.busy())
      return false;
    receive_recovery_result();
    receive_save_result();
    queue_autosave();
    file_flush_stage_ = 2;
    return false;
  }
  if (file_flush_stage_ == 2) {
    if (save_worker_.busy() || recovery_worker_.busy())
      return false;
    receive_recovery_result();
    receive_save_result();
    if (!save_error_.empty())
      throw std::runtime_error(save_error_);
    if (edit_session_.dirty())
      throw std::runtime_error(
          "O Documento ainda possui alterações não salvas.");
    // Preserve the native undo-to-baseline acknowledgment contract. This rare
    // readback remains UI-owned; it never blesses a stale baseline.
    if (saved_document_ && service_.current() && !recovery_session_.empty() &&
        saved_revision_ < edit_revision_) {
      try {
        const auto disk = service_.writing().document(saved_document_->id);
        if (disk && project::same_document_state(*disk, *saved_document_)) {
          persistence::DocumentRecoveryStore{}.acknowledge(
              *service_.current(), recovery_session_, *disk, edit_revision_);
          saved_revision_ = edit_revision_;
          recovery_error_.clear();
        }
      } catch (const std::exception &e) {
        recovery_error_ = e.what();
      }
      update_recovery_status();
    }
    remember_document_position();
    file_flush_stage_ = 3;
  }
  return true;
}
std::function<void()> WritingWorkspace::prepare_project_objects() {
  if (!service_.current())
    return {};
  const auto owner = *service_.current();
  std::function<void()> objects;
  if (object_editor_ && current_document_)
    objects = object_editor_->prepare_project_save(owner.path(),
                                                   capture_editor_state());
  return [owner, objects = std::move(objects)] {
    // A crash may occur after the main Document commit and before its journal
    // receipt. With no dirty text the native writers have nothing to submit.
    // Let the owning SQLite store perform normal hot-journal recovery before
    // transport; never remove auxiliary files or resolve records ourselves.
    persistence::prepare_document_recovery_transport(owner);
    if (objects)
      objects();
  };
}
void WritingWorkspace::finish_project_flush() {
  file_flush_stage_ = 0;
  if (object_editor_)
    object_editor_->finish_project_save();
  schedule_common_sheet();
  if (edit_session_.dirty()) {
    schedule_autosave();
    schedule_recovery();
  }
}
} // namespace inde::ui

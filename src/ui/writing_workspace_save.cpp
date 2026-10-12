#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "editor_diagnostics.hpp"
#include <stdexcept>

namespace inde::ui {
void WritingWorkspace::receive_save_result() {
  if (auto result = save_worker_.take_latest()) {
    editor_diagnostic("save_worker", result->elapsed);
    if (!current_document_ || !service_.current() || result->epoch != edit_epoch_ ||
        result->document_id != current_document_->id || result->project_id != service_.current()->manifest().project_id)
      throw std::runtime_error("Confirmação de salvamento pertence a outro contexto de edição");
    if (result->persisted) {
      saved_document_ = std::move(result->persisted);
      saved_revision_=result->persisted_revision;
      edit_session_.acknowledge_saved(*saved_document_);
      current_document_->created_at = saved_document_->created_at;
      current_document_->updated_at = saved_document_->updated_at;
    }
    save_error_ = result->error;
    if(!result->journal_error.empty()) recovery_error_=result->journal_error;
    if (!save_error_.empty()) {
      save_submitted_ = false;
      signal_status_message_.emit("Autosave falhou; texto local preservado. Use Salvar para tentar novamente.");
    }
    update_editor_status();
    update_recovery_status();
  }
}
void WritingWorkspace::queue_autosave() {
  if (ime_active_) return;
  receive_save_result();
  if (!current_document_ || !saved_document_) return;
  if (!edit_session_.dirty() && !save_worker_.busy()) return;
  if (save_worker_.busy() && save_submitted_ && submitted_revision_ == edit_revision_) return;
  EditorPhaseTimer measurement("autosave_snapshot_submit");
  commit_undo_group();
  auto snapshot = service_.writing().save_snapshot(capture_editor_state(), *saved_document_, edit_epoch_, edit_revision_);
  const auto buffer=editor_.get_buffer();
  snapshot.recovery_session=recovery_session_;
  snapshot.cursor=static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_insert()).get_offset());
  snapshot.bound=static_cast<std::size_t>(buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset()); snapshot.durable_revision=saved_revision_;
  save_worker_.submit(std::move(snapshot));
  submitted_revision_ = edit_revision_; save_submitted_ = true; save_error_.clear();
  update_editor_status();
  save_delivery_.disconnect();
  save_delivery_ = Glib::signal_timeout().connect([this] {
    try { receive_save_result(); }
    catch (const std::exception &error) {
      save_error_ = author::diagnostic(error.what()); update_editor_status(); return false;
    }
    return save_worker_.busy();
  }, 30);
}
}

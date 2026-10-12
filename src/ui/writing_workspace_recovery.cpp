#include "inde/ui/author_diagnostics.hpp"
#include "editor_diagnostics.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/writing_workspace.hpp"
#include <algorithm>
namespace inde::ui {
void WritingWorkspace::begin_recovery() {
  end_recovery();
  saved_revision_ = checkpoint_submitted_ = checkpoint_confirmed_ = 0;
  checkpoint_requested_ = false;
  recovery_error_.clear();
  recovery_session_ = project::new_uuid();
  try {
    if (service_.current())
      recovery_worker_.begin(*service_.current(), recovery_session_);
  } catch (const std::exception &e) {
    recovery_session_.clear();
    recovery_error_ = author::diagnostic(e.what());
  }
  update_recovery_status();
}
void WritingWorkspace::end_recovery() {
  recovery_debounce_.disconnect();
  recovery_periodic_.disconnect();
  recovery_delivery_.disconnect();
  // The same lease protects journal writes made by the save worker as well.
  save_worker_.drain();
  recovery_worker_.clear();
  recovery_session_.clear();
}
void WritingWorkspace::receive_recovery_result() {
  if (auto r = recovery_worker_.take_latest()) {
    if (r->session_id != recovery_session_)
      throw std::runtime_error(
          "Confirmação de ponto de recuperação pertence a outra sessão");
    editor_diagnostic("journal_worker", r->elapsed);
    recovery_error_ = r->error;
    if (r->error.empty())
      checkpoint_confirmed_ = std::max(checkpoint_confirmed_, r->revision);
    update_recovery_status();
  }
}
void WritingWorkspace::queue_recovery(bool force) {
  if (ime_active_) return;
  try {
    receive_recovery_result();
    if (!current_document_ || !saved_document_ || !service_.current() ||
        recovery_session_.empty())
      return;
    if (!edit_session_.dirty() && !save_worker_.busy())
      return;
    if (!force && checkpoint_requested_ &&
        checkpoint_submitted_ == edit_revision_)
      return;
    EditorPhaseTimer timer("journal_snapshot_submit");
    const auto state = capture_edit_state();
    project::RecoveryRecord r{
        service_.current()->manifest().project_id,
        recovery_session_,
        edit_revision_,
        saved_revision_,
        {state.document, state.selection.cursor, state.selection.bound},
        *saved_document_,
        "pending",
        {},
        {}};
    recovery_worker_.submit({*service_.current(), std::move(r)});
    checkpoint_requested_ = true;
    checkpoint_submitted_ = edit_revision_;
    recovery_delivery_.disconnect();
    recovery_delivery_ = Glib::signal_timeout().connect(
        [this] {
          try {
            receive_recovery_result();
          } catch (const std::exception &e) {
            recovery_error_ = author::diagnostic(e.what());
            update_recovery_status();
            return false;
          }
          return recovery_worker_.busy();
        },
        30);
    update_recovery_status();
  } catch (const std::exception &e) {
    recovery_error_ = author::diagnostic(e.what());
    update_recovery_status();
  }
}
void WritingWorkspace::schedule_recovery() {
  if (loading_editor_ || restoring_undo_ || !current_document_)
    return;
  recovery_debounce_.disconnect();
  recovery_debounce_ = Glib::signal_timeout().connect(
      [this] {
        queue_recovery();
        return false;
      },
      350);
  if (!recovery_periodic_.connected())
    recovery_periodic_ = Glib::signal_timeout().connect(
        [this] {
          queue_recovery();
          return current_document_ &&
                 (edit_session_.dirty() || save_worker_.busy() ||
                  recovery_worker_.busy());
        },
        1000);
  update_recovery_status();
}
void WritingWorkspace::update_recovery_status() {
  if (!current_document_) {
    recovery_status_.set_text("");
    return;
  }
  if (!recovery_error_.empty()) {
    recovery_status_.set_text(
        "Recuperação indisponível — confira o salvamento");
    recovery_status_.set_tooltip_text(recovery_error_);
    return;
  }
  recovery_status_.set_tooltip_text(
      "Somente o último ponto de recuperação confirmado pode ser recuperado. Teclas ainda não confirmadas podem se perder.");
  if (!edit_session_.dirty() && !save_worker_.busy())
    recovery_status_.set_text("Recuperação: texto salvo no Projeto");
  else if (checkpoint_confirmed_ && checkpoint_confirmed_ >= edit_revision_)
    recovery_status_.set_text(
        "Ponto de recuperação r" + std::to_string(checkpoint_confirmed_) + " confirmado");
  else
    recovery_status_.set_text("Ponto de recuperação r" +
                              std::to_string(checkpoint_confirmed_) +
                              "; alterações mais recentes aguardando");
}
} // namespace inde::ui

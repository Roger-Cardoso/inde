#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "editor_diagnostics.hpp"
#include <stdexcept>

namespace inde::ui {
void WritingWorkspace::require_completed_composition() const {
  if (ime_active_)
    throw std::runtime_error("Há uma composição IME em andamento. Confirme o texto ou "
                             "cancele a composição no editor antes de mudar de contexto.");
}
void WritingWorkspace::build_ime_guard() {
  // GTK owns its IM context and commits only accepted text to TextBuffer.
  // gtkmm 4.10 does not wrap TextView::preedit-changed, so use the public C
  // signal without replacing the native input engine or intercepting keys.
  g_signal_connect(editor_.gobj(),"preedit-changed",G_CALLBACK(+[](
      GtkTextView *,gchar *preedit,gpointer data) {
    auto *self=static_cast<WritingWorkspace *>(data);
    try { self->on_preedit_changed(preedit); }
    catch (const std::exception &e) { self->signal_status_message_.emit(author::diagnostic(e.what())); }
  }),this);
}
void WritingWorkspace::on_preedit_changed(const char *preedit) {
  const bool active=preedit && *preedit;
  if (!active && !ime_active_) return;
  if (active && !ime_active_) {
    commit_undo_group(); // isolate preceding ordinary typing
    ime_commit_pending_=false;
    ime_start_revision_=edit_revision_;
  }
  ime_active_=active;
  if (active) {
    close_inline_suggestions();
    ime_commit_delivery_.disconnect();
    signal_status_message_.emit("Composição IME em andamento — texto provisório ainda não salvo.");
    return;
  }
  ime_commit_pending_=true;
  const auto epoch=edit_epoch_;
  ime_commit_delivery_.disconnect();
  // preedit-end can precede buffer insertion during the same GTK dispatch.
  // Commit once after that dispatch, not from inside the input method signal.
  ime_commit_delivery_=Glib::signal_idle().connect([this,epoch] {
    if (epoch!=edit_epoch_ || ime_active_) return false;
    try {
      commit_undo_group();
      ime_commit_pending_=false;
      if (edit_session_.dirty()) { schedule_autosave(); schedule_recovery(); }
      signal_status_message_.emit(edit_revision_ > ime_start_revision_
          ? "Composição IME confirmada; texto incorporado ao salvamento."
          : "Composição IME encerrada sem inserir texto provisório.");
    } catch (const std::exception &e) { signal_status_message_.emit(author::diagnostic(e.what())); }
    return false;
  });
}
} // namespace inde::ui

#include "inde/ui/author_diagnostics.hpp"
#include "inde/application/editorial_review.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
namespace inde::ui {
project::Document
WritingWorkspace::apply_editorial_change(const project::Document &expected,
                                         const project::Document &after,
                                         const std::string &label) {
  if (expected.id != after.id)
    throw std::runtime_error("A decisão pertence a outro Documento");
  if (!current_document_ || current_document_->id != expected.id)
    return service_.writing().update_document_if_unchanged(after, expected);
  flush_changes();
  const auto now = service_.writing().document(expected.id);
  if (!now || !project::same_document_state(*now, expected) ||
      !project::same_document_state(capture_editor_state(), expected, false))
    throw std::runtime_error("O Documento mudou; reabra a decisão editorial");
  position_restore_.disconnect();
  auto state = capture_edit_state();
  state.document = after;
  const auto size = project::utf8_character_count(after.content);
  state.selection.cursor = std::min(state.selection.cursor, size);
  state.selection.bound = std::min(state.selection.bound, size);
  apply_edit_transaction(std::move(state), label);
  try {
    flush_changes();
  } catch (const std::exception &e) {
    throw std::runtime_error(
        std::string("Decisão não confirmada no disco; a alteração local está "
                    "preservada na Escrita, com Desfazer disponível. Resolva a "
                    "falha de salvamento antes de continuar. ") +
        author::diagnostic(e.what()));
  }
  return *service_.writing().document(expected.id);
}
void WritingWorkspace::suggest_editorial_change() {
  if (!current_document_ || !owner_window())
    return;
  try {
    require_completed_composition();
    flush_changes();
    const auto state = capture_edit_state();
    const auto start = std::min(state.selection.cursor, state.selection.bound),
               end = std::max(state.selection.cursor, state.selection.bound);
    if (start == end)
      throw std::runtime_error("Selecione um trecho para propor uma mudança");
    const auto project_id = service_.current()->manifest().project_id;
    auto *dialog =
        new OverlayDialog("Sugerir alteração", *owner_window(), true);
    dialog->set_secondary_text(
        "A sugestão não muda o texto agora. Ela aparecerá em Revisar; aceitar "
        "exige confirmar o trecho ainda vigente. Campo vazio propõe remover o "
        "trecho.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    dialog->add_button("Guardar sugestão", Gtk::ResponseType::ACCEPT);
    auto *replacement = Gtk::make_managed<Gtk::TextView>();
    replacement->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    set_accessible_label(*replacement, "Texto proposto");
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_min_content_height(130);
    scroll->set_max_content_height(200);
    scroll->set_propagate_natural_height(true);
    scroll->set_child(*replacement);
    auto *note = Gtk::make_managed<Gtk::Entry>();
    note->set_placeholder_text("Motivo da sugestão (opcional)");
    note->set_max_length(1000);
    auto *label = Gtk::make_managed<Gtk::Label>("Texto proposto");
    label->set_xalign(0);
    dialog->get_content_area()->append(*label);
    dialog->get_content_area()->append(*scroll);
    dialog->get_content_area()->append(*note);
    dialog->signal_response().connect([this, dialog, replacement, note, state,
                                       start, end, project_id](int response) {
      dialog->hide();
      if (response != Gtk::ResponseType::ACCEPT)
        return;
      try {
        if (!service_.current() ||
            service_.current()->manifest().project_id != project_id)
          throw std::runtime_error("O Projeto mudou");
        const auto after = application::propose_change(
            state.document, start, end,
            replacement->get_buffer()->get_text().raw(),
            note->get_text().raw());
        (void)apply_editorial_change(state.document, after,
                                     "Guardar sugestão editorial");
        signal_status_message_.emit("Sugestão guardada; o texto não foi "
                                    "substituído. Abra Revisar para decidir.");
      } catch (const std::exception &e) {
        show_error("Sugestão não guardada", e);
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível sugerir alteração", e);
  }
}
} // namespace inde::ui

#include "inde/ui/author_diagnostics.hpp"
#include "editor_diagnostics.hpp"
#include "inde/application/document_recovery_service.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "writing_semantic_ui_support.hpp"
namespace inde::ui {
void WritingWorkspace::build_recovery_ui() {
  library_header_.append(recovery_library_button_);
  for (auto *button : {&recovery_library_button_, &recovery_editor_button_}) {
    button->set_tooltip_text("Ver ponto de recuperaçãos de sessões interrompidas. Nenhuma restauração é automática.");
    button->signal_clicked().connect([this] { show_recovery_records(); });
  }
  recovery_notice_.set_halign(Gtk::Align::START);
  recovery_notice_.set_ellipsize(Pango::EllipsizeMode::END);
  recovery_notice_.set_visible(false);
  library_heading_.append(recovery_notice_);
  recovery_status_.set_halign(Gtk::Align::END);
  recovery_status_.set_ellipsize(Pango::EllipsizeMode::END);
  recovery_status_.add_css_class("dim-label");
  editor_footer_.append(recovery_status_);
}
void WritingWorkspace::refresh_recovery_notice() {
  if (!service_.current())
    return;
  try {
    EditorPhaseTimer timer("journal_discovery_gtk");
    const auto page =
        persistence::DocumentRecoveryStore{}.pending(*service_.current());
    const auto text =
        page.items.empty()
            ? ""
            : std::to_string(page.items.size()) +
                  (page.more ? "+ ponto de recuperaçãos" : " ponto de recuperação(s)") +
                  " aguardando sua decisão em Recuperação";
    recovery_notice_.set_text(text);
    recovery_notice_.set_visible(!text.empty());
    recovery_library_button_.set_label(
        page.items.empty()
            ? "Recuperação"
            : "Recuperação (" + std::to_string(page.items.size()) +
                  (page.more ? "+)" : ")"));
    recovery_editor_button_.set_label(recovery_library_button_.get_label());
  } catch (const std::exception &e) {
    recovery_notice_.set_text(
        "Registro de recuperação indisponível/incompatível; evidências não foram apagadas");
    recovery_notice_.set_tooltip_text(author::diagnostic(e.what()));
    recovery_notice_.set_visible(true);
  }
}
void WritingWorkspace::show_recovery_records() {
  if (!service_.current() || !owner_window())
    return;
  try {
    const auto page =
        persistence::DocumentRecoveryStore{}.pending(*service_.current());
    auto *dialog =
        new OverlayDialog("Recuperação de escrita", *owner_window(), true);
    dialog->set_secondary_text(
        page.items.empty()
            ? "Nenhum ponto de recuperação pendente de sessão inativa. Escrita não confirmada pelo registro de recuperação pode não ser recuperável."
            : "Nenhuma alteração será aplicada sem sua escolha. Sessões ainda "
              "ativas em outra instância não são oferecidas.");
    dialog->add_button("Fechar", Gtk::ResponseType::CANCEL);
    auto *list = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
    for (const auto &entry : page.items) {
      auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
      auto *label = Gtk::make_managed<Gtk::Label>(
          entry.title + " — r" + std::to_string(entry.revision) + " — " +
          entry.updated_at);
      label->set_wrap(true);
      label->set_xalign(0);
      row->append(*label);
      auto *button = Gtk::make_managed<Gtk::Button>("Examinar ponto de recuperação");
      row->append(*button);
      list->append(*row);
      button->signal_clicked().connect(
          [this, dialog, session = entry.session_id] {
            dialog->hide();
            preview_recovery(session);
          });
    }
    if (page.more) {
      auto *label = Gtk::make_managed<Gtk::Label>(
          "Há mais registros preservados. Resolva estes para examinar os "
          "seguintes.");
      label->set_wrap(true);
      list->append(*label);
    }
    dialog->get_content_area()->append(*list);
    auto *maintenance = Gtk::make_managed<Gtk::Button>("Retenção e exportação…");
    dialog->get_content_area()->append(*maintenance);
    maintenance->signal_clicked().connect([this,dialog] {
      semantic_ui::retire(dialog);
      show_recovery_retention();
    });
    dialog->signal_response().connect([dialog](int) { dialog->hide(); });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível consultar a recuperação", e);
  }
}
void WritingWorkspace::preview_recovery(const std::string &session) {
  if (!service_.current() || !owner_window())
    return;
  try {
    application::DocumentRecoveryService recovery(service_.writing());
    const auto owner = *service_.current();
    const auto preview = recovery.inspect(owner, session);
    const auto hash = preview.record.checksum;
    auto *dialog = new OverlayDialog("Examinar ponto de recuperação de escrita",
                                     *owner_window(), true);
    dialog->set_secondary_text(
        preview.explanation +
        "\nCópia de resgate contém texto e estilos, sem trechos marcados ou vínculos. Descartar apenas arquiva este registro, sem apagá-lo.");
    dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    if (preview.original_unchanged || preview.already_persisted)
      dialog->add_button("Restaurar original", Gtk::ResponseType::ACCEPT);
    dialog->add_button("Criar cópia de resgate", 1001);
    dialog->add_button("Descartar ponto de recuperação", 1002);
    auto *text = Gtk::make_managed<Gtk::TextView>();
    text->set_editable(false);
    text->set_cursor_visible(false);
    text->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    Glib::ustring content(preview.record.state.document.content);
    text->get_buffer()->set_text(content.substr(0, 2000));
    auto *scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_min_content_height(160);
    scroll->set_child(*text);
    dialog->get_content_area()->append(*scroll);
    auto *info = Gtk::make_managed<Gtk::Label>(
        "Prévia: até 2.000 caracteres. Seleção recuperável " +
        std::to_string(preview.record.state.cursor) + " / " +
        std::to_string(preview.record.state.bound) +
        ". Undo anterior à interrupção não é reconstruído.");
    info->set_wrap(true);
    dialog->get_content_area()->append(*info);
    dialog->signal_response().connect([this, dialog, owner, session,
                                       hash](int response) {
      dialog->hide();
      if (response == Gtk::ResponseType::CANCEL ||
          response == Gtk::ResponseType::DELETE_EVENT)
        return;
      try {
        if (!service_.current() ||
            service_.current()->manifest().project_id !=
                owner.manifest().project_id ||
            service_.current()->path() != owner.path())
          throw std::runtime_error("Projeto ativo mudou durante a prévia");
        application::DocumentRecoveryService service(service_.writing());
        if (response == Gtk::ResponseType::ACCEPT) {
          flush_changes();
          const auto state = service.restore(owner, session, hash);
          open_document(state.document.id);
          if (!current_document_ || current_document_->id != state.document.id)
            throw std::runtime_error(
                "Original restaurado no Projeto, mas sua abertura no editor falhou; ponto de recuperação preservado");
          restore_edit_state({state.document, {state.cursor, state.bound}});
          edit_session_.open(capture_edit_state());
          update_editor_status();
          signal_status_message_.emit(
              "Ponto de recuperação restaurado e persistido após sua confirmação");
        } else if (response == 1001) {
          const auto copy = service.create_copy(owner, session, hash);
          if (!current_document_ || !edit_session_.dirty())
            open_document(copy.id);
          signal_status_message_.emit("Cópia de resgate criada na Biblioteca; "
                                      "nenhum vínculo importado");
        } else if (response == 1002) {
          auto *confirm =
              new OverlayDialog("Arquivar ponto de recuperação?", *owner_window(), true);
          confirm->set_secondary_text(
              "O registro deixará de ser oferecido. A evidência continuará no registro de recuperação; nenhum Documento será alterado.");
          confirm->add_button("Cancelar", Gtk::ResponseType::CANCEL);
          confirm->add_button("Confirmar descarte", Gtk::ResponseType::ACCEPT);
          confirm->signal_response().connect([this, confirm, owner, session,
                                              hash](int choice) {
            confirm->hide();
            if (choice != Gtk::ResponseType::ACCEPT)
              return;
            try {
              application::DocumentRecoveryService(service_.writing())
                  .discard(owner, session, hash);
              refresh_recovery_notice();
              signal_status_message_.emit(
                  "Ponto de recuperação arquivado por sua decisão; evidência preservada");
            } catch (const std::exception &e) {
              show_error("Não foi possível arquivar o ponto de recuperação", e);
            }
          });
          confirm->present();
          return;
        }
        refresh_recovery_notice();
      } catch (const std::exception &e) {
        show_error("Não foi possível concluir a recuperação", e);
      }
    });
    dialog->present();
  } catch (const std::exception &e) {
    show_error("Ponto de recuperação não pode ser aplicado; evidência preservada", e);
  }
}
} // namespace inde::ui

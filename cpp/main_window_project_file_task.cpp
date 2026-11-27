#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/main_window.hpp"
#include "object_native_ui.hpp"
#include "project_file_session.hpp"
#include "project_file_task.hpp"
namespace inde::ui {
using Phase = project_file::OperationPhase;
namespace {
ObjectDocumentEditor *common_editor(Gtk::Widget &widget) {
  if (auto *editor = dynamic_cast<ObjectDocumentEditor *>(&widget);
      editor && editor->get_mapped())
    return editor;
  for (auto *child = widget.get_first_child(); child;
       child = child->get_next_sibling())
    if (auto *editor = common_editor(*child))
      return editor;
  return nullptr;
}
} // namespace
void MainWindow::setup_file_confirmation() {
  file_task_panel_.set_spacing(12);
  file_task_status_.set_name("native-file-task-stage");
  file_task_status_.set_wrap(true);
  file_task_status_.set_focusable(true);
  file_task_panel_.append(file_task_spinner_);
  file_task_panel_.append(file_task_status_);
  file_task_panel_.append(file_task_cancel_);
  file_task_panel_.append(file_task_exit_button_);
  file_task_cancel_.signal_clicked().connect(
      [this] { cancel_file_confirmation(); });
  file_task_exit_button_.signal_clicked().connect([this] {
    file_task_exit_ = !file_task_exit_;
    file_task_exit_button_.set_label(file_task_exit_ ? "Continuar no editor"
                                                     : "Sair ao terminar");
  });
  auto keys = Gtk::EventControllerKey::create();
  keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect(
      [this](guint key, guint, Gdk::ModifierType mods) {
        const auto mask =
            static_cast<unsigned>(mods) &
            (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SHIFT_MASK | GDK_SUPER_MASK);
        if (file_task_ && mask == GDK_CONTROL_MASK &&
            (key == GDK_KEY_q || key == GDK_KEY_Q)) {
          queue_file_confirmation_exit();
          return true;
        }
        return false;
      },
      false);
  add_controller(keys);
}
void MainWindow::queue_file_confirmation_exit() {
  file_task_exit_ = true;
  file_task_exit_button_.set_label("Continuar no editor");
}
void MainWindow::cancel_file_confirmation() {
  if (file_task_ && file_task_->cancel()) {
    file_task_cancelled_ = true;
    file_task_exit_ = false;
    file_task_cancel_.set_sensitive(false);
    file_task_status_.grab_focus();
    file_task_exit_button_.set_label("Sair ao terminar");
    file_task_status_.set_text(
        "Cancelando a operação; o trabalho será preservado.");
  }
}
void MainWindow::present_file_confirmation() {
  if (!file_task_)
    return;
  file_task_token_ = modal_host_.present_dialog(
      file_task_title_,
      "A edição está pausada durante a conferência. O trabalho permanece na "
      "área de recuperação.",
      file_task_panel_, {}, [this](int) {
        file_task_token_ = 0;
        cancel_file_confirmation();
        file_task_retry_ = Glib::signal_idle().connect(sigc::track_obj(
            [this] {
              file_task_retry_.disconnect();
              present_file_confirmation();
              return false;
            },
            *this));
      });
}
void MainWindow::start_file_confirmation(std::function<void()> next,
                                         bool exiting) {
  if (!project_file_ || file_task_ || modal_host_.has_dialog())
    return;
  try {
    const bool paragraph_focus =
        get_focus() &&
        get_focus()->get_name().raw().starts_with("writing-paragraph-");
    auto focus = std::shared_ptr<GWeakRef>(new GWeakRef{}, [](GWeakRef *ref) {
      g_weak_ref_clear(ref);
      delete ref;
    });
    g_weak_ref_init(focus.get(),
                    get_focus() ? G_OBJECT(get_focus()->gobj()) : nullptr);
    comic_workspace_.save_changes();
    writing_workspace_.flush_changes();
    remember_context();
    // Match the previous explicit Save contract; closing does not stamp a
    // fresh manifest merely because the window/project is being closed.
    if (!exiting && !next)
      service_.save();
    file_task_next_ = [this, focus, paragraph_focus, next = std::move(next)] {
      if (paragraph_focus) {
        // Use the editor's guarded focus path. Direct GTK focus can select a
        // containing object, rebuild the paragraph and lose the caret.
        if (auto *editor = common_editor(writing_workspace_))
          editor->focus_text(-1);
      } else if (auto *object = g_weak_ref_get(focus.get())) {
        auto *widget = GTK_WIDGET(object);
        if (gtk_widget_get_mapped(widget) && gtk_widget_is_sensitive(widget))
          gtk_widget_grab_focus(widget);
        g_object_unref(object);
      }
      if (file_barrier_ready_ && !file_task_exit_ && next)
        next();
    };
    file_task_title_ = "Salvando projeto";
    file_task_cancel_.set_label("Cancelar salvamento");
    file_task_exit_ = exiting;
    file_task_cancelled_ = false;
    file_task_cancel_.set_sensitive(true);
    file_task_exit_button_.set_label(exiting ? "Continuar no editor"
                                             : "Sair ao terminar");
    file_task_status_.set_text("Conferindo o conteúdo do projeto…");
    auto observer = file_task_observe_;
    file_task_ = std::make_shared<project_file::ConfirmationTask>(
        *project_file_, [observer, title = file_task_title_](
                            Phase, project_file::OperationControl &control) {
          if (observer)
            observer(title, control);
        });
    file_task_spinner_.start();
    present_file_confirmation();
    root_.set_sensitive(false);
    file_task_poll_ = Glib::signal_timeout().connect(
        sigc::track_obj([this] { return poll_file_confirmation(); }, *this),
        20);
  } catch (...) {
    stop_file_confirmation();
    file_confirmation_failed(std::current_exception(), exiting);
  }
}
void MainWindow::start_file_operation(
    const std::string &title, project_file::ConfirmationTask::Work work,
    std::function<void(const project_file::ConfirmationResult &)> done) {
  if (file_task_ || modal_host_.has_dialog())
    return;
  file_task_done_ = std::move(done);
  try {
    // The caller has drained the writers, either through source confirmation
    // or before capture. Draining again can publish a fresh composition receipt
    // after that confirmation and make the workspace differ from its baseline.
    // Automatic context writes stay paused until this worker is collected.
    file_task_next_ = {};
    file_task_title_ = title;
    file_task_exit_ = false;
    file_task_cancelled_ = false;
    file_task_cancel_.set_label("Cancelar operação");
    file_task_cancel_.set_sensitive(true);
    file_task_exit_button_.set_label("Sair ao terminar");
    file_task_status_.set_text("Preparando a operação…");
    auto observer = file_task_observe_;
    file_task_ = std::make_shared<project_file::ConfirmationTask>(
        std::move(work),
        [observer, title](Phase, project_file::OperationControl &control) {
          if (observer)
            observer(title, control);
        });
    file_task_spinner_.start();
    present_file_confirmation();
    root_.set_sensitive(false);
    file_task_poll_ = Glib::signal_timeout().connect(
        sigc::track_obj([this] { return poll_file_confirmation(); }, *this),
        20);
  } catch (...) {
    auto completion = std::move(file_task_done_);
    stop_file_confirmation();
    project_file::ConfirmationResult result;
    result.error = std::current_exception();
    if (completion)
      completion(result);
    else
      show_error("Operação do arquivo",
                 std::runtime_error("Não foi possível iniciar a operação."));
  }
}
bool MainWindow::poll_file_confirmation() {
  if (!file_task_)
    return false;
  if (!file_task_->ready()) {
    const bool cancellable = file_task_->cancellable();
    const bool focus_status = !cancellable && file_task_cancel_.is_focus();
    file_task_cancel_.set_sensitive(cancellable);
    if (focus_status)
      file_task_status_.grab_focus();
    if (!file_task_cancelled_) {
      switch (file_task_->phase()) {
      case Phase::Reading:
        file_task_status_.set_text("Lendo o conteúdo do projeto…");
        break;
      case Phase::Checking:
        file_task_status_.set_text("Conferindo o conteúdo…");
        break;
      case Phase::Writing:
        file_task_status_.set_text("Preparando o conteúdo da operação…");
        break;
      case Phase::Committing:
        file_task_status_.set_text(
            "Confirmando no arquivo. Aguarde o resultado…");
        break;
      case Phase::Finishing:
        file_task_status_.set_text("Finalizando e conferindo a recuperação…");
        break;
      case Phase::Done:
        break;
      }
    }
    return true;
  }
  const auto result = file_task_->result();
  const auto exiting = file_task_exit_;
  auto done = std::move(file_task_done_);
  auto next = std::move(file_task_next_);
  file_task_.reset();
  file_task_retry_.disconnect();
  file_task_spinner_.stop();
  root_.set_sensitive(true);
  modal_host_.dismiss_dialog(file_task_token_);
  file_task_token_ = 0;
  if (done) {
    bool completed = false;
    try {
      done(result);
      completed = true;
    } catch (const std::exception &e) {
      show_error("Operação do arquivo", e);
    }
    if (completed && !result.error && !result.cancelled && exiting)
      close();
    else if (result.cancelled) {
      if (auto *editor = common_editor(writing_workspace_))
        editor->focus_text(-1);
      status_bar_.set_text(
          "Operação cancelada. O projeto anterior e as áreas de trabalho foram "
          "preservados; confirmações anteriores continuam salvas.");
    }
    return false;
  }
  if (result.cancelled) {
    if (next)
      next();
    status_bar_.set_text(
        "Salvamento cancelado; o trabalho permanece na área de recuperação.");
  } else if (result.error) {
    if (next)
      next();
    file_confirmation_failed(result.error, exiting);
  } else {
    preserve_file_work_ = false;
    status_bar_.set_text("Arquivo único confirmado — estado " +
                         std::to_string(result.sequence));
    if (exiting) {
      if (next)
        next();
      file_exit_ready_ = true;
      close();
      return false;
    }
    if (next) {
      file_barrier_ready_ = true;
      try {
        next();
      } catch (const std::exception &e) {
        show_error("Após o salvamento", e);
      }
      file_barrier_ready_ = false;
    }
  }
  return false;
}
void MainWindow::stop_file_confirmation() {
  file_task_poll_.disconnect();
  file_task_retry_.disconnect();
  if (file_task_) {
    file_task_->cancel();
    file_task_->wait();
    file_task_.reset();
  }
  file_task_next_ = {};
  file_task_done_ = {};
  if (file_task_token_) {
    modal_host_.dismiss_dialog(file_task_token_);
    file_task_token_ = 0;
  }
  root_.set_sensitive(true);
}
void MainWindow::file_confirmation_failed(std::exception_ptr error,
                                          bool exiting) {
  try {
    std::rethrow_exception(error);
  } catch (const std::exception &e) {
    if (!exiting) {
      show_error("Não foi possível confirmar o arquivo", e);
      return;
    }
    auto *d =
        new OverlayDialog("Gravação do arquivo não confirmada", *this, true);
    d->set_secondary_text(
        author::diagnostic(e.what()) +
        "\nO trabalho permanece em: " + project_file_->working().string() +
        "\nVocê pode voltar e tentar novamente ou encerrar mantendo essa área "
        "para a recuperação na próxima abertura.");
    d->add_button("Voltar ao editor", Gtk::ResponseType::CANCEL);
    d->add_button("Encerrar preservando o trabalho", Gtk::ResponseType::YES);
    d->signal_response().connect([this, d](int r) {
      d->hide();
      if (r == Gtk::ResponseType::YES) {
        preserve_file_work_ = true;
        close();
      }
    });
    d->present();
  }
}
} // namespace inde::ui

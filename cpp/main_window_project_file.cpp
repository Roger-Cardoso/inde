#include "inde/project/manifest.hpp"
#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/main_window.hpp"
#include "project_file_session.hpp"
#include "project_file_transport.hpp"
namespace inde::ui {
namespace fs = std::filesystem;
using Choice = project_file::RecoveryChoice;
namespace {
void retire_file_dialog(OverlayDialog *d) {
  d->hide();
  // OverlayDialog owns its deferred destruction after the response.
}
} // namespace
void MainWindow::flush_project_file() {
  if (!project_file_ || file_barrier_ready_)
    return;
  comic_workspace_.save_changes();
  writing_workspace_.flush_changes();
  remember_context();
  project_file_->save("Confirmar trabalho no editor");
  preserve_file_work_ = false;
}
void MainWindow::create_legacy_project(const fs::path &p,
                                       const std::string &name) {
  flush_project_file();
  if (project_file_ && !file_barrier_ready_)
    project_file_->finish();
  service_.set_recent_location(std::nullopt);
  try {
    service_.create(p, name);
  } catch (...) {
    if (project_file_)
      service_.set_recent_location(project_file_->file());
    throw;
  }
  project_file_.reset();
  preserve_file_work_ = false;
}
void MainWindow::open_legacy_project(const fs::path &p) {
  flush_project_file();
  if (project_file_ && !file_barrier_ready_)
    project_file_->finish();
  service_.set_recent_location(std::nullopt);
  try {
    service_.open(p);
  } catch (...) {
    if (project_file_)
      service_.set_recent_location(project_file_->file());
    throw;
  }
  project_file_.reset();
  preserve_file_work_ = false;
}
void MainWindow::choose_project_file(bool create) {
  if (modal_host_.has_dialog() || !guard_comic_project_change())
    return;
  auto d = Gtk::FileChooserNative::create(
      create ? "Criar arquivo único INDE" : "Abrir arquivo único INDE", *this,
      create ? Gtk::FileChooser::Action::SAVE : Gtk::FileChooser::Action::OPEN,
      create ? "Criar" : "Abrir", "Cancelar");
  if (create)
    d->set_current_name("Novo projeto.inde");
  auto filter = Gtk::FileFilter::create();
  filter->set_name("Projeto INDE (*.inde)");
  filter->add_pattern("*.inde");
  d->add_filter(filter);
  d->signal_response().connect([this, d, create](int response) {
    if (response != Gtk::ResponseType::ACCEPT || !d->get_file())
      return;
    fs::path path = d->get_file()->get_path();
    try {
      if (create && path.extension() != ".inde")
        path += ".inde";
      if (create) {
        writing_workspace_.flush_changes();
        flush_project_file();
        const auto root = project_file::FileSession::default_root() /
                          ("new-" + project::new_uuid());
        fs::create_directories(root);
        fs::permissions(root, fs::perms::owner_all, fs::perm_options::replace);
        application::ProjectService builder;
        builder.set_recent_location(path);
        builder.create(root / "source.inde", path.stem().string());
        builder.save();
        builder.close();
        project_file::pack_directory(root / "source.inde", path,
                                     project_file::TransportProfile::Blocks);
      }
      open_project_at(path);
    } catch (const std::exception &e) {
      show_error("Arquivo único", e);
    }
  });
  d->show();
}
void MainWindow::open_project_file_at(const fs::path &path, Choice choice) {
  if (!guard_comic_project_change())
    return;
  try {
    if (project_file_ && fs::equivalent(path, project_file_->file())) {
      if (!project_file_->needs_reopen())
        return;
      // Flush native writers, retain their durable workspace, and release the
      // old lease before observing the authoritative file again.
      comic_workspace_.save_changes();
      writing_workspace_.flush_changes();
      remember_context();
      project_file_.reset();
      close_project();
    }
    if (choice == Choice::Ask) {
      writing_workspace_.flush_changes();
      remember_context();
      flush_project_file();
    }
    auto next = project_file::FileSession::open(
        path, project_file::FileSession::default_root(), choice);
    if (project_file_)
      project_file_->finish();
    // Stop readers that retain the previous materialization before rebinding.
    review_workspace_.reset();
    cartography_workspace_.reset();
    service_.set_recent_location(next->file());
    try {
      service_.open(next->working());
    } catch (...) {
      service_.set_recent_location(
          project_file_ ? std::optional<fs::path>(project_file_->file())
                        : std::nullopt);
      throw;
    }
    project_file_ = std::move(next);
    preserve_file_work_ = false;
    review_workspace_.reset();
    comic_workspace_.reset();
    planning_workspace_.reset();
    writing_workspace_.reset();
    catalog_workspace_.reset();
    editorial_workspace_.reset();
    graphs_workspace_.reset();
    pending_views_.clear();
    restored_project_id_.clear();
    builder_return_context_.reset();
    builder_return_.set_visible(false);
    removal_returns_.clear();
    removal_return_.set_visible(false);
    update_project_view();
  } catch (const project_file::RecoveryRequired &e) {
    auto *d = new OverlayDialog("Retomar projeto", *this, true);
    d->set_secondary_text(
        std::string("Há uma área preservada diferente do arquivo salvo. ") +
        (!e.resumable
             ? "O registro não pôde ser validado para retomada automática. "
             : "") +
        (e.file_advanced ? "O arquivo também mudou desde aquela sessão. "
                           "Retomar o trabalho permitirá salvá-lo como outra "
                           "versão, conservando o estado atual no histórico. "
                         : "") +
        "Abrir o salvo mantém a recuperação reservada, sem apagá-la.\nÁrea "
        "preservada: " +
        e.working.string());
    d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    d->add_button("Abrir o estado salvo", Gtk::ResponseType::NO);
    if (e.resumable)
      d->add_button("Retomar o trabalho preservado", Gtk::ResponseType::YES);
    d->signal_response().connect([this, d, path](int r) {
      retire_file_dialog(d);
      if (r == Gtk::ResponseType::YES)
        open_project_file_at(path, Choice::Working);
      else if (r == Gtk::ResponseType::NO)
        open_project_file_at(path, Choice::Saved);
    });
    d->present();
  } catch (const std::exception &e) {
    show_error("Não foi possível abrir o arquivo único", e);
    refresh_recents();
  }
}
bool MainWindow::finish_project_file_close() {
  if (!project_file_ || preserve_file_work_ || file_barrier_ready_) return true;
  start_file_confirmation({}, true);return false;
}
void MainWindow::copy_project_file() {
  if (!service_.current() || modal_host_.has_dialog() ||
      !guard_comic_project_change())
    return;
  const auto choose = [this](bool current_only) {
    auto d = Gtk::FileChooserNative::create(
        "Salvar cópia transportável do mesmo projeto", *this,
        Gtk::FileChooser::Action::SAVE, "Salvar cópia", "Cancelar");
    d->set_current_name(
        (project_file_ ? project_file_->file() : service_.current()->path())
            .stem()
            .string() +
        " — Cópia.inde");
    d->signal_response().connect([this, d, current_only](int r) {
      if (r != Gtk::ResponseType::ACCEPT || !d->get_file())
        return;
      try {
        fs::path path = d->get_file()->get_path();
        if (path.extension() != ".inde")
          path += ".inde";
        if (project_file_ && !current_only) {
          flush_project_file();
          project_file_->copy_to(path);
        } else {
          comic_workspace_.save_changes();
          writing_workspace_.flush_changes();
          remember_context();
          if (!project_file_)
            service_.save();
          project_file::pack_directory(service_.current()->path(), path,
                                       project_file::TransportProfile::Blocks);
          if (current_only) {
            // The new file has been verified and published. Release the old
            // lease without trying another write to a full/unavailable file.
            // Its history and durable working area both remain preserved.
            auto previous = std::move(project_file_);
            close_project();
            if (service_.current()) {
              project_file_ = std::move(previous);
              service_.set_recent_location(project_file_->file());
              return;
            }
            previous.reset();
            open_project_at(path);
            if (!project_file_) {
              status_bar_.set_text("A cópia foi criada, mas não foi ativada. "
                                   "Abra o novo arquivo para continuar.");
              return;
            }
            status_bar_.set_text(
                "Continuando na cópia do estado atual; o arquivo original e "
                "seu histórico foram mantidos.");
            return;
          }
        }
        status_bar_.set_text("Cópia transportável criada; o projeto original "
                             "continua ativo. A cópia mantém sua identidade.");
      } catch (const std::exception &e) {
        show_error("Não foi possível criar a cópia", e);
      }
    });
    d->show();
  };
  if (!project_file_) {
    choose(false);
    return;
  }
  auto *mode = new OverlayDialog("Que cópia você quer criar?", *this, true);
  mode->set_secondary_text(
      "A cópia mantém a identidade do mesmo projeto. Você pode levar também as "
      "versões anteriores ou continuar em um arquivo novo contendo o estado "
      "atual. O arquivo original e seu histórico serão preservados nos dois "
      "casos.");
  mode->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  mode->add_button("Copiar com o histórico", Gtk::ResponseType::YES);
  mode->add_button("Continuar em cópia do estado atual", Gtk::ResponseType::NO);
  mode->signal_response().connect([mode, choose](int r) {
    retire_file_dialog(mode);
    if (r == Gtk::ResponseType::YES)
      choose(false);
    else if (r == Gtk::ResponseType::NO)
      choose(true);
  });
  mode->present();
}
void MainWindow::show_file_history() {
  if (!project_file_ || modal_host_.has_dialog())
    return;
  try {
    flush_project_file();
    auto *d = new OverlayDialog("Estados do arquivo único", *this, true);
    d->set_secondary_text(
        "Retomar cria uma nova versão e conserva o trabalho posterior. O "
        "trabalho atual foi confirmado antes de abrir esta lista.");
    auto *selector = Gtk::make_managed<Gtk::ComboBoxText>();
    selector->set_name("project-file-state");
    for (const auto &r : project_file_->history())
      if (r.sequence > 0)
        selector->append(std::to_string(r.sequence),
                         std::to_string(r.sequence) + " — " + r.label + " — " +
                             r.created_at);
    selector->set_active_id(std::to_string(project_file_->baseline().sequence));
    d->get_content_area()->append(*selector);
    d->add_button("Voltar", Gtk::ResponseType::CANCEL);
    d->add_button("Retomar este estado", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect([this, d, selector](int r) {
      const auto selected = selector->get_active_id().raw();
      retire_file_dialog(d);
      if (r != Gtk::ResponseType::ACCEPT || selected.empty())
        return;
      try {
        const auto old_work = project_file_->working();
        project_file_->restore(std::stoll(selected));
        if (project_file_->working() != old_work) {
          review_workspace_.reset();
          cartography_workspace_.reset();
          comic_workspace_.reset();
          writing_workspace_.reset();
          service_.close();
          service_.open(project_file_->working());
          planning_workspace_.reset();
          catalog_workspace_.reset();
          editorial_workspace_.reset();
          graphs_workspace_.reset();
          builder_return_context_.reset();
          builder_return_.set_visible(false);
          removal_returns_.clear();
          removal_return_.set_visible(false);
          restored_project_id_.clear();
          pending_views_.clear();
          update_project_view();
        }
        status_bar_.set_text(
            "Estado retomado; versões posteriores preservadas no arquivo.");
      } catch (const std::exception &e) {
        show_error("Não foi possível retomar o estado", e);
      }
    });
    d->present();
  } catch (const std::exception &e) {
    show_error("Estados do arquivo", e);
  }
}
} // namespace inde::ui

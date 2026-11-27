#include "inde/project/manifest.hpp"
#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/main_window.hpp"
#include "project_file_session.hpp"
#include "project_file_task.hpp"
#include "project_file_transport.hpp"
#include <algorithm>
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
  if (file_task_ || (project_file_ && !file_barrier_ready_))
    throw std::runtime_error(
        "A confirmação do arquivo precisa terminar antes desta operação.");
}
void MainWindow::create_legacy_project(const fs::path &p,
                                       const std::string &name) {
  if (project_file_ && !file_barrier_ready_) {
    start_file_confirmation(
        [this, p, name] { create_legacy_project(p, name); });
    return;
  }
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
  comic_workspace_.reset();
  review_workspace_.reset();
  planning_workspace_.reset();
  writing_workspace_.reset();
  update_project_view();
}
void MainWindow::open_legacy_project(const fs::path &p) {
  if (project_file_ && !file_barrier_ready_)
    throw std::runtime_error("Confirme o projeto atual antes da troca.");
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
      if (create)
        create_project_file_at(path);
      else
        open_project_file_at(path, Choice::Ask);
    } catch (const std::exception &e) {
      show_error("Arquivo único", e);
    }
  });
  d->show();
}
void MainWindow::rebind_project_file() {
  review_workspace_.reset();
  cartography_workspace_.reset();
  comic_workspace_.reset();
  writing_workspace_.reset();
  service_.close();
  service_.set_recent_location(project_file_->file());
  service_.open(project_file_->working());
  planning_workspace_.reset();
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
}
void MainWindow::adopt_project_file(
    std::unique_ptr<project_file::FileSession> next) {
  // Opening the native domain is GTK-owned. Validate it before releasing the
  // former file lease; a failed transport operation never changes the binding.
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
}
void MainWindow::create_project_file_at(const fs::path &path) {
  if (!guard_comic_project_change() || modal_host_.has_dialog())
    return;
  if (project_file_ && !file_barrier_ready_) {
    start_file_confirmation([this, path] { create_project_file_at(path); });
    return;
  }
  auto candidate =
      std::make_shared<std::unique_ptr<project_file::FileSession>>();
  const auto root = project_file::FileSession::default_root();
  start_file_operation(
      "Criando arquivo único",
      [path, root, candidate](auto &control) {
        control.checkpoint(project_file::OperationPhase::Reading);
        if (fs::exists(fs::symlink_status(path)))
          throw project_file::Error(
              "destination_exists",
              "O destino já existe. Escolha um arquivo novo.");
        const auto seed = root / ("new-" + project::new_uuid());
        fs::create_directories(seed);
        fs::permissions(seed, fs::perms::owner_all, fs::perm_options::replace);
        {
          application::ProjectService builder;
          builder.set_recent_location(path);
          builder.create(seed / "source.inde", path.stem().string());
          builder.save();
          builder.close();
        }
        control.check();
        project_file::pack_directory(seed / "source.inde", path,
                                     project_file::TransportProfile::Blocks,
                                     &control);
        *candidate =
            project_file::FileSession::open(path, root, Choice::Ask, &control);
        return (*candidate)->baseline().sequence;
      },
      [this, path, candidate](const auto &result) {
        if (result.error) {
          project_file_open_failed(path, result.error);
          return;
        }
        if (!result.cancelled)
          adopt_project_file(std::move(*candidate));
      },
      !project_file_);
}
void MainWindow::open_project_file_at(const fs::path &path, Choice choice) {
  if (!guard_comic_project_change() || modal_host_.has_dialog())
    return;
  try {
    const bool same =
        project_file_ && fs::equivalent(path, project_file_->file());
    if (same) {
      if (!project_file_->needs_reopen())
        return;
      comic_workspace_.save_changes();
      writing_workspace_.flush_changes();
      remember_context();
      // An uncertain lease must be released to inspect that same file. Preserve
      // its work, and close its native view instead of leaving an unbound
      // editor.
      project_file_.reset();
      close_project();
      if (service_.current())
        throw std::runtime_error(
            "Feche o projeto antes de conferir novamente o arquivo.");
    }
    if (project_file_ && !file_barrier_ready_) {
      start_file_confirmation(
          [this, path, choice] { open_project_file_at(path, choice); });
      return;
    }
    auto candidate =
        std::make_shared<std::unique_ptr<project_file::FileSession>>();
    const auto root = project_file::FileSession::default_root();
    start_file_operation(
        "Abrindo arquivo único",
        [path, choice, root, candidate](auto &control) {
          *candidate =
              project_file::FileSession::open(path, root, choice, &control);
          return (*candidate)->baseline().sequence;
        },
        [this, path, candidate](const auto &result) {
          if (result.error) {
            project_file_open_failed(path, result.error);
            return;
          }
          if (!result.cancelled)
            adopt_project_file(std::move(*candidate));
        },
        !project_file_);
  } catch (const std::exception &e) {
    show_error("Não foi possível abrir o arquivo único", e);
  }
}
void MainWindow::project_file_open_failed(const fs::path &path,
                                          std::exception_ptr error) {
  try {
    std::rethrow_exception(error);
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
  if (!project_file_ || preserve_file_work_ || file_barrier_ready_)
    return true;
  start_file_confirmation({}, true);
  return false;
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
        copy_project_file_to(path, current_only);
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
void MainWindow::copy_project_file_to(const fs::path &path, bool current_only) {
  if (!service_.current() || !guard_comic_project_change() ||
      modal_host_.has_dialog())
    return;
  if (project_file_ && !current_only && !file_barrier_ready_) {
    start_file_confirmation([this, path, current_only] {
      copy_project_file_to(path, current_only);
    });
    return;
  }
  const auto source = service_.current()->path();
  auto *session = project_file_.get();
  const auto root = project_file::FileSession::default_root();
  auto candidate =
      std::make_shared<std::unique_ptr<project_file::FileSession>>();
  start_file_operation(
      "Criando cópia do projeto",
      [path, source, root, session, current_only, candidate](auto &control) {
        if (session && !current_only)
          session->copy_to(path, &control);
        else
          project_file::pack_directory(
              source, path, project_file::TransportProfile::Blocks, &control);
        // Publication is already sealed: a subsequent failure must report that
        // the destination exists, and leave the original project active.
        if (current_only)
          *candidate = project_file::FileSession::open(path, root, Choice::Ask,
                                                       &control);
        return current_only ? (*candidate)->baseline().sequence : 0;
      },
      [this, path, current_only, candidate](const auto &result) {
        if (result.error) {
          try {
            std::rethrow_exception(result.error);
          } catch (const std::exception &e) {
            show_error(
                "Não foi possível concluir a cópia. Confira o destino: " +
                    path.string(),
                e);
          }
          return;
        }
        if (result.cancelled)
          return;
        if (current_only) {
          adopt_project_file(std::move(*candidate));
          status_bar_.set_text("Continuando na cópia do estado atual; o "
                               "original e seu histórico foram preservados.");
        } else
          status_bar_.set_text(
              "Cópia transportável criada; o projeto original continua ativo.");
      },
      !session || current_only, !session);
}
void MainWindow::show_file_history() {
  if (!project_file_ || file_task_ || modal_host_.has_dialog())
    return;
  if (!file_barrier_ready_) {
    start_file_confirmation([this] { show_file_history(); });
    return;
  }
  auto rows = std::make_shared<std::vector<project_file::Revision>>();
  auto *session = project_file_.get();
  start_file_operation(
      "Consultando estados do arquivo",
      [rows, session](auto &control) {
        control.checkpoint(project_file::OperationPhase::Reading);
        *rows = session->history(&control);
        control.check();
        return session->baseline().sequence;
      },
      [this, rows](const auto &result) {
        if (result.error) {
          try {
            std::rethrow_exception(result.error);
          } catch (const std::exception &e) {
            show_error("Estados do arquivo", e);
          }
          return;
        }
        if (!result.cancelled && !file_task_exit_)
          present_file_history(std::move(*rows));
      });
}
void MainWindow::present_file_history(
    std::vector<project_file::Revision> rows) {
  rows.erase(std::remove_if(rows.begin(), rows.end(),
                            [](const auto &r) { return r.sequence == 0; }),
             rows.end());
  auto entries =
      std::make_shared<std::vector<project_file::Revision>>(std::move(rows));
  auto page = std::make_shared<std::size_t>(
      entries->empty() ? 0 : (entries->size() - 1) / 100);
  auto *d = new OverlayDialog("Estados do arquivo único", *this, true);
  d->set_secondary_text(
      "Retomar cria uma nova versão e conserva o trabalho posterior. O "
      "trabalho atual foi confirmado antes de abrir esta lista.");
  auto *selector = Gtk::make_managed<Gtk::ComboBoxText>();
  selector->set_name("project-file-state");
  auto *navigation =
      Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 8);
  auto *older = Gtk::make_managed<Gtk::Button>("Estados anteriores");
  older->set_name("project-file-history-older");
  auto *newer = Gtk::make_managed<Gtk::Button>("Estados seguintes");
  newer->set_name("project-file-history-newer");
  auto *range = Gtk::make_managed<Gtk::Label>();
  range->set_name("project-file-history-range");
  navigation->append(*older);
  navigation->append(*range);
  navigation->append(*newer);
  d->get_content_area()->append(*selector);
  d->get_content_area()->append(*navigation);
  const auto fill = [entries, page, selector, older, newer, range] {
    selector->remove_all();
    const auto start = *page * 100,
               end = std::min(start + 100, entries->size());
    for (auto i = start; i < end; ++i) {
      const auto &r = entries->at(i);
      selector->append(std::to_string(r.sequence), std::to_string(r.sequence) +
                                                       " — " + r.label + " — " +
                                                       r.created_at);
    }
    if (end > start)
      selector->set_active(static_cast<int>(end - start - 1));
    older->set_sensitive(start > 0);
    newer->set_sensitive(end < entries->size());
    range->set_text(entries->empty() ? "Nenhum estado"
                                     : std::to_string(start + 1) + "–" +
                                           std::to_string(end) + " de " +
                                           std::to_string(entries->size()));
  };
  older->signal_clicked().connect([page, fill] {
    if (*page)
      --*page;
    fill();
  });
  newer->signal_clicked().connect([page, entries, fill] {
    if ((*page + 1) * 100 < entries->size())
      ++*page;
    fill();
  });
  fill();
  d->add_button("Voltar", Gtk::ResponseType::CANCEL);
  d->add_button("Retomar este estado", Gtk::ResponseType::ACCEPT);
  d->signal_response().connect([this, d, selector](int response) {
    const auto selected = selector->get_active_id().raw();
    retire_file_dialog(d);
    if (response != Gtk::ResponseType::ACCEPT || selected.empty() ||
        !project_file_)
      return;
    const auto old_work = project_file_->working();
    auto *session = project_file_.get();
    start_file_operation(
        "Retomando estado do arquivo",
        [session, sequence = std::stoll(selected)](auto &control) {
          return session->restore(sequence, &control).sequence;
        },
        [this, old_work](const auto &result) {
          if (result.error) {
            // A post-COMMIT error can require a new native binding. Never keep
            // writing through the old repositories after the file has advanced.
            if (project_file_->working() != old_work ||
                project_file_->needs_reopen()) {
              auto retained = std::move(project_file_);
              close_project();
            }
            try {
              std::rethrow_exception(result.error);
            } catch (const std::exception &e) {
              show_error("Não foi possível retomar o estado; as áreas foram "
                         "preservadas",
                         e);
            }
            return;
          }
          if (result.cancelled)
            return;
          if (project_file_->working() != old_work) {
            try {
              rebind_project_file();
            } catch (...) {
              auto error = std::current_exception();
              auto retained = std::move(project_file_);
              close_project();
              std::rethrow_exception(error);
            }
          }
          status_bar_.set_text(
              "Estado retomado; versões posteriores preservadas no arquivo.");
        });
  });
  d->present();
}
} // namespace inde::ui

#include "inde/ui/main_window.hpp"
#include "inde/ui/project_operations.hpp"
namespace inde::ui {
void MainWindow::navigate_removal_use(const project::OperationPreview &preview,
                                      const project::RemovalUse &use,
                                      std::function<void()> completed) {
  if (!service_.current() ||
      service_.current()->manifest().project_id != preview.project_id)
    throw std::runtime_error(
        "O Projeto ativo mudou; reabra a exclusão no Projeto de origem.");
  // Flush before observing another source; saved navigation never contains
  // text.
  writing_workspace_.flush_changes();
  const auto context = capture_context();
  try {
    if (use.location_table == "documents") {
      show_workspace(WorkspaceId::Writing);
      writing_workspace_.reveal_document(use.location_id);
      if (writing_workspace_.current_document_id() != use.location_id)
        throw std::runtime_error(
            "O Documento não pôde ser aberto; escrita de origem preservada.");
      writing_workspace_.reveal_usage_offset(use.text_offset);
      writing_workspace_.open_removal_use_tools(use);
    } else if (use.location_table == "works") {
      show_workspace(WorkspaceId::Catalog);
      catalog_workspace_.reveal_work_for_edit(use.location_id);
    } else if (use.location_table == "editorial_nodes") {
      show_workspace(WorkspaceId::Editorial);
      editorial_workspace_.reveal_node(use.location_id);
    } else if (use.location_table == "editorial_structures" ||
               (use.location_table == "structure_models" &&
                use.model_layer == "editorial")) {
      show_workspace(WorkspaceId::Editorial);
      editorial_workspace_.reveal_removal_use(use);
    } else if (use.location_table == "cartographic_positions") {
      show_workspace(WorkspaceId::Cartography);
      cartography_workspace_.reveal_removal_use(use);
    } else if (use.location_table == "project_operations") {
      show_project_operations();
      if (!operations_dialog_)
        throw std::runtime_error(
            "Histórico indisponível; nada foi restaurado.");
      auto *view = dynamic_cast<ProjectOperationsView *>(
          operations_dialog_->get_content_area()->get_first_child());
      if (!view)
        throw std::runtime_error(
            "Histórico indisponível; nada foi restaurado.");
      view->open_operation(use.location_id);
    } else {
      show_workspace(WorkspaceId::Planning);
      planning_workspace_.reveal_removal_use(use);
    }
  } catch (...) {
    restore_context(context);
    throw;
  }
  removal_returns_.push_back({context, preview, std::move(completed)});
  removal_return_.set_visible(true);
  status_bar_.set_text("Uso localizado: " + use.path +
                       ". Após resolver o vínculo, volte à exclusão.");
}
void MainWindow::return_to_removal() {
  if (removal_returns_.empty())
    return;
  const auto frame = removal_returns_.back();
  try {
    if (!service_.current() ||
        service_.current()->manifest().project_id != frame.preview.project_id)
      throw std::runtime_error(
          "O Projeto mudou. Abra a exclusão novamente no Projeto de origem.");
    writing_workspace_.flush_changes();
    restore_context(frame.context);
    confirm_project_removal(*this, service_, frame.preview.removal,
                            frame.preview.target_id, frame.completed);
    removal_returns_.pop_back();
    removal_return_.set_visible(!removal_returns_.empty());
  } catch (const std::exception &e) {
    show_error("Não foi possível voltar à exclusão", e);
  }
}
void MainWindow::show_project_operations() {
  try {
    if (!service_.current())
      throw std::runtime_error("Abra um Projeto para consultar suas operações");
    writing_workspace_.flush_changes();
    const auto context = capture_context();
    operations_dialog_ = std::make_unique<OverlayDialog>(
        "Histórico de alterações do projeto", *this);
    operations_dialog_->set_secondary_text(
        "Reversão de cadastro e organização, separada da edição textual.");
    operations_dialog_->add_button("Fechar", Gtk::ResponseType::CLOSE);
    auto *view = Gtk::make_managed<ProjectOperationsView>(service_, [this,
                                                                     context] {
      // No pending editor may replay its pre-reversal baseline on top of the
      // confirmed transaction. Retake navigation only; never saved text.
      writing_workspace_.reset();
      review_workspace_.reset();
      planning_workspace_.reset();
      catalog_workspace_.reset();
      editorial_workspace_.reset();
      graphs_workspace_.reset();
      cartography_workspace_.reset();
      pending_views_.clear();
      if (!service_.current()) {
        close_project();
        status_bar_.set_text(
            "Operação confirmada; reabra o Projeto para retomar");
        return;
      }
      restore_context(context);
      remember_context();
      status_bar_.set_text(
          "Operação do Projeto revertida; contexto retomado com fontes atuais");
    });
    operations_dialog_->get_content_area()->append(*view);
    operations_dialog_->signal_response().connect([this](int) {
      if (operations_dialog_) {
        operations_dialog_->hide();
        // The adapter deletes itself after signal_response; do not retain a
        // second owner that would delete it again on the next opening.
        static_cast<void>(operations_dialog_.release());
      }
    });
    operations_dialog_->present();
  } catch (const std::exception &e) {
    show_error("Histórico de operações indisponível", e);
  }
}
} // namespace inde::ui

#include "inde/ui/main_window.hpp"
#include "inde/ui/accessibility.hpp"

namespace inde::ui {
void MainWindow::setup_keyboard() {
  header_.pack_end(commands_button_);
  set_accessible_label(commands_button_, "Buscar comandos — Ctrl+Shift+P");
  keyboard_ = std::make_unique<KeyboardCommands>(*this, commands_button_,
      [this] { return modal_host_.has_dialog() || writing_workspace_.composing_input(); },
      [this](const std::string &message) { status_bar_.set_text(message); });
  auto any = [](Gtk::Widget *) { return true; };
  auto project = [this] { return service_.current() ? std::string{} : "Abra ou crie um projeto primeiro. Não é necessário cadastrar uma obra para escrever."; };
  auto action = [this, any](std::string id, std::string label, std::string group, std::string key,
                            std::function<std::string()> disabled = {}) {
    auto name = id;
    keyboard_->add({std::move(id), std::move(label), std::move(group), std::move(key), {}, any,
        std::move(disabled), [this, name] { activate_action("win." + name); }});
  };
  add_action("command-palette", [this] { keyboard_->show(); });
  add_action("keyboard-help", [this] { keyboard_->show(true); });
  commands_button_.signal_clicked().connect([this] { keyboard_->show(); });
  action("command-palette", "Buscar comandos", "Geral", "<Control><Shift>p");
  action("keyboard-help", "Teclado, ajuda e personalização", "Geral", "<Control>F1");
  keyboard_->add({"context-help", "Ajuda deste contexto", "Geral", "F1", {}, any, {}, [this] { keyboard_->show(true); }});
  action("new-project", "Novo projeto", "Projeto", "<Control>n");
  action("open-project", "Abrir projeto…", "Projeto", "<Control>o");
  action("save-project", "Salvar rascunho e projeto", "Projeto", "<Control>s", project);
  action("save-project-as", "Salvar uma cópia…", "Projeto", "<Control><Shift>s", project);
  action("close-project", "Fechar projeto", "Projeto", "<Control><Shift>w", project);
  action("quit", "Sair com proteção dos rascunhos", "Projeto", "<Control>q");
  action("show-writing", "Escrever", "Navegação", "<Control>1", project);
  action("show-planning", "Planejar", "Navegação", "<Control>2", project);
  action("show-review", "Revisar", "Navegação", "<Control>3", project);
  action("show-editorial", "Manuscrito", "Navegação", "<Control>4", project);
  action("show-graphs", "Cronologia — projeção somente leitura", "Navegação", "<Control>5", project);
  action("show-cartography", "Lugares e mapas", "Navegação", "<Control>6", project);
  action("show-catalog", "Obras e universos", "Navegação", "<Control>7", project);
  action("saved-workspaces", "Espaços de trabalho salvos…", "Navegação", "<Control><Shift>9", project);
  action("project-operations", "Histórico de alterações do projeto…", "Projeto", "<Control><Shift>h", project);
  action("toggle-focus", "Modo Foco", "Escrever", "<Control><Shift>f", [this, project] {
    if (auto reason = project(); !reason.empty()) return reason;
    return workspace_shell_.current() == WorkspaceId::Writing ? std::string{} : "O modo Foco está disponível em Escrever.";
  });
  keyboard_->add({"main-menu", "Abrir menu principal", "Geral", "F10", {}, any, {}, [this] { application_menu_.popup(); }});
  keyboard_->add({"leave-view", "Sair da vista atual sem fechar projeto", "Navegação", "<Control>w", {}, any, project, [this] {
    if (workspace_shell_.current() == WorkspaceId::Writing) writing_workspace_.leave_view();
    else if (builder_return_context_) { restore_context(*builder_return_context_); builder_return_context_.reset(); update_builder_return(); }
    else show_workspace(WorkspaceId::Writing);
  }});
  action("proofread-current", "Revisar ortografia do campo em foco", "Campo de texto", "F7",[this]{return writing_workspace_.composing_input() ? std::string("Conclua a composição de texto antes de revisar.") : std::string{};});
  action("manage-dictionaries", "Gerenciar dicionários…", "Ferramentas", "");
  action("author-vocabulary", "Vocabulário do INDE…", "Ajuda", "");
  action("theme-light", "Tema claro", "Aparência", "");
  action("theme-dark", "Tema escuro", "Aparência", "");
  action("theme-system", "Tema do sistema", "Aparência", "");
  keyboard_->add({"auxiliary-region", "Mostrar ou ocultar painel de contexto", "Contexto", "F9", {}, any, [this,project]{
    if(auto reason=project();!reason.empty())return reason;
    return workspace_shell_.current()==WorkspaceId::Review ? std::string("Esta vista de revisão não possui painel auxiliar.") : std::string{};
  }, [this] {
    if (workspace_shell_.current() == WorkspaceId::Writing) activate_action("win.writing-tools");
    else if (workspace_shell_.current() == WorkspaceId::Planning || workspace_shell_.current() == WorkspaceId::Catalog) toggle_sidebar();
    else if(workspace_shell_.current()==WorkspaceId::Cartography)activate_action("win.map-context");
    else toggle_inspector();
  }});
  keyboard_->regions([this] {
    if(workspace_shell_.current()==WorkspaceId::Writing) {
      std::vector<Gtk::Widget *> regions{&header_,&workspace_shell_.navigation_region()};
      const auto writing=writing_workspace_.keyboard_regions();regions.insert(regions.end(),writing.begin(),writing.end());return regions;
    }
    Gtk::Widget *workspace = nullptr;
    switch (workspace_shell_.current()) {
      case WorkspaceId::Writing: workspace = &writing_workspace_; break;
      case WorkspaceId::Planning: workspace = &planning_workspace_; break;
      case WorkspaceId::Review: workspace = &review_workspace_; break;
      case WorkspaceId::Catalog: workspace = &catalog_workspace_; break;
      case WorkspaceId::Editorial: workspace = &editorial_workspace_; break;
      case WorkspaceId::Graphs: workspace = &graphs_workspace_; break;
      case WorkspaceId::Cartography: workspace = &cartography_workspace_; break;
    }
    return std::vector<Gtk::Widget *>{&header_, &workspace_shell_.navigation_region(), workspace};
  });
  writing_workspace_.install_keyboard(*keyboard_);
  cartography_workspace_.install_keyboard(*keyboard_);
  planning_workspace_.install_keyboard(*keyboard_);
  review_workspace_.install_keyboard(*keyboard_);
  catalog_workspace_.install_keyboard(*keyboard_);
  editorial_workspace_.install_keyboard(*keyboard_);
}
}

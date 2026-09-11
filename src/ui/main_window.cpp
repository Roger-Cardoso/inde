#include "inde/ui/main_window.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/dictionary_manager_dialog.hpp"
#include "inde/ui/overlay_dialog.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>

namespace inde::ui {
MainWindow::MainWindow(std::optional<std::filesystem::path> initial_project)
    : proofreading_service_(dictionary_store_, spelling_provider_),
      proofreading_controller_(proofreading_service_) {
  install_application_theme();

auto display = Gdk::Display::get_default();
  if (display) {
    auto icon_theme = Gtk::IconTheme::get_for_display(display);
    // Permite que o GTK encontre o ícone dentro do pacote AppImage
    icon_theme->add_search_path(Glib::get_user_data_dir() + "/icons");
  }
  set_title("INDE");
  set_default_size(1200, 760);
  set_size_request(760, 520);
  build_ui();
  setup_actions();
  setup_menus();
  ProofreadingController::set_global(&proofreading_controller_);
  proofreading_controller_.watch(*this);
  refresh_recents();
  maximize();
  if (initial_project)
    open_project_at(*initial_project);
}

void MainWindow::build_ui() {
  add_css_class("inde-window");
  header_.add_css_class("inde-header");
  set_titlebar(header_);
  header_heading_.set_spacing(0);
  header_title_.add_css_class("header-title");
  header_subtitle_.add_css_class("header-subtitle");
  header_title_.set_ellipsize(Pango::EllipsizeMode::END);
  header_subtitle_.set_ellipsize(Pango::EllipsizeMode::END);
  header_heading_.append(header_title_);
  header_heading_.append(header_subtitle_);
  header_.set_title_widget(header_heading_);

  new_button_.set_label("Novo");
  new_button_.set_tooltip_text("Novo projeto (Ctrl+N)");
  set_accessible_label(new_button_, "Novo projeto");
  set_accessible_description(new_button_,
                             "Cria um novo projeto INDE. Atalho Control+N.");
  open_button_.set_label("Abrir");
  open_button_.set_tooltip_text("Abrir projeto (Ctrl+O)");
  set_accessible_label(open_button_, "Abrir projeto");
  set_accessible_description(
      open_button_, "Abre uma pasta de projeto INDE. Atalho Control+O.");
  save_button_.set_label("Salvar");
  save_button_.set_tooltip_text("Salvar projeto (Ctrl+S)");
  set_accessible_label(save_button_, "Salvar projeto");
  set_accessible_description(save_button_,
                             "Salva o projeto atual. Atalho Control+S.");
  application_menu_.set_icon_name("open-menu-symbolic");
  application_menu_.set_tooltip_text("Menu do aplicativo");
  set_accessible_label(application_menu_, "Menu do aplicativo");
  set_accessible_description(application_menu_,
                             "Abre comandos de projeto e ajuda.");
  application_menu_.add_css_class("header-action");
  application_menu_.add_css_class("circular");

  header_.pack_start(new_button_);
  header_.pack_start(open_button_);
  header_.pack_end(save_button_);
  header_.pack_end(application_menu_);
  // Filtros pertencem às barras de cada workspace; o atalho global continua
  // disponível, mas não duplica o mesmo comando no cabeçalho.
  sidebar_toggle_.set_visible(false);
  for (auto *button :
       {&sidebar_toggle_, &new_button_, &open_button_, &inspector_toggle_,
        &close_button_, &save_button_, &save_as_button_})
    button->add_css_class("header-action");
  save_button_.set_sensitive(false);
  save_as_button_.set_sensitive(false);
  close_button_.set_sensitive(false);
  sidebar_toggle_.set_sensitive(false);
  inspector_toggle_.set_sensitive(false);
  inspector_toggle_.set_visible(false);

  set_child(modal_host_);
  modal_host_.set_content(root_);
  menu_bar_.add_css_class("app-menubar");
  root_.append(menu_bar_);
  root_.append(stack_);
  stack_.set_vexpand(true);
  // As telas de boas-vindas e de Projeto possuem conteúdos muito diferentes.
  // Uma pilha homogênea propaga para a janela o maior tamanho mínimo de uma
  // página oculta e pode forçar o gerenciador de janelas a desmaximizá-la.
  stack_.set_hhomogeneous(false);
  stack_.set_vhomogeneous(false);
  stack_.set_transition_type(Gtk::StackTransitionType::CROSSFADE);

  welcome_.set_halign(Gtk::Align::CENTER);
  welcome_.set_valign(Gtk::Align::CENTER);
  welcome_.set_size_request(460, -1);
  welcome_.set_margin(24);
  welcome_title_.add_css_class("title-1");
  welcome_subtitle_.add_css_class("dim-label");
  recent_title_.add_css_class("title-3");
  recent_title_.set_margin_top(24);
  welcome_.append(welcome_title_);
  welcome_.append(welcome_subtitle_);
  welcome_.append(recent_title_);
  welcome_.append(recent_list_);
  recent_list_.add_css_class("boxed-list");

  project_view_.set_halign(Gtk::Align::FILL);
  project_view_.set_valign(Gtk::Align::FILL);
  project_view_.set_hexpand(true);
  project_view_.append(workspace_shell_);
  project_view_.append(status_bar_);
  workspace_shell_.add_workspace(catalog_workspace_, WorkspaceId::Catalog);
  workspace_shell_.add_workspace(planning_workspace_, WorkspaceId::Planning);
  workspace_shell_.add_workspace(writing_workspace_, WorkspaceId::Writing);
  workspace_shell_.add_workspace(editorial_workspace_, WorkspaceId::Editorial);
  workspace_shell_.add_workspace(graphs_workspace_, WorkspaceId::Graphs);
  workspace_shell_.show(WorkspaceId::Catalog);
  catalog_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) { status_bar_.set_text(message); });
  catalog_workspace_.signal_open_work_requested().connect(
      [this](const std::string &work_id) {
        editorial_workspace_.reveal_work(work_id);
        show_workspace(WorkspaceId::Editorial);
      });
  editorial_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) { status_bar_.set_text(message); });
  editorial_workspace_.signal_open_document_requested().connect(
      [this](const std::string &id) {
        show_workspace(WorkspaceId::Writing);
        writing_workspace_.reveal_document(id);
      });
  editorial_workspace_.signal_create_document_requested().connect(
      [this](const std::string &node_id) {
        show_workspace(WorkspaceId::Writing);
        writing_workspace_.create_document_for_editorial_node(node_id);
      });
  editorial_workspace_.signal_filter_documents_requested().connect(
      [this](const std::string &node_id) {
        show_workspace(WorkspaceId::Writing);
        writing_workspace_.show_library_for_editorial_node(node_id);
      });
  planning_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) { status_bar_.set_text(message); });
  planning_workspace_.signal_open_document_requested().connect(
      [this](const std::string &id) {
        show_workspace(WorkspaceId::Writing);
        writing_workspace_.reveal_document(id);
      });
  planning_workspace_.signal_filter_documents_requested().connect(
      [this](const std::string &entity_id) {
        show_workspace(WorkspaceId::Writing);
        writing_workspace_.show_library_for_entity(entity_id);
      });
  writing_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) { status_bar_.set_text(message); });
  writing_workspace_.signal_entity_source_requested().connect(
      [this](const std::string &entity_id) {
        show_workspace(WorkspaceId::Planning);
        planning_workspace_.reveal_entity(entity_id);
      });
  writing_workspace_.signal_editorial_source_requested().connect(
      [this](const std::string &node_id) {
        show_workspace(WorkspaceId::Editorial);
        editorial_workspace_.reveal_node(node_id);
      });
  graphs_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) { status_bar_.set_text(message); });
  graphs_workspace_.signal_entity_source_requested().connect(
      [this](const std::string &id) {
        show_workspace(WorkspaceId::Planning);
        planning_workspace_.reveal_entity(id);
      });
  graphs_workspace_.signal_time_point_source_requested().connect(
      [this](const std::string &id) {
        show_workspace(WorkspaceId::Planning);
        planning_workspace_.reveal_time_point(id);
      });
  status_bar_.set_halign(Gtk::Align::START);
  status_bar_.set_hexpand(true);
  status_bar_.add_css_class("status-bar");
  status_bar_.set_margin_start(12);
  status_bar_.set_margin_end(12);
  status_bar_.set_margin_bottom(6);
  stack_.add(welcome_, "welcome");
  stack_.add(project_view_, "project");
  stack_.set_visible_child("welcome");
}

void MainWindow::refresh_recents() {
  while (auto *child = recent_list_.get_first_child())
    recent_list_.remove(*child);
  const auto recents = service_.recent_projects();
  recent_title_.set_visible(!recents.empty());
  recent_list_.set_visible(!recents.empty());
  for (const auto &path : recents) {
    auto *button = Gtk::make_managed<Gtk::Button>(path.stem().string());
    button->set_tooltip_text(path.string());
    button->set_halign(Gtk::Align::FILL);
    button->signal_clicked().connect([this, path] { open_project_at(path); });
    recent_list_.append(*button);
  }
}

void MainWindow::update_project_view() {
  const auto *current = service_.current();
  if (!current)
    return;
  set_title(current->manifest().name + " — INDE");
  header_title_.set_text(current->manifest().name + " — INDE");
  header_subtitle_.set_visible(false);
  status_bar_.set_text("Todas as alterações foram salvas");
  save_button_.set_sensitive(true);
  save_as_button_.set_sensitive(true);
  close_button_.set_sensitive(true);
  stack_.set_visible_child("project");
  refresh_recents();
  const auto workspace = workspace_shell_.current();
  // A troca de workspace já atualiza a superfície de destino. Atualizar todos
  // durante a abertura materializava consultas e widgets invisíveis, custo que
  // cresce justamente nos Projetos grandes.
  refresh_workspace(workspace);
  sidebar_toggle_.set_sensitive(workspace == WorkspaceId::Catalog ||
                                workspace == WorkspaceId::Planning);
  inspector_toggle_.set_sensitive(workspace == WorkspaceId::Graphs);
  inspector_toggle_.set_visible(workspace == WorkspaceId::Graphs);
}

void MainWindow::toggle_sidebar() {
  if (workspace_shell_.current() == WorkspaceId::Planning)
    planning_workspace_.toggle_navigation();
  else if (workspace_shell_.current() == WorkspaceId::Catalog)
    catalog_workspace_.toggle_navigation();
}

void MainWindow::toggle_inspector() {
  if (workspace_shell_.current() == WorkspaceId::Planning)
    planning_workspace_.toggle_inspector();
  else if (workspace_shell_.current() == WorkspaceId::Editorial)
    editorial_workspace_.toggle_inspector();
  else if (workspace_shell_.current() == WorkspaceId::Graphs)
    graphs_workspace_.toggle_inspector();
}

void MainWindow::refresh_workspace(WorkspaceId workspace) {
  const auto started = std::chrono::steady_clock::now();
  if (workspace == WorkspaceId::Planning)
    planning_workspace_.refresh();
  else if (workspace == WorkspaceId::Catalog)
    catalog_workspace_.refresh();
  else if (workspace == WorkspaceId::Editorial)
    editorial_workspace_.refresh();
  else if (workspace == WorkspaceId::Writing)
    writing_workspace_.refresh();
  else if (workspace == WorkspaceId::Graphs)
    graphs_workspace_.refresh();
  if (std::getenv("INDE_PROFILE_WORKSPACES")) {
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started);
    std::clog << "INDE_PROFILE workspace=" << workspace_name(workspace)
              << " refresh_ms=" << elapsed.count() << '\n';
  }
}

void MainWindow::show_workspace(WorkspaceId workspace) {
  const bool preserve_maximized = is_maximized();
  if (workspace_shell_.current() == WorkspaceId::Writing &&
      workspace != WorkspaceId::Writing) {
    try {
      writing_workspace_.flush_changes();
    } catch (const std::exception &error) {
      show_error("Não foi possível salvar o Documento", error);
      return;
    }
  }
  refresh_workspace(workspace);
  workspace_shell_.show(workspace);
  const bool has_filters =
      workspace == WorkspaceId::Catalog || workspace == WorkspaceId::Planning;
  sidebar_toggle_.set_sensitive(has_filters);
  inspector_toggle_.set_sensitive(workspace == WorkspaceId::Graphs);
  inspector_toggle_.set_visible(workspace == WorkspaceId::Graphs);
  if (workspace == WorkspaceId::Planning)
    status_bar_.set_text("Planejamento narrativo ativo");
  else if (workspace == WorkspaceId::Writing)
    status_bar_.set_text("Biblioteca de Documentos ativa");
  else if (workspace == WorkspaceId::Graphs)
    status_bar_.set_text("Linha do tempo ficcional ativa (somente leitura)");
  else if (workspace == WorkspaceId::Catalog)
    status_bar_.set_text("Catálogo global de IPs e Obras ativo");
  else
    status_bar_.set_text("Workspace editorial ativo");
  if (preserve_maximized)
    Glib::signal_idle().connect_once([this] {
      if (!is_maximized())
        maximize();
    });
}

void MainWindow::setup_actions() {
  add_action("new-project", sigc::mem_fun(*this, &MainWindow::create_project));
  add_action("open-project", sigc::mem_fun(*this, &MainWindow::open_project));
  add_action("save-project", sigc::mem_fun(*this, &MainWindow::save_project));
  add_action("save-project-as",
             sigc::mem_fun(*this, &MainWindow::save_project_as));
  add_action("close-project", sigc::mem_fun(*this, &MainWindow::close_project));
  add_action("toggle-sidebar",
             sigc::mem_fun(*this, &MainWindow::toggle_sidebar));
  add_action("toggle-inspector",
             sigc::mem_fun(*this, &MainWindow::toggle_inspector));
  add_action("show-catalog", [this] { show_workspace(WorkspaceId::Catalog); });
  add_action("show-planning",
             [this] { show_workspace(WorkspaceId::Planning); });
  add_action("show-writing", [this] { show_workspace(WorkspaceId::Writing); });
  add_action("show-editorial",
             [this] { show_workspace(WorkspaceId::Editorial); });
  add_action("show-graphs", [this] { show_workspace(WorkspaceId::Graphs); });
  add_action("proofread-current",
             [this] { proofreading_controller_.show_current_issue(); });
  add_action("manage-dictionaries",
             sigc::mem_fun(*this, &MainWindow::show_dictionaries));
  add_action("about", sigc::mem_fun(*this, &MainWindow::show_about));
  add_action("quit", [this] { close(); });
  catalog_workspace_.install_actions(*this);
  editorial_workspace_.install_actions(*this);
  planning_workspace_.install_actions(*this);
  writing_workspace_.install_actions(*this);

  new_button_.set_action_name("win.new-project");
  open_button_.set_action_name("win.open-project");
  save_button_.set_action_name("win.save-project");
  save_as_button_.set_action_name("win.save-project-as");
  sidebar_toggle_.set_action_name("win.toggle-sidebar");
  close_button_.set_action_name("win.close-project");
  inspector_toggle_.set_action_name("win.toggle-inspector");

  // Vincular uma ação pode atualizar a semântica acessível do GTK. Registrar
  // os nomes depois do vínculo preserva a leitura dos ícones pela AT-SPI.
  set_accessible_label(new_button_, "Novo projeto");
  set_accessible_label(open_button_, "Abrir projeto");
  set_accessible_label(save_button_, "Salvar projeto");
}

void MainWindow::setup_menus() {
  const auto file = Gio::Menu::create();
  file->append("Novo projeto", "win.new-project");
  file->append("Abrir projeto…", "win.open-project");
  file->append("Salvar", "win.save-project");
  file->append("Salvar uma cópia…", "win.save-project-as");
  file->append("Fechar projeto", "win.close-project");
  file->append("Sair", "win.quit");

  const auto navigate = Gio::Menu::create();
  navigate->append("Catálogo", "win.show-catalog");
  navigate->append("Planejamento", "win.show-planning");
  navigate->append("Escrita", "win.show-writing");
  navigate->append("Editorial", "win.show-editorial");
  navigate->append("Gráficos", "win.show-graphs");

  const auto edit = Gio::Menu::create();
  edit->append("Desfazer edição", "win.writing-undo");
  edit->append("Refazer edição", "win.writing-redo");
  edit->append("Negrito", "win.writing-bold");
  edit->append("Itálico", "win.writing-italic");
  edit->append("Sublinhado", "win.writing-underline");

  const auto view = Gio::Menu::create();
  view->append("Abrir filtros", "win.toggle-sidebar");
  view->append("Alternar painel contextual", "win.toggle-inspector");

  const auto tools = Gio::Menu::create();
  tools->append("Revisar campo atual", "win.proofread-current");
  tools->append("Dicionários…", "win.manage-dictionaries");

  const auto help = Gio::Menu::create();
  help->append("Sobre o INDE", "win.about");

  const auto bar = Gio::Menu::create();
  bar->append_submenu("Arquivo", file);
  bar->append_submenu("Editar", edit);
  bar->append_submenu("Navegar", navigate);
  bar->append_submenu("Exibir", view);
  bar->append_submenu("Ferramentas", tools);
  bar->append_submenu("Ajuda", help);
  menu_bar_.set_menu_model(bar);

  const auto application = Gio::Menu::create();
  application->append_section(file);
  application->append_section(edit);
  application->append_section(tools);
  application->append_section(help);
  application_menu_.set_menu_model(application);
}

void MainWindow::show_dictionaries() {
  show_dictionary_manager(*this, proofreading_service_,
                          proofreading_controller_);
}

void MainWindow::show_about() {
  auto *dialog = new OverlayDialog("Sobre o INDE", *this, true);
  dialog->set_secondary_text(
      "Integrated Narrative Development Environment\n"
      "Um ambiente local para catálogo, planejamento, escrita, estrutura "
      "editorial e visualização temporal de projetos narrativos.");
  dialog->add_button("Fechar", Gtk::ResponseType::CLOSE);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
}

void MainWindow::create_project() {
  auto dialog = Gtk::FileChooserNative::create("Criar projeto INDE", *this,
                                               Gtk::FileChooser::Action::SAVE,
                                               "Criar", "Cancelar");
  dialog->set_current_name("Novo projeto.inde");
  dialog->signal_response().connect([this, dialog](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      writing_workspace_.flush_changes();
      const auto file = dialog->get_file();
      if (!file)
        return;
      auto path = std::filesystem::path(file->get_path());
      auto name = path.stem().string();
      service_.create(path, name.empty() ? "Novo projeto" : name);
      planning_workspace_.reset();
      writing_workspace_.reset();
      update_project_view();
    } catch (const std::exception &error) {
      show_error("Não foi possível criar o projeto", error);
    }
  });
  dialog->show();
}

void MainWindow::open_project() {
  auto dialog = Gtk::FileDialog::create();
  dialog->set_title("Abrir projeto INDE");
  dialog->select_folder(
      *this, [this, dialog](const Glib::RefPtr<Gio::AsyncResult> &result) {
        try {
          const auto folder = dialog->select_folder_finish(result);
          if (folder)
            open_project_at(folder->get_path());
        } catch (const Gtk::DialogError &) {
          // Cancelamento pelo usuário não é um erro do aplicativo.
        } catch (const Glib::Error &error) {
          const std::runtime_error wrapped(error.what());
          show_error("Não foi possível selecionar o projeto", wrapped);
        }
      });
}

void MainWindow::open_project_at(const std::filesystem::path &path) {
  try {
    writing_workspace_.flush_changes();
    service_.open(path);
    planning_workspace_.reset();
    writing_workspace_.reset();
    update_project_view();
  } catch (const std::exception &error) {
    show_error("Não foi possível abrir o projeto", error);
    refresh_recents();
  }
}

void MainWindow::save_project() {
  try {
    writing_workspace_.flush_changes();
    service_.save();
    update_project_view();
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar o projeto", error);
  }
}

void MainWindow::save_project_as() {
  const auto *current = service_.current();
  if (!current)
    return;
  auto dialog = Gtk::FileChooserNative::create(
      "Salvar uma cópia independente", *this, Gtk::FileChooser::Action::SAVE,
      "Salvar", "Cancelar");
  dialog->set_current_name(current->path().stem().string() + " — Cópia.inde");
  dialog->signal_response().connect([this, dialog](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      writing_workspace_.flush_changes();
      const auto file = dialog->get_file();
      if (!file)
        return;
      service_.save_as(file->get_path());
      update_project_view();
      status_bar_.set_text(
          "Cópia independente criada; este é agora o projeto ativo");
    } catch (const std::exception &error) {
      show_error("Não foi possível salvar como", error);
    }
  });
  dialog->show();
}

void MainWindow::close_project() {
  try {
    writing_workspace_.flush_changes();
    service_.close();
  } catch (const std::exception &error) {
    show_error("Não foi possível fechar o projeto", error);
    return;
  }
  catalog_workspace_.reset();
  editorial_workspace_.reset();
  planning_workspace_.reset();
  writing_workspace_.reset();
  graphs_workspace_.reset();
  header_title_.set_text("INDE");
  header_subtitle_.set_text("Ambiente de desenvolvimento narrativo");
  header_subtitle_.set_visible(true);
  set_title("INDE");
  save_button_.set_sensitive(false);
  save_as_button_.set_sensitive(false);
  close_button_.set_sensitive(false);
  sidebar_toggle_.set_sensitive(false);
  inspector_toggle_.set_sensitive(false);
  inspector_toggle_.set_visible(false);
  stack_.set_visible_child("welcome");
  refresh_recents();
  status_bar_.set_text("Projeto fechado");
}

bool MainWindow::on_close_request() {
  try {
    writing_workspace_.flush_changes();
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar o Documento", error);
    return true;
  }
  return Gtk::ApplicationWindow::on_close_request();
}

void MainWindow::show_error(const Glib::ustring &title,
                            const std::exception &error) {
  auto *dialog =
      new OverlayDialog(*this, error.what(), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->show();
}

} // namespace inde::ui

#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/author_vocabulary_dialog.hpp"
#include "inde/ui/catalog_vocabulary.hpp"
#include "inde/ui/project_operations.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/ui_preferences.hpp"
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
  setup_keyboard();
  modal_host_.signal_active_changed().connect([this](bool active){header_.set_sensitive(!active);});
  setup_menus();
  ProofreadingController::set_global(&proofreading_controller_);
  proofreading_controller_.watch(*this);
  refresh_recents();
  maximize();
  if (initial_project)
    open_project_at(*initial_project);
  context_save_=Glib::signal_timeout().connect([this] {
    if(service_.current() && !modal_host_.has_dialog())remember_context();
    return true;
  },2000);
}

MainWindow::~MainWindow() { context_save_.disconnect(); maximize_idle_.disconnect(); }

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
  header_.pack_end(application_menu_);
  appearance_menu_.set_icon_name("weather-clear-symbolic");
  appearance_menu_.set_tooltip_text("Aparência: claro, escuro ou sistema");
  set_accessible_label(appearance_menu_, "Escolher tema");
  header_.pack_end(appearance_menu_);
  focus_button_.set_tooltip_text("Ocultar navegação e ferramentas (Ctrl+Shift+F)");
  set_accessible_label(focus_button_, "Modo Foco");
  header_.pack_end(focus_button_);
  removal_return_.set_visible(false);
  removal_return_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::return_to_removal));
  header_.pack_start(removal_return_);
  builder_return_.set_visible(false);header_.pack_start(builder_return_);
  builder_return_.signal_clicked().connect([this] {
    if(!builder_return_context_)return;
    try { const auto context=*builder_return_context_;restore_context(context);builder_return_context_.reset();builder_return_.set_visible(false);remember_context(); }
    catch(const std::exception &e) {show_error("Não foi possível voltar ao contexto de origem",e);}
  });
  focus_button_.set_sensitive(false);
  focus_button_.signal_toggled().connect([this] {
    if (modal_host_.has_dialog()) {
      focus_button_.set_active(focused_);
      return;
    }
    set_focus_mode(focus_button_.get_active());
  });
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
  menu_bar_.set_visible(false);
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
  workspace_shell_.add_workspace(review_workspace_, WorkspaceId::Review);
  review_workspace_.set_change_handler([this](const project::Document &before,const project::Document &after,const std::string &label) {
    return writing_workspace_.apply_editorial_change(before,after,label);
  });
  review_workspace_.signal_open_document_requested().connect([this](const std::string &id) {
    focus_button_.set_active(false);
    show_workspace(WorkspaceId::Writing);
    writing_workspace_.reveal_document(id);
  });
  workspace_shell_.add_workspace(editorial_workspace_, WorkspaceId::Editorial);
  workspace_shell_.add_workspace(graphs_workspace_, WorkspaceId::Graphs);
  workspace_shell_.add_workspace(cartography_workspace_,
                                 WorkspaceId::Cartography);
  cartography_workspace_.signal_entity_source_requested().connect(
      [this](const std::string &id) {
        show_workspace(WorkspaceId::Planning);
        planning_workspace_.reveal_entity(id);
      });
  planning_workspace_.signal_cartography_requested().connect(
      [this](const std::string &id) {
        show_workspace(WorkspaceId::Cartography);
        cartography_workspace_.reveal_entity(id);
      });
  workspace_shell_.show(WorkspaceId::Catalog);
  catalog_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) { status_bar_.set_text(message); });
  catalog_workspace_.signal_vocabulary_changed().connect([this] {
    workspace_shell_.refresh_vocabulary();
    setup_menus();
    if (workspace_shell_.current() == WorkspaceId::Catalog)
      status_bar_.set_text(author::catalog_name());
  });
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
  planning_workspace_.signal_write_unit_requested().connect([this](const std::string &id) {
    try { writing_workspace_.flush_changes();const auto context=capture_context();show_workspace(WorkspaceId::Writing);writing_workspace_.reveal_document(id);
      if(writing_workspace_.current_document_id()!=id)throw std::runtime_error("Documento não pôde ser aberto; planejamento preservado");
      builder_return_context_=context;update_builder_return();
    } catch(const std::exception &e) {show_error("Não foi possível abrir o texto associado",e);}
  });
  planning_workspace_.signal_realization_requested().connect([this](const application::BuilderRealization &r) {
    try {
      writing_workspace_.flush_changes();const auto context=capture_context();
      writing_workspace_.reveal_realization(r);
      show_workspace(WorkspaceId::Writing);
      // show_workspace can restore an older preference. Reapply this explicit
      // target after that restoration, without silently choosing another range.
      writing_workspace_.reveal_realization(r);
      builder_return_context_=context;update_builder_return();remember_context();
    } catch(const std::exception &e){show_error("Não foi possível abrir o trecho/ocorrência",e);}
  });
  planning_workspace_.signal_filter_documents_requested().connect(
      [this](const std::string &entity_id) {
        show_workspace(WorkspaceId::Writing);
        writing_workspace_.show_library_for_entity(entity_id);
      });
  writing_workspace_.signal_status_message().connect(
      [this](const Glib::ustring &message) { status_bar_.set_text(message); });
  writing_workspace_.signal_narrative_source_requested().connect([this](const project::RemovalUse &use,const ViewState &origin){
    writing_workspace_.flush_changes();auto state=capture_context().as_object();state["writing"]=origin;const ViewState context=state;
    try{show_workspace(WorkspaceId::Planning);planning_workspace_.reveal_removal_use(use);
      builder_return_context_=context;update_builder_return();remember_context();}
    catch(...){restore_context(context);throw;}
  });
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
  if(builder_return_context_ && builder_return_context_->at("project").as_string()!=current->manifest().project_id){builder_return_context_.reset();builder_return_.set_visible(false);}
  if (!removal_returns_.empty() && removal_returns_.back().preview.project_id != current->manifest().project_id) {
    removal_returns_.clear(); removal_return_.set_visible(false);
  }
  set_title(current->manifest().name + " — INDE");
  header_title_.set_text(current->manifest().name + " — INDE");
  header_subtitle_.set_visible(false);
  status_bar_.set_text("Todas as alterações foram salvas");
  save_button_.set_sensitive(true);
  save_as_button_.set_sensitive(true);
  close_button_.set_sensitive(true);
  stack_.set_visible_child("project");
  refresh_recents();
  const auto group = project_preference_group(current->manifest().project_id);
  if(restored_project_id_!=current->manifest().project_id) {
    restored_project_id_=current->manifest().project_id;
    pending_views_.clear();
    const auto context=ui_preference(group,"context-v1");
    if(!context.empty()) {
      try { restore_context(parse_view_state(context));return; }
      catch(const std::exception &error) {
        pending_views_.clear();
        status_bar_.set_text(std::string("Contexto salvo indisponível; Projeto preservado: ")+author::diagnostic(error.what()));
      }
    }
  }
  auto workspace = WorkspaceId::Writing;
  const auto saved_workspace = ui_preference(group, "workspace", "writing");
  for (const auto candidate : {WorkspaceId::Catalog, WorkspaceId::Planning,
       WorkspaceId::Editorial, WorkspaceId::Graphs, WorkspaceId::Cartography, WorkspaceId::Review})
    if (workspace_name(candidate) == saved_workspace) workspace = candidate;
  workspace_shell_.show(workspace);
  update_builder_return();
  // A troca de workspace já atualiza a superfície de destino. Atualizar todos
  // durante a abertura materializava consultas e widgets invisíveis, custo que
  // cresce justamente nos Projetos grandes.
  refresh_workspace(workspace);
  const auto document_id = ui_preference(group, "document");
  if (writing_workspace_.current_document_id().empty() && !document_id.empty() && service_.writing().document(document_id))
    writing_workspace_.reveal_document(document_id);
  focus_button_.set_sensitive(workspace == WorkspaceId::Writing);
  if (workspace == WorkspaceId::Writing) {
    focus_button_.set_active(ui_preference("appearance", "focus", "false") == "true");
    set_focus_mode(focus_button_.get_active());
  }
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
  else if (workspace == WorkspaceId::Cartography)
    cartography_workspace_.refresh();
  else if (workspace == WorkspaceId::Review)
    review_workspace_.refresh();
  if (std::getenv("INDE_PROFILE_WORKSPACES")) {
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started);
    std::clog << "INDE_PROFILE workspace=" << workspace_name(workspace)
              << " refresh_ms=" << elapsed.count() << '\n';
  }
}

void MainWindow::show_workspace(WorkspaceId workspace) {
  if(modal_host_.has_dialog())return;
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
  if (workspace != WorkspaceId::Writing) focus_button_.set_active(false);
  refresh_workspace(workspace);
  context_warning_.clear();
  restore_pending_view(workspace);
  workspace_shell_.show(workspace);
  update_builder_return();
  focus_button_.set_sensitive(workspace == WorkspaceId::Writing && service_.current());
  const bool has_filters =
      workspace == WorkspaceId::Catalog || workspace == WorkspaceId::Planning;
  sidebar_toggle_.set_sensitive(has_filters);
  inspector_toggle_.set_sensitive(workspace == WorkspaceId::Graphs);
  inspector_toggle_.set_visible(workspace == WorkspaceId::Graphs);
  if (workspace == WorkspaceId::Planning)
    status_bar_.set_text("Planejamento narrativo ativo");
  else if (workspace == WorkspaceId::Writing)
    status_bar_.set_text("Escrever");
  else if (workspace == WorkspaceId::Graphs)
    status_bar_.set_text("Cronologia ativa (somente leitura)");
  else if (workspace == WorkspaceId::Catalog)
    status_bar_.set_text(author::catalog_name());
  else if (workspace == WorkspaceId::Cartography)
    status_bar_.set_text("Lugares e mapas");
  else if (workspace == WorkspaceId::Review)
    status_bar_.set_text("Revisão editorial do Projeto");
  else
    status_bar_.set_text("Manuscrito");
  if(!context_warning_.empty())status_bar_.set_text(context_warning_);
  remember_context();
  maximize_idle_.disconnect();
  if (preserve_maximized)
    maximize_idle_ = Glib::signal_idle().connect(sigc::track_object([this] {
      if (!is_maximized())
        maximize();
      return false;
    }, *this));
}

void MainWindow::setup_actions() {
  add_action("toggle-focus", [this] {
    if (!modal_host_.has_dialog() && service_.current() && workspace_shell_.current() == WorkspaceId::Writing)
      focus_button_.set_active(!focus_button_.get_active());
  });
  add_action("theme-light", [] { set_application_theme(ThemeMode::Light); });
  add_action("theme-dark", [] { set_application_theme(ThemeMode::Dark); });
  add_action("theme-system", [] { set_application_theme(ThemeMode::System); });
  add_action("toggle-menubar", [this] { menu_bar_.set_visible(!menu_bar_.get_visible()); });
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
  add_action("show-review", [this] { show_workspace(WorkspaceId::Review); });
  add_action("saved-workspaces", [this] { manage_workspaces(); });
  add_action("project-operations", [this] { show_project_operations(); });
  add_action("show-editorial",
             [this] { show_workspace(WorkspaceId::Editorial); });
  add_action("show-graphs", [this] { show_workspace(WorkspaceId::Graphs); });
  add_action("show-cartography",
             [this] { show_workspace(WorkspaceId::Cartography); });
  add_action("proofread-current",
             [this] { proofreading_controller_.show_current_issue(); });
  add_action("manage-dictionaries",
             sigc::mem_fun(*this, &MainWindow::show_dictionaries));
  add_action("about", sigc::mem_fun(*this, &MainWindow::show_about));
  add_action("author-vocabulary", [this] { show_author_vocabulary(*this); });
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
  file->append("Histórico de alterações do projeto…", "win.project-operations");
  file->append("Fechar projeto", "win.close-project");
  file->append("Sair", "win.quit");

  const auto navigate = Gio::Menu::create();
  navigate->append("Escrever", "win.show-writing");
  navigate->append("Planejar", "win.show-planning");
  navigate->append("Revisar", "win.show-review");
  navigate->append("Manuscrito", "win.show-editorial");
  navigate->append("Cronologia", "win.show-graphs");
  navigate->append("Lugares e mapas", "win.show-cartography");
  navigate->append(author::catalog_name(), "win.show-catalog");

  const auto edit = Gio::Menu::create();
  file->append("Exportar Documento…", "win.writing-export");
  edit->append("Desfazer edição", "win.writing-undo");
  edit->append("Refazer edição", "win.writing-redo");
  edit->append("Sugerir alteração da seleção…", "win.writing-suggest");
  edit->append("Negrito", "win.writing-bold");
  edit->append("Itálico", "win.writing-italic");
  edit->append("Sublinhado", "win.writing-underline");

  const auto view = Gio::Menu::create();
  view->append("Modo Foco", "win.toggle-focus");
  view->append("Espaços de trabalho…", "win.saved-workspaces");
  view->append("Mostrar barra de menus", "win.toggle-menubar");
  view->append("Abrir filtros", "win.toggle-sidebar");
  view->append("Mostrar ou ocultar detalhes", "win.toggle-inspector");

  const auto tools = Gio::Menu::create();
  tools->append("Revisar campo atual", "win.proofread-current");
  tools->append("Dicionários…", "win.manage-dictionaries");

  const auto help = Gio::Menu::create();
  help->append("Comandos…", "win.command-palette");
  help->append("Teclado e comandos…", "win.keyboard-help");
  help->append("Vocabulário do INDE…", "win.author-vocabulary");
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
  application->append_submenu("Navegar", navigate);
  application->append_submenu("Exibir", view);
  application->append_section(tools);
  application->append_section(help);
  application_menu_.set_menu_model(application);
  const auto appearance = Gio::Menu::create();
  appearance->append("Claro", "win.theme-light");
  appearance->append("Escuro", "win.theme-dark");
  appearance->append("Seguir sistema", "win.theme-system");
  appearance_menu_.set_menu_model(appearance);
}

void MainWindow::set_focus_mode(bool focused) {
  focused_ = focused;
  workspace_shell_.set_focus_mode(focused);
  writing_workspace_.set_focus_mode(focused);
  status_bar_.set_visible(!focused);
  new_button_.set_visible(!focused);
  open_button_.set_visible(!focused);
  if (focused) menu_bar_.set_visible(false);
  set_ui_preference("appearance", "focus", focused ? "true" : "false");
}

void MainWindow::remember_context() {
  if (!service_.current() || restoring_context_) return;
  const auto group = project_preference_group(service_.current()->manifest().project_id);
  const auto put=[&](const std::string &key,const std::string &value) {
    if(ui_preference(group,key)!=value) {
      set_ui_preference(group,key,value);
      if(ui_preference(group,key)!=value)throw std::runtime_error("Não foi possível confirmar as preferências da interface");
    }
  };
  try {
    put("workspace",std::string(workspace_name(workspace_shell_.current())));
    if(!pending_views_.contains("writing"))put("document",writing_workspace_.current_document_id());
    const auto data=serialize_view(capture_context());(void)parse_view_state(data);put("context-v1",data);
  }
  catch(const std::exception &error) { status_bar_.set_text(std::string("Contexto não salvo: ")+author::diagnostic(error.what())); }
}

void MainWindow::show_dictionaries() {
  show_dictionary_manager(*this, proofreading_service_,
                          proofreading_controller_);
}

void MainWindow::show_about() {
  auto *dialog = new OverlayDialog("Sobre o INDE", *this, true);
  dialog->set_secondary_text(
      "Integrated Narrative Development Environment\nUm ambiente local para catálogo, planejamento, escrita, organização do manuscrito e visualização temporal de projetos narrativos.");
  dialog->add_button("Fechar", Gtk::ResponseType::CLOSE);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->present();
}

void MainWindow::create_project() {
  if(modal_host_.has_dialog())return;
  auto dialog = Gtk::FileChooserNative::create("Criar projeto INDE", *this,
                                               Gtk::FileChooser::Action::SAVE,
                                               "Criar", "Cancelar");
  dialog->set_current_name("Novo projeto.inde");
  dialog->signal_response().connect([this, dialog](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      writing_workspace_.flush_changes();
      remember_context();
      const auto file = dialog->get_file();
      if (!file)
        return;
      auto path = std::filesystem::path(file->get_path());
      auto name = path.stem().string();
      service_.create(path, name.empty() ? "Novo projeto" : name);
      review_workspace_.reset();
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
  if(modal_host_.has_dialog())return;
  auto dialog = Gtk::FileChooserNative::create(
      "Abrir projeto INDE", *this, Gtk::FileChooser::Action::SELECT_FOLDER,
      "Abrir", "Cancelar");
  dialog->signal_response().connect([this, dialog](int response) {
    if (response != Gtk::ResponseType::ACCEPT)
      return;
    try {
      const auto folder = dialog->get_file();
      if (folder)
        open_project_at(folder->get_path());
    } catch (const Glib::Error &error) {
      const std::runtime_error wrapped(author::diagnostic(error.what()));
      show_error("Não foi possível selecionar o projeto", wrapped);
    }
  });
  dialog->show();
}

void MainWindow::open_project_at(const std::filesystem::path &path) {
  try {
    writing_workspace_.flush_changes();
    remember_context();
    review_workspace_.reset();
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
  if(modal_host_.has_dialog())return;
  try {
    writing_workspace_.flush_changes();
    remember_context();
    service_.save();
    update_project_view();
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar o projeto", error);
  }
}

void MainWindow::save_project_as() {
  if(modal_host_.has_dialog())return;
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
      remember_context();
      review_workspace_.reset();
      service_.save_as(file->get_path());
      writing_workspace_.reset(); // The copied project has a new persistence context.
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
  if(modal_host_.has_dialog())return;
  try {
    writing_workspace_.flush_changes();
    remember_context();
    review_workspace_.reset();
    focus_button_.set_active(false);
    focus_button_.set_sensitive(false);
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
  cartography_workspace_.reset();
  pending_views_.clear();
  restored_project_id_.clear();
  builder_return_context_.reset();builder_return_.set_visible(false);
  removal_returns_.clear(); removal_return_.set_visible(false);
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
    remember_context();
    review_workspace_.reset();
  } catch (const std::exception &error) {
    show_error("Não foi possível salvar o Documento", error);
    return true;
  }
  context_save_.disconnect(); // Do not persist GTK's post-close focus teardown.
  maximize_idle_.disconnect();
  if (operations_dialog_)
    operations_dialog_->hide(); // The owner window is still a live GTK object.
  return Gtk::ApplicationWindow::on_close_request();
}

void MainWindow::show_error(const Glib::ustring &title,
                            const std::exception &error) {
  auto *dialog =
      new OverlayDialog(*this, author::diagnostic(error.what()), false, Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::CLOSE, true);
  dialog->set_title(title);
  dialog->signal_response().connect([dialog](int) { dialog->hide(); });
  dialog->show();
}

} // namespace inde::ui

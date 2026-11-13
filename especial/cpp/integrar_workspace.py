"""Integration of the native Comic workspace into the frozen INDE shell."""
def expand(name, text):
    def replace(before, after):
        nonlocal text
        if before not in text:
            raise RuntimeError('Workspace integration: base changed: ' + name + ': ' + before[:80])
        text=text.replace(before,after)
    if name=='include/inde/ui/main_window.hpp':
        replace('private:\n', 'private:\n  friend struct ComicWorkspaceProbe;\n')
        replace('#include "publication_service.hpp"','#include "publication_service.hpp"\n#include "comic_ui.hpp"')
        replace('  void show_publication();','  void show_publication();\n  bool guard_comic_project_change();')
        replace('  publication::Service publication_service_{service_};','  publication::Service publication_service_{service_};\n  ComicWorkspace comic_workspace_{publication_service_};')
    elif name=='src/ui/main_window.cpp':
        replace('bool MainWindow::on_close_request() {','bool MainWindow::on_close_request() {\n  if(!guard_comic_project_change())return true;')
        for method in ['create_project','open_project','save_project_as','close_project']:
            replace('void MainWindow::'+method+'() {','void MainWindow::'+method+'() {\n  if(!guard_comic_project_change())return;')
        replace('void MainWindow::open_project_at(const std::filesystem::path &path) {','void MainWindow::open_project_at(const std::filesystem::path &path) {\n  if(!guard_comic_project_change())return;')
        replace('void MainWindow::save_project() {\n  if(modal_host_.has_dialog())return;\n  try {','void MainWindow::save_project() {\n  if(modal_host_.has_dialog())return;\n  try {\n    comic_workspace_.save_changes();')
        replace('  workspace_shell_.add_workspace(writing_workspace_, WorkspaceId::Writing);','  workspace_shell_.add_workspace(writing_workspace_, WorkspaceId::Writing);\n  workspace_shell_.add_workspace(comic_workspace_, WorkspaceId::Comic);')
        replace('    graphs_workspace_.toggle_inspector();', '    graphs_workspace_.toggle_inspector();\n  else if (workspace_shell_.current() == WorkspaceId::Comic)comic_workspace_.toggle_inspector();')
        text=text.replace('inspector_toggle_.set_visible(workspace == WorkspaceId::Graphs);', 'inspector_toggle_.set_visible(workspace == WorkspaceId::Graphs || workspace == WorkspaceId::Comic);').replace('inspector_toggle_.set_sensitive(workspace == WorkspaceId::Graphs);', 'inspector_toggle_.set_sensitive(workspace == WorkspaceId::Graphs || workspace == WorkspaceId::Comic);')
        replace('  if (workspace == WorkspaceId::Planning)\n    planning_workspace_.refresh();','  if (workspace == WorkspaceId::Comic)\n    comic_workspace_.activate();\n  else if (workspace == WorkspaceId::Planning)\n    planning_workspace_.refresh();')
        replace('  const bool preserve_maximized = is_maximized();','  if(workspace_shell_.current()==WorkspaceId::Comic && workspace!=WorkspaceId::Comic && !comic_workspace_.can_switch()){workspace_shell_.show(WorkspaceId::Comic);return;}\n  const bool preserve_maximized = is_maximized();')
        replace('  if (workspace == WorkspaceId::Planning)\n    status_bar_.set_text','  if (workspace == WorkspaceId::Comic)\n    status_bar_.set_text("Comic");\n  else if (workspace == WorkspaceId::Planning)\n    status_bar_.set_text')
        replace('  add_action("show-writing", [this] { show_workspace(WorkspaceId::Writing); });','  add_action("show-writing", [this] { show_workspace(WorkspaceId::Writing); });\n  add_action("show-comic", [this] { show_workspace(WorkspaceId::Comic); });\n  add_action("comic-undo", [this] { if(workspace_shell_.current()==WorkspaceId::Comic)comic_workspace_.undo(); });\n  add_action("comic-redo", [this] { if(workspace_shell_.current()==WorkspaceId::Comic)comic_workspace_.redo(); });')
        replace('WorkspaceId::Cartography, WorkspaceId::Review})','WorkspaceId::Cartography, WorkspaceId::Review, WorkspaceId::Comic})')
        replace('    service_.open(path);', '    service_.open(path);\n    comic_workspace_.reset();')
        replace('      service_.create(path, name.empty() ? \"Novo projeto\" : name);', '      service_.create(path, name.empty() ? \"Novo projeto\" : name);\n      comic_workspace_.reset();')
        replace('      comic::Service(publication_service_).adopt_project_copy(previous_publication_owner);', '      comic::Service(publication_service_).adopt_project_copy(previous_publication_owner);\n      comic_workspace_.reset();')
        replace('    service_.close();','    comic_workspace_.reset();\n    service_.close();')
        # Program-wide menu uses the same names and explicit media choice.
        replace('  navigate->append("Escrever", "win.show-writing");','  const auto create=Gio::Menu::create();create->append("Escrita","win.show-writing");create->append("Comic","win.show-comic");navigate->append_submenu("Criar",create);')
        text=text.replace('navigate->append("Lugares e mapas"','navigate->append("Mapas"').replace('navigate->append(author::catalog_name()','navigate->append("Obras"').replace('append_submenu("Navegar", navigate)','append_submenu("Espaços", navigate)')
    elif name=='src/ui/main_window_context.cpp':
        replace('{"writing",writing},{"planning",planning},{"editorial",editorial},{"review",review}', '{"writing",writing},{"planning",planning},{"editorial",editorial},{"review",review},{"comic",pending_views_.contains("comic")?pending_views_.at("comic"):comic_workspace_.capture_view()}')
        replace('    else if(target==WorkspaceId::Review)review_workspace_.restore_view(value);','    else if(target==WorkspaceId::Review)review_workspace_.restore_view(value);\n    else if(target==WorkspaceId::Comic)comic_workspace_.restore_view(value);')
        replace('WorkspaceId::Graphs,WorkspaceId::Cartography})','WorkspaceId::Graphs,WorkspaceId::Cartography,WorkspaceId::Comic})')
        replace('  restoring_context_=true;', '  const auto comic=v.as_object().find("comic");\n  if(comic!=v.as_object().end())pending_views_.emplace("comic",comic->second);\n  restoring_context_=true;')
        text=text.replace('contexto de Escrita, Planejar, Manuscrito e Revisar','contexto de Escrita, Comic, Planejar, Manuscrito e Revisar')
    elif name=='src/ui/main_window_keyboard.cpp':
        replace('modal_host_.has_dialog() || writing_workspace_.composing_input()', 'modal_host_.has_dialog() || writing_workspace_.composing_input() || comic_workspace_.composing_input()')
        replace('  action("show-writing", "Escrever",','  action("show-comic", "Comic", "Navegação", "<Control>8", project);\n  action("show-writing", "Escrita",')
        text=text.replace('"Lugares e mapas", "Navegação"','"Mapas", "Navegação"').replace('"Obras e universos", "Navegação"','"Obras", "Navegação"')
        replace('    Gtk::Widget *workspace = nullptr;', '    if(workspace_shell_.current()==WorkspaceId::Comic){std::vector<Gtk::Widget*> regions{&header_,&workspace_shell_.navigation_region()};const auto comic=comic_workspace_.keyboard_regions();regions.insert(regions.end(),comic.begin(),comic.end());return regions;}\n    Gtk::Widget *workspace = nullptr;')
        replace('  writing_workspace_.install_keyboard(*keyboard_);', '  comic_workspace_.install_keyboard(*keyboard_);\n  writing_workspace_.install_keyboard(*keyboard_);')
        replace('      case WorkspaceId::Cartography: workspace = &cartography_workspace_; break;', '      case WorkspaceId::Cartography: workspace = &cartography_workspace_; break;\n      case WorkspaceId::Comic: workspace = &comic_workspace_; break;')
    return text

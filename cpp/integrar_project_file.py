"""Native file lifecycle, applied last to the reviewed GTK overlay."""
def expand(name, text):
    def replace(old,new,count=1):
        nonlocal text
        if text.count(old)!=count:raise RuntimeError(f'P5 integration changed: {name}: {old[:100]!r}')
        text=text.replace(old,new)
    if name=='include/inde/ui/main_window.hpp':
        replace('#include "publication_service.hpp"','#include "publication_service.hpp"\nnamespace inde::project_file { class FileSession; enum class RecoveryChoice; }')
        replace('  void open_project();','''  void open_project();
  void choose_project_file(bool create);
  void open_project_file_at(const std::filesystem::path &, project_file::RecoveryChoice);
  void flush_project_file();
  void create_legacy_project(const std::filesystem::path &, const std::string &);
  void open_legacy_project(const std::filesystem::path &);
  bool finish_project_file_close();
  void copy_project_file();
  void show_file_history();''')
        replace('  application::ProjectService service_;','''  application::ProjectService service_;
  std::unique_ptr<project_file::FileSession> project_file_;
  bool preserve_file_work_{};''')
    elif name=='src/ui/main_window.cpp':
        replace('#include "inde/ui/main_window.hpp"', '#include "inde/ui/main_window.hpp"\n#include "project_file_session.hpp"')
        replace('void MainWindow::setup_actions() {','''void MainWindow::setup_actions() {
  add_action("new-project-file", [this]{choose_project_file(true);});
  add_action("open-project-file", [this]{choose_project_file(false);});
  add_action("copy-project-file", sigc::mem_fun(*this,&MainWindow::copy_project_file));
  add_action("project-file-history", sigc::mem_fun(*this,&MainWindow::show_file_history));''')
        replace('  file->append("Novo projeto", "win.new-project");','''  file->append("Novo projeto", "win.new-project");
  file->append("Novo arquivo único .inde…", "win.new-project-file");
  file->append("Abrir arquivo único .inde…", "win.open-project-file");
  file->append("Salvar cópia em arquivo único…", "win.copy-project-file");
  file->append("Estados do arquivo único…", "win.project-file-history");''')
        replace('    service_.open(path);','    open_legacy_project(path);')
        replace('      service_.create(path, name.empty() ? "Novo projeto" : name);','      create_legacy_project(path, name.empty() ? "Novo projeto" : name);')
        replace('void MainWindow::open_project_at(const std::filesystem::path &path) {','''void MainWindow::open_project_at(const std::filesystem::path &path) {
  std::error_code file_error;
  if(std::filesystem::is_regular_file(path,file_error)) {
    open_project_file_at(path,project_file::RecoveryChoice::Ask);return;
  }''')
        replace('    service_.save();','    service_.save();\n    if(project_file_){project_file_->save("Salvar no editor");preserve_file_work_=false;}')
        replace('void MainWindow::save_project_as() {','''void MainWindow::save_project_as() {
  if(project_file_){copy_project_file();return;}''')
        replace('    service_.close();','''    flush_project_file();
    if(project_file_)project_file_->finish();
    service_.close();
    project_file_.reset();service_.set_recent_location(std::nullopt);''')
        replace('  context_save_.disconnect(); // Do not persist GTK\'s post-close focus teardown.','''  if(!finish_project_file_close())return true;
  context_save_.disconnect(); // Do not persist GTK's post-close focus teardown.''')
        replace('  header_subtitle_.set_visible(false);', '  header_subtitle_.set_name("native-project-file");\n  header_subtitle_.set_visible(bool(project_file_));\n  if(project_file_){header_subtitle_.set_text(project_file_->file().filename().string());header_subtitle_.set_tooltip_text(project_file_->file().string());}')
        replace('    if(project_file_){project_file_->save("Salvar no editor");preserve_file_work_=false;}\n    update_project_view();', '    if(project_file_){project_file_->save("Salvar no editor");preserve_file_work_=false;}\n    update_project_view();\n    if(project_file_)status_bar_.set_text("Arquivo único confirmado — estado "+std::to_string(project_file_->baseline().sequence));')
        # Workspace autosave is durable recovery, not a .inde confirmation.
        occurrences=text.count('status_bar_.set_text(message);')
        replace('status_bar_.set_text(message);','status_bar_.set_text(project_file_ ? Glib::ustring("Na área de trabalho: ")+message : message);',occurrences)
        replace('  status_bar_.set_text("Todas as alterações foram salvas");','''  status_bar_.set_text(project_file_ ? "Arquivo único aberto — use Salvar para confirmar o trabalho" : "Todas as alterações foram salvas");''')
    elif name=='src/ui/main_window_keyboard.cpp':
        replace('  action("open-project", "Abrir projeto…", "Projeto", "<Control>o");', '\n'.join([
            '  action("open-project", "Abrir projeto…", "Projeto", "<Control>o");',
            '  action("open-project-file", "Abrir arquivo único .inde…", "Projeto", "<Control><Alt>o");',
            '  action("new-project-file", "Novo arquivo único .inde…", "Projeto", "<Control><Alt>n");',
            '  action("project-file-history", "Estados do arquivo único…", "Projeto", "<Control><Alt>h", [this]{return project_file_ ? std::string{} : std::string("Abra um arquivo único primeiro.");});',
            '  action("copy-project-file", "Salvar cópia em arquivo único…", "Projeto", "<Control><Alt>s", project);']))
    elif name=='src/main.cpp':
        replace('    std::optional<std::filesystem::path> initial_project;', '    if(argc==4 && std::string(argv[1])=="--arquivo-usabilidade")return run_object_usage_probe(application,argv[2],argv[3],true);\n    std::optional<std::filesystem::path> initial_project;')
    return text

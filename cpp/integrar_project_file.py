"""Native file lifecycle, applied last to the reviewed GTK overlay."""
def expand(name, text):
    def replace(old,new,count=1):
        nonlocal text
        if text.count(old)!=count:raise RuntimeError(f'P5 integration changed: {name}: {old[:100]!r}')
        text=text.replace(old,new)
    if name=='include/inde/ui/main_window.hpp':
        replace('  friend struct ComicWorkspaceProbe;', '  friend struct ComicWorkspaceProbe;\n  friend struct ProjectFileTaskProbe;')
        replace('#include "publication_service.hpp"','#include "publication_service.hpp"\nnamespace inde::project_file { class FileSession; class ConfirmationTask; class OperationControl; class Preparation; enum class RecoveryChoice; struct ConfirmationResult; struct Revision; }')
        replace('  void open_project();','''  void open_project();
  void choose_project_file(bool create);
  void open_project_file_at(const std::filesystem::path &, project_file::RecoveryChoice);
  void flush_project_file();
  void create_project_file_at(const std::filesystem::path &);
  void copy_project_file_to(const std::filesystem::path &, bool);
  void adopt_project_file(std::unique_ptr<project_file::FileSession>);
  void rebind_project_file();
  void project_file_open_failed(const std::filesystem::path &, std::exception_ptr);
  void present_file_history(std::vector<project_file::Revision>);
  void start_file_operation(const std::string &,
      std::function<std::int64_t(project_file::OperationControl &)>,
      std::function<void(const project_file::ConfirmationResult &)>, bool = false, bool = false);
  void create_legacy_project(const std::filesystem::path &, const std::string &);
  void open_legacy_project(const std::filesystem::path &);
  bool finish_project_file_close();
  void copy_project_file();
  void show_file_history();
  void setup_file_confirmation();
  void start_file_confirmation(std::function<void()> = {}, bool = false);
  void present_file_confirmation();
  void launch_file_task(const std::string &, std::function<std::int64_t(project_file::OperationControl &)>, bool, bool, bool);
  void advance_file_preparation();
  void cancel_file_confirmation();
  void queue_file_confirmation_exit();
  bool poll_file_confirmation();
  void stop_file_confirmation();
  void file_confirmation_failed(std::exception_ptr, bool);''')
        replace('  application::ProjectService service_;','''  application::ProjectService service_;
  std::unique_ptr<project_file::FileSession> project_file_;
  bool preserve_file_work_{};
  std::shared_ptr<project_file::ConfirmationTask> file_task_;
  std::function<void(const std::string &, project_file::OperationControl &)> file_task_observe_;
  std::function<void()> file_task_next_;
  std::function<void(const project_file::ConfirmationResult &)> file_task_done_;
  std::shared_ptr<project_file::Preparation> file_preparation_;
  std::function<std::int64_t(project_file::OperationControl &)> file_prepared_work_;
  bool file_native_started_{}, file_stamp_native_{}, file_finish_announced_{};
  std::string file_task_title_{"Salvando projeto"};
  sigc::connection file_task_poll_, file_task_retry_;
  std::uint64_t file_task_token_{};
  bool file_task_exit_{}, file_task_cancelled_{}, file_barrier_ready_{}, file_exit_ready_{};
  Gtk::Box file_task_panel_{Gtk::Orientation::VERTICAL};
  Gtk::Spinner file_task_spinner_;
  Gtk::Label file_task_status_;
  Gtk::Button file_task_cancel_{"Cancelar salvamento"};
  Gtk::Button file_task_exit_button_{"Sair após salvar"};''')
    elif name=='include/inde/ui/writing_workspace.hpp':
        replace('  void flush_changes();','''  void flush_changes();
  bool poll_project_flush();
  std::function<void()> prepare_project_objects();
  void finish_project_flush();''')
        replace('  application::DocumentSaveCoordinator save_worker_;','  application::DocumentSaveCoordinator save_worker_;\n  unsigned file_flush_stage_{};')
    elif name=='src/ui/main_window.cpp':
        replace('if(service_.current() && !modal_host_.has_dialog())remember_context();','if(service_.current() && !file_task_ && !modal_host_.has_dialog())remember_context();')
        replace('  setup_keyboard();','  setup_keyboard();\n  setup_file_confirmation();')
        replace('header_.set_sensitive(!active);','header_.set_sensitive(!active && !file_task_);')
        replace('MainWindow::~MainWindow() {','MainWindow::~MainWindow() { stop_file_confirmation();')
        replace('void MainWindow::save_project() {','void MainWindow::save_project() {\n  if(project_file_){start_file_confirmation();return;}')
        replace('  if(modal_host_.has_dialog())return;\n  try {\n    writing_workspace_.flush_changes();\n    remember_context();\n    review_workspace_.reset();\n    focus_button_', '  if(modal_host_.has_dialog())return;\n  if(project_file_ && !file_barrier_ready_){start_file_confirmation([this]{close_project();});return;}\n  try {\n    if(!file_barrier_ready_)writing_workspace_.flush_changes();\n    remember_context();\n    review_workspace_.reset();\n    focus_button_')
        replace('  try {\n    writing_workspace_.flush_changes();\n    remember_context();\n    review_workspace_.reset();\n  } catch (const std::exception &error)', '  if(project_file_ && !preserve_file_work_){start_file_confirmation({},true);return true;}\n  try {\n    writing_workspace_.flush_changes();\n    remember_context();\n    review_workspace_.reset();\n  } catch (const std::exception &error)')
        replace('bool MainWindow::on_close_request() {', 'bool MainWindow::on_close_request() {\n  if(file_exit_ready_){file_exit_ready_=false;context_save_.disconnect();maximize_idle_.disconnect();return Gtk::ApplicationWindow::on_close_request();}\n  if(file_task_){queue_file_confirmation_exit();return true;}\n  if(preserve_file_work_ && project_file_){context_save_.disconnect();maximize_idle_.disconnect();return Gtk::ApplicationWindow::on_close_request();}')
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
        replace('''    writing_workspace_.flush_changes();
    remember_context();
    review_workspace_.reset();
    open_legacy_project(path);''','''    if(!file_barrier_ready_)writing_workspace_.flush_changes();
    remember_context();
    review_workspace_.reset();
    open_legacy_project(path);''')
        replace('''      writing_workspace_.flush_changes();
      remember_context();
      const auto file = dialog->get_file();''','''      if(!project_file_)writing_workspace_.flush_changes();
      remember_context();
      const auto file = dialog->get_file();''')
        replace('''      service_.create(path, name.empty() ? "Novo projeto" : name);
      comic_workspace_.reset();
      review_workspace_.reset();
      planning_workspace_.reset();
      writing_workspace_.reset();
      update_project_view();''','      create_legacy_project(path, name.empty() ? "Novo projeto" : name);')
        replace('void MainWindow::open_project_at(const std::filesystem::path &path) {','''void MainWindow::open_project_at(const std::filesystem::path &path) {
  std::error_code file_error;
  if(std::filesystem::is_regular_file(path,file_error)) {
    open_project_file_at(path,project_file::RecoveryChoice::Ask);return;
  }
  if(project_file_ && !file_barrier_ready_){start_file_confirmation([this,path]{open_project_at(path);});return;}''')
        replace('    service_.save();','    service_.save();\n    if(project_file_){project_file_->save("Salvar no editor");preserve_file_work_=false;}')
        replace('void MainWindow::save_project_as() {','''void MainWindow::save_project_as() {
  if(project_file_){copy_project_file();return;}''')
        replace('    service_.close();','''    flush_project_file();
    if(project_file_ && !file_barrier_ready_)project_file_->finish();
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
        replace('return modal_host_.has_dialog() || writing_workspace_.composing_input()', 'return bool(file_task_) || modal_host_.has_dialog() || writing_workspace_.composing_input()')
        replace('  action("open-project", "Abrir projeto…", "Projeto", "<Control>o");', '\n'.join([
            '  action("open-project", "Abrir projeto…", "Projeto", "<Control>o");',
            '  action("open-project-file", "Abrir arquivo único .inde…", "Projeto", "<Control><Alt>o");',
            '  action("new-project-file", "Novo arquivo único .inde…", "Projeto", "<Control><Alt>n");',
            '  action("project-file-history", "Estados do arquivo único…", "Projeto", "<Control><Alt>h", [this]{return project_file_ ? std::string{} : std::string("Abra um arquivo único primeiro.");});',
            '  action("copy-project-file", "Salvar cópia em arquivo único…", "Projeto", "<Control><Alt>s", project);']))
    elif name=='src/main.cpp':
        replace('    std::optional<std::filesystem::path> initial_project;', '    if(argc==4 && std::string(argv[1])=="--arquivo-usabilidade")return run_object_usage_probe(application,argv[2],argv[3],true);\n    std::optional<std::filesystem::path> initial_project;')
    return text

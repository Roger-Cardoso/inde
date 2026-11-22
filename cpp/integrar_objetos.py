"""Deterministic native entry point; frozen archives remain untouched."""
def expand(name,text):
    def replace(before,after):
        nonlocal text
        if before not in text: raise RuntimeError('Object integration: base changed: '+name+': '+before[:70])
        text=text.replace(before,after)
    if name=='include/inde/ui/main_window.hpp':
        replace('class PublicationPage;', 'class ObjectDocumentWindow;\nclass PublicationPage;')
        replace('  void show_publication();','  void show_publication();\n  void show_objects();\n  std::unique_ptr<ObjectDocumentWindow> object_window_;\n  Gtk::Button object_button_{"Objetos"};')
    elif name=='src/ui/main_window.cpp':
        replace('#include "publication_ui.hpp"','#include "publication_ui.hpp"\n#include "object_native_ui.hpp"')
        replace('  header_.pack_start(publication_button_);','  header_.pack_start(publication_button_);\n  header_.pack_start(object_button_);\n  object_button_.set_action_name("win.document-objects");\n  object_button_.set_tooltip_text("Compor o documento selecionado com figuras, tabelas, fórmulas e matemática");')
        replace('bool MainWindow::on_close_request() {', 'bool MainWindow::on_close_request() {\n  if(object_window_ && object_window_->get_visible()){object_window_->close();return true;}')
        replace('publication_button_.set_sensitive(false);','publication_button_.set_sensitive(false);object_button_.set_sensitive(false);')
        replace('publication_button_.set_sensitive(true);','publication_button_.set_sensitive(true);object_button_.set_sensitive(true);')
        replace('void MainWindow::setup_actions() {','void MainWindow::setup_actions() {\n  add_action("document-objects", sigc::mem_fun(*this, &MainWindow::show_objects));')
        replace('    comic_workspace_.save_changes();','    comic_workspace_.save_changes();\n    if(object_window_ && service_.current())object_window_->save_for_project(service_.current()->path());')
        replace('    comic_workspace_.reset();','    comic_workspace_.reset();object_window_.reset();')
        # Kept in the owning translation unit so unique_ptr sees the complete type.
        replace('MainWindow::~MainWindow() {','''void MainWindow::show_objects(){
  if(!service_.current()||modal_host_.has_dialog())return;
  if(!writing_workspace_.publication_ready()){
    status_bar_.set_text("Aguarde o salvamento do texto na Escrita antes de abrir Objetos.");return;
  }
  try{
    object_window_.reset();
    object_window_=std::make_unique<ObjectDocumentWindow>(service_,*this,writing_workspace_.current_document_id());
    object_window_->present();
  }catch(const std::exception &e){show_error("Compor documento",e);}
}
MainWindow::~MainWindow() {''')
    elif name=='src/main.cpp':
        replace('    // Routing belongs', '    if(argc==4 && std::string(argv[1])=="--objetos-smoke")return run_object_window_smoke(application,argv[2],argv[3]);\n    // Routing belongs')
    return text

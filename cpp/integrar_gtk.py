"""Overlay determinístico: preserva os 641 arquivos da base congelada."""
from pathlib import Path
import sys
from integrar_workspace import expand
base, out = map(Path, sys.argv[1:])
def emit(name, replacements):
    text = (base / name).read_text()
    for before, after in replacements:
        if before not in text:
            raise RuntimeError('A base mudou: ' + name + ': ' + before[:60])
        text = text.replace(before, after)
    text = expand(name, text)
    path = out / name
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)
emit('include/inde/ui/main_window.hpp', [
    ('class MainWindow final', 'class PublicationPage;\nclass MainWindow final'),
    ('#include <gtkmm.h>', '#include <gtkmm.h>\n#include "publication_service.hpp"'),
    ('  void build_ui();', '  void build_ui();\n  void show_publication();\n  void present_publication();'),
    ('  application::ProjectService service_;', '  application::ProjectService service_;\n  publication::Service publication_service_{service_};\n  std::unique_ptr<PublicationPage> publication_page_;\n  std::uint64_t publication_token_{};\n  sigc::connection publication_retry_;\n  Gtk::Button publication_button_{"Publicação"};'),
])
emit('include/inde/ui/writing_workspace.hpp', [
    ('  void flush_changes();', '  void flush_changes();\n  bool publication_ready() const { return !composing_input() && !edit_session_.dirty() && !save_worker_.busy() && save_error_.empty(); }'),
])
emit('src/ui/main_window.cpp', [
    ('MainWindow::~MainWindow() {', 'MainWindow::~MainWindow() { publication_retry_.disconnect(); if(publication_token_)modal_host_.dismiss_dialog(publication_token_); publication_page_.reset();'),
    ('bool MainWindow::on_close_request() {', 'bool MainWindow::on_close_request() {\n  if(publication_token_ || publication_retry_.connected()){\n    if(publication_page_ && !publication_page_->can_leave())return true;\n    publication_retry_.disconnect();modal_host_.dismiss_dialog(publication_token_);publication_token_=0;\n  }'),
    ('#include \"inde/ui/main_window.hpp\"', '#include \"inde/ui/main_window.hpp\"\n#include \"publication_ui.hpp\"\n#include \"comic_service.hpp\"'),
    ('  header_.pack_start(open_button_);', '  header_.pack_start(open_button_);\n  header_.pack_start(publication_button_);\n  publication_button_.set_action_name("win.web-publication");'),
    ('  close_button_.set_sensitive(false);', '  close_button_.set_sensitive(false);\n  publication_button_.set_sensitive(false);'),
    ('  close_button_.set_sensitive(true);', '  close_button_.set_sensitive(true);\n  publication_button_.set_sensitive(true);'),
    ('void MainWindow::setup_actions() {', 'void MainWindow::setup_actions() {\n  add_action("web-publication", sigc::mem_fun(*this, &MainWindow::show_publication));'),
    ('      service_.save_as(file->get_path());', '      static_cast<void>(publication_service_.sites(true));\n      static_cast<void>(comic::Service(publication_service_).books(true));\n      const auto previous_publication_owner = service_.current()->manifest().project_id;\n      service_.save_as(file->get_path());\n      publication_service_.adopt_project_copy(previous_publication_owner);\n      comic::Service(publication_service_).adopt_project_copy(previous_publication_owner);'),
])
emit('src/main.cpp', [
    ('"io.github.inde"', '"io.github.inde.publicacao.laboratorio"'),
    ('#include <gtkmm/application.h>', '#include <gtkmm/application.h>\n#include "main_window_smoke.hpp"'),
    ('    Gtk::Window::set_default_icon_name("inde");', '    Gtk::Window::set_default_icon_name("inde");\n    if(argc==3 && std::string(argv[1])=="--integracao-revisao-smoke")return run_review_integration_smoke(application,argv[2]);\n    if(argc==3 && std::string(argv[1])=="--comic")return run_comic_workspace(application,argv[2]);\n    if(argc==3 && (std::string(argv[1])=="--importacao" || std::string(argv[1])=="--importacao-smoke"))return run_import_review(application,argv[2],std::string(argv[1])=="--importacao-smoke");\n    if(argc==4 && std::string(argv[1])=="--comic01-smoke")return run_comic_window_smoke(application,argv[2],argv[3]);\n    if(argc==4 && std::string(argv[1])=="--pub03b-smoke")return run_imported_window_smoke(application,argv[2],argv[3]);\n    if(argc==4 && std::string(argv[1])=="--pub02b-smoke")return run_catalog_window_smoke(application,argv[2],argv[3]);\n    if(argc==4 && std::string(argv[1])=="--pub02-smoke")return run_main_window_smoke(application,argv[2],argv[3]);'),
])

emit('src/ui/overlay_dialog.cpp', [
    ('  modal_layer_.set_visible(true);', '  const bool publication = g_object_get_data(G_OBJECT(gobj()), "inde-publication-layout");\n  modal_card_.set_size_request(std::max(240, std::min(publication ? 960 : 680, get_width()-56)), -1);\n  modal_scroll_.set_max_content_height(std::min(publication ? 760 : 620, std::max(90, get_height()-180)));\n  modal_layer_.set_visible(true);'),
    ('std::min(680, width - 56)', 'std::min(g_object_get_data(G_OBJECT(gobj()), "inde-publication-layout") ? 960 : 680, width - 56)'),
    ('std::min(620, budget)', 'std::min(g_object_get_data(G_OBJECT(gobj()), "inde-publication-layout") ? 760 : 620, budget)'),
])

emit("src/ui/main_window_context.cpp", [])
emit("src/ui/main_window_keyboard.cpp", [])

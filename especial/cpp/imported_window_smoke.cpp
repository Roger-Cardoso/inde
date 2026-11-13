#include "main_window_smoke.hpp"
#include "inde/ui/main_window.hpp"
#include "ui_test_driver.hpp"
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
template<class T>T *find_imported_widget(Gtk::Widget &root){if(auto *match=dynamic_cast<T*>(&root))return match;for(auto *child=root.get_first_child();child;child=child->get_next_sibling())if(auto *match=find_imported_widget<T>(*child))return match;return nullptr;}
Gtk::Button *find_button(Gtk::Widget &,const std::string &);void capture_window(Gtk::Widget &,const fs::path &);std::string file_hash(const fs::path &);
static inde::publication::V read(const fs::path &p){std::ifstream f(p);return inde::persistence::json::parse(std::string{std::istreambuf_iterator<char>(f),{}});}
int run_imported_window_smoke(const Glib::RefPtr<Gtk::Application> &app,const fs::path &project,const fs::path &picture){
 std::unique_ptr<inde::ui::MainWindow> window;inde::ui::WritingWorkspace *writing{};Gtk::TextView *editor{};Glib::RefPtr<Gtk::TextBuffer> buffer;std::string id,text,view,before;int stage=0,ticks=0,result=1,frames=0;
 auto action=[&](const char *name){auto a=window->lookup_action(name);if(!a)throw std::runtime_error(name);a->activate();};
 app->signal_activate().connect([&]{window=std::make_unique<inde::ui::MainWindow>(project);app->add_window(*window);window->present();Glib::signal_timeout().connect([&]{try{
  if(++ticks>180)throw std::runtime_error("Timeout do Projeto importado");
  if(stage==0){action("show-writing");writing=find_imported_widget<inde::ui::WritingWorkspace>(*window);if(!writing)throw std::runtime_error("Escrita ausente");const auto receipt=read(project/"data/importacao-literature.json");id=receipt.at("chapters").as_array().back().at("document").as_string();writing->reveal_document(id);editor=find_imported_widget<Gtk::TextView>(*writing);if(!editor)throw std::runtime_error("Editor ausente");buffer=editor->get_buffer();text=buffer->get_text();buffer->select_range(buffer->get_iter_at_offset(2),buffer->get_iter_at_offset(8));view=inde::ui::serialize_view(writing->capture_view());before=file_hash(project/"data/project.sqlite3");action("web-publication");auto *page=find_imported_widget<inde::ui::PublicationPage>(*window);if(!page)throw std::runtime_error("Painel ausente");inde::ui::PublicationPageProbe::catalog_generate(*page);stage=1;return true;}
  if(stage==1){auto *page=find_imported_widget<inde::ui::PublicationPage>(*window);if(!page||!inde::ui::PublicationPageProbe::done(*page))return true;if(++frames<3)return true;const auto output=inde::ui::PublicationPageProbe::catalog_output(*page);const auto info=read(output/"obra-1/dados/info-obra.json");if(info.at("id_obra").as_string()!="obra-literature-origem"||!fs::is_regular_file(output/"mares-literature/capitulo-7/index.html")||!fs::is_regular_file(output/"mares-antigas/capitulo-11/index.html"))throw std::runtime_error("A geração GTK perdeu identidades ou rotas");capture_window(*window,picture);std::cout<<"Saída importada: "<<output<<'\n';auto *back=find_button(*window,"Voltar ao INDE");g_signal_emit_by_name(back->gobj(),"clicked");stage=2;frames=0;return true;}
  if(++frames<3)return true;
  if(writing->current_document_id()!=id||editor->get_buffer()!=buffer||buffer->get_text().raw()!=text||inde::ui::serialize_view(writing->capture_view())!=view||file_hash(project/"data/project.sqlite3")!=before){throw std::runtime_error("Publicar o Projeto importado alterou a sessão ou o banco autoral");}capture_window(*window,picture.parent_path()/"editor-importado-pub03b.png");result=0;std::cout<<"PUB-03B GTK: Obra importada aberta, Site gerado, IDs/rotas preservados e retorno à mesma sessão OK\n";window->close();app->quit();return false;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';window->hide();app->quit();return false;}},250);});app->run();window.reset();return result;
}

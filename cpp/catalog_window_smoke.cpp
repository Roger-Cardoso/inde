#include "main_window_smoke.hpp"
#include "inde/ui/main_window.hpp"
#include "ui_test_driver.hpp"
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
template<class T>T *find_widget(Gtk::Widget &root){if(auto *match=dynamic_cast<T*>(&root))return match;for(auto *child=root.get_first_child();child;child=child->get_next_sibling())if(auto *match=find_widget<T>(*child))return match;return nullptr;}
Gtk::Button *find_button(Gtk::Widget &,const std::string &);
void capture_window(Gtk::Widget &,const fs::path &);
std::string file_hash(const fs::path &);
static inde::publication::V read(const fs::path &path){std::ifstream f(path);return inde::persistence::json::parse(std::string{std::istreambuf_iterator<char>(f),{}});}
int run_catalog_window_smoke(const Glib::RefPtr<Gtk::Application> &app,const fs::path &project,const fs::path &picture){
 std::unique_ptr<inde::ui::MainWindow> window;inde::ui::WritingWorkspace *writing{};Gtk::TextView *editor{};Glib::RefPtr<Gtk::TextBuffer> buffer;std::string view,body,doc,db_hash;std::string preview_record;int stage=0,ticks=0,result=1,preview_ticks=0;
 auto action=[&](const char *name){auto a=window->lookup_action(name);if(!a)throw std::runtime_error(name);a->activate();};
 app->signal_activate().connect([&]{window=std::make_unique<inde::ui::MainWindow>(project);app->add_window(*window);window->present();Glib::signal_timeout().connect([&]{try{
  if(++ticks>180)throw std::runtime_error("Timeout do catálogo GTK");
  if(stage==0){action("show-writing");writing=find_widget<inde::ui::WritingWorkspace>(*window);if(!writing)throw std::runtime_error("Editor ausente");
   const auto snapshot=read(project.parent_path()/"retrato.json");doc=snapshot.at("units").as_array().back().at("documents").as_array().front().at("id").as_string();writing->reveal_document(doc);editor=find_widget<Gtk::TextView>(*writing);if(!editor)throw std::runtime_error("Buffer ausente");buffer=editor->get_buffer();body=buffer->get_text();buffer->select_range(buffer->get_iter_at_offset(2),buffer->get_iter_at_offset(7));view=inde::ui::serialize_view(writing->capture_view());db_hash=file_hash(project/"data/project.sqlite3");
   action("web-publication");auto *page=find_widget<inde::ui::PublicationPage>(*window);if(!page)throw std::runtime_error("Publicação ausente");inde::ui::PublicationPageProbe::catalog_draft(*page);
   window->close();if(!window->get_visible())throw std::runtime_error("Fechou catálogo pendente");auto *button=find_button(*window,"Voltar ao INDE");if(!button)throw std::runtime_error("Retorno ausente");g_signal_emit_by_name(button->gobj(),"clicked");stage=1;return true;
  }
  if(stage==1){auto *page=find_widget<inde::ui::PublicationPage>(*window);if(!page)return true;inde::ui::PublicationPageProbe::catalog_commit(*page);inde::ui::PublicationPageProbe::catalog_generate(*page);window->close();if(!window->get_visible())throw std::runtime_error("Fechou durante a geração do Site");stage=2;return true;}
  if(stage==2){auto *page=find_widget<inde::ui::PublicationPage>(*window);if(!page||!inde::ui::PublicationPageProbe::done(*page))return true;const auto output=inde::ui::PublicationPageProbe::catalog_output(*page);const auto catalog=read(output/"catalogo.json").as_array();if(catalog.size()!=2||catalog.front().at("endereco_publico").as_string()!="observatorio-das-estrelas")throw std::runtime_error("O Site não respeitou a ordem do catálogo");
   capture_window(*window,picture);std::cout<<"Saída do Site: "<<output.string()<<'\n';if(std::getenv("INDE_TESTAR_PREVIA")){inde::ui::PublicationPageProbe::preview(*page);stage=20;return true;}auto *button=find_button(*window,"Voltar ao INDE");g_signal_emit_by_name(button->gobj(),"clicked");stage=3;return true;}
  if(stage==20){auto *page=find_widget<inde::ui::PublicationPage>(*window);const auto control=inde::ui::PublicationPageProbe::preview_control(*page);if(!fs::is_regular_file(control))return true;preview_record=inde::persistence::json::serialize(read(control),false);inde::ui::PublicationPageProbe::preview(*page);stage=21;return true;}
  if(stage==21){if(++preview_ticks<8)return true;auto *page=find_widget<inde::ui::PublicationPage>(*window);const auto control=inde::ui::PublicationPageProbe::preview_control(*page);if(preview_record!=inde::persistence::json::serialize(read(control),false))throw std::runtime_error("Reabrir criou outro servidor");std::cout<<"PUB-03A GTK: reabrir a mesma geração conserva PID, porta e recibo da prévia OK\n";g_signal_emit_by_name(find_button(*window,"Voltar ao INDE")->gobj(),"clicked");stage=3;return true;}
  if(writing->current_document_id()!=doc||editor->get_buffer()!=buffer||buffer->get_text().raw()!=body||inde::ui::serialize_view(writing->capture_view())!=view||file_hash(project/"data/project.sqlite3")!=db_hash)throw std::runtime_error("Catálogo alterou sessão ou banco autoral");
  action("web-publication");if(!find_widget<inde::ui::PublicationPage>(*window))throw std::runtime_error("Catálogo salvo não reabriu");
  result=0;std::cout<<"PUB-02B GTK: escolhas explícitas, reordenação pelos botões, rascunho preservado, salvamento, geração de duas Obras e retorno à sessão OK\n";window->close();app->quit();return false;
 }catch(const std::exception &error){std::cerr<<error.what()<<'\n';window->hide();app->quit();return false;}},250);});app->run();window.reset();return result;
}

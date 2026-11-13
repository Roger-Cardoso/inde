#include "main_window_smoke.hpp"
#include "inde/ui/main_window.hpp"
#include "ui_test_driver.hpp"
#include <gtk/gtk.h>
#include <gsk/gsk.h>
#include <cairomm/cairomm.h>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
template<class T>T *find_widget(Gtk::Widget &root){if(auto *match=dynamic_cast<T*>(&root))return match;for(auto *child=root.get_first_child();child;child=child->get_next_sibling())if(auto *match=find_widget<T>(*child))return match;return nullptr;}
Gtk::Button *find_button(Gtk::Widget &root,const std::string &label){if(auto *b=dynamic_cast<Gtk::Button*>(&root);b&&b->get_label().raw()==label)return b;for(auto *child=root.get_first_child();child;child=child->get_next_sibling())if(auto *b=find_button(*child,label))return b;return nullptr;}
void capture_window(Gtk::Widget &w,const fs::path &p){
 auto *snap=gtk_snapshot_new();auto *paint=gtk_widget_paintable_new(GTK_WIDGET(w.gobj()));gdk_paintable_snapshot(paint,GDK_SNAPSHOT(snap),w.get_width(),w.get_height());g_object_unref(paint);auto *node=gtk_snapshot_free_to_node(snap);if(!node){auto *window=dynamic_cast<Gtk::Window*>(&w);if(window&&window->get_child()){auto *direct=gtk_snapshot_new();gtk_widget_snapshot_child(GTK_WIDGET(window->gobj()),GTK_WIDGET(window->get_child()->gobj()),direct);node=gtk_snapshot_free_to_node(direct);}}if(!node)throw std::runtime_error("Snapshot vazio; mapped="+std::to_string(w.get_mapped())+" size="+std::to_string(w.get_width())+"x"+std::to_string(w.get_height()));auto surface=Cairo::ImageSurface::create(Cairo::Surface::Format::ARGB32,w.get_width(),w.get_height());auto ctx=Cairo::Context::create(surface);gsk_render_node_draw(node,ctx->cobj());gsk_render_node_unref(node);surface->write_to_png(p.string());
}
std::string file_hash(const fs::path &p){std::ifstream f(p,std::ios::binary);const std::string contents{std::istreambuf_iterator<char>(f),{}};return inde::publication::hash(contents);}
int run_main_window_smoke(const Glib::RefPtr<Gtk::Application> &app,const fs::path &project,const fs::path &picture){
 std::unique_ptr<inde::ui::MainWindow> window;inde::ui::WritingWorkspace *writing{};Gtk::TextView *editor{};Glib::RefPtr<Gtk::TextBuffer> buffer;std::string selected_doc,view,before,edited;int stage=0,ticks=0,result=1;
 auto action=[&](const char *name){auto a=window->lookup_action(name);if(!a)throw std::runtime_error(std::string("Ação ausente: ")+name);a->activate();};
 app->signal_activate().connect([&]{window=std::make_unique<inde::ui::MainWindow>(project);app->add_window(*window);window->present();
 Glib::signal_timeout().connect([&]{try{
  if(++ticks>180)throw std::runtime_error("Timeout da sessão integrada");
  if(stage==0){
   action("show-writing");writing=find_widget<inde::ui::WritingWorkspace>(*window);if(!writing)throw std::runtime_error("Escrita ausente");
   std::ifstream f(project.parent_path()/"selecao.json");std::string raw{std::istreambuf_iterator<char>(f),{}};auto selection=inde::persistence::json::parse(raw);
   // Choose the unannotated second chapter; content changes must respect native semantic evidence.
   std::ifstream sf(project.parent_path()/"retrato.json");std::string sr{std::istreambuf_iterator<char>(sf),{}};auto snapshot=inde::persistence::json::parse(sr);
   selected_doc=snapshot.at("units").as_array().back().at("documents").as_array().front().at("id").as_string();writing->reveal_document(selected_doc);
   editor=find_widget<Gtk::TextView>(*writing);if(!editor)throw std::runtime_error("Editor ausente");buffer=editor->get_buffer();before=buffer->get_text();
   const auto db_before=file_hash(project/"data/project.sqlite3");buffer->insert(buffer->end(),"\nEdição na sessão viva.");edited=buffer->get_text();
   action("web-publication");if(find_widget<inde::ui::PublicationPage>(*window))throw std::runtime_error("Abriu com texto pendente");if(file_hash(project/"data/project.sqlite3")!=db_before)throw std::runtime_error("Abertura provocou salvamento implícito");
   writing->flush_changes();stage=1;return true;
  }
  if(stage==1){
   if(!writing->publication_ready())return true;
   buffer->select_range(buffer->get_iter_at_offset(3),buffer->get_iter_at_offset(9));view=inde::ui::serialize_view(writing->capture_view());
   action("web-publication");auto *page=find_widget<inde::ui::PublicationPage>(*window);if(!page)throw std::runtime_error("Painel integrado não abriu");
   inde::ui::PublicationPageProbe::draft_site(*page);window->close();if(!window->get_visible())throw std::runtime_error("Fechou com formulário pendente");
   auto *footer=find_button(*window,"Voltar ao INDE");if(!footer)throw std::runtime_error("Retorno ausente");g_signal_emit_by_name(footer->gobj(),"clicked");stage=15;return true;
  }
  if(stage==15){
   auto *page=find_widget<inde::ui::PublicationPage>(*window);if(!page)throw std::runtime_error("O formulário pendente não reabriu");
   inde::ui::PublicationPageProbe::commit_site(*page);inde::ui::PublicationPageProbe::edition(*page);inde::ui::PublicationPageProbe::recover(*page);inde::ui::PublicationPageProbe::generate(*page);window->close();if(!window->get_visible())throw std::runtime_error("Fechou durante a geração");stage=2;return true;
  }
  if(stage==2){auto *page=find_widget<inde::ui::PublicationPage>(*window);if(!page||!inde::ui::PublicationPageProbe::done(*page))return true;if(page->get_width()<std::min(850,window->get_width()-120))throw std::runtime_error("Painel não recebeu a largura própria");capture_window(*window,picture);auto *button=find_button(*window,"Voltar ao INDE");if(!button)throw std::runtime_error("Retorno ausente");g_signal_emit_by_name(button->gobj(),"clicked");stage=3;return true;}
  if(stage==3){if(writing->current_document_id()!=selected_doc||editor->get_buffer()!=buffer||buffer->get_text().raw()!=edited||inde::ui::serialize_view(writing->capture_view())!=view)throw std::runtime_error("Retorno alterou Documento, buffer, texto ou seleção");capture_window(*window,picture.parent_path()/"editor-retorno-pub02.png");editor->grab_focus();action("writing-undo");stage=4;return true;}
  if(buffer->get_text().raw()!=before)throw std::runtime_error("Histórico de desfazer não foi preservado");
  writing->flush_changes();action("web-publication");if(!find_widget<inde::ui::PublicationPage>(*window))throw std::runtime_error("Painel não reabriu ao final");
  std::cout<<"PUB-02 GTK integrado: sessão viva, bloqueio sem salvamento implícito, CRUD, geração, retorno, seleção e desfazer OK\n";result=0;window->close();app->quit();return false;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';if(window)window->hide();app->quit();return false;}},250);
 });app->run();window.reset();return result;
}

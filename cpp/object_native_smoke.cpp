#include "main_window_smoke.hpp"
#include "object_native_ui.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/project/document_state.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include <webkit/webkit.h>
#include <fstream>
#include <iostream>
void capture_window(Gtk::Widget &,const std::filesystem::path &);
namespace {
WebKitWebView *webview(Gtk::Widget &w){if(WEBKIT_IS_WEB_VIEW(w.gobj()))return WEBKIT_WEB_VIEW(w.gobj());for(auto *c=w.get_first_child();c;c=c->get_next_sibling())if(auto *v=webview(*c))return v;return nullptr;}
struct Probe {std::string status;bool querying=false;};
void query(WebKitWebView *v,Probe &p){p.querying=true;webkit_web_view_evaluate_javascript(v,"document.body?.dataset.nativeSmoke || ''",-1,nullptr,nullptr,nullptr,+[](GObject *o,GAsyncResult *r,gpointer data){auto &out=*static_cast<Probe*>(data);GError *error=nullptr;auto *value=webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(o),r,&error);if(error)g_error_free(error);if(value){auto *raw=jsc_value_to_string(value);out.status=raw?raw:"";g_free(raw);g_object_unref(value);}out.querying=false;},&p);}
}
int run_object_window_smoke(const Glib::RefPtr<Gtk::Application> &app,const std::filesystem::path &project,const std::filesystem::path &picture){
 std::string id,original;try{
  inde::application::ProjectService service;service.create(project,"Objetos no INDE");auto d=service.writing().create_document("Matemática na narrativa");d.content="Uma travessia pode seguir muitas direções.\nAs relações também podem ser expressas com matemática.";d.paragraphs=inde::project::identify_paragraphs(d.content);d=service.writing().update_document(d);id=d.id;original=inde::persistence::encode_document_snapshot(d);service.close();
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
 std::unique_ptr<inde::ui::MainWindow> main;inde::ui::ObjectDocumentWindow *objects=nullptr;Probe probe;int stage=0,ticks=0,result=1,draft_ticks=0;bool injected=false;
 const auto eval=[](WebKitWebView *v,const std::string &s){webkit_web_view_evaluate_javascript(v,s.c_str(),-1,nullptr,nullptr,nullptr,nullptr,nullptr);};
 app->signal_activate().connect([&]{main=std::make_unique<inde::ui::MainWindow>(project);app->add_window(*main);main->present();
 Glib::signal_timeout().connect([&]{try{
  if(++ticks>480)throw std::runtime_error("Timeout no editor nativo: "+probe.status);
  if(stage==0){
   Gtk::Widget *root=main.get();std::function<inde::ui::WritingWorkspace*(Gtk::Widget&)> find=[&](Gtk::Widget &w)->inde::ui::WritingWorkspace*{if(auto *p=dynamic_cast<inde::ui::WritingWorkspace*>(&w))return p;for(auto *c=w.get_first_child();c;c=c->get_next_sibling())if(auto *p=find(*c))return p;return nullptr;};auto *writing=find(*root);if(!writing)throw std::runtime_error("Escrita ausente");writing->reveal_document(id);if(!writing->publication_ready())return true;main->lookup_action("document-objects")->activate();stage=1;return true;
  }
  objects=nullptr;for(auto *w:app->get_windows())if(auto *p=dynamic_cast<inde::ui::ObjectDocumentWindow*>(w))objects=p;
  if(stage==2){
   if(objects&&objects->get_visible())return true;
   main->lookup_action("save-project")->activate();main->lookup_action("document-objects")->activate();stage=3;injected=false;probe.status.clear();return true;
  }
  if(!objects){throw std::runtime_error("Janela Objetos não abriu pelo INDE, etapa "+std::to_string(stage));}
  auto *v=webview(*objects);if(!v){throw std::runtime_error("Canvas nativo ausente");}
  if(!injected){if(!webkit_web_view_get_uri(v)||webkit_web_view_is_loading(v))return true;std::ifstream f(std::filesystem::path(INDE_OBJECT_UI_DIR)/"native-smoke.js");const std::string script((std::istreambuf_iterator<char>(f)),{});if(script.empty())throw std::runtime_error("Teste da interface ausente");if(stage==3)eval(v,"document.body.dataset.smokeReopen='yes';");eval(v,script);injected=true;}
  if(!probe.querying&&probe.status.empty()){query(v,probe);}
  if(probe.status.starts_with("error:")){throw std::runtime_error(probe.status);}
  if(probe.status=="draft") {if(draft_ticks++==0){objects->close();main->close();}if(draft_ticks<3)return true;if(!objects->get_visible()||!main->get_visible())throw std::runtime_error("Fechou com rascunho");eval(v,"document.body.dataset.nativeDraftChecked='yes';");probe.status.clear();return true;}
  if(stage==1&&probe.status=="saved"){
   capture_window(*objects,picture);objects->close();stage=2;return true;
  }
  if(stage==3&&probe.status=="reopened"){
   capture_window(*objects,picture.parent_path()/"nativo-reaberto.png");std::ofstream out(picture.parent_path()/"nativo-smoke.json");out<<"{\"native_window\":true,\"project_bound\":true,\"no_http\":true,\"equations\":3,\"mathml\":true,\"table_and_formula\":true,\"draft_close_guard\":true,\"save_reopen\":true,\"source_document\":\""<<id<<"\"}\n";out.close();result=0;app->quit();return false;
  }
  return true;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';app->quit();return false;}},250);
 });app->run();main.reset();
 if(result==0){inde::application::ProjectService check;check.open(project);if(inde::persistence::encode_document_snapshot(*check.writing().document(id))!=original){std::cerr<<"Texto de origem alterado\n";return 1;}std::cout<<"Native GTK/WebKit: actual INDE action, real project, math, table, formula, draft protection, save/reopen and owner preservation OK\n";}
 return result;
}

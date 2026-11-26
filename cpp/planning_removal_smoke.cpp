#include "main_window_smoke.hpp"
#include "inde/ui/main_window.hpp"
#include "ui/contextual_planning_page.hpp"
#include "ui/contextual_canvas.hpp"
#include <fstream>
#include <iostream>

void capture_window(Gtk::Widget &,const std::filesystem::path &);
namespace {
template<class T,class F> T *find(Gtk::Widget &w,F f) {
 if(auto *t=dynamic_cast<T *>(&w);t && f(*t))return t;
 for(auto *c=w.get_first_child();c;c=c->get_next_sibling())if(auto *r=find<T>(*c,f))return r;
 return nullptr;
}
void require(bool yes,const std::string &m){if(!yes)throw std::runtime_error(m);}
void click(Gtk::Widget &w,const std::string &name){
 auto *b=find<Gtk::Button>(w,[&](auto &v){return v.get_mapped() && v.get_sensitive() &&
   (v.get_label().raw()==name || find<Gtk::Label>(v,[&](auto &l){return l.get_text().raw()==name;}));});
 require(b,"Visible action: "+name);g_signal_emit_by_name(b->gobj(),"clicked");
}
void menu(Gtk::Widget &w,const std::string &name){
 auto *m=find<Gtk::MenuButton>(w,[&](auto &v){return v.get_mapped() && v.get_label().raw()==name;});
 require(m,"Menu: "+name);m->popup();
}
void remove_key(inde::ui::ContextualCanvas &c){
 auto *list=gtk_widget_observe_controllers(GTK_WIDGET(c.gobj()));bool handled=false;
 for(guint i=0;i<g_list_model_get_n_items(list);++i){auto *v=G_OBJECT(g_list_model_get_item(list,i));
  if(GTK_IS_EVENT_CONTROLLER_KEY(v)){gboolean yes=false;g_signal_emit_by_name(v,"key-pressed",GDK_KEY_Delete,0,GdkModifierType(0),&yes);handled=handled||yes;}
  g_object_unref(v);
 }
 g_object_unref(list);require(handled,"Delete controller");
}
}
int run_planning_removal_smoke(const Glib::RefPtr<Gtk::Application> &app,
 const std::filesystem::path &path,const std::filesystem::path &picture){
 using namespace inde;
 if(std::filesystem::exists(path))throw std::runtime_error("Smoke requires a new synthetic project");
 std::string work,plan,other,a,moment,lane_id;
 {
 application::ProjectService s;s.create(path,"Exclusão visual sintética");application::ContextualPlanningService c(s);
 const auto ip=s.create_intellectual_property("QA","","").id;work=s.create_work(ip,"Obra","","","pt-BR","Planejamento").id;
 plan=c.create_plan(work,"Composição para excluir");other=c.create_plan(work,"Composição preservada");
 a=c.create_moment(c.snapshot(plan),"Reutilizado","Conservar conteúdo");c.create_moment(c.snapshot(plan),"Outro trecho","");
 moment=c.snapshot(plan).participations.front().moment_id;c.reuse_moment(c.snapshot(other),c.moment(moment));
 auto items=application::contextual_visual_layout(c.snapshot(plan));application::ContextualVisualItem lane;lane.id=project::new_uuid();lane_id=lane.id;lane.kind="lane";lane.label="Faixa independente";lane.x=1100;lane.y=500;items.push_back(lane);c.save_visual(c.snapshot(plan),items);
 s.save_as(path.parent_path()/"Demo-planejamento.inde");
 }
 std::unique_ptr<ui::MainWindow> window;int phase=0,ticks=0,result=1;sigc::connection timer;
 app->signal_activate().connect([&]{window=std::make_unique<ui::MainWindow>(path);app->add_window(*window);window->set_default_size(1280,900);window->present();
 timer=Glib::signal_timeout().connect([&]{try{
  require(++ticks<100,"Planning smoke timeout");if(!window->get_mapped())return true;
  auto *page=find<ui::ContextualPlanningPage>(*window,[](auto &){return true;});require(page,"Planning page");
  auto *canvas=find<ui::ContextualCanvas>(*page,[](auto &){return true;});require(canvas,"Canvas");
  auto observe=[&](auto f){application::ProjectService s;s.open(path);application::ContextualPlanningService c(s);f(s,c);};
  std::cout<<"Planning removal phase "<<phase<<std::endl;
  switch(phase){
  case 0:{auto action=window->lookup_action("show-planning");require(bool(action),"Planning navigation");action->activate();
   auto *stack=find<Gtk::Stack>(*window,[](auto &v){return v.get_child_by_name("narrative")!=nullptr;});require(stack,"Narrative pages");stack->set_visible_child("narrative");
   auto *builder=find<ui::NarrativeStructureWorkspace>(*window,[](auto &){return true;});require(builder,"Narrative workspace");builder->refresh();click(*builder,"Planejamento visual");page->reveal(work,plan);break;}
  case 1: require(canvas->get_mapped(),"Canvas mapped");canvas->select_part(a);remove_key(*canvas);break;
  case 2: capture_window(*window,picture.parent_path()/"planejamento-confirmacao.png");click(*window,"Cancelar");observe([&](auto &,auto &c){require(c.snapshot(plan).participations.size()==2,"Cancel mutated plan");});remove_key(*canvas);break;
  case 3: click(*window,"Excluir elementos");break;
  case 4: observe([&](auto &,auto &c){require(c.snapshot(plan).participations.size()==1,"Delete did not persist");require(c.moment(moment).description=="Conservar conteúdo","Shared content lost");require(c.snapshot(other).participations.size()==1,"Reuse lost");});
   {const auto cam=canvas->camera();canvas->context_target((1120-cam[0])*cam[2],(520-cam[1])*cam[2],Gdk::ModifierType(0));require(canvas->selection().contains(lane_id),"Lane target");menu(*page,"Superfície");}break;
  case 5: click(*window,"Excluir elementos selecionados…");break;
  case 6: click(*window,"Excluir elementos");break;
  case 7: observe([&](auto &,auto &c){require(c.snapshot(plan).visual.size()==1,"Lane remained");});menu(*page,"Superfície");break;
  case 8: click(*window,"Desfazer disposição");break;
  case 9: observe([&](auto &,auto &c){require(c.snapshot(plan).visual.size()==2,"Lane undo failed");});menu(*page,"Montagem");break;
  case 10: click(*window,"Excluir composição visual…");break;
  case 11: click(*window,"Cancelar");observe([&](auto &,auto &c){require(c.snapshot(plan).participations.size()==1,"Plan cancel mutated");});menu(*page,"Montagem");break;
  case 12: click(*window,"Excluir composição visual…");break;
  case 13: observe([&](auto &,auto &c){const auto s=c.snapshot(plan);c.set_note(s,s.participations.front().id,"Alteração concorrente");});click(*window,"Excluir composição");break;
  case 14: {auto *error=find<Gtk::Label>(*window,[](auto &v){return v.get_mapped() && v.get_text().find("Composição mudou")!=Glib::ustring::npos;});require(error,"Stale dialog must remain and explain refusal");click(*window,"Cancelar");page->refresh(work);menu(*page,"Montagem");break;}
  case 15: click(*window,"Excluir composição visual…");break;
  case 16: click(*window,"Excluir composição");break;
  case 17: observe([&](auto &,auto &c){require(c.plans(work).size()==1 && c.plans(work).front().id==other,"Plan deletion failed");require(c.moment(moment).title=="Reutilizado","Moment lost");});require(canvas->items().size()==1,"Remaining plan not displayed");capture_window(*window,picture);break;
  case 18: result=0;window->close();app->quit();return false;
  default: throw std::runtime_error("Bad phase");
  }
  ++phase;return true;
 }catch(const std::exception &e){std::cerr<<"Planning smoke: "<<e.what()<<std::endl;window->hide();app->quit();return false;}},160);
 });
 app->run();timer.disconnect();if(result==0)std::cout<<"Planning native deletion, cancellation, local undo, stale refusal and normal exit OK\n";return result;
}

#include "inde/ui/main_window.hpp"
#include "inde/ui/saved_workspaces.hpp"
#include "inde/ui/ui_preferences.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/project/manifest.hpp"
#include "ui/widget_probe.hpp"
#include "../src/ui/rendering_policy.hpp"
#include <iostream>
#include <cmath>
using namespace inde::test::ui;
int main(int argc,char **argv) {
  if(argc!=3)return 2;
  const bool restart=std::string(argv[2])=="--verify-restart";
  if(!restart && std::string(argv[2])!="--allow-fixture-writes")return 2;
  const std::filesystem::path fixture=argv[1];std::string project_id,doc,other,original,entity,prefix,node;
  {
    inde::application::ProjectService seed;seed.open(fixture);project_id=seed.current()->manifest().project_id;
    if(!restart) {
      prefix="QA-Continuidade-"+inde::project::new_uuid();
      const auto ip=seed.create_intellectual_property(prefix,"","").id;
      const auto work=seed.create_work(ip,"Obra de continuidade","","","pt-BR","Planejamento").id;
      node=seed.create_structural_node(work,std::nullopt,inde::project::StructuralNodeType::Chapter,"Capítulo retomado").id;
      auto d=seed.writing().create_document("Continuidade — Água 🙂");
      for(int i=0;i<400;++i)d.content+="Água, memória e escrita 🙂 — linha "+std::to_string(i)+".\n";
      original=d.content;doc=seed.writing().update_document(d).id;
      other=seed.writing().create_document("Outro caderno de QA").id;
      const auto types=seed.narrative().entity_types();
      const auto type=std::find_if(types.begin(),types.end(),[](const auto &t){return t.key=="character";});
      require(type!=types.end(),"Missing character type");
      for(int i=0;i<108;++i)entity=seed.narrative().create_entity(type->id,prefix+" "+std::to_string(i),"Somente QA").id;
    } else {
      const auto prefs=inde::ui::parse_view_state(inde::ui::ui_preference(inde::ui::project_preference_group(project_id),"context-v1"));
      std::cout<<"RESTART_PREF cursor="<<prefs.at("writing").at("cursor").as_integer()<<" bound="<<prefs.at("writing").at("bound").as_integer()<<" scroll="<<inde::ui::view_scroll(prefs.at("writing").at("scroll"))<<std::endl;
      const auto entries=inde::ui::saved_workspaces(project_id);require(entries.size()==2,"Named workspaces did not survive process restart");
      const auto first=inde::ui::parse_view_state(entries[0].state);doc=first.at("writing").at("document").as_string();
      node=first.at("editorial").at("editor").at("node").as_string();
      const auto second=inde::ui::parse_view_state(entries[1].state);prefix=second.at("planning").at("search").as_string();
      entity=second.at("planning").at("entity").as_string();original=seed.writing().document(doc)->content;
    }
    seed.close();
  }
  auto app=Gtk::Application::create("io.github.inde.continuity-smoke",Gio::Application::Flags::NON_UNIQUE);
  inde::ui::configure_rendering_backend();inde::ui::MainWindow *window{};int step{},result=1;sigc::connection tick;std::vector<sigc::connection> traces;gint64 reopened_at{},perturbed_at{};
  app->signal_activate().connect([&] {
    window=new inde::ui::MainWindow(fixture);app->add_window(*window);window->present();
    tick=Glib::signal_timeout().connect([&] {
      try {
        auto *writing=find<inde::ui::WritingWorkspace>(*window,[](auto &){return true;});
        auto *planning=find<inde::ui::PlanningWorkspace>(*window,[](auto &){return true;});require(writing && planning,"Missing production views");
        auto *manuscript=find<inde::ui::EditorialStructureWorkspace>(*window,[](auto &){return true;});require(manuscript,"Missing Manuscript");
        auto *editor=find<Gtk::TextView>(*writing,[](auto &v){return v.get_editable();});require(editor,"Missing editor");
        auto action=[&](const char *name){require(gtk_widget_activate_action(GTK_WIDGET(window->gobj()),name,nullptr),name);};
        auto dialog=[&]() {auto *d=find<Gtk::Box>(*window,[](auto &v){return v.has_css_class("modal-card") && v.get_mapped();});require(d,"Missing modal");return d;};
        auto remember=[&](const std::string &name){action("win.saved-workspaces");auto *d=dialog();auto *e=find<Gtk::Entry>(*d,[](auto &v){return v.get_placeholder_text()=="Nome do novo espaço";});require(e,"Missing workspace name");e->set_text(name);click(*d,"Salvar este espaço de trabalho");};
        auto check_position=[&] {
          std::cout<<"POSITION step="<<step<<" cursor="<<writing->capture_view().at("cursor").as_integer()<<" bound="<<writing->capture_view().at("bound").as_integer()<<" scroll="<<inde::ui::view_scroll(writing->capture_view().at("scroll"))<<std::endl;
          require(writing->current_document_id()==doc,"Wrong resumed Document");
          require(editor->get_buffer()->get_text().raw()==original,"Context changed text");
          require(editor->get_buffer()->get_iter_at_mark(editor->get_buffer()->get_insert()).get_offset()==25,"Unicode cursor lost");
          require(editor->get_buffer()->get_iter_at_mark(editor->get_buffer()->get_selection_bound()).get_offset()==17,"Selection bound lost");
          require(inde::ui::view_scroll(writing->capture_view().at("scroll"))>500,"Scroll was not restored");
        };
        if(step==0 && g_getenv("INDE_CONTINUITY_TRACE")) {
          auto *scroll=find<Gtk::ScrolledWindow>(*writing,[](auto &s){return find<Gtk::TextView>(s,[](auto &v){return v.get_editable();})!=nullptr;});
          require(scroll,"Trace scroller missing");
          auto trace=[&,scroll,editor] {
            const auto a=scroll->get_vadjustment();
            std::cout<<"ADJUST time="<<g_get_monotonic_time()<<" step="<<step<<" value="<<a->get_value()<<" upper="<<a->get_upper()<<" page="<<a->get_page_size()<<" mapped="<<editor->get_mapped()<<" focus="<<editor->has_focus()<<std::endl;
          };
          traces.push_back(scroll->get_vadjustment()->signal_value_changed().connect(trace));
          traces.push_back(scroll->get_vadjustment()->signal_changed().connect(trace));
        }
        if(restart) {
          if(step==0) {check_position();action("win.show-planning");}
          else if(step==1) {
            const auto v=planning->capture_view();require(v.at("search").as_string()==prefix,"Search lost across process restart");
            require(v.at("offset").as_integer()==100,"Page 2 lost across process restart");
            require(v.at("entity").as_string()==entity && pages(*planning,"detail").get_visible_child_name()=="detail","Entity detail lost across restart");
            std::cout<<"SNAPSHOT: restart-planning"<<std::endl;
          } else if(step==2) {action("win.show-editorial");}
          else if(step==3) {
            const auto v=manuscript->capture_view();require(v.at("editor").at("node").as_string()==node && v.at("editor").at("detail").as_boolean(),"Manuscript unit lost across process restart");
            std::cout<<"SNAPSHOT: restart-manuscript"<<std::endl;
          } else if(step==4) {action("win.show-writing");
          } else {check_position();result=0;std::cout<<"PASS actual process restart"<<std::endl;window->close();return false;}
        } else if(step==0) {action("win.show-editorial");manuscript->reveal_node(node);action("win.show-writing");writing->reveal_document(doc);}
        else if(step==1) {
          const auto b=editor->get_buffer();b->select_range(b->get_iter_at_offset(25),b->get_iter_at_offset(17));
          auto v=writing->capture_view().as_object();v["scroll"]=inde::ui::scroll_state(1400);writing->restore_view(v);
        } else if(step==2) {action("win.toggle-focus");remember("Escrita à noite");std::cout<<"SNAPSHOT: saved-workspaces"<<std::endl;}
        else if(step==3) {click(*dialog(),"Fechar");action("win.toggle-focus");writing->reveal_document(other);}
        else if(step==4) {action("win.saved-workspaces");click(*dialog(),"Retomar");}
        else if(step==5) {check_position();auto *focus=find<Gtk::ToggleButton>(*window,[](auto &b){return b.get_label()=="Foco";});require(focus && focus->get_active(),"Named workspace lost Focus toggle");action("win.toggle-focus");action("win.show-planning");auto *s=find<Gtk::Entry>(*planning,[](auto &e){return e.get_placeholder_text()=="Pesquisar nome, alias ou resumo";});require(s,"Missing search");s->set_text(prefix);}
        else if(step==6) {auto v=planning->capture_view().as_object();v["scroll"]=inde::ui::scroll_state(500);planning->restore_view(v);}
        else if(step==7) {
          require(inde::ui::view_scroll(planning->capture_view().at("scroll"))>=300,"Explorer scroll was not restored");
          click(*planning,"Próxima");
          auto *card=find<Gtk::Button>(*planning,[](auto &b){return b.has_css_class("entity-card") && b.get_mapped();});
          require(card,"No second-page entity");g_signal_emit_by_name(card->gobj(),"clicked");
          entity=planning->capture_view().at("entity").as_string();remember("Planejar a cena");
        }
        else if(step==8) {
          click(*dialog(),"Fechar");action("win.show-writing");
          std::cout<<"BEFORE_CLOSE scroll="<<inde::ui::view_scroll(writing->capture_view().at("scroll"))<<std::endl;
          action("win.close-project");
          const auto saved=inde::ui::parse_view_state(inde::ui::ui_preference(inde::ui::project_preference_group(project_id),"context-v1"));
          std::cout<<"SAVED_CLOSE scroll="<<inde::ui::view_scroll(saved.at("writing").at("scroll"))<<std::endl;
          click(*window,fixture.stem().string());
          reopened_at=g_get_monotonic_time();
          // Exercise a delayed GTK scroll-to-insert after reopen, rather than
          // relying on the machine timing to reproduce the focus/layout race.
          Glib::signal_timeout().connect_once([editor,&perturbed_at] {
            perturbed_at=g_get_monotonic_time();
            if(g_getenv("INDE_CONTINUITY_TRACE"))std::cout<<"PERTURB time="<<g_get_monotonic_time()<<std::endl;
            editor->scroll_to(editor->get_buffer()->get_insert());
          },180);
        }
        else if(step==9) {
          // Reopening is synchronous but GTK layout/selection handoff is not.
          // A recurring test timer may already be due when reopen returns.
          // Require the deliberate perturbation to have actually fired, then
          // allow eight 50 ms settling ticks. Still fail within a bounded time;
          // do not accept a saved intent in place of the actual adjustment.
          const auto now=g_get_monotonic_time();
          require(now-reopened_at<4000000,"GTK reopen did not settle within four seconds");
          if(!perturbed_at || now-perturbed_at<400000)return true;
          check_position();action("win.show-planning");
        }
        else if(step==10) {
          const auto v=planning->capture_view();require(v.at("search").as_string()==prefix && v.at("offset").as_integer()==100,"Planning query lost on reopen");
          require(v.at("entity").as_string()==entity,"Entity lost on reopen");action("win.show-writing");action("win.writing-export");std::cout<<"SNAPSHOT: export-preview-light"<<std::endl;
        } else if(step==11) {
          auto *d=dialog();auto *p=find<Gtk::TextView>(*d,[](auto &v){return !v.get_editable();});require(p && p->get_buffer()->get_text().raw()==original,"Export preview changed text");
          require(find<Gtk::Label>(*d,[](auto &l){return l.get_text().raw().find("não a formatação")!=std::string::npos;}),"Export losses not visible");
          click(*d,"Cancelar");action("win.theme-dark");action("win.writing-export");std::cout<<"SNAPSHOT: export-preview-dark"<<std::endl;
        } else if(step==12) {
          click(*dialog(),"Cancelar");
          const auto remembered=inde::ui::parse_view_state(inde::ui::ui_preference(inde::ui::project_preference_group(project_id),"position-"+doc));
          std::cout<<"AFTER_PREVIEW scroll="<<inde::ui::view_scroll(writing->capture_view().at("scroll"))<<" remembered="<<inde::ui::view_scroll(remembered.at("scroll"))<<std::endl;
          auto stale=writing->capture_view().as_object();stale["document"]="missing-document";writing->restore_view(stale);
          require(writing->current_document_id().empty(),"Missing Document did not fall back to library");writing->reveal_document(doc);
        } else if(step==13) {
          check_position();auto deep=std::string(30,'[')+std::string(30,']');bool rejected{};
          try{(void)inde::ui::parse_view_state(deep);}catch(const std::exception &){rejected=true;}require(rejected,"Unbounded context parser");
          auto position=writing->capture_view().as_object();position["scroll"]=inde::ui::scroll_state(1000);writing->restore_view(position);
          auto *controllers=gtk_widget_observe_controllers(GTK_WIDGET(editor->gobj()));bool wheel{};
          for(guint i=0;i<g_list_model_get_n_items(controllers);++i) {
            auto *item=g_list_model_get_item(controllers,i);
            if(GTK_IS_EVENT_CONTROLLER_SCROLL(item) && gtk_event_controller_get_propagation_phase(GTK_EVENT_CONTROLLER(item))==GTK_PHASE_CAPTURE) {
              gboolean consumed{};g_signal_emit_by_name(item,"scroll",0.0,1.0,&consumed);wheel=true;
            }
            g_object_unref(item);
          }
          g_object_unref(controllers);require(wheel,"User wheel cancellation controller missing");
          auto *scroll=find<Gtk::ScrolledWindow>(*writing,[&](auto &s){return find<Gtk::TextView>(s,[](auto &v){return v.get_editable();})!=nullptr;});
          require(scroll,"Editor scroller missing");scroll->get_vadjustment()->set_value(650);
        } else if(step==14) {
          require(std::abs(inde::ui::view_scroll(writing->capture_view().at("scroll"))-650)<1,"Restoration overrode user wheel position");
          auto position=writing->capture_view().as_object();position["scroll"]=inde::ui::scroll_state(1400);writing->restore_view(position);
        } else if(step==15) {
          check_position();
          result=0;std::cout<<"PASS named workspaces, selection, scroll, query, reopen, export preview"<<std::endl;window->close();
          const auto saved=inde::ui::parse_view_state(inde::ui::ui_preference(inde::ui::project_preference_group(project_id),"context-v1"));
          std::cout<<"WINDOW_CLOSE cursor="<<saved.at("writing").at("cursor").as_integer()<<" bound="<<saved.at("writing").at("bound").as_integer()<<" scroll="<<inde::ui::view_scroll(saved.at("writing").at("scroll"))<<std::endl;
          require(saved.at("writing").at("bound").as_integer()==17,"Selection lost at close barrier");return false;
        }
        ++step;return true;
      } catch(const std::exception &error){std::cerr<<"FAIL step="<<step<<" "<<error.what()<<std::endl;window->close();return false;}
    },1300);
  });
  char *args[]={argv[0],nullptr};app->run(1,args);tick.disconnect();for(auto &trace:traces)trace.disconnect();delete window;return result;
}

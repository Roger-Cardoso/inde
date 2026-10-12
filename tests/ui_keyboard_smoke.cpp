#include "inde/ui/main_window.hpp"
#include "inde/ui/keyboard_commands.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/calendar_picker.hpp"
#include "inde/ui/timeline_view.hpp"
#include "../src/ui/rendering_policy.hpp"
#include "ui/widget_probe.hpp"
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include <gdk/x11/gdkx.h>
using namespace inde;
using namespace inde::test::ui;
namespace {
void native_chord(Gtk::Window &window,guint key,bool shift=false) {
  auto *x11=dlopen("libX11.so.6",RTLD_NOW),*xtst=dlopen("libXtst.so.6",RTLD_NOW);
  require(x11 && xtst,"XTest dependencies missing");
  auto open=reinterpret_cast<Display *(*)(const char *)>(dlsym(x11,"XOpenDisplay"));
  auto close=reinterpret_cast<int (*)(Display *)>(dlsym(x11,"XCloseDisplay"));
  auto code=reinterpret_cast<KeyCode (*)(Display *,KeySym)>(dlsym(x11,"XKeysymToKeycode"));
  auto focus=reinterpret_cast<int (*)(Display *,Window,int,Time)>(dlsym(x11,"XSetInputFocus"));
  auto sync=reinterpret_cast<int (*)(Display *,Bool)>(dlsym(x11,"XSync"));
  auto send=reinterpret_cast<int (*)(Display *,unsigned int,Bool,unsigned long)>(dlsym(xtst,"XTestFakeKeyEvent"));
  require(open && close && code && focus && sync && send,"XTest symbols missing");
  auto *display=open(nullptr);require(display,"XTest display missing");
  focus(display,gdk_x11_surface_get_xid(window.get_surface()->gobj()),RevertToParent,CurrentTime);
  sync(display,False);
  send(display,code(display,GDK_KEY_Control_L),True,20);
  if(shift)send(display,code(display,GDK_KEY_Shift_L),True,20);
  send(display,code(display,key),True,20);send(display,code(display,key),False,20);
  if(shift)send(display,code(display,GDK_KEY_Shift_L),False,20);
  send(display,code(display,GDK_KEY_Control_L),False,20);sync(display,False);close(display);dlclose(xtst);dlclose(x11);
}
ui::KeyboardCommands *registry(Gtk::Window &w) {
  auto *items=gtk_widget_observe_controllers(GTK_WIDGET(w.gobj()));ui::KeyboardCommands *answer{};
  for(guint i=0;i<g_list_model_get_n_items(items);++i){auto *item=G_OBJECT(g_list_model_get_item(items,i));if(auto *p=g_object_get_data(item,"inde-router-owner"))answer=static_cast<ui::KeyboardCommands *>(p);g_object_unref(item);}
  g_object_unref(items);require(answer,"Window command router missing");return answer;
}
bool key(Gtk::Widget &widget,guint value,unsigned mods=0,bool router_only=false) {
  auto *items=gtk_widget_observe_controllers(widget.gobj());bool handled{};
  for(guint i=0;i<g_list_model_get_n_items(items);++i){auto *item=G_OBJECT(g_list_model_get_item(items,i));
    if(GTK_IS_EVENT_CONTROLLER_KEY(item) && (!router_only || g_object_get_data(item,"inde-router-owner"))){gboolean result{};g_signal_emit_by_name(item,"key-pressed",value,0u,mods,&result);handled|=result;}
    g_object_unref(item);
  }g_object_unref(items);return handled;
}
}
int main(int argc,char **argv) {
  if(argc!=3 || std::string(argv[2])!="--allow-fixture-writes")return 2;
  ui::configure_rendering_backend();
  std::string id;
  {application::ProjectService service;service.open(argv[1]);auto d=service.writing().create_document("Teste de teclado — cópia isolada");
   d.content="Água e memória.\nUma seleção autoral permanece aqui.\n";d=service.writing().update_document(d);id=d.id;service.close();}
  auto app=Gtk::Application::create("io.github.inde.keyboard-probe",Gio::Application::Flags::NON_UNIQUE);
  std::unique_ptr<ui::MainWindow> window;sigc::connection timer;int phase{},result=1;
  Gtk::TextView *editor{};ui::WritingWorkspace *writing{};bool native_watch{};
  std::string content;std::size_t checks{};
  ui::OverlayDialog *controls{}; // Response-based OverlayDialog disposes itself.
  ui::CalendarPicker *calendar{};ui::TimelineView *timeline{};std::string timeline_selection;
  int destructive_response=Gtk::ResponseType::NONE;bool safe_form_handled{};
  std::string capture_stage;
  auto captured=[&](const std::string &stage) {
    if(capture_stage!=stage){capture_stage=stage;std::cout<<"SNAPSHOT: "<<stage<<std::endl;return false;}
    const auto *directory=std::getenv("INDE_UI_CAPTURE_ACK_DIR");
    return !directory || std::filesystem::exists(std::filesystem::path(directory)/stage);
  };
  app->signal_activate().connect([&]{
    window=std::make_unique<ui::MainWindow>(std::filesystem::path(argv[1]));app->add_window(*window);window->present();
    auto native_log=Gtk::EventControllerKey::create();native_log->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    native_log->signal_key_pressed().connect([&](guint value,guint code,Gdk::ModifierType mods){
      if(native_watch)std::cout<<"NATIVE-EVENT: key="<<value<<" code="<<code<<" modifiers="<<static_cast<unsigned>(mods)<<" focus="<<(window->get_focus() ? G_OBJECT_TYPE_NAME(window->get_focus()->gobj()) : "none")<<std::endl;
      return false;
    },false);window->add_controller(native_log);
    timer=Glib::signal_timeout().connect([&]{try {
      if(!window->get_mapped())return true;
      auto &w=*window;
      auto *host=ui::find_modal_overlay(w);require(host,"Overlay host missing");
      switch(phase) {
      case 0: {
        writing=find<ui::WritingWorkspace>(w,[](auto &){return true;});require(writing,"Writing workspace missing");
        writing->reveal_document(id);break;
      }
      case 1: {
        editor=find<Gtk::TextView>(*writing,[](auto &v){return v.get_mapped() && v.get_editable();});require(editor,"Mapped editor missing");
        content=editor->get_buffer()->get_text();editor->grab_focus();
        auto *r=registry(w);require(r->commands().size()>170,"Registry incomplete");
        for(const auto &c:r->commands())std::cout<<"COMMAND: "<<c.id<<"|"<<r->binding(c)<<"|"<<c.label<<"|"<<c.group<<'\n';
        auto a=editor->get_buffer()->get_iter_at_offset(0),b=editor->get_buffer()->get_iter_at_offset(4);editor->get_buffer()->select_range(a,b);
        require(key(w,GDK_KEY_b,GDK_CONTROL_MASK,true),"Editor Ctrl+B not routed");++checks;break;
      }
      case 2: {
        require(editor->get_buffer()->get_text().raw()==content,"Formatting changed text");
        auto *entry=find<Gtk::Entry>(*writing,[](auto &e){return e.get_mapped();});require(entry,"Editor title field missing");entry->grab_focus();
        require(!key(w,GDK_KEY_b,GDK_CONTROL_MASK,true),"Ctrl+B hit document from title field");
        require(!key(w,GDK_KEY_z,GDK_CONTROL_MASK,true),"Global undo stole native field undo");
        require(!key(w,GDK_KEY_b,GDK_CONTROL_MASK|GDK_ALT_MASK,true),"Extra Alt modifier accepted");checks+=4;
        editor->grab_focus();
        editor->get_buffer()->select_range(editor->get_buffer()->get_iter_at_offset(0),editor->get_buffer()->get_iter_at_offset(4));
        require(key(w,GDK_KEY_p,GDK_CONTROL_MASK|GDK_SHIFT_MASK,true),"Palette chord missing");break;
      }
      case 3: {
        auto *search=find<Gtk::SearchEntry>(w,[](auto &s){return s.get_mapped() && s.get_placeholder_text()=="Buscar comando ou recurso…";});require(search,"Palette search missing");
        require(!key(w,GDK_KEY_2,GDK_CONTROL_MASK,true),"Background navigation through palette");
        if(!captured("keyboard-palette"))return true;
        auto *popover=find<Gtk::Popover>(w,[&](auto &p){return p.get_mapped() && ui::focus_inside(w.get_focus(),p);});require(popover,"Palette missing");
        require(key(*popover,GDK_KEY_Escape),"Palette Escape missing");checks+=2;break;
      }
      case 4: {
        require(ui::focus_inside(w.get_focus(),*editor),"Palette failed to restore editor focus");
        Gtk::TextIter a,b;require(editor->get_buffer()->get_selection_bounds(a,b) && std::abs(a.get_offset()-b.get_offset())==4,"Palette lost selection");
        g_signal_emit_by_name(editor->gobj(),"preedit-changed","あ");require(!key(w,GDK_KEY_p,GDK_CONTROL_MASK|GDK_SHIFT_MASK,true),"IME allowed palette interruption");
        g_signal_emit_by_name(editor->gobj(),"preedit-changed","");checks+=3;break;
      }
      case 5: {
        require(key(w,GDK_KEY_n,GDK_CONTROL_MASK|GDK_SHIFT_MASK,true),"New free document chord missing");break;
      }
      case 6: {
        require(host->has_dialog(),"New document form missing");
        require(!key(w,GDK_KEY_2,GDK_CONTROL_MASK,true),"Navigation leaked behind modal");
        require(!key(w,GDK_KEY_s,GDK_CONTROL_MASK,true),"Save leaked behind modal");
        auto *layer=find<Gtk::Box>(*host,[](auto &b){return b.get_mapped() && b.has_css_class("modal-layer");});
        if(!layer)layer=find<Gtk::Box>(*host,[](auto &b){return b.get_mapped() && b.has_css_class("modal-scrim");});
        require(layer,"Modal layer missing");
        auto *entry=find<Gtk::Entry>(*layer,[](auto &e){return e.get_mapped() && e.get_placeholder_text()=="Nome do Documento";});require(entry,"New document title missing");entry->set_text("Documento livre pelo teclado");
        if(!captured("keyboard-safe-form"))return true;
        require(layer,"Modal capture layer missing");
        safe_form_handled=key(*layer,GDK_KEY_Return,GDK_CONTROL_MASK);checks+=3;break;
      }
      case 7: {
        require(safe_form_handled,"Safe Ctrl+Enter missing");
        if(host->has_dialog()) {
          count(*host,[](auto &widget){if(auto *label=dynamic_cast<Gtk::Label *>(&widget);label && label->get_mapped())std::cerr<<"MODAL: "<<label->get_text()<<'\n';return false;});
        }
        require(!host->has_dialog(),"Safe form remained open");
        require(writing->current_document_id()!=id,"Free document not created");
        editor=find<Gtk::TextView>(*writing,[](auto &v){return v.get_mapped() && v.get_editable();});require(editor,"New editor missing");editor->grab_focus();
        require(key(w,GDK_KEY_w,GDK_CONTROL_MASK,true),"Leave view chord missing");break;
      }
      case 8: {
        require(pages(*writing,"library").get_visible_child_name()=="library","Ctrl+W did not leave view");
        auto *r=registry(w);std::string error;
        require(!r->rebind("show-planning","<Control>1",error),"Conflict accepted");
        require(!r->rebind("show-planning","<Control><Alt>k",error),"AltGr conflict accepted");
        require(!r->rebind("show-planning","<Control><Shift>u",error),"Unicode chord accepted");
        require(!r->rebind("show-planning","a",error),"Printable unmodified chord accepted");
        require(r->rebind("show-planning","<Control><Shift>F12",error),error);
        require(key(w,GDK_KEY_F12,GDK_CONTROL_MASK|GDK_SHIFT_MASK,true),"Rebound chord not routed");checks+=7;break;
      }
      case 9: {
        require(find<ui::PlanningWorkspace>(w,[](auto &p){return p.get_mapped();}),"Rebound navigation failed");
        auto *r=registry(w);std::string error;require(r->rebind("show-planning","",error),error);
        require(!key(w,GDK_KEY_F12,GDK_CONTROL_MASK|GDK_SHIFT_MASK,true),"Removed chord remained active");
        r->restore_defaults();require(key(w,GDK_KEY_2,GDK_CONTROL_MASK,true),"Default restoration failed");checks+=3;
        require(key(w,GDK_KEY_F1,GDK_CONTROL_MASK,true),"Shortcut help missing");break;
      }
      case 10: {
        if(!captured("keyboard-help"))return true;
        auto *popover=find<Gtk::Popover>(w,[&](auto &p){return p.get_mapped() && ui::focus_inside(w.get_focus(),p);});require(popover,"Shortcut help missing");
        key(*popover,GDK_KEY_Escape);w.activate_action("win.show-writing");writing->reveal_document(id);break;
      }
      case 11: {
        editor=find<Gtk::TextView>(*writing,[](auto &v){return v.get_mapped() && v.get_editable();});require(editor,"Editor after navigation missing");editor->grab_focus();
        require(editor->get_buffer()->get_text().raw()==content,"Navigation changed author text");
        require(key(w,GDK_KEY_s,GDK_CONTROL_MASK,true),"Save command missing");
        ++checks;native_watch=true;native_chord(w,GDK_KEY_p,true);break;
      }
      case 12: {
        auto *popover=find<Gtk::Popover>(w,[&](auto &p){return p.get_mapped() && ui::focus_inside(w.get_focus(),p);});require(popover,"Native Ctrl+Shift+P did not reach GTK router");
        key(*popover,GDK_KEY_Escape);++checks;break;
      }
      case 13: native_chord(w,GDK_KEY_9,true);break;
      case 14: {
        require(host->has_dialog(),"Native Shift+digit shortcut failed on keyboard layout");
        click(*host,"Fechar");++checks;break;
      }
      case 15: {
        require(!host->has_dialog(),"Workspace modal not dismissed");
        controls=new ui::OverlayDialog("Controles de calendário e cronologia",w);
        calendar=Gtk::make_managed<ui::CalendarPicker>();calendar->set_calendar(project::calendar_template("gregorian"));
        calendar->set_selection(project::TemporalBoundary{2024,2,29,{}});
        timeline=Gtk::make_managed<ui::TimelineView>();timeline->set_content_height(160);
        application::TimelineSnapshot snapshot;snapshot.axis.id="isolated";snapshot.axis.name="Projeção de teste";
        snapshot.points={{"p1",-10,"Antes",{}},{"p2",10,"Agora",{}},{"p3",30,"Depois",{}}};timeline->set_snapshot(snapshot);
        timeline->signal_item_selected().connect([&](auto,const auto &id){timeline_selection=id;});
        controls->get_content_area()->append(*calendar);controls->get_content_area()->append(*timeline);
        controls->add_button("Fechar",Gtk::ResponseType::CLOSE);controls->signal_response().connect([&](int){controls->hide();});controls->present();break;
      }
      case 16: {
        auto *grid=find<Gtk::Grid>(*calendar,[](auto &g){return g.get_mapped();});require(grid,"Calendar grid missing");
        auto *day=find<Gtk::Button>(*grid,[](auto &b){return b.get_mapped() && b.get_sensitive();});require(day,"Calendar day missing");day->grab_focus();
        require(key(*grid,GDK_KEY_Page_Down,GDK_SHIFT_MASK),"Calendar year navigation missing");
        ++checks;break;
      }
      case 17: {
        auto *grid=find<Gtk::Grid>(*calendar,[](auto &g){return g.get_mapped();});require(grid,"Calendar grid after navigation missing");
        require(calendar->visible_month().year==2025,"Calendar did not advance year");
        require(calendar->boundary().year==2024 && calendar->boundary().day==29,"Calendar navigation changed chosen boundary");
        require(!key(*grid,GDK_KEY_Page_Down,GDK_CONTROL_MASK),"Calendar surplus modifier accepted");
        auto *year=find<Gtk::Entry>(*calendar,[](auto &e){return e.get_mapped();});require(year,"Calendar year entry missing");year->grab_focus();
        require(!key(*grid,GDK_KEY_Right),"Calendar grid intercepted year text navigation");checks+=4;
        timeline->grab_focus();break;
      }
      case 18: {
        require(key(*timeline,GDK_KEY_End),"Timeline keyboard selection missing");
        require(key(*timeline,GDK_KEY_Return),"Timeline inspection missing");require(timeline_selection=="p3","Timeline selected wrong endpoint");
        require(!key(*timeline,GDK_KEY_End,GDK_CONTROL_MASK),"Timeline surplus modifier accepted");checks+=4;
        controls->hide();delete controls;controls=nullptr;
        controls=new ui::OverlayDialog("Confirmação de risco — teste sem exclusão",w);
        controls->add_button("Cancelar",Gtk::ResponseType::CANCEL);controls->add_button("Remover",Gtk::ResponseType::REJECT);
        controls->signal_response().connect([&](int response){destructive_response=response;controls->hide();});controls->present();break;
      }
      case 19: {
        auto *button=dynamic_cast<Gtk::Button *>(w.get_focus());require(!button || button->get_label()!="Remover","Destructive button got initial focus");
        auto *layer=find<Gtk::Box>(*host,[](auto &b){return b.get_mapped() && b.has_css_class("modal-scrim");});require(layer,"Destructive modal layer missing");
        require(!key(*layer,GDK_KEY_Return,GDK_CONTROL_MASK),"Ctrl+Enter accepted destructive action");
        require(destructive_response==Gtk::ResponseType::NONE,"Destructive action executed");
        require(key(*layer,GDK_KEY_Escape),"Escape did not cancel risk dialog");checks+=4;break;
      }
      case 20: {
        require(destructive_response!=Gtk::ResponseType::REJECT && !host->has_dialog(),"Risk confirmation was not cancelled");controls=nullptr;
        registry(w)->show(true);break;
      }
      case 21: {
        auto *entry=find<Gtk::Entry>(w,[](auto &e){return GTK_IS_ENTRY(e.gobj()) && e.get_mapped() && e.get_placeholder_text()=="Clique aqui e pressione o novo atalho…";});require(entry,"Shortcut capture field missing");entry->grab_focus();
        require(key(*entry,GDK_KEY_F12,GDK_CONTROL_MASK|GDK_SHIFT_MASK),"Shortcut capture missing");
        require(entry->get_text().raw().find("<Control>")==std::string::npos && entry->get_text().raw().find("F12")!=std::string::npos,"Shortcut capture exposed accelerator syntax");
        if(!captured("keyboard-binding"))return true;
        auto *popover=find<Gtk::Popover>(w,[&](auto &p){return p.get_mapped() && ui::focus_inside(w.get_focus(),p);});require(popover,"Capture popover missing");click(*popover,"Alterar atalho");
        auto *r=registry(w);auto found=std::find_if(r->commands().begin(),r->commands().end(),[](auto &c){return c.id=="command-palette";});require(found!=r->commands().end(),"Palette identity missing");
        require(ui::shortcut_matches(GDK_KEY_F12,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK|GDK_SHIFT_MASK),r->binding(*found)),"Captured preference not saved");
        r->restore_defaults();key(*popover,GDK_KEY_Escape);checks+=4;break;
      }
      case 22: {
        w.activate_action("win.show-planning");require(key(w,GDK_KEY_8,GDK_CONTROL_MASK|GDK_SHIFT_MASK,true),"Montage global navigation missing");++checks;break;
      }
      case 23: {
        require(writing->get_mapped() && pages(*writing,"compositions").get_visible_child_name()=="compositions","Global montage shortcut did not open montage from planning");++checks;
        require(key(w,GDK_KEY_q,GDK_CONTROL_MASK,true),"Protected quit shortcut missing");++checks;
        require(!w.gobj() || !w.get_visible(),"Quit did not close the window");
        std::cout<<"PASS keyboard GTK "<<checks<<" focus, modal, IME, palette, native X11 layout, preferences and actual quit checks\n"<<std::flush;result=0;app->quit();return false;
      }
      }++phase;return true;
    }catch(const std::exception &e){std::cerr<<"FAIL keyboard phase="<<phase<<": "<<e.what()<<std::endl;window->hide();app->quit();return false;}},700);
  });
  int one=1;app->run(one,argv);timer.disconnect();window.reset();return result;
}

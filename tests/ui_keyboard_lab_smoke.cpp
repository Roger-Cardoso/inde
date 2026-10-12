#include "inde/experimental/blocks_lab_window.hpp"
#include "inde/ui/application_theme.hpp"
#include "../src/ui/rendering_policy.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
namespace {
ui::KeyboardCommands &registry(Gtk::Window &window) {
  auto *items=gtk_widget_observe_controllers(GTK_WIDGET(window.gobj()));ui::KeyboardCommands *found{};
  for(guint i=0;i<g_list_model_get_n_items(items);++i) {
    auto *item=G_OBJECT(g_list_model_get_item(items,i));
    if(auto *owner=g_object_get_data(item,"inde-router-owner"))found=static_cast<ui::KeyboardCommands *>(owner);
    g_object_unref(item);
  }
  g_object_unref(items);require(found,"Laboratory router missing");return *found;
}
}
int main(int argc,char **argv) {
  if(argc!=3 || std::string(argv[2])!="--allow-fixture-writes")return 2;
  ui::configure_rendering_backend();
  auto app=Gtk::Application::create("io.github.inde.keyboard-lab-probe",Gio::Application::Flags::NON_UNIQUE);
  app->signal_startup().connect([]{ui::install_application_theme();});
  std::unique_ptr<experimental::BlocksLabWindow> window;sigc::connection timer;int phase{},result=1;
  std::string capture_stage;
  auto captured=[&](const std::string &stage) {
    if(capture_stage!=stage){capture_stage=stage;std::cout<<"SNAPSHOT: "<<stage<<std::endl;return false;}
    const auto *directory=std::getenv("INDE_UI_CAPTURE_ACK_DIR");
    return !directory || std::filesystem::exists(std::filesystem::path(directory)/stage);
  };
  app->signal_activate().connect([&] {
    window=std::make_unique<experimental::BlocksLabWindow>(std::filesystem::path(argv[1]));app->add_window(*window);window->present();
    timer=Glib::signal_timeout().connect([&] {try {
      if(!window->get_mapped())return true;
      auto &w=*window;auto &keys=registry(w);
      switch(phase) {
      case 0:
        require(keys.commands().size()>=35,"Laboratory catalog incomplete");
        for(const auto &c:keys.commands())std::cout<<"COMMAND: "<<c.id<<"|"<<keys.binding(c)<<"|"<<c.label<<"|"<<c.group<<'\n';
        require(keys.dispatch(GDK_KEY_p,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK|GDK_SHIFT_MASK)),"Laboratory palette chord missing");break;
      case 1: {
        auto *popover=find<Gtk::Popover>(w,[](auto &p){return p.get_mapped();});require(popover,"Laboratory palette missing");
        if(!captured("keyboard-lab-palette"))return true;
        popover->popdown();break;
      }
      case 2: {
        auto *entry=find<Gtk::Entry>(w,[](auto &e){return e.get_mapped();});require(entry,"Laboratory title entry missing");entry->grab_focus();
        require(!keys.dispatch(GDK_KEY_z,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK)),"Laboratory undo stole native field");
        auto *text=find<Gtk::Widget>(*entry,[](auto &x){return GTK_IS_TEXT(x.gobj());});require(text,"Entry text missing");
        g_object_set_data(G_OBJECT(text->gobj()),"inde-preedit-active",GINT_TO_POINTER(1));
        require(!keys.dispatch(GDK_KEY_p,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK|GDK_SHIFT_MASK)),"Lab interrupted input composition");
        g_object_set_data(G_OBJECT(text->gobj()),"inde-preedit-active",nullptr);
        require(keys.dispatch(GDK_KEY_F1,static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK)),"Laboratory help missing");break;
      }
      case 3: {
        require(find<Gtk::SearchEntry>(w,[](auto &s){return s.get_mapped();}),"Laboratory help not displayed");
        if(!captured("keyboard-lab-help"))return true;
        auto *popover=find<Gtk::Popover>(w,[](auto &p){return p.get_mapped();});require(popover,"Lab help popover missing");popover->popdown();break;
      }
      case 4:
        require(ui::text_input_focus(w.get_focus()),"Laboratory failed to restore field focus");
        require(keys.dispatch(GDK_KEY_F6,static_cast<Gdk::ModifierType>(0)),"Laboratory region cycle missing");
        std::cout<<"PASS keyboard GTK laboratory 8 catalog, scope, IME, palette, help and focus checks\n"<<std::flush;
        result=0;w.hide();app->quit();return false;
      }
      ++phase;return true;
    }catch(const std::exception &e){std::cerr<<"FAIL keyboard laboratory phase="<<phase<<": "<<e.what()<<std::endl;window->hide();app->quit();return false;}},700);
  });
  int one=1;app->run(one,argv);timer.disconnect();window.reset();return result;
}

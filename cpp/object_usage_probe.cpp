#include "main_window_smoke.hpp"
#include "inde/project/document_paragraphs.hpp"
#include "inde/ui/main_window.hpp"
#include "object_native_probe.hpp"
#include "object_native_ui.hpp"
#include <cmath>
#include <fstream>
#include <gdk/x11/gdkx.h>
#include <unistd.h>

namespace {
using V = inde::persistence::json::Value;
using O = V::Object;
template<class T> T *locate(Gtk::Widget &w) {
  if (auto *p = dynamic_cast<T *>(&w)) return p;
  for (auto *c=w.get_first_child();c;c=c->get_next_sibling())
    if (auto *p=locate<T>(*c)) return p;
  return nullptr;
}
}

// Isolated input-test host. The probe observes widgets; all mutations in the
// driver come through X11 pointer/keyboard events, including focus and dialogs.
int run_object_usage_probe(const Glib::RefPtr<Gtk::Application> &app,
                          const std::filesystem::path &project,
                          const std::filesystem::path &state_file) {
  if(std::filesystem::exists(project))
    throw std::runtime_error("O teste de uso exige um destino novo e descartável.");
  std::unique_ptr<inde::ui::MainWindow> main;
  sigc::connection poll;
  app->signal_activate().connect([&] {
    main=std::make_unique<inde::ui::MainWindow>(std::nullopt);
    app->add_window(*main);main->unmaximize();main->set_default_size(1190,750);
    main->present();
    poll=Glib::signal_timeout().connect([&] {
      if(!main->gobj() || !GTK_IS_WINDOW(main->gobj()) || !main->get_visible())return false;
      O state{{"pid",static_cast<std::int64_t>(::getpid())}};
      V::Array controls;
      std::function<void(Gtk::Widget &)> visit=[&](Gtk::Widget &w) {
        std::string label;
        if(auto *b=dynamic_cast<Gtk::Button *>(&w)) {
          label=b->get_label();
          if(label.empty()) {
            std::function<void(GtkWidget *)> text=[&](GtkWidget *v) {
              if(GTK_IS_LABEL(v) && label.empty())label=gtk_label_get_text(GTK_LABEL(v));
              for(auto *c=gtk_widget_get_first_child(v);c;c=gtk_widget_get_next_sibling(c))text(c);
            };
            text(w.gobj());
          }
        }
        if(auto *b=dynamic_cast<Gtk::CheckButton *>(&w))label=b->get_label();
        const auto name=w.get_name().raw();
        if(name.starts_with("native-"))if(auto *l=dynamic_cast<Gtk::Label *>(&w))label=l->get_text();
        if(!label.empty() || dynamic_cast<Gtk::Entry *>(&w) || name.starts_with("native-") ||
           name.starts_with("specialty-") || name.starts_with("writing-paragraph-")) {
          graphene_rect_t bounds{};
          auto *coordinate_root=GTK_WIDGET(main->gobj());
          auto *native=gtk_widget_get_native(w.gobj());
          GdkSurface *popup_surface=nullptr;
          if(native && GTK_IS_POPOVER(native)) {
            coordinate_root=GTK_WIDGET(native);
            popup_surface=gtk_native_get_surface(native);
          }
          const bool placed=gtk_widget_compute_bounds(w.gobj(),coordinate_root,&bounds);
          O c{{"name",name},{"label",label},{"mapped",w.get_mapped()},
              {"sensitive",bool(gtk_widget_is_sensitive(w.gobj()))},
              {"focus",w.is_focus()}};
          if(auto *b=dynamic_cast<Gtk::ToggleButton *>(&w))c["active"]=b->get_active();
          if(auto *entry=dynamic_cast<Gtk::Entry *>(&w)) {
            c["role"]="entry";c["value"]=entry->get_text().raw();
          }
          if(placed) {
            if(popup_surface && GDK_IS_X11_SURFACE(popup_surface))
              c["window"]=static_cast<std::int64_t>(gdk_x11_surface_get_xid(popup_surface));
            auto *picked=gtk_widget_pick(coordinate_root,
                bounds.origin.x+bounds.size.width/2,
                bounds.origin.y+bounds.size.height/2,GTK_PICK_DEFAULT);
            c["hittable"]=bool(picked &&
                (picked==w.gobj() || gtk_widget_is_ancestor(picked,w.gobj())));
            c["x"]=static_cast<std::int64_t>(std::lround(bounds.origin.x));
            c["y"]=static_cast<std::int64_t>(std::lround(bounds.origin.y));
            c["width"]=static_cast<std::int64_t>(std::lround(bounds.size.width));
            c["height"]=static_cast<std::int64_t>(std::lround(bounds.size.height));
          }
          controls.emplace_back(std::move(c));
        }
        if(name=="writing-text-editor") {
          const auto b=dynamic_cast<Gtk::TextView &>(w).get_buffer();
          state["content"]=b->get_text().raw();
          state["cursor"]=b->get_iter_at_mark(b->get_insert()).get_offset();
          state["bound"]=b->get_iter_at_mark(b->get_selection_bound()).get_offset();
        }
        if(name=="native-status")state["status"]=dynamic_cast<Gtk::Label &>(w).get_text().raw();
        for(auto *c=w.get_first_child();c;c=c->get_next_sibling())visit(*c);
      };
      visit(*main);
      if(auto *writing=locate<inde::ui::WritingWorkspace>(*main))
        state["composing"]=writing->composing_input();
      // Native file dialogs are separate GtkWindows, outside the app widget
      // tree. Observe their controls too, without setting chooser responses.
      auto *tops=gtk_window_get_toplevels();
      for(guint n=0;n<g_list_model_get_n_items(tops);++n) {
        auto *top=GTK_WINDOW(g_list_model_get_item(tops,n));
        if(top!=GTK_WINDOW(main->gobj()) && gtk_widget_get_mapped(GTK_WIDGET(top))) {
          auto *surface=gtk_native_get_surface(GTK_NATIVE(top));
          const auto xid=GDK_IS_X11_SURFACE(surface)?gdk_x11_surface_get_xid(surface):0;
          std::function<void(GtkWidget *)> inspect=[&](GtkWidget *widget) {
            const char *caption=GTK_IS_BUTTON(widget)?gtk_button_get_label(GTK_BUTTON(widget)):nullptr;
            if(gtk_widget_get_mapped(widget) && ((caption && *caption) || GTK_IS_EDITABLE(widget))) {
              graphene_rect_t b{};
              if(gtk_widget_compute_bounds(widget,GTK_WIDGET(top),&b)) {
                auto *hit=gtk_widget_pick(GTK_WIDGET(top),b.origin.x+b.size.width/2,b.origin.y+b.size.height/2,GTK_PICK_DEFAULT);
                O c{{"name",gtk_widget_get_name(widget)},{"label",caption?caption:""},
                    {"mapped",true},{"sensitive",bool(gtk_widget_is_sensitive(widget))},
                    {"focus",gtk_root_get_focus(GTK_ROOT(top)) == widget},
                    {"hittable",bool(hit && (hit==widget || gtk_widget_is_ancestor(hit,widget)))},
                    {"window",static_cast<std::int64_t>(xid)},
                    {"x",static_cast<std::int64_t>(std::lround(b.origin.x))},
                    {"y",static_cast<std::int64_t>(std::lround(b.origin.y))},
                    {"width",static_cast<std::int64_t>(std::lround(b.size.width))},
                    {"height",static_cast<std::int64_t>(std::lround(b.size.height))}};
                if(GTK_IS_EDITABLE(widget))c["value"]=gtk_editable_get_text(GTK_EDITABLE(widget));
                controls.emplace_back(std::move(c));
              }
            }
            for(auto *child=gtk_widget_get_first_child(widget);child;child=gtk_widget_get_next_sibling(child))inspect(child);
          };
          inspect(GTK_WIDGET(top));
        }
        g_object_unref(top);
      }
      state["controls"]=std::move(controls);
      if(auto *objects=locate<inde::ui::ObjectDocumentEditor>(*main)) {
        state["document"]=inde::ui::ObjectNativeProbe::state(*objects).at("source_document");
        state["busy"]=inde::ui::ObjectNativeProbe::busy(*objects);
        state["tools_open"]=inde::ui::ObjectNativeProbe::tools_open(*objects);
        state["draft"]=inde::ui::ObjectNativeProbe::draft(*objects);
        state["error"]=inde::ui::ObjectNativeProbe::error(*objects);
        state["objects"]=inde::ui::ObjectNativeProbe::state(*objects);
      }
      const auto tmp=state_file.string()+".tmp";
      {std::ofstream out(tmp);out<<inde::persistence::json::serialize(state)<<'\n';}
      std::filesystem::rename(tmp,state_file);
      return true;
    },100,Glib::PRIORITY_LOW);
  });
  const auto result=app->run();poll.disconnect();main.reset();return result;
}

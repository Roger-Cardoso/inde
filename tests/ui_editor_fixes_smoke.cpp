#include "../src/ui/rendering_policy.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <gdk/x11/gdkx.h>
#include <iostream>
#include <set>
using namespace inde;
using namespace inde::test::ui;
class FixesWindow final : public Gtk::ApplicationWindow {
public:
  FixesWindow(const std::filesystem::path &path, int &result)
      : writing_(service_), result_(result) {
    service_.open(path);
    auto d =
        service_.writing().create_document("QA — seleção e comandos inline");
    d.content =
        "Água 🙂: seleção deve continuar visível ao consultar o dicionário.";
    for (int i = 0; i < 30; ++i)
      d.inline_commands.push_back(
          {"@", "modelo" + std::to_string(i),
           "Modelo " + std::to_string(i) + " — " + std::string(120, 'x'),
           "Conteúdo literal " + std::to_string(i)});
    baseline_ = service_.writing().update_document(d);
    host_.set_content(writing_);
    set_child(host_);
    set_default_size(1000, 720);
    add_css_class("inde-window");
    writing_.install_actions(*this);
    writing_.refresh();
    writing_.reveal_document(d.id);
    editor_ =
        find<Gtk::TextView>(writing_, [](auto &v) { return v.get_editable(); });
    require(editor_, "Editor missing");
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL editor fixes phase=" << phase_ << ": "
                      << e.what() << std::endl;
            hide();
            return false;
          }
        },
        850);
  }
  ~FixesWindow() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  project::Document baseline_;
  int &result_;
  int phase_{};
  int scroll_attempts_{};
  bool dictionary_frame_{};
  Gtk::TextView *editor_{};
  sigc::connection timer_;
  Gtk::Box &modal() {
    auto *p = find<Gtk::Box>(host_, [](auto &b) {
      return b.get_mapped() && b.has_css_class("modal-card");
    });
    require(p, "Modal missing");
    return *p;
  }
  Gtk::Button &dictionary(Gtk::Widget &root) {
    auto *b = find<Gtk::Button>(root, [](auto &b) {
      return find<Gtk::Label>(b, [](auto &l) {
        return l.get_text() == "Consultar seleção no dicionário…";
      });
    });
    require(b, "Dictionary context item missing");
    return *b;
  }
  bool step() {
    std::cout << "STEP: " << phase_ << std::endl;
    switch (phase_) {
    case 0: {
      auto b = editor_->get_buffer();
      b->select_range(b->get_iter_at_offset(4), b->get_iter_at_offset(0));
      editor_->grab_focus();
      Gdk::Rectangle rect;
      editor_->get_iter_location(b->get_iter_at_offset(1), rect);
      int x{}, y{};
      editor_->buffer_to_window_coords(Gtk::TextWindowType::WIDGET,
                                       rect.get_x(), rect.get_y(), x, y);
      graphene_point_t a{static_cast<float>(x + 2), static_cast<float>(y + 5)},
          z{};
      require(gtk_widget_compute_point(GTK_WIDGET(editor_->gobj()),
                                       GTK_WIDGET(gobj()), &a, &z),
              "Editor coordinates missing");
      auto xid = std::to_string(gdk_x11_surface_get_xid(get_surface()->gobj()));
      Glib::spawn_async({},
                        {"xdotool", "windowfocus", "--sync", xid, "mousemove",
                         "--window", xid, std::to_string(static_cast<int>(z.x)),
                         std::to_string(static_cast<int>(z.y)), "click", "3"},
                        Glib::SpawnFlags::SEARCH_PATH);
      break;
    }
    case 1: {
      auto *p = find<Gtk::Popover>(writing_, [](auto &p) {
        return p.get_mapped() && p.has_css_class("context-menu");
      });
      require(p, "Physical right-click did not open context menu");
      require(!p->get_has_arrow(), "Context has surplus arrow");
      auto *s = find<Gtk::ScrolledWindow>(*p, [](auto &) { return true; });
      require(s && s->get_allocated_height() <= 370,
              "Context menu exceeds bounded contents");
      Gtk::TextIter a, z;
      require(editor_->get_buffer()->get_selection_bounds(a, z) &&
                  a.get_offset() == 0 && z.get_offset() == 4,
              "Right-click lost selected word");
      require(dictionary(*p).get_sensitive(), "Selected term unavailable");
      std::cout << "SNAPSHOT: context-selection" << std::endl;
      break;
    }
    case 2: {
      auto *p = find<Gtk::Popover>(writing_, [](auto &p) {
        return p.get_mapped() && p.has_css_class("context-menu");
      });
      require(p, "Context menu vanished");
      auto *s = find<Gtk::ScrolledWindow>(*p, [](auto &) { return true; });
      auto a = s->get_vadjustment();
      a->set_value(a->get_upper() - a->get_page_size());
      g_signal_emit_by_name(dictionary(*p).gobj(), "clicked");
      break;
    }
    case 3: {
      auto *q = find<Gtk::SearchEntry>(modal(), [](auto &q) {
        return q.get_placeholder_text() == "Palavra ou expressão exata";
      });
      require(q && q->get_text() == "Água", "Selection not used for query");
      require(find<Gtk::Label>(
                  modal(),
                  [](auto &l) {
                    return l.get_text().raw().find("BASE REVISADA") !=
                               std::string::npos ||
                           l.get_text().raw().find("Nenhum sentido da palavra") !=
                               std::string::npos;
                  }),
              "Explicit selection query not executed");
      std::cout << "SNAPSHOT: dictionary-selection" << std::endl;
      // A selected word may have no entry. Use a known catalog term to verify
      // disclosure without pretending that the selection had a definition.
      q->set_text("banco");
      click(modal(), "Consultar alternativas");
      auto *details = find<Gtk::Expander>(modal(), [](auto &e) {
        return e.get_label() == "Fontes e detalhes da revisão";
      });
      require(details && !details->get_expanded(),
              "Dictionary technical details are not progressively disclosed");
      require(find<Gtk::Label>(modal(), [](auto &l) {
        return l.has_css_class("heading") &&
               l.get_text().raw().find("Revisão da fonte pendente") != std::string::npos;
      }), "Pending source review is hidden inside technical details");
      require(details->get_child() && find<Gtk::Label>(*details->get_child(), [](auto &l) {
        return l.get_text().raw().find("UPOS:") != std::string::npos;
      }), "Technical dictionary metadata was lost");
      details->set_expanded(true);
      break;
    }
    case 4:
      if (!dictionary_frame_) {
        dictionary_frame_ = true;
        std::cout << "SNAPSHOT: dictionary-details" << std::endl;
        return true; // show the populated result after GTK has rendered it
      }
      click(modal(), "Fechar");
      break;
    case 5:
      require(gtk_widget_activate_action(GTK_WIDGET(gobj()),
                                         "win.writing-inline", nullptr),
              "Inline action missing");
      break;
    case 6: {
      auto &card = modal();
      require(!find<Gtk::Button>(card,
                                 [](auto &b) {
                                   static const std::set<std::string> duplicated{
                                     "Trechos marcados","Vincular elemento da história","Marcar seleção","Comentar seleção",
                                     "Marcações do texto","Adicionar entidade à seleção",
                                     "Associar trecho à ficha","Adicionar relação ao elemento da história do trecho",
                                     "Analisar distribuição semântica"};
                                   return duplicated.contains(b.get_label().raw());
                                 }),
              "Duplicated generic commands remain");
      auto *close = find<Gtk::Button>(
          card, [](auto &b) { return b.get_label() == "Fechar"; });
      require(close, "Footer close missing");
      graphene_rect_t bounds{};
      require(gtk_widget_compute_bounds(GTK_WIDGET(close->gobj()),
                                        GTK_WIDGET(gobj()), &bounds),
              "Footer coordinates unavailable");
      require(bounds.origin.y >= 0 && bounds.origin.y + bounds.size.height <=
                                          get_allocated_height(),
              "Footer clipped off screen");
      auto *s = find<Gtk::ScrolledWindow>(card, [](auto &s) {
        return find<Gtk::Box>(s, [](auto &b){return b.has_css_class("modal-body");})!=nullptr;
      });
      require(s, "Modal scroll missing");
      auto a = s->get_vadjustment();
      require(a->get_upper() > a->get_page_size(),
              "Long model list not scrollable");
      std::cout << "SNAPSHOT: inline-top" << std::endl;
      break;
    }
    case 7: {
      auto &card = modal();
      auto *manage = find<Gtk::Button>(card, [](auto &b) {
        return b.get_label() == "Gerenciar textos prontos";
      });
      require(manage, "Bottom command missing");
      auto *scroll=find<Gtk::ScrolledWindow>(card,[](auto &s){return find<Gtk::Box>(s,[](auto &b){return b.has_css_class("modal-body");})!=nullptr;});
      require(scroll,"Body scroll missing");
      auto a=scroll->get_vadjustment();
      std::cout<<"SCROLL value="<<a->get_value()<<" upper="<<a->get_upper()<<" page="<<a->get_page_size()<<std::endl;
      a->set_value(a->get_upper()-a->get_page_size());
      if(++scroll_attempts_<=3)return true; // Settle GTK's wrapping/allocation before measuring the final item.
      graphene_rect_t b{};
      require(gtk_widget_compute_bounds(GTK_WIDGET(manage->gobj()),
                                        GTK_WIDGET(gobj()), &b),
              "Bottom command has no bounds");
      graphene_rect_t viewport{};
      require(gtk_widget_compute_bounds(GTK_WIDGET(scroll->gobj()),GTK_WIDGET(gobj()),&viewport),"Scroll bounds missing");
      std::cout<<"BOTTOM item="<<b.origin.y<<"+"<<b.size.height<<" viewport="<<viewport.origin.y<<"+"<<viewport.size.height<<std::endl;
      require(b.origin.y >= viewport.origin.y &&
                  b.origin.y + b.size.height <= viewport.origin.y+viewport.size.height+1,
              "Bottom command hidden behind footer");
      std::cout << "SNAPSHOT: inline-bottom" << std::endl;
      break;
    }
    case 8:
      click(modal(), "Fechar");
      break;
    case 9: {
      auto b = editor_->get_buffer();
      b->place_cursor(b->end());
      editor_->grab_focus();
      auto xid = std::to_string(gdk_x11_surface_get_xid(get_surface()->gobj()));
      Glib::spawn_async({}, {"xdotool", "key", "--window", xid, "shift+F10"},
                        Glib::SpawnFlags::SEARCH_PATH);
      break;
    }
    case 10: {
      auto *p = find<Gtk::Popover>(writing_, [](auto &p) {
        return p.get_mapped() && p.has_css_class("context-menu");
      });
      require(p, "Keyboard context menu missing");
      require(!dictionary(*p).get_sensitive(),
              "Dictionary enabled without selection");
      p->popdown();
      break;
    }
    case 11:
      writing_.flush_changes();
      require(project::same_document_state(
                  *service_.writing().document(baseline_.id), baseline_),
              "Menu or query changed authorial Document");
      result_ = 0;
      std::cout << "PASS editor fixes: physical right-click preserves "
                   "selection, dictionary lookup read-only, bounded single "
                   "context surface, inline list/footer reachable"
                << std::endl;
      hide();
      return false;
    }
    ++phase_;
    return true;
  }
};
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes")
    return 2;
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.editor-fixes-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  int result = 1;
  FixesWindow *window{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    window = new FixesWindow(argv[1], result);
    app->add_window(*window);
    window->present();
  });
  char *args[] = {argv[0], nullptr};
  app->run(1, args);
  delete window;
  return result;
}

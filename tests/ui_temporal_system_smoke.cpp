#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/calendar_picker.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <algorithm>
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class TemporalWindow final : public Gtk::ApplicationWindow {
public:
  TemporalWindow(const std::filesystem::path &p, int &result, bool restart)
      : planning_(service_), result_(result), restart_(restart) {
    service_.open(p);
    host_.set_content(planning_);
    set_child(host_);
    set_default_size(1000, 720);
    add_css_class("inde-window");
    if (!restart_) {
      axis_ = service_.planning().create_time_axis("QA — Tempo Terra");
      point_ = service_.planning().create_time_point(axis_.id, 2299161,
                                                     "QA — Corte");
    } else {
      for (const auto &a : service_.planning().time_axes())
        if (a.name == "QA — Tempo Terra")
          axis_ = a;
      require(!axis_.id.empty(), "Restart axis absent");
    }
    planning_.refresh();
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL temporal phase=" << phase_ << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        800);
  }
  ~TemporalWindow() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::PlanningWorkspace planning_;
  ui::ModalOverlayHost host_;
  int &result_;
  bool restart_;
  int phase_{};
  bool months_bottom_{}, clock_bottom_{};
  sigc::connection timer_;
  project::FictionalTimeAxis axis_;
  project::FictionalTimePoint point_;
  ui::TemporalLibraryPage &library() {
    auto *v =
        find<ui::TemporalLibraryPage>(planning_, [](auto &) { return true; });
    require(v, "Library missing");
    return *v;
  }
  ui::CalendarPicker &picker(Gtk::Widget &root) {
    auto *p =
        find<ui::CalendarPicker>(root, [](auto &p) { return p.get_mapped(); });
    require(p, "Graphic picker missing");
    return *p;
  }
  template <class T> T &field(Gtk::Widget &root, const std::string &text) {
    auto *l = find<Gtk::Label>(root, [&](auto &l) {
      return l.get_text().raw() == text && l.get_mnemonic_widget();
    });
    require(l, "Missing field " + text);
    auto *w = dynamic_cast<T *>(l->get_mnemonic_widget());
    require(w, "Wrong field type " + text);
    return *w;
  }
  Gtk::Box &items(const std::string &name) {
    auto *b = find<Gtk::Box>(library(), [&](auto &b) {
      auto *l = dynamic_cast<Gtk::Label *>(b.get_first_child());
      return l && l->get_text().raw() == name;
    });
    require(b, "Missing items " + name);
    return *b;
  }
  void row(Gtk::Widget &root, int index) {
    auto *list = find<Gtk::ListBox>(root, [](auto &) { return true; });
    require(list && list->get_row_at_index(index), "Missing row");
    list->select_row(*list->get_row_at_index(index));
  }
  void footer() {
    auto *save = find<Gtk::Button>(library(), [](auto &b) {
      return b.get_visible() && b.has_css_class("suggested-action");
    });
    require(save, "Save missing");
    graphene_rect_t r{};
    require(gtk_widget_compute_bounds(GTK_WIDGET(save->gobj()),
                                      GTK_WIDGET(gobj()), &r) &&
                r.origin.y >= 0 &&
                r.origin.y + r.size.height <= get_allocated_height(),
            "Save footer clipped");
  }
  project::FictionalCalendar cal(const std::string &name) {
    for (auto c : service_.worldbuilding().calendars())
      if (c.name == name)
        return c;
    throw std::runtime_error("Calendar absent " + name);
  }
  void snap(const std::string &name) {
    std::cout << "SNAPSHOT: " << name << std::endl;
  }
  bool step() {
    auto &v = library();
    std::cout << "STEP: " << phase_ << std::endl;
    if (restart_) {
      if (phase_ == 0) {
        require(service_.worldbuilding().adoptions(axis_.id).size() == 1,
                "Restart adoption lost");
        require(service_.worldbuilding().temporal_references(axis_.id).size() ==
                    1,
                "Restart reference lost");
        click(planning_, "Tempo");
      }
      if (phase_ == 1) {
        click(v, "Uso, mudanças e datas…");
      }
      if (phase_ == 2) {
        click(v, "Datas e horários — QA — Tempo Terra");
      }
      if (phase_ == 3) {
        auto r = service_.worldbuilding().temporal_references(axis_.id).front();
        require(r.clock == std::vector<int>({7, 11}), "Restart clock lost");
        snap("restart");
      }
      if (phase_ == 4) {
        result_ = 0;
        std::cout << "PASS temporal restart\n";
        hide();
        return false;
      }
      ++phase_;
      return true;
    }
    if (phase_ == 0)
      click(planning_, "Tempo");
    if (phase_ == 1) {
      require(v.get_mapped(), "Direct Tempo entry not mapped");
      snap("library");
      click(v, "Usar modelo real…");
    }
    if (phase_ == 2)
      click(v, "Gregoriano proléptico");
    if (phase_ == 3) {
      field<Gtk::Entry>(v, "Nome do calendário e relógio").set_text("QA — Gregoriano");
      footer();
      click(v, "Salvar calendário e relógio");
    }
    if (phase_ == 4) {
      cal("QA — Gregoriano");
      click(v, "Usar modelo real…");
    }
    if (phase_ == 5)
      click(v, "Juliano proléptico");
    if (phase_ == 6) {
      field<Gtk::Entry>(v, "Nome do calendário e relógio").set_text("QA — Juliano");
      click(v, "Salvar calendário e relógio");
    }
    if (phase_ == 7)
      click(v, "Criar calendário e relógio próprios…");
    if (phase_ == 8) {
      require(!find<Gtk::TextView>(v, [](auto &) { return true; }),
              "Calendar still uses text syntax");
      field<Gtk::Entry>(v, "Nome do calendário e relógio").set_text("QA — Vigílias");
      auto &box = items("Meses");
      row(box, 0);
      field<Gtk::Entry>(box, "Nome do mês").set_text("Aurora");
      field<Gtk::Entry>(box, "Dias do mês").set_text("30");
      click(box, "Atualizar");
      field<Gtk::Entry>(box, "Nome do mês").set_text("Bruma");
      field<Gtk::Entry>(box, "Dias do mês").set_text("20");
      click(box, "Adicionar");
      snap("months-crud");
    }
    if (phase_ == 9) {
      if (!months_bottom_) {
        auto *scroll = find<Gtk::ScrolledWindow>(
            v, [](auto &s) { return s.get_vexpand(); });
        require(scroll, "Page scroll absent");
        const auto a = scroll->get_vadjustment();
        a->set_value(a->get_upper() - a->get_page_size());
        months_bottom_ = true;
        snap("months-crud-bottom");
        return true;
      }
      pages(v, "clock").set_visible_child("clock");
    }
    if (phase_ == 10) {
      auto &box = items("Relógio e nomes de medidas");
      row(box, 0);
      field<Gtk::Entry>(box, "Nome singular").set_text("vigília");
      field<Gtk::Entry>(box, "Nome plural").set_text("vigílias");
      field<Gtk::Entry>(box, "Abreviação").set_text("v");
      field<Gtk::Entry>(box, "Quantidade na unidade anterior").set_text("8");
      click(box, "Atualizar");
      row(box, 1);
      field<Gtk::Entry>(box, "Nome singular").set_text("pulso");
      field<Gtk::Entry>(box, "Nome plural").set_text("pulsos");
      field<Gtk::Entry>(box, "Abreviação").set_text("p");
      field<Gtk::Entry>(box, "Quantidade na unidade anterior").set_text("12");
      click(box, "Atualizar");
      row(box, 2);
      click(box, "Remover");
      footer();
      snap("clock-crud");
    }
    if (phase_ == 11) {
      if (!clock_bottom_) {
        auto *scroll = find<Gtk::ScrolledWindow>(
            v, [](auto &s) { return s.get_vexpand(); });
        require(scroll, "Page scroll absent");
        const auto a = scroll->get_vadjustment();
        a->set_value(a->get_upper() - a->get_page_size());
        clock_bottom_ = true;
        snap("clock-crud-bottom");
        return true;
      }
      click(v, "Salvar calendário e relógio");
    }
    if (phase_ == 12) {
      auto c = cal("QA — Vigílias");
      require(c.clock.size() == 2 && c.months.size() == 2, "CRUD not saved");
      click(v, "Uso, mudanças e datas…");
    }
    if (phase_ == 13)
      click(v, "Adoções — QA — Tempo Terra");
    if (phase_ == 14) {
      field<Gtk::ComboBoxText>(v, "Calendário inicial")
          .set_active_id(cal("QA — Juliano").id);
      field<Gtk::ComboBoxText>(v, "Marco que corresponde à data").set_active_id(point_.id);
      picker(v).show_month({1582, 10, 1});
      click(picker(v), "5");
      auto *ack = find<Gtk::CheckButton>(v, [](auto &b) {
        return b.get_label().raw().find("Declaro") != std::string::npos;
      });
      require(ack, "Initial measure ack missing");
      ack->set_active(true);
      click(v, "Confirmar calendário inicial");
    }
    if (phase_ == 15) {
      field<Gtk::ComboBoxText>(v, "Novo calendário")
          .set_active_id(cal("QA — Gregoriano").id);
      picker(v).show_month({1582, 10, 1});
      click(picker(v), "5");
      field<Gtk::ComboBoxText>(v, "Como o calendário passa a ser usado").set_active_id("coexist");
    }
    if (phase_ == 16) {
      auto *ack = find<Gtk::CheckButton>(v, [](auto &b) {
        return b.get_label().raw().find("Conferi a prévia") !=
               std::string::npos;
      });
      require(ack, "Cutover ack missing");
      ack->set_active(true);
      footer();
      snap("cutover-preview");
    }
    if (phase_ == 17)
      click(v, "Confirmar adoção");
    if (phase_ == 18) {
      require(service_.worldbuilding().adoptions(axis_.id).size() == 1,
              "Cutover not saved");
      require(service_.worldbuilding().parallel_dates(point_).size() == 2,
              "Coexistence missing");
      require(service_.worldbuilding().point_date(point_) ==
                  "15 de Outubro, ano 1582",
              "Cutover day mismatch");
      click(v, "Calendários e relógios");
    }
    if (phase_ == 19)
      click(v, "Uso, mudanças e datas…");
    if (phase_ == 20)
      click(v, "Datas e horários — QA — Tempo Terra");
    if (phase_ == 21)
      click(v, "Adicionar data ou horário…");
    if (phase_ == 22) {
      field<Gtk::Entry>(v, "Nome da data ou período").set_text("QA — Chegada");
      field<Gtk::ComboBoxText>(v, "Calendário da data de origem")
          .set_active_id(cal("QA — Vigílias").id);
      picker(v).set_selection(project::TemporalBoundary{12, 1, 3, {7, 11}});
      footer();
      snap("date-clock");
    }
    if (phase_ == 23)
      click(v, "Salvar data e horário");
    if (phase_ == 24) {
      auto refs = service_.worldbuilding().temporal_references(axis_.id);
      require(refs.size() == 1 && refs[0].clock == std::vector<int>({7, 11}),
              "Authorial reference missing");
      snap("references");
    }
    if (phase_ == 25)
      click(v, "Calendários e relógios");
    if (phase_ == 26)
      click(v, "Uso, mudanças e datas…");
    if (phase_ == 27)
      click(v, "Conferir datas paralelas — QA — Tempo Terra");
    if (phase_ == 28)
      field<Gtk::ComboBoxText>(v, "Marco da cronologia")
          .set_active_id(point_.id);
    if (phase_ == 29) {
      require(find<Gtk::Label>(v,
                               [](auto &l) {
                                 auto text = l.get_text().raw();
                                 return text.find("QA — Juliano") !=
                                            std::string::npos &&
                                        text.find("QA — Gregoriano") !=
                                            std::string::npos;
                               }),
              "Parallel presentations not visible");
      snap("parallel-dates");
    }
    if (phase_ == 30)
      planning_.reveal_time_point(point_.id);
    if (phase_ == 31)
      click(planning_, "Novo ponto");
    if (phase_ == 32) {
      auto *modal = find<Gtk::Box>(host_, [](auto &b) {
        return b.get_mapped() && b.has_css_class("modal-card");
      });
      require(modal, "New dated point modal absent");
      picker(*modal).show_month({1582, 10, 1});
      click(picker(*modal), "16");
      field<Gtk::Entry>(*modal, "Nome do marco").set_text("QA — Depois");
      snap("dated-point-successor");
    }
    if (phase_ == 33)
      click(host_, "Criar ponto por data");
    if (phase_ == 34) {
      const auto points = service_.planning().time_points(axis_.id);
      auto p = std::find_if(points.begin(), points.end(), [](const auto &p) {
        return p.label == "QA — Depois";
      });
      require(p != points.end() && p->ordinal == 2299162,
              "Successor dated point not saved");
    }
    if (phase_ == 35) {
      auto *menu = find<Gtk::MenuButton>(planning_, [](auto &b) {
        return b.get_tooltip_text() == "Ações da linha do tempo atual";
      });
      require(menu, "Axis actions missing");
      menu->popup();
    }
    if (phase_ == 36)
      click(planning_, "Ver mês do calendário…");
    if (phase_ == 37) {
      auto *modal = find<Gtk::Box>(host_, [](auto &b) {
        return b.get_mapped() && b.has_css_class("modal-card");
      });
      require(modal, "Calendar consultation missing");
      picker(*modal).show_month({1582, 10, 1});
      auto *day = find<Gtk::Button>(*modal, [](auto &b) {
        return b.has_css_class("calendar-day") &&
               b.get_label().raw().starts_with("16 ·");
      });
      require(day, "Day marker missing");
      g_signal_emit_by_name(day->gobj(), "clicked");
      require(
          find<Gtk::Button>(
              *modal, [](auto &b) { return b.get_label() == "QA — Depois"; }),
          "Daily record missing");
      snap("adoption-month-consultation");
    }
    if (phase_ == 38)
      click(host_, "Fechar");
    if (phase_ == 39) {
      result_ = 0;
      std::cout << "PASS temporal production UI CRUD, templates, clocks, "
                   "adoption, coexistence, date/time, footer\n";
      hide();
      return false;
    }
    ++phase_;
    return true;
  }
};
int main(int argc, char **argv) {
  if (argc != 3 || (std::string(argv[2]) != "--verify-restart" &&
                    std::string(argv[2]) != "--allow-fixture-writes"))
    return 2;
  auto app = Gtk::Application::create("io.github.inde.temporal-system-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::configure_rendering_backend();
  int result = 1;
  TemporalWindow *w{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    w = new TemporalWindow(argv[1], result,
                           std::string(argv[2]) == "--verify-restart");
    app->add_window(*w);
    w->present();
  });
  app->run();
  delete w;
  return result;
}

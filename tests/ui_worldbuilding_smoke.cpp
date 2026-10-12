#include "../src/ui/rendering_policy.hpp"
#include "inde/project/document_state.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class WorldWindow final : public Gtk::ApplicationWindow {
public:
  WorldWindow(const std::filesystem::path &path, int &result, bool restart)
      : planning_(service_), result_(result), restart_(restart) {
    service_.open(path);
    host_.set_content(planning_);
    set_child(host_);
    set_default_size(1000, 720);
    add_css_class("inde-window");
    if (!restart_) {
      a_ = service_.narrative().create_entity(
          "00000000-0000-4000-9000-000000000001", "QA — Mundo Ágata");
      b_ = service_.narrative().create_entity(a_.entity_type_id,
                                              "QA — Mundo Bento");
      project::EntityAlias alias;
      alias.entity_id = b_.id;
      alias.name = "Lua Água";
      alias.kind = "nickname";
      service_.worldbuilding().save_alias(alias);
      axis_ = service_.planning().create_time_axis("QA — Era do mundo");
      original_ = service_.planning().create_time_point(axis_.id, 77,
                                                        "QA — Marco original");
      baseline_ =
          service_.writing().create_document("QA — Mundo texto intacto");
      baseline_.content =
          "Lua Água 🙂 continua uma escolha autoral, sem binding implícito.";
      baseline_ = service_.writing().update_document(baseline_);
    }
    planning_.refresh();
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL worldbuilding phase=" << phase_ << ": "
                      << e.what() << std::endl;
            hide();
            return false;
          }
        },
        900);
  }
  ~WorldWindow() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::PlanningWorkspace planning_;
  ui::ModalOverlayHost host_;
  int &result_;
  bool restart_{};
  int phase_{};
  int month_scroll_attempts_{};
  sigc::connection timer_;
  project::NarrativeEntity a_, b_;
  project::FictionalTimeAxis axis_;
  project::FictionalTimePoint original_, dated_;
  project::Document baseline_;
  Gtk::Box &modal() {
    auto *box = find<Gtk::Box>(host_, [](auto &b) {
      return b.get_mapped() && b.has_css_class("modal-card");
    });
    require(box, "Modal missing");
    return *box;
  }
  template <class T> T &field(const std::string &label) {
    auto *l = find<Gtk::Label>(modal(), [&](auto &l) {
      return l.get_text().raw() == label && l.get_mnemonic_widget();
    });
    require(l, "Field missing: " + label);
    auto *v = dynamic_cast<T *>(l->get_mnemonic_widget());
    require(v, "Field type wrong: " + label);
    return *v;
  }
  void action(const std::string &tooltip, const std::string &label) {
    auto *menu = find<Gtk::MenuButton>(planning_, [&](auto &m) {
      return m.get_tooltip_text().raw() == tooltip;
    });
    require(menu && menu->get_sensitive() && menu->get_popover(),
            "Menu unavailable: " + tooltip);
    click(*menu->get_popover(), label);
  }
  bool contains(const std::string &text) {
    return find<Gtk::Label>(modal(), [&](auto &l) {
             return l.get_text().raw().find(text) != std::string::npos;
           }) != nullptr;
  }
  Gtk::CheckButton &ack() {
    auto *b = find<Gtk::CheckButton>(modal(), [](auto &b) {
      return b.get_label().raw().find("Conferi a interpretação") !=
             std::string::npos;
    });
    require(b, "Acknowledgement missing");
    return *b;
  }
  void footer_bounds() {
    auto *close = find<Gtk::Button>(
        modal(), [](auto &b) { return b.get_label() == "Fechar"; });
    require(close, "Close missing");
    graphene_rect_t r{};
    require(gtk_widget_compute_bounds(GTK_WIDGET(close->gobj()),
                                      GTK_WIDGET(gobj()), &r),
            "No footer bounds");
    require(r.origin.y >= 0 &&
                r.origin.y + r.size.height <= get_allocated_height(),
            "Footer clipped");
  }
  void in_view(Gtk::Widget &item, Gtk::ScrolledWindow &scroll) {
    graphene_rect_t r{}, viewport{};
    require(gtk_widget_compute_bounds(GTK_WIDGET(item.gobj()),
                                      GTK_WIDGET(gobj()), &r) &&
                gtk_widget_compute_bounds(GTK_WIDGET(scroll.gobj()),
                                          GTK_WIDGET(gobj()), &viewport),
            "Scroll geometry missing");
    require(r.origin.y >= viewport.origin.y &&
                r.origin.y + r.size.height <=
                    viewport.origin.y + viewport.size.height + 1,
            "Last item hidden behind footer or outside scroll viewport");
  }
  bool step() {
    std::cout << "STEP: " << phase_ << std::endl;
    if (restart_) {
      persistence::EntityQuery q;
      q.search = "QA — Mundo Ágata";
      auto found = service_.narrative().entities(q);
      require(found.size() == 1, "Restart entity absent");
      a_ = found[0];
      require(service_.worldbuilding().aliases(a_.id).size() == 1,
              "Restart aliases absent");
      for (const auto &a : service_.planning().time_axes())
        if (a.name == "QA — Era do mundo")
          axis_ = a;
      auto b = service_.worldbuilding().binding(axis_.id);
      require(b && b->epoch_ordinal == 501, "Restart binding absent");
      auto c = service_.worldbuilding().calendar(b->calendar_id);
      require(c.months.size() == 3 && c.leap_cycle == 4,
              "Restart definition wrong");
      persistence::PlanningQuery points;
      points.search = "QA — Travessia";
      auto list = service_.planning().time_points(axis_.id, points);
      require(list.size() == 1, "Restart dated point absent");
      planning_.reveal_time_point(list[0].id);
      std::cout << "SNAPSHOT: world-restart" << std::endl;
      if (phase_++ == 0)
        return true;
      result_ = 0;
      std::cout << "PASS worldbuilding actual process restart: aliases, "
                   "immutable calendar, binding, dated point"
                << std::endl;
      hide();
      return false;
    }
    switch (phase_) {
    case 0:
      planning_.reveal_entity(a_.id);
      action("Ações do elemento da história", "Nomes alternativos…");
      break;
    case 1:
      click(modal(), "Adicionar alias");
      break;
    case 2:
      field<Gtk::Entry>("Nome alternativo").set_text("Lua Água");
      field<Gtk::ComboBoxText>("Tipo de nome alternativo").set_active_id("title");
      field<Gtk::Entry>("Nota de uso").set_text("Título usado no arquipélago");
      click(modal(), "Salvar nome alternativo");
      break;
    case 3:
      require(service_.worldbuilding().aliases(a_.id).size() == 1,
              "Alias not saved");
      footer_bounds();
      std::cout << "SNAPSHOT: aliases-manager" << std::endl;
      break;
    case 4:
      click(modal(), "Fechar");
      break;
    case 5:
      require(find<Gtk::Label>(planning_,
                               [](auto &l) {
                                 return l.get_text().raw().find(
                                            "Nomes alternativos: Lua Água") !=
                                        std::string::npos;
                               }),
              "Compact aliases absent");
      std::cout << "SNAPSHOT: aliases-profile" << std::endl;
      break;
    case 6:
      click(planning_, "← Voltar aos resultados");
      {
        auto *search = find<Gtk::Entry>(planning_, [](auto &e) {
          return e.get_placeholder_text() == "Pesquisar nome, alias ou resumo";
        });
        require(search, "Search missing");
        search->set_text("lua água");
      }
      break;
    case 7: {
      persistence::EntityQuery q;
      q.search = "lua água";
      require(service_.narrative().entity_count(q) == 2,
              "Ambiguous aliases not all returned");
      require(find<Gtk::Label>(planning_,
                               [](auto &l) {
                                 return l.get_text().raw().find(
                                            "nome alternativo") !=
                                        std::string::npos;
                               }),
              "Alias reason missing");
      std::cout << "SNAPSHOT: aliases-search" << std::endl;
      break;
    }
    case 8:
      planning_.reveal_time_point(original_.id);
      action("Ações da linha do tempo atual", "Calendário da linha do tempo…");
      break;
    case 9:
      click(modal(), "Criar definição de calendário…");
      break;
    case 10: {
      auto *library = find<ui::TemporalLibraryPage>(
          planning_, [](auto &p) { return p.get_mapped(); });
      require(library,
              "Legacy create action did not route to temporal library");
      // This driver covers aliases and legacy chronology/month views.
      // CRUD is now exercised by ui_temporal_system_smoke, not a removed modal.
      project::FictionalCalendar c;
      c.name = "QA — Calendário das marés";
      c.months = {{"Aurora", 30}, {"Bruma", 20}, {"Noite", 10}};
      c.weekdays = {"Luz", "Sombra", "Eco"};
      c.leap_cycle = 4;
      c.leap_month = 2;
      (void)service_.worldbuilding().create_calendar(c);
      planning_.reveal_time_point(original_.id);
      action("Ações da linha do tempo atual", "Calendário da linha do tempo…");
      break;
    }
    case 11: {
      for (const auto &c : service_.worldbuilding().calendars())
        if (c.name == "QA — Calendário das marés")
          field<Gtk::ComboBoxText>("Calendário do eixo").set_active_id(c.id);
      break;
    }
    case 12:
      require(!service_.worldbuilding().binding(axis_.id),
              "Definition auto-assigned");
      require(!ack().get_active(), "Acknowledgement inherited");
      field<Gtk::Entry>("Ordinal do ano 0, mês 1, dia 1").set_text("500");
      std::cout << "SNAPSHOT: calendar-preview" << std::endl;
      break;
    case 13:
      ack().set_active(true);
      click(modal(), "Confirmar calendário do eixo");
      break;
    case 14:
      require(service_.worldbuilding().binding(axis_.id)->epoch_ordinal == 500,
              "Binding not saved");
      require(!ack().get_active(), "Acknowledgement reused");
      require(service_.planning().time_point(original_.id)->ordinal == 77,
              "Existing chronology changed");
      footer_bounds();
      std::cout << "SNAPSHOT: calendar-assigned" << std::endl;
      break;
    case 15:
      click(modal(), "Fechar");
      click(planning_, "Novo ponto");
      break;
    case 16:
      field<Gtk::Entry>("Ano ficcional").set_text("4");
      field<Gtk::ComboBoxText>("Mês ficcional").set_active_id("2");
      field<Gtk::Entry>("Dia ficcional").set_text("22");
      field<Gtk::Entry>("Nome do marco").set_text("QA — Travessia");
      require(contains("Dia inexistente"), "Impossible date not refused");
      std::cout << "SNAPSHOT: calendar-invalid-date" << std::endl;
      break;
    case 17:
      field<Gtk::Entry>("Dia ficcional").set_text("21");
      click(modal(), "Criar ponto por data");
      break;
    case 18: {
      persistence::PlanningQuery q;
      q.search = "QA — Travessia";
      auto list = service_.planning().time_points(axis_.id, q);
      require(list.size() == 1, "Dated point not created");
      dated_ = list[0];
      require(service_.worldbuilding().point_date(dated_) ==
                  "21 de Bruma, ano 4",
              "Wrong date conversion");
      auto event = service_.narrative().create_entity(
          "00000000-0000-4000-9000-000000000003", "QA — Chegada");
      (void)service_.planning().place_event(event.id, dated_.id);
      action("Ações da linha do tempo atual", "Ver mês do calendário…");
      break;
    }
    case 19:
      field<Gtk::Entry>("Ano do mês").set_text("4");
      field<Gtk::ComboBoxText>("Mês para consulta").set_active_id("2");
      click(modal(), "Mostrar mês");
      break;
    case 20:
      require(contains("QA — Chegada") && contains("QA — Travessia"),
              "Month lacks actual facts");
      {
        auto *scroll = find<Gtk::ScrolledWindow>(modal(), [](auto &s) {
          return find<Gtk::Grid>(s, [](auto &) { return true; }) != nullptr &&
                 !find<Gtk::Box>(
                     s, [](auto &b) { return b.has_css_class("modal-body"); });
        });
        require(scroll, "Month scroll missing");
        const auto adjustment = scroll->get_vadjustment();
        adjustment->set_value(adjustment->get_upper() -
                              adjustment->get_page_size());
        if (++month_scroll_attempts_ <= 3)
          return true;
        auto *point = find<Gtk::Button>(*scroll, [](auto &b) {
          return find<Gtk::Label>(b, [](auto &l) {
                   return l.get_text() == "QA — Travessia";
                 }) != nullptr;
        });
        require(point, "Month origin button missing");
        in_view(*point, *scroll);
      }
      footer_bounds();
      std::cout << "SNAPSHOT: calendar-month" << std::endl;
      break;
    case 21:
      click(modal(), "QA — Travessia");
      break;
    case 22:
      action("Ações da linha do tempo atual", "Calendário da linha do tempo…");
      {
        auto old = service_.worldbuilding().binding(axis_.id);
        service_.worldbuilding().assign_calendar(axis_.id, old->calendar_id,
                                                 501, old);
        field<Gtk::Entry>("Ordinal do ano 0, mês 1, dia 1").set_text("500");
        ack().set_active(true);
        click(modal(), "Confirmar calendário do eixo");
        require(contains("Não confirmado") && !ack().get_active(),
                "Stale UI approval accepted");
        require(
            field<Gtk::Entry>("Ordinal do ano 0, mês 1, dia 1").get_text() ==
                "500",
            "Conflict discarded form");
        std::cout << "SNAPSHOT: calendar-conflict" << std::endl;
        break;
      }
    case 23:
      click(modal(), "Fechar");
      // Return from a temporal slice to the unscoped entity explorer using
      // the public control; this entity has no presence in that slice.
      click(planning_, "Limpar contexto");
      planning_.reveal_entity(a_.id);
      action("Ações do elemento da história", "Nomes alternativos…");
      break;
    case 24:
      click(modal(), "Adicionar alias");
      field<Gtk::Entry>("Nome alternativo").set_text(" lua Água ");
      click(modal(), "Salvar nome alternativo");
      break;
    case 25:
      require(contains("Não salvo") &&
                  contains("já está cadastrado nesta entidade") &&
                  service_.worldbuilding().aliases(a_.id).size() == 1,
              "Duplicate alias accepted");
      require(field<Gtk::Entry>("Nome alternativo").get_text() == " lua Água ",
              "Duplicate error discarded input");
      std::cout << "SNAPSHOT: alias-duplicate" << std::endl;
      break;
    case 26:
      click(modal(), "Fechar");
      break;
    case 27:
      require(project::same_document_state(
                  *service_.writing().document(baseline_.id), baseline_, true),
              "Aliases changed authorial text");
      require(service_.narrative().entity(a_.id)->name == a_.name,
              "Alias renamed canonical entity");
      require(service_.planning().time_point(original_.id)->ordinal == 77,
              "Calendar moved old point");
      result_ = 0;
      std::cout << "PASS worldbuilding public GTK: "
                   "aliases/search/ambiguity/duplicate, calendar "
                   "definition/preview/ack/date/month/stale conflict, original "
                   "text and chronology preserved"
                << std::endl;
      hide();
      return false;
    }
    ++phase_;
    return true;
  }
};
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  const std::string mode = argv[2];
  if (mode != "--allow-fixture-writes" && mode != "--verify-restart")
    return 2;
  auto app = Gtk::Application::create("io.github.inde.worldbuilding-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  ui::configure_rendering_backend();
  int result = 1;
  WorldWindow *window{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    window = new WorldWindow(argv[1], result, mode == "--verify-restart");
    app->add_window(*window);
    window->present();
  });
  char *args[] = {argv[0], nullptr};
  app->run(1, args);
  delete window;
  return result;
}

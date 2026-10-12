#include "../src/ui/rendering_policy.hpp"
#include "inde/application/project_service.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/cartography_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/temporal_library_page.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &path, int &result, bool restart)
      : temporal_(service_), map_(service_), result_(result),
        restart_(restart) {
    service_.open(path);
    add_css_class("inde-window");
    host_.set_content(pages_);
    pages_.add(temporal_, "time", "Tempo");
    pages_.add(map_, "map", "Mapa");
    set_child(host_);
    set_default_size(1100, 760);
    if (!restart_) {
      auto c = project::calendar_template("gregorian");
      c.name = "QA Sistema descartável";
      calendar_ = service_.worldbuilding().create_calendar(c);
      c.name = "QA Sistema em uso";
      blocked_ = service_.worldbuilding().create_calendar(c);
      auto axis = service_.planning().create_time_axis("QA Exclusão");
      service_.worldbuilding().assign_calendar(axis.id, blocked_.id, 0, {});
      local_ = service_.narrative()
                   .create_entity("00000000-0000-4000-9000-000000000002",
                                  "QA Local preservado", "")
                   .id;
      project::geo::Planet p;
      p.id = project::new_uuid();
      p.name = "QA Mapa descartável";
      p.created_at = project::utc_now();
      p.detail = 0;
      service_.cartography().accept(project::geo::generate(p));
      planet_ = p.id;
      service_.cartography().change_position(
          p.id, local_, {}, project::geo::Position{p.id, local_, {10, 20}});
      service_.cartography().add_terrain_lock({project::new_uuid(),
                                               p.id,
                                               "QA Proteção",
                                               {0, 0},
                                               2,
                                               project::utc_now()});
    }
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL deletion " << phase_ << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        1200);
  }
  ~Window() override { timer_.disconnect(); }

private:
  application::ProjectService service_;
  ui::ModalOverlayHost host_;
  Gtk::Stack pages_;
  ui::TemporalLibraryPage temporal_;
  ui::CartographyWorkspace map_;
  int &result_;
  bool restart_;
  int phase_{};
  project::FictionalCalendar calendar_, blocked_;
  std::string planet_, local_, map_operation_;
  sigc::connection timer_;
  void acknowledge() {
    auto *check = find<Gtk::CheckButton>(host_, [](auto &v) {
      return v.get_label() == "Conferi os efeitos desta operação";
    });
    require(check && check->get_sensitive(), "Acknowledgment unavailable");
    check->set_active(true);
    click(host_, "Excluir e registrar");
  }
  bool has_calendar(const std::string &name) {
    for (auto &c : service_.worldbuilding().calendars())
      if (c.name == name)
        return true;
    return false;
  }
  bool step() {
    if (!get_mapped())
      return true;
    if (restart_) {
      require(!has_calendar("QA Sistema descartável") &&
                  has_calendar("QA Sistema em uso"),
              "Calendar deletion not persisted");
      for (auto &p : service_.cartography().planets())
        require(p.name != "QA Mapa descartável", "Map deletion not persisted");
      require(!service_.project_operations().empty(), "History lost");
      result_ = 0;
      std::cout << "PASS temporal deletion restart\n";
      hide();
      return false;
    }
    switch (phase_) {
    case 0:
      temporal_.edit_calendar(calendar_);
      break;
    case 1: {
      auto *expander = find<Gtk::Expander>(temporal_, [](auto &v) {
        return v.get_label() == "Excluir este calendário e relógio…";
      });
      require(expander, "Calendar delete option missing");
      expander->set_expanded(true);
      click(temporal_, "Excluir calendário e relógio…");
      std::cout << "SNAPSHOT: deletion-calendar-confirm" << std::endl;
      break;
    }
    case 2:
      click(host_, "Cancelar / fechar");
      require(has_calendar(calendar_.name), "Cancel deleted calendar");
      break;
    case 3:
      click(temporal_, "Excluir calendário e relógio…");
      acknowledge();
      break;
    case 4:
      require(!has_calendar(calendar_.name), "Calendar not removed");
      pages_.set_visible_child("map");
      map_.refresh();
      break;
    case 5:
      // Refresh may select a preexisting Atlas map; choose the explicit QA
      // target.
      {
        auto *combo = find<Gtk::ComboBoxText>(
            map_, [&](auto &v) { return v.set_active_id(planet_); });
        require(combo, "QA map unavailable");
      }
      click(map_, "Excluir mapa…");
      std::cout << "SNAPSHOT: deletion-map-confirm" << std::endl;
      break;
    case 6:
      click(host_, "Cancelar / fechar");
      require(service_.cartography().position(planet_, local_).has_value(),
              "Cancel deleted map position");
      break;
    case 7:
      click(map_, "Excluir mapa…");
      acknowledge();
      break;
    case 8:
      require(service_.narrative().entity(local_).has_value(),
              "Local deleted with map");
      require(!service_.cartography().position(planet_, local_),
              "Position not removed");
      if (service_.cartography().planets().empty()) {
        auto *remove = find<Gtk::Button>(
            map_, [](auto &v) { return v.get_label() == "Excluir mapa…"; });
        require(remove && !remove->get_sensitive(),
                "Delete remains enabled without a map");
        require(!find<Gtk::Label>(map_,
                                  [](auto &v) {
                                    return v.get_text().find("chunks ativos") !=
                                           Glib::ustring::npos;
                                  }),
                "Deleted map metrics remain visible");
      }
      map_operation_ = service_.project_operations().front().id;
      std::cout << "SNAPSHOT: deletion-map-after" << std::endl;
      break;
    case 9: {
      auto impact = service_.reversal_impact(map_operation_);
      require(impact.allowed, impact.blocked_reason);
      service_.confirm_project_operation(impact);
    }
      map_.refresh();
      require(service_.cartography().position(planet_, local_).has_value(),
              "Position not restored");
      require(service_.cartography().terrain_locks(planet_).size() == 1,
              "Lock not restored");
      break;
    case 10:
      service_.confirm_project_operation(service_.removal_impact(
          project::RemovalKind::CartographicMap, planet_));
      map_.reset();
      map_.refresh();
      pages_.set_visible_child("time");
      temporal_.edit_calendar(blocked_);
      break;
    case 11: {
      auto *expander = find<Gtk::Expander>(temporal_, [](auto &v) {
        return v.get_label() == "Excluir este calendário e relógio…";
      });
      require(expander, "Delete option missing for used system");
      expander->set_expanded(true);
      click(temporal_, "Excluir calendário e relógio…");
      std::cout << "SNAPSHOT: deletion-calendar-blocked" << std::endl;
      break;
    }
    case 12: {
      auto *check = find<Gtk::CheckButton>(host_, [](auto &v) {
        return v.get_label() == "Conferi os efeitos desta operação";
      });
      require(check && !check->get_sensitive(),
              "Used calendar deletion not blocked");
      click(host_, "Cancelar / fechar");
      require(has_calendar(blocked_.name), "Blocked system changed");
      result_ = 0;
      std::cout << "PASS temporal deletion: cancel, confirmation, dependency "
                   "block, map restoration, local preservation\n";
      hide();
      return false;
    }
    }
    ++phase_;
    return true;
  }
};
int main(int argc, char **argv) {
  if (argc != 3 || !std::getenv("INDE_CLIPBOARD_TEST_ISOLATED"))
    return 2;
  const std::string mode = argv[2];
  if (mode != "--allow-fixture-writes" && mode != "--verify-restart")
    return 2;
  ui::configure_rendering_backend();
  auto app = Gtk::Application::create("io.github.inde.deletionqa",
                                      Gio::Application::Flags::NON_UNIQUE);
  int result = 1;
  Window *window{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    window = new Window(argv[1], result, mode == "--verify-restart");
    app->add_window(*window);
    window->present();
  });
  app->run(1, argv);
  delete window;
  return result;
}

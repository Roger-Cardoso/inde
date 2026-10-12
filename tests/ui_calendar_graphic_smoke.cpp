#include "../src/ui/rendering_policy.hpp"
#include "inde/application/project_service.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/calendar_picker.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &path, int &result, bool restart)
      : result_(result), restart_(restart) {
    service_.open(path);
    add_css_class("inde-window");
    set_child(picker_);
    set_default_size(640, 620);
    if (!restart_)
      for (auto engine : project::calendar_template_engines())
        service_.worldbuilding().create_calendar(
            project::calendar_template(engine));
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            return step();
          } catch (const std::exception &e) {
            std::cerr << "FAIL calendar graphic " << phase_ << ": " << e.what()
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
  ui::CalendarPicker picker_;
  int &result_;
  bool restart_;
  int phase_{};
  sigc::connection timer_;
  bool step() {
    if (!get_mapped())
      return true;
    if (restart_) {
      bool found = false;
      for (auto &axis : service_.planning().time_axes())
        if (axis.name == "QA Calendário Gráfico") {
          auto refs = service_.worldbuilding().temporal_references(axis.id);
          require(refs.size() == 8, "Restart reference count");
          for (const auto &r : refs) {
            auto c = service_.worldbuilding().calendar(r.calendar_id);
            picker_.set_calendar(c);
            picker_.set_selection(
                project::TemporalBoundary{r.year, r.month, r.day, r.clock});
            require(picker_.boundary().day == r.day, "Restart precision lost");
          }
          found = true;
        }
      require(found, "Restart axis missing");
      result_ = 0;
      std::cout << "PASS temporal calendar graphic restart\n";
      hide();
      return false;
    }
    auto engines = project::calendar_template_engines();
    if (phase_ < static_cast<int>(engines.size())) {
      auto c = project::calendar_template(engines[phase_]);
      picker_.set_calendar(c);
      bool unset = false;
      try {
        picker_.boundary();
      } catch (const std::exception &) {
        unset = true;
      }
      require(unset, "Opening invented a selection");
      auto y = c.engine == "chinese"  ? 2023
               : c.engine == "hebrew" ? 5784
                                      : 2024;
      auto m = c.engine == "maya-haab"        ? 19
               : c.engine == "ethiopic"       ? 13
               : c.engine == "egyptian-civil" ? 13
                                              : 1;
      picker_.show_month({y, m, project::calendar_first_day(c)});
      auto first = project::calendar_first_day(c);
      click(picker_, std::to_string(first));
      const auto chosen = picker_.boundary();
      require(chosen.day == first && chosen.month == m && chosen.clock.empty(),
              "Day/unknown clock lost");
      require(count(picker_,
                    [](auto &w) {
                      return w.has_css_class("calendar-day") &&
                             w.get_focusable();
                    }) == 1,
              "Grid has multiple tab stops");
      click(picker_, "›");
      require(picker_.boundary() == chosen, "Navigation changed selection");
      auto saved = service_.worldbuilding().create_calendar(c);
      std::string axis;
      for (const auto &a : service_.planning().time_axes())
        if (a.name == "QA Calendário Gráfico")
          axis = a.id;
      if (axis.empty())
        axis = service_.planning().create_time_axis("QA Calendário Gráfico").id;
      project::TemporalReference r;
      r.axis_id = axis;
      r.calendar_id = saved.id;
      r.label = "QA Gráfico " + c.engine;
      r.year = chosen.year;
      r.month = chosen.month;
      r.day = chosen.day;
      service_.worldbuilding().save_temporal_reference(r);
      std::cout << "SNAPSHOT: graphic-" << c.engine << std::endl;
    } else if (phase_ == 8) {
      auto c = project::calendar_template("gregorian");
      c.engine = "custom";
      c.leap_cycle = 0;
      c.months = {{"Aurora", 60}, {"Noite", 5}};
      c.weekdays.clear();
      for (int i = 0; i < 16; ++i)
        c.weekdays.push_back("Dia autoral " + std::to_string(i));
      c.clock = {{"batida", "batidas", "b", 10}, {"pulso", "pulsos", "p", 100}};
      picker_.set_calendar(c);
      picker_.set_range(true);
      click(picker_, "1");
      click(picker_, "5");
      require(picker_.boundary().day == 1 && picker_.boundary(true).day == 5,
              "Two-click range failed");
      require(count(picker_,
                    [](auto &w) {
                      return w.has_css_class("calendar-selected");
                    }) == 2,
              "Range endpoints not marked");
      require(count(picker_,
                    [](auto &w) {
                      return w.has_css_class("calendar-in-range");
                    }) == 3,
              "Range interior not marked");
      click(picker_, "Editar início");
      click(picker_, "Usar só este ano");
      require(!picker_.boundary().month && !picker_.boundary().day,
              "Year precision invented a date");
      click(picker_, "Usar só este mês");
      require(picker_.boundary().month == 1 && !picker_.boundary().day,
              "Month precision lost");
      std::cout << "SNAPSHOT: graphic-custom-wide-week" << std::endl;
    } else if (phase_ == 9) {
      click(picker_, "Limpar escolha");
      bool unset = false;
      try {
        picker_.boundary();
      } catch (const std::exception &) {
        unset = true;
      }
      require(unset, "Clear retained date");
      result_ = 0;
      std::cout
          << "PASS temporal calendar graphic: eight engines, navigation, "
             "selection, precision, range, wide custom week, persistence\n";
      hide();
      return false;
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
  auto app = Gtk::Application::create("io.github.inde.calendargraphicqa",
                                      Gio::Application::Flags::NON_UNIQUE);
  int result = 1;
  Window *w{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();
    w = new Window(argv[1], result, mode == "--verify-restart");
    app->add_window(*w);
    w->present();
  });
  app->run(1, argv);
  delete w;
  return result;
}

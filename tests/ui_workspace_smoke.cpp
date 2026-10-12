// Opt-in GTK regression: requires a display and a disposable fixture copy.
// Uses public widgets/signals, not private access or replacement callbacks.
#include "inde/ui/application_theme.hpp"
#include "inde/ui/cartography_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "ui/widget_probe.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
using inde::test::ui::click;
using inde::test::ui::find;
using inde::test::ui::pages;
using inde::test::ui::require;

class SmokeWindow final : public Gtk::Window {
public:
  SmokeWindow(std::filesystem::path fixture, int &result)
      : planning_(service_), cartography_(service_),
        fixture_(std::move(fixture)), result_(result) {
    set_title("INDE — disposable UI modularization smoke");
    set_default_size(1280, 900);
    add_css_class("inde-window");
    inde::ui::install_application_theme();
    workspaces_.add(planning_, "planning");
    workspaces_.add(cartography_, "cartography");
    host_.set_content(workspaces_);
    set_child(host_);
    steps_ = {
        {"open/refresh",
         [this] {
           service_.open(fixture_);
           planning_.refresh();
           cartography_.refresh();
         }},
        {"explorer/filters",
         [this] {
           click(planning_, "Personagens e mundo");
           expect_page("explorer");
           auto &page =
               *pages(planning_, "explorer").get_child_by_name("explorer");
           click(page, "Filtros");
           click(page, "Ver resultados");
         }},
        {"relations/filters",
         [this] {
           click(planning_, "Explorar relações");
           expect_page("relations");
           auto &page =
               *pages(planning_, "relations").get_child_by_name("relations");
           click(page, "Filtros");
           click(page, "Ver relações");
         }},
        {"time/history",
         [this] {
           click(planning_, "Tempo da história");
           expect_page("time");
           click(planning_, "← Voltar");
           expect_page("relations");
           click(planning_, "Avançar →");
           expect_page("time");
         }},
        {"narrative/roles",
         [this] {
           click(planning_, "Plano narrativo");
           expect_page("narrative");
           auto &narrative =
               *pages(planning_, "narrative").get_child_by_name("narrative");
           click(narrative, "Papéis na história");
           require(pages(narrative, "roles").get_visible_child_name() ==
                       "roles",
                   "Roles signal failed");
           click(narrative, "Planos");
           auto &stack = pages(narrative, "library");
           auto *paned = find<Gtk::Paned>(*stack.get_child_by_name("library"),
                                          [](auto &) { return true; });
           require(paned, "Missing narrative library panels");
           auto *list = find<Gtk::ListBox>(*paned->get_end_child(),
                                           [](auto &) { return true; });
           require(list, "Missing narrative structures list");
           if (auto *row = list->get_row_at_index(0)) {
             list->select_row(*row);
             g_signal_emit_by_name(list->gobj(), "row-activated", row->gobj());
             require(stack.get_visible_child_name() == "detail",
                     "Structure activation failed");
             click(narrative, "← Modelos");
             std::cout << "CHECK: narrative structure detail exercised"
                       << std::endl;
           } else {
             std::cout << "SKIP: narrative structure detail; empty library"
                       << std::endl;
           }
         }},
        {"entity/detail",
         [this] {
           inde::persistence::EntityQuery query;
           query.limit = 1;
           auto entities = service_.narrative().entities(query);
           if (!entities.empty()) {
             planning_.reveal_entity(entities.front().id);
             expect_page("detail");
             click(planning_, "← Voltar aos resultados");
             std::cout << "CHECK: entity detail exercised" << std::endl;
           } else {
             std::cout << "SKIP: entity detail; empty project" << std::endl;
           }
         }},
        {"map/view",
         [this] {
           workspaces_.set_visible_child("cartography");
           cartography_.refresh();
           auto *mode = find<Gtk::ComboBoxText>(cartography_, [](auto &c) {
             return c.get_active_id() == "pan";
           });
           require(mode, "Missing cartographic mode");
           mode->set_active_id("measure");
           mode->set_active_id("pan");
           auto *rendering = find<Gtk::ComboBoxText>(cartography_, [](auto &c) {
             return c.get_active_id() == "smooth";
           });
           require(rendering, "Missing cartographic rendering");
           rendering->set_active_id("raster");
           rendering->set_active_id("smooth");
           planets_before_ = service_.cartography().planets().size();
           click(cartography_, "Criar planeta");
           require(host_.has_dialog(), "Missing planet form");
           click(host_, "Gerar prévia");
         }},
        {"map/generation/preview",
         [this] {
           auto *accept = find<Gtk::Button>(cartography_, [](auto &b) {
             return b.get_label() == "Aceitar prévia";
           });
           require(accept && accept->get_visible(),
                   "Generation did not produce a preview within timeout");
           click(cartography_, "+");
           click(cartography_, "−");
           click(cartography_, "Ver planeta");
           click(cartography_, "Descartar prévia");
           require(service_.cartography().planets().size() == planets_before_,
                   "Discard wrote a planet");
         }},
        {"map/generation/cancel",
         [this] {
           click(cartography_, "Criar planeta");
           click(host_, "Gerar prévia");
           click(cartography_, "Cancelar geração");
         }},
        {"close/reset/reopen",
         [this] {
           cartography_.reset();
           service_.close();
           planning_.reset();
           service_.open(fixture_);
           planning_.refresh();
           cartography_.refresh();
           workspaces_.set_visible_child("planning");
           click(planning_, "Personagens e mundo");
           expect_page("explorer");
           require(service_.cartography().planets().size() == planets_before_,
                   "Cancel wrote a planet");
         }},
        {"clean shutdown",
         [this] {
           cartography_.reset();
           planning_.reset();
           service_.close();
         }},
    };
    timer_ = Glib::signal_timeout().connect([this] { return tick(); }, 200);
  }
  ~SmokeWindow() override { timer_.disconnect(); }

private:
  void expect_page(const std::string &name) {
    require(pages(planning_, "explorer").get_visible_child_name().raw() == name,
            "Planning page signal failed: " + name);
  }
  bool tick() {
    try {
      // Await the asynchronous preview without blocking the GTK main loop.
      if (step_ == 7) {
        auto *accept = find<Gtk::Button>(cartography_, [](auto &b) {
          return b.get_label() == "Aceitar prévia";
        });
        if ((!accept || !accept->get_visible()) && ++preview_waits_ < 50)
          return true;
      }
      if (step_ == steps_.size()) {
        std::cout << "PASS: " << steps_.size()
                  << " GTK steps; fixture=" << fixture_ << std::endl;
        result_ = 0;
        hide();
        return false;
      }
      steps_[step_].second();
      std::cout << "PASS step " << step_ + 1 << ": " << steps_[step_].first
                << std::endl;
      ++step_;
      return true;
    } catch (const std::exception &error) {
      std::cerr << "FAIL step " << step_ + 1 << ": " << error.what()
                << std::endl;
      result_ = 1;
      hide();
      return false;
    }
  }
  inde::application::ProjectService service_;
  inde::ui::PlanningWorkspace planning_;
  inde::ui::CartographyWorkspace cartography_;
  Gtk::Stack workspaces_;
  inde::ui::ModalOverlayHost host_;
  std::filesystem::path fixture_;
  int &result_;
  std::size_t step_{}, planets_before_{}, preview_waits_{};
  sigc::connection timer_;
  std::vector<std::pair<std::string, std::function<void()>>> steps_;
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: ui_workspace_smoke DISPOSABLE_FIXTURE_COPY\n";
    return 2;
  }
  int result = 1;
  auto application = Gtk::Application::create(
      "io.github.inde.ui-smoke", Gio::Application::Flags::NON_UNIQUE);
  application->make_window_and_run<SmokeWindow>(
      1, argv, std::filesystem::path(argv[1]), result);
  return result;
}

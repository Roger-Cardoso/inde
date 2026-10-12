// Opt-in GTK regression. Mutations are restricted to an explicitly supplied
// disposable fixture; no private access, replacement callbacks or SQL in UI.
#include "inde/project/manifest.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "ui/widget_probe.hpp"

#include <algorithm>
#include <functional>
#include <iostream>

namespace {
using namespace inde::test::ui;

class ExplorerSmokeWindow final : public Gtk::Window {
public:
  ExplorerSmokeWindow(std::filesystem::path fixture, int &result)
      : planning_(service_), fixture_(std::move(fixture)), result_(result) {
    set_title("INDE — disposable entity explorer regression");
    set_default_size(1280, 900);
    add_css_class("inde-window");
    inde::ui::install_application_theme();
    host_.set_content(planning_);
    set_child(host_);
    steps_ = {
        {"setup/pagination/facets/cards", [this] { explorer_case(); }},
        {"entity-type/entity CRUD and protection", [this] { commands_case(); }},
        {"temporal origin/reset/reopen", [this] { temporal_case(); }},
    };
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            if (step_ == steps_.size()) {
              result_ = 0;
              std::cout << "PASS: 3 explorer scenarios; fixture=" << fixture_
                        << std::endl;
              hide();
              return false;
            }
            steps_[step_].second();
            std::cout << "PASS scenario " << step_ + 1 << ": "
                      << steps_[step_].first << std::endl;
            ++step_;
            return true;
          } catch (const std::exception &e) {
            result_ = 1;
            std::cerr << "FAIL scenario " << step_ + 1 << ": " << e.what()
                      << std::endl;
            hide();
            return false;
          }
        },
        200);
  }
  ~ExplorerSmokeWindow() override { timer_.disconnect(); }

private:
  Gtk::Widget &explorer() {
    return *pages(planning_, "explorer").get_child_by_name("explorer");
  }
  void expect_page(const char *name) {
    require(pages(planning_, "explorer").get_visible_child_name() == name,
            "Unexpected planning page");
  }
  Gtk::Entry &search() {
    auto *entry = find<Gtk::Entry>(explorer(), [](auto &e) {
      return e.get_placeholder_text() == "Pesquisar nome, alias ou resumo";
    });
    require(entry, "Missing explorer search");
    return *entry;
  }
  void results(std::size_t matching, std::size_t visible) {
    const auto snapshot =
        service_.planning().explore_entities(service_.planning_context());
    require(snapshot.matching_count == matching,
            "Unexpected query result count");
    require(count(explorer(),
                  [](auto &w) { return w.has_css_class("entity-card"); }) ==
                visible,
            "Unexpected card count");
    require(find<Gtk::Label>(explorer(),
                             [&](auto &l) {
                               return l.get_text().raw().starts_with(
                                   std::to_string(matching) +
                                   (matching == 1 ? " resultado • página "
                                                  : " resultados • página "));
                             }),
            "Pagination label disagrees with query");
    require(!host_.has_dialog(), "Unexpected error dialog");
  }
  Gtk::CheckButton *facet(const std::string &name) {
    return find<Gtk::CheckButton>(explorer(), [&](auto &check) {
      return check.get_child() &&
             find<Gtk::Label>(*check.get_child(), [&](auto &l) {
               return l.get_text().raw().starts_with(name + " (");
             });
    });
  }
  void toggle_facet(const std::string &name, bool active) {
    auto *check = facet(name);
    require(check, "Missing facet: " + name);
    // Toggling rebuilds the facet widgets synchronously; do not reuse check.
    check->set_active(active);
  }
  template <class T> T &field(const std::string &label) {
    auto *heading = find<Gtk::Label>(host_, [&](auto &l) {
      return l.get_text().raw() == label && l.get_mnemonic_widget();
    });
    require(heading, "Missing labeled form field: " + label);
    auto *widget = dynamic_cast<T *>(heading->get_mnemonic_widget());
    require(widget, "Wrong form widget: " + label);
    return *widget;
  }
  Gtk::MenuButton &menu(const char *tooltip) {
    auto *button = find<Gtk::MenuButton>(
        planning_, [&](auto &b) { return b.get_tooltip_text() == tooltip; });
    require(button, "Missing action menu");
    return *button;
  }
  void action(Gtk::MenuButton &menu_button, const char *caption) {
    require(menu_button.get_sensitive() && menu_button.get_popover(),
            "Action menu unavailable");
    auto *button = find<Gtk::Button>(*menu_button.get_popover(), [&](auto &b) {
      return b.get_child() && find<Gtk::Label>(*b.get_child(), [&](auto &l) {
               return l.get_text() == caption;
             });
    });
    require(button && button->get_sensitive(), "Missing menu action");
    g_signal_emit_by_name(button->gobj(), "clicked");
  }
  void refresh_explorer() {
    click(planning_, "Personagens e mundo");
    planning_.refresh();
  }
  void clear() { click(explorer(), "Limpar contexto"); }
  void modal(const char *caption) {
    auto *footer = find<Gtk::Box>(host_, [](auto &box) {
      return box.has_css_class("modal-footer");
    });
    require(host_.has_dialog() && footer, "Missing modal footer");
    click(*footer, caption);
  }

  void explorer_case() {
    service_.open(fixture_);
    initial_count_ = service_.narrative().entity_count();
    initial_types_ = service_.narrative().entity_types().size();
    const auto token = inde::project::new_uuid();
    prefix_ = "QA-" + token + " — Órbita 🖋";
    auto a = service_.narrative().create_entity_type("qa-a-" + token,
                                                     "QA-A " + token);
    auto b = service_.narrative().create_entity_type("qa-b-" + token,
                                                     "QA-B " + token);
    std::vector<std::string> ids;
    for (int i = 0; i < 102; ++i)
      ids.push_back(service_.narrative()
                        .create_entity(i == 101 ? b.id : a.id,
                                       prefix_ + " " + std::to_string(i),
                                       "Ação, memória e Unicode.")
                        .id);
    refresh_explorer();
    clear();
    search().set_text(prefix_);
    results(102, 100);
    toggle_facet(a.name, true);
    results(101, 100);
    toggle_facet(b.name, true);
    results(102, 100);
    require(service_.planning_context().entity_type_ids.size() == 2,
            "Multi-facet OR lost selections");
    toggle_facet(a.name, false);
    results(1, 1);
    toggle_facet(b.name, false);
    toggle_facet(a.name, true);
    search().set_text(prefix_ + " absent");
    results(0, 0);
    require(facet(a.name) && facet(a.name)->get_active(),
            "Selected zero-count facet disappeared");
    search().set_text(prefix_);
    toggle_facet(a.name, false);
    results(102, 100);
    auto *card = find<Gtk::Button>(
        explorer(), [](auto &b) { return b.has_css_class("entity-card"); });
    require(card, "Missing card");
    auto *identity = card;
    g_signal_emit_by_name(card->gobj(), "clicked");
    expect_page("detail");
    require(service_.planning_context().search == prefix_,
            "Card navigation lost search");
    click(planning_, "← Voltar aos resultados");
    expect_page("explorer");
    results(102, 100);
    require(find<Gtk::Button>(explorer(),
                              [](auto &b) {
                                return b.has_css_class("entity-card");
                              }) == identity,
            "Return rebuilt cards");
    click(explorer(), "Próxima");
    results(102, 2);
    require(service_.planning_context().offset == 100,
            "Next-page offset wrong");
    click(explorer(), "Anterior");
    results(102, 100);
    click(explorer(), "Próxima");
    for (int i = 0; i < 2; ++i) {
      service_.narrative().delete_entity(ids.back());
      ids.pop_back();
    }
    planning_.refresh();
    results(100, 100);
    require(service_.planning_context().offset == 0,
            "Out-of-range page was not clamped");
    for (const auto &id : ids)
      service_.narrative().delete_entity(id);
    service_.narrative().delete_entity_type(a.id);
    service_.narrative().delete_entity_type(b.id);
    refresh_explorer();
    clear();
    require(service_.narrative().entity_count() == initial_count_,
            "Synthetic pagination data leaked");
  }

  void commands_case() {
    const std::string type_key = "qa-ui-" + inde::project::new_uuid();
    const std::string type_name = prefix_ + " tipo";
    click(explorer(), "Nova categoria");
    require(host_.has_dialog(), "Type form missing");
    field<Gtk::Entry>("Chave técnica").set_text(type_key);
    field<Gtk::Entry>("Nome do tipo").set_text(type_name);
    field<Gtk::Entry>("Descrição")
        .set_text("Teste descartável — definição autoral.");
    modal("Criar");
    require(!host_.has_dialog(), "Type creation failed");
    auto types = service_.narrative().entity_types();
    auto created = std::find_if(types.begin(), types.end(),
                                [&](auto &t) { return t.key == type_key; });
    require(created != types.end(), "UI did not persist type");
    const auto type_id = created->id;
    auto &type_menu = menu("Ações da categoria de elemento selecionada");
    action(type_menu, "Editar tipo");
    field<Gtk::Entry>("Nome do tipo").set_text(type_name + " editado");
    modal("Salvar");
    require(!host_.has_dialog(), "Type edit failed");
    types = service_.narrative().entity_types();
    require(std::any_of(types.begin(), types.end(),
                        [&](auto &t) {
                          return t.id == type_id && t.key == type_key &&
                                 t.name == type_name + " editado";
                        }),
            "Type edit lost identity/key");
    click(explorer(), "Novo elemento");
    field<Gtk::ComboBoxText>("Categoria do elemento").set_active_id(type_id);
    field<Gtk::Entry>("Nome do elemento da história").set_text(prefix_ + " criada");
    field<Gtk::Entry>("Resumo").set_text("Órbita, ação e 🖋.");
    modal("Criar");
    expect_page("detail");
    require(!host_.has_dialog(), "Entity creation failed");
    inde::persistence::EntityQuery query;
    query.entity_type_ids = {type_id};
    auto entities = service_.narrative().entities(query);
    require(entities.size() == 1 &&
                entities.front().summary == "Órbita, ação e 🖋.",
            "Unicode/create lost content");
    const auto id = entities.front().id;
    action(menu("Ações do elemento da história"), "Editar elemento da história");
    field<Gtk::Entry>("Nome do elemento da história").set_text(prefix_ + " editada");
    field<Gtk::Entry>("Resumo").set_text("Revisão — íntegra 🖋.");
    modal("Salvar");
    require(!host_.has_dialog(), "Entity edit failed");
    const auto updated = service_.narrative().entity(id);
    require(updated && updated->name == prefix_ + " editada" &&
                updated->summary == "Revisão — íntegra 🖋.",
            "Entity edit lost content/identity");
    click(planning_, "← Voltar aos resultados");
    refresh_explorer();
    action(type_menu, "Remover tipo");
    modal("Remover");
    require(host_.has_dialog(), "Used type was not protected");
    require(service_.narrative().entity(id).has_value(),
            "Protection deleted entity");
    modal("Fechar");
    require(!host_.has_dialog(), "Error dialog did not close");
    planning_.reveal_entity(id);
    expect_page("detail");
    action(menu("Ações do elemento da história"), "Excluir elemento da história");
    auto *acknowledge=find<Gtk::CheckButton>(host_,[](auto&c){return c.get_label()=="Conferi os efeitos desta operação";});
    require(acknowledge&&!acknowledge->get_active()&&acknowledge->get_sensitive(),"Entity impact acknowledgment missing");
    acknowledge->set_active(true);
    click(host_,"Excluir e registrar");
    require(!service_.narrative().entity(id) && !host_.has_dialog(),
            "Entity deletion failed");
    refresh_explorer();
    clear();
    auto *type_combo = find<Gtk::ComboBoxText>(explorer(), [&](auto &c) {
      return c.get_active_id().raw() == type_id;
    });
    require(type_combo, "Custom type selection lost");
    action(type_menu, "Remover tipo");
    modal("Remover");
    require(!host_.has_dialog(), "Type deletion failed");
    require(service_.narrative().entity_count() == initial_count_,
            "Entity cleanup changed original count");
    require(service_.narrative().entity_types().size() == initial_types_,
            "Type cleanup changed original count");
    auto *combo = find<Gtk::ComboBoxText>(explorer(), [&](auto &c) {
      return c.get_active_id().raw() == type_combo->get_active_id().raw();
    });
    require(combo, "Missing type controls");
    combo->set_active_id("00000000-0000-4000-9000-000000000001");
    require(!type_menu.get_sensitive(),
            "Builtin type actions are not protected");
    std::cout << "CHECK: Unicode CRUD, stable UUID/key and used/builtin type "
                 "protection"
              << std::endl;
  }

  void temporal_case() {
    clear();
    auto axes = service_.planning().time_axes();
    if (!axes.empty()) {
      inde::persistence::PlanningQuery query;
      query.limit = 1;
      auto points = service_.planning().time_points(axes.front().id, query);
      if (!points.empty()) {
        planning_.reveal_time_point(points.front().id);
        expect_page("time");
        require(service_.planning_context().fictional_time_point_id ==
                    points.front().id,
                "Temporal origin lost context");
        click(planning_, "← Voltar");
        expect_page("explorer");
        std::cout << "CHECK: temporal origin exercised" << std::endl;
      }
    } else
      std::cout << "SKIP: temporal origin; no axis" << std::endl;
    service_.close();
    planning_.reset();
    service_.open(fixture_);
    refresh_explorer();
    clear();
    require(service_.narrative().entity_count() == initial_count_,
            "Reopen count differs");
    require(service_.narrative().entity_types().size() == initial_types_,
            "Reopen type count differs");
    service_.close();
    planning_.reset();
  }
  inde::application::ProjectService service_;
  inde::ui::PlanningWorkspace planning_;
  inde::ui::ModalOverlayHost host_;
  std::filesystem::path fixture_;
  int &result_;
  std::size_t step_{}, initial_count_{}, initial_types_{};
  std::string prefix_;
  sigc::connection timer_;
  std::vector<std::pair<std::string, std::function<void()>>> steps_;
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes") {
    std::cerr << "Usage: ui_explorer_smoke DISPOSABLE_FIXTURE_COPY "
                 "--allow-fixture-writes\n";
    return 2;
  }
  int result = 1;
  auto app = Gtk::Application::create("io.github.inde.explorer-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  app->make_window_and_run<ExplorerSmokeWindow>(
      1, argv, std::filesystem::path(argv[1]), result);
  return result;
}

// Opt-in temporal-form regression against the production UI objects.
// Mutations require a disposable fixture and an explicit command-line flag.
#include "../src/ui/rendering_policy.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "ui/widget_probe.hpp"

#include <algorithm>
#include <iostream>

namespace {
using namespace inde::test::ui;
class TemporalWindow final : public Gtk::Window {
public:
  TemporalWindow(std::filesystem::path fixture, int &result)
      : planning_(service_), fixture_(std::move(fixture)), result_(result) {
    set_default_size(1280, 900);
    host_.set_content(planning_);
    set_child(host_);
    service_.open(fixture_);
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            exercise();
            result_ = 0;
            std::cout << "PASS: temporal CRUD, selectors, protections, cascade "
                         "and reopen"
                      << std::endl;
          } catch (const std::exception &error) {
            std::cerr << "FAIL temporal: " << error.what() << std::endl;
          }
          hide();
          return false;
        },
        300);
  }
  ~TemporalWindow() override { timer_.disconnect(); }

private:
  Gtk::Widget &time_page() {
    return *pages(planning_, "time").get_child_by_name("time");
  }
  Gtk::Widget &detail() {
    return *pages(planning_, "detail").get_child_by_name("detail");
  }
  template <class T> T &field(const char *caption) {
    auto *label = find<Gtk::Label>(host_, [&](auto &l) {
      return l.get_text() == caption && l.get_mnemonic_widget();
    });
    require(label, std::string("Missing form field: ") + caption);
    auto *widget = dynamic_cast<T *>(label->get_mnemonic_widget());
    require(widget, std::string("Wrong form control: ") + caption);
    return *widget;
  }
  void modal(const char *caption) {
    auto *footer = find<Gtk::Box>(
        host_, [](auto &box) { return box.has_css_class("modal-footer"); });
    require(host_.has_dialog() && footer, "Missing modal footer");
    click(*footer, caption);
  }
  void saved() { require(!host_.has_dialog(), "Unexpected form error"); }
  void expected_error() {
    require(host_.has_dialog(), "Expected domain error missing");
    modal("Fechar");
  }
  void action(const char *tooltip, const char *caption) {
    auto *menu = find<Gtk::MenuButton>(
        planning_, [&](auto &m) { return m.get_tooltip_text() == tooltip; });
    require(menu && menu->get_sensitive() && menu->get_popover(),
            "Menu unavailable");
    auto *button = find<Gtk::Button>(*menu->get_popover(), [&](auto &b) {
      return b.get_child() && find<Gtk::Label>(*b.get_child(), [&](auto &l) {
               return l.get_text() == caption;
             });
    });
    require(button && button->get_sensitive(),
            std::string("Menu action unavailable: ") + caption);
    g_signal_emit_by_name(button->gobj(), "clicked");
  }
  Gtk::ComboBoxText &axis_combo() {
    auto *combo =
        find<Gtk::ComboBoxText>(time_page(), [](auto &) { return true; });
    require(combo, "Axis combo missing");
    return *combo;
  }
  void open_time(const std::string &axis) {
    click(planning_, "Tempo da história");
    require(axis_combo().set_active_id(axis), "Axis not available");
  }
  void reveal(const std::string &id) {
    click(planning_, "Limpar contexto");
    planning_.reveal_entity(id);
    saved();
  }
  void expand_presence() {
    auto *section = find<Gtk::Expander>(detail(), [](auto &e) {
      return e.get_label() == "Presença e localização";
    });
    require(section, "Presence section missing");
    section->set_expanded(true);
  }
  void choose(inde::ui::IncrementalSelector &selector,
              const std::string &search_text, const std::string &label,
              const std::string &id) {
    auto *search =
        find<Gtk::SearchEntry>(selector, [](auto &) { return true; });
    require(search, "Selector search missing");
    search->set_text(search_text);
    // Exercise the real provider callback synchronously; not a debounce
    // benchmark.
    g_signal_emit_by_name(search->gobj(), "search-changed");
    auto *button = find<Gtk::Button>(selector, [&](auto &b) {
      return b.get_child() && find<Gtk::Label>(*b.get_child(), [&](auto &l) {
               return l.get_text().raw() == label;
             });
    });
    require(button, "Selector option missing: " + label);
    g_signal_emit_by_name(button->gobj(), "clicked");
    require(selector.selected_id() == id, "Selector UUID lost");
  }
  void point_form(const std::string &ordinal, const std::string &label) {
    click(time_page(), "Novo ponto");
    field<Gtk::Entry>("Posição na linha do tempo").set_text(ordinal);
    field<Gtk::Entry>("Nome do marco").set_text(label);
    field<Gtk::Entry>("Descrição").set_text("Tempo explícito 🖋");
    modal("Criar");
  }
  std::size_t point_count() {
    std::size_t total = 0;
    for (const auto &axis : service_.planning().time_axes())
      total += service_.planning().time_points(axis.id).size();
    return total;
  }
  void exercise() {
    auto &narrative = service_.narrative();
    auto &temporal = service_.planning();
    const auto initial_entities = narrative.entity_count();
    const auto initial_axes = temporal.time_axes().size();
    const auto initial_points = point_count();
    const auto prefix = "QA-tempo-" + inde::project::new_uuid() + " — Órbita";
    const auto types = narrative.entity_types();
    auto type_id = [&](const char *key) {
      auto found = std::find_if(types.begin(), types.end(),
                                [&](auto &t) { return t.key == key; });
      require(found != types.end(), "Builtin type missing");
      return found->id;
    };
    const auto event =
        narrative.create_entity(type_id("event"), prefix + " evento");
    const auto character =
        narrative.create_entity(type_id("character"), prefix + " personagem");
    const auto location =
        narrative.create_entity(type_id("location"), prefix + " local");
    planning_.refresh();
    click(planning_, "Tempo da história");
    const auto principal = axis_combo().get_active_id().raw();
    action("Ações da linha do tempo atual", "Excluir linha do tempo");
    modal("Remover");
    expected_error();
    require(temporal.time_axes().size() == initial_axes,
            "Principal axis removed");
    click(time_page(), "Nova linha do tempo");
    modal("Cancelar");
    saved();
    require(temporal.time_axes().size() == initial_axes,
            "Cancel persisted an axis");
    click(time_page(), "Nova linha do tempo");
    field<Gtk::Entry>("Nome da linha do tempo").set_text(prefix);
    field<Gtk::Entry>("Descrição").set_text("Eixo autoral 🖋");
    modal("Criar");
    saved();
    const auto axis = axis_combo().get_active_id().raw();
    require(axis != principal, "Custom axis not selected");
    action("Ações da linha do tempo atual", "Editar linha do tempo");
    field<Gtk::Entry>("Nome da linha do tempo").set_text(prefix + " revisto");
    modal("Salvar");
    saved();
    auto axes = temporal.time_axes();
    require(std::any_of(axes.begin(), axes.end(),
                        [&](auto &a) {
                          return a.id == axis && a.name == prefix + " revisto";
                        }),
            "Axis UUID or Unicode lost");
    point_form("10x", prefix + " inválido");
    expected_error();
    point_form("9223372036854775808", prefix + " overflow");
    expected_error();
    require(temporal.time_points(axis).empty(), "Invalid ordinal persisted");
    point_form("10", prefix + " início");
    saved();
    point_form("20", prefix + " fim");
    saved();
    auto points = temporal.time_points(axis);
    require(points.size() == 2, "Points not persisted");
    const auto start = points[0].id, end = points[1].id;
    click(time_page(), "20 — " + prefix + " fim");
    action("Ações do ponto selecionado", "Editar ponto");
    field<Gtk::Entry>("Posição na linha do tempo").set_text("30");
    modal("Salvar");
    saved();
    require(temporal.time_point(end)->ordinal == 30,
            "Point identity/order lost");
    action("Ações da linha do tempo atual", "Excluir linha do tempo");
    modal("Remover");
    expected_error();
    require(temporal.time_axes().size() == initial_axes + 1,
            "Used axis removed");
    std::cout << "CHECK: axis/point CRUD, Unicode, cancel, integer parsing and "
                 "used/default protection"
              << std::endl;

    const auto point_label = prefix + " revisto — 10 — " + prefix + " início";
    const auto end_label = prefix + " revisto — 30 — " + prefix + " fim";
    reveal(event.id);
    click(detail(), "Situar");
    choose(field<inde::ui::IncrementalSelector>("Ponto de tempo da história"),
           prefix, point_label, start);
    field<Gtk::Entry>("Descrição da ocorrência").set_text("Ocorrência 🖋");
    modal("Salvar");
    saved();
    auto occurrence = temporal.event_occurrence_for_entity(event.id);
    require(occurrence && occurrence->time_point_id == start,
            "Occurrence not situated");
    const auto occurrence_id = occurrence->id;
    action("Ações do acontecimento", "Editar ocorrência");
    choose(field<inde::ui::IncrementalSelector>("Ponto de tempo da história"),
           prefix, end_label, end);
    field<Gtk::Entry>("Descrição da ocorrência")
        .set_text("Ocorrência revista 🖋");
    modal("Salvar");
    saved();
    occurrence = temporal.event_occurrence_for_entity(event.id);
    require(occurrence && occurrence->id == occurrence_id &&
                occurrence->time_point_id == end &&
                occurrence->description == "Ocorrência revista 🖋",
            "Occurrence UUID/content lost");
    open_time(axis);
    click(time_page(), "30 — " + prefix + " fim");
    action("Ações do ponto selecionado", "Remover ponto");
    modal("Remover");
    expected_error();
    require(temporal.time_point(end).has_value(), "Used event point removed");
    reveal(event.id);
    auto add_participant = [&] {
      auto *section = find<Gtk::Expander>(detail(), [](auto &e) {
        return e.get_label() == "Acontecimento e participantes";
      });
      require(section, "Event section missing");
      click(*section, "Adicionar");
      auto &selector =
          field<inde::ui::IncrementalSelector>("Elemento da história participante");
      choose(selector, prefix, character.name, character.id);
      require(!find<Gtk::Label>(
                  selector,
                  [&](auto &l) { return l.get_text().raw() == event.name; }),
              "Event offered as participant");
      field<Gtk::Entry>("Papel no acontecimento").set_text("Testemunha 🖋");
      field<Gtk::Entry>("Notas").set_text("Notas autorais");
      modal("Adicionar");
      saved();
    };
    add_participant();
    auto participants = temporal.event_participations(occurrence_id);
    require(participants.size() == 1 &&
                participants[0].participant_entity_id == character.id,
            "Participation missing");
    const auto participant_id = participants[0].id;
    click(detail(), character.name + " — Testemunha 🖋");
    action("Ações do participante selecionado", "Editar participante");
    field<Gtk::Entry>("Papel no acontecimento").set_text("Agente 🖋");
    modal("Salvar");
    saved();
    participants = temporal.event_participations(occurrence_id);
    require(participants.size() == 1 && participants[0].id == participant_id &&
                participants[0].role == "Agente 🖋",
            "Participation identity/content lost");
    click(detail(), character.name + " — Agente 🖋");
    action("Ações do participante selecionado", "Remover participante");
    modal("Remover");
    saved();
    require(temporal.event_participations(occurrence_id).empty(),
            "Participation deletion failed");
    add_participant();
    std::cout << "CHECK: event/participation CRUD, selectors, UUID and used "
                 "point protection"
              << std::endl;

    reveal(character.id);
    expand_presence();
    click(detail(), "Nova presença");
    auto &local_selector = field<inde::ui::IncrementalSelector>("Local");
    choose(local_selector, prefix, location.name, location.id);
    require(!find<Gtk::Label>(
                local_selector,
                [&](auto &l) { return l.get_text().raw() == character.name; }),
            "Non-Local offered as Local");
    choose(field<inde::ui::IncrementalSelector>("Início"), prefix, end_label,
           end);
    choose(field<inde::ui::IncrementalSelector>("Fim opcional"), prefix,
           point_label, start);
    modal("Criar");
    expected_error();
    inde::persistence::PresenceQuery presence_query;
    presence_query.entity_id = character.id;
    require(temporal.presences(presence_query).empty(),
            "Reversed interval persisted");
    click(detail(), "Nova presença");
    choose(field<inde::ui::IncrementalSelector>("Local"), prefix, location.name,
           location.id);
    choose(field<inde::ui::IncrementalSelector>("Início"), prefix, point_label,
           start);
    field<Gtk::Entry>("Descrição da presença").set_text("Presença aberta 🖋");
    modal("Criar");
    saved();
    auto presences = temporal.presences(presence_query);
    require(presences.size() == 1 && !presences[0].end_time_point_id &&
                presences[0].start_time_point_id == start,
            "Open presence missing");
    const auto presence_id = presences[0].id;
    click(detail(), location.name + " — intervalo aberto");
    action("Ações da presença selecionada", "Editar presença");
    choose(field<inde::ui::IncrementalSelector>("Fim opcional"), prefix,
           end_label, end);
    field<Gtk::Entry>("Descrição da presença").set_text("Presença fechada 🖋");
    modal("Salvar");
    saved();
    presences = temporal.presences(presence_query);
    require(presences.size() == 1 && presences[0].id == presence_id &&
                presences[0].end_time_point_id == end &&
                presences[0].description == "Presença fechada 🖋",
            "Closed presence UUID/content lost");
    open_time(axis);
    click(time_page(), "10 — " + prefix + " início");
    action("Ações do ponto selecionado", "Remover ponto");
    modal("Remover");
    expected_error();
    require(temporal.time_point(start).has_value(),
            "Used presence point removed");
    reveal(character.id);
    expand_presence();
    click(detail(), location.name + " — intervalo fechado");
    action("Ações da presença selecionada", "Remover presença");
    modal("Remover");
    saved();
    require(temporal.presences(presence_query).empty(),
            "Presence deletion failed");
    std::cout << "CHECK: open/closed presence CRUD, selectors, interval "
                 "validation and point protection"
              << std::endl;

    reveal(event.id);
    action("Ações do acontecimento", "Remover ocorrência");
    modal("Remover");
    saved();
    require(!temporal.event_occurrence_for_entity(event.id) &&
                temporal.event_participations(occurrence_id).empty(),
            "Occurrence cascade failed");
    open_time(axis);
    for (const auto &label :
         {"10 — " + prefix + " início", "30 — " + prefix + " fim"}) {
      click(time_page(), label);
      action("Ações do ponto selecionado", "Remover ponto");
      modal("Remover");
      saved();
    }
    require(temporal.time_points(axis).empty(), "Unused point deletion failed");
    action("Ações da linha do tempo atual", "Excluir linha do tempo");
    modal("Remover");
    saved();
    for (const auto &entity : {event, character, location})
      narrative.delete_entity(entity.id);
    click(planning_, "Limpar contexto");
    require(narrative.entity_count() == initial_entities &&
                temporal.time_axes().size() == initial_axes &&
                point_count() == initial_points,
            "Synthetic data not removed");
    service_.close();
    planning_.reset();
    service_.open(fixture_);
    planning_.refresh();
    require(service_.narrative().entity_count() == initial_entities &&
                service_.planning().time_axes().size() == initial_axes &&
                point_count() == initial_points,
            "Reopen counts changed");
    service_.close();
    planning_.reset();
    std::cout
        << "CHECK: occurrence cascade, cleanup and reopened original counts"
        << std::endl;
  }
  inde::application::ProjectService service_;
  inde::ui::PlanningWorkspace planning_;
  inde::ui::ModalOverlayHost host_;
  std::filesystem::path fixture_;
  int &result_;
  sigc::connection timer_;
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes") {
    std::cerr << "Usage: ui_temporal_smoke DISPOSABLE_FIXTURE "
                 "--allow-fixture-writes\n";
    return 2;
  }
  inde::ui::configure_rendering_backend();
  int result = 1;
  auto app = Gtk::Application::create("io.github.inde.temporal-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  TemporalWindow *window = nullptr;
  app->signal_activate().connect([&] {
    window = new TemporalWindow(argv[1], result);
    app->add_window(*window);
    window->present();
  });
  app->run(1, argv);
  delete window;
  return result;
}

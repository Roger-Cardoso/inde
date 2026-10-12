// Opt-in regression; mutations are restricted to a disposable fixture copy.
#include "../src/ui/rendering_policy.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "ui/widget_probe.hpp"

#include <algorithm>
#include <iostream>

namespace {
using namespace inde::test::ui;
class RelationsWindow final : public Gtk::Window {
public:
  RelationsWindow(std::filesystem::path fixture, int &result)
      : planning_(service_), result_(result) {
    set_default_size(1280, 900);
    host_.set_content(planning_);
    set_child(host_);
    service_.open(fixture);
    timer_ = Glib::signal_timeout().connect(
        [this] {
          try {
            exercise();
            result_ = 0;
            std::cout << "PASS: relation CRUD, qualifiers, protection, "
                         "counterpart and context"
                      << std::endl;
          } catch (const std::exception &error) {
            std::cerr << "FAIL relations: " << error.what() << std::endl;
          }
          hide();
          return false;
        },
        300);
  }
  ~RelationsWindow() override { timer_.disconnect(); }

private:
  Gtk::Widget &relations() {
    return *pages(planning_, "relations").get_child_by_name("relations");
  }
  Gtk::Widget &detail() {
    return *pages(planning_, "detail").get_child_by_name("detail");
  }
  template <class T> T &field(const char *caption) {
    auto *label = find<Gtk::Label>(host_, [&](auto &l) {
      return l.get_text() == caption && l.get_mnemonic_widget();
    });
    require(label, "Missing form field");
    auto *widget = dynamic_cast<T *>(label->get_mnemonic_widget());
    require(widget, "Wrong form control");
    return *widget;
  }
  void modal(const char *caption) {
    auto *footer = find<Gtk::Box>(
        host_, [](auto &box) { return box.has_css_class("modal-footer"); });
    require(host_.has_dialog() && footer, "Missing modal footer");
    click(*footer, caption);
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
    require(button && button->get_sensitive(), "Menu action unavailable");
    g_signal_emit_by_name(button->gobj(), "clicked");
  }
  void select_card() {
    auto *card = find<Gtk::Button>(
        relations(), [](auto &b) { return b.has_css_class("catalog-card"); });
    require(card, "Relation card missing");
    g_signal_emit_by_name(card->gobj(), "clicked");
  }
  void exercise() {
    auto &narrative = service_.narrative();
    const auto initial_count = narrative.entity_count();
    const auto initial_types = narrative.relation_types().size();
    const auto initial_relations = narrative.relations().size();
    const auto prefix = "zzzz-QA-" + inde::project::new_uuid() + " — Órbita";
    const auto entity_types = narrative.entity_types();
    const auto type_id = [&](const char *key) {
      auto found = std::find_if(entity_types.begin(), entity_types.end(),
                                [&](auto &t) { return t.key == key; });
      require(found != entity_types.end(), "Builtin type missing");
      return found->id;
    };
    const auto source =
        narrative.create_entity(type_id("character"), prefix + " origem");
    const auto target =
        narrative.create_entity(type_id("character"), prefix + " destino");
    const auto location =
        narrative.create_entity(type_id("location"), prefix + " local");
    std::vector<std::string> filler_ids;
    for (int i = 0; i < 100; ++i)
      filler_ids.push_back(narrative.create_entity(type_id("character"),
          "000-" + prefix + " " + std::to_string(i)).id);
    inde::persistence::EntityQuery first_page;
    first_page.limit = 100;
    const auto first_entities = narrative.entities(first_page);
    require(std::none_of(first_entities.begin(), first_entities.end(),
                        [&](auto &e) { return e.id == source.id; }),
            "Off-page origin fixture was not established");
    planning_.refresh();
    click(planning_, "Explorar relações");
    click(relations(), "Nova categoria");
    const auto key = "qa-relation-" + inde::project::new_uuid();
    field<Gtk::Entry>("Chave técnica").set_text(key);
    field<Gtk::Entry>("Como se lê de um elemento para o outro")
        .set_text(prefix + " acompanha");
    field<Gtk::Entry>("Como se lê no sentido contrário").set_text(prefix + " acompanhado por");
    modal("Criar");
    require(!host_.has_dialog(), "Type create failed");
    auto types = narrative.relation_types();
    auto created = std::find_if(types.begin(), types.end(),
                                [&](auto &t) { return t.key == key; });
    require(created != types.end(), "Type not persisted");
    const auto relation_type = created->id;
    action("Ações do tipo de relação", "Editar tipo");
    field<Gtk::Entry>("Descrição").set_text("Revisão íntegra 🖋");
    modal("Salvar");
    require(!host_.has_dialog(), "Type edit failed");
    types = narrative.relation_types();
    require(std::any_of(types.begin(), types.end(),
                        [&](auto &t) {
                          return t.id == relation_type && t.key == key &&
                                 t.description == "Revisão íntegra 🖋";
                        }),
            "Type identity/content lost");
    click(relations(), "Nova relação");
    field<inde::ui::IncrementalSelector>("De")
        .set_selected(source.id, source.name);
    field<inde::ui::IncrementalSelector>("Para")
        .set_selected(target.id, target.name);
    field<inde::ui::IncrementalSelector>("Onde (opcional)")
        .set_selected(location.id, location.name);
    field<inde::ui::IncrementalSelector>("Por causa de (opcional)")
        .set_selected(source.id, source.name);
    std::optional<std::string> time_point;
    for (const auto &axis : service_.planning().time_axes()) {
      const auto points = service_.planning().time_points(axis.id);
      if (!points.empty()) {
        time_point = points.front().id;
        field<inde::ui::IncrementalSelector>("Quando (opcional)")
            .set_selected(*time_point, points.front().label);
        break;
      }
    }
    field<Gtk::Entry>("Notas de contexto").set_text(prefix + " — ação 🖋");
    modal("Criar");
    require(!host_.has_dialog(), "Relation create failed");
    inde::persistence::RelationQuery query;
    query.relation_type_id = relation_type;
    auto records = narrative.relations(query);
    require(records.size() == 1, "Relation not persisted");
    const auto id = records.front().id;
    require(records.front().source_entity_id == source.id &&
                records.front().target_entity_id == target.id &&
                records.front().location_entity_id == location.id &&
                records.front().cause_entity_id == source.id &&
                records.front().fictional_time_point_id == time_point,
            "Qualifiers/direction lost");
    std::cout << (time_point ? "CHECK: temporal qualifier exercised"
                             : "SKIP: no temporal point")
              << std::endl;
    auto *search = find<Gtk::SearchEntry>(relations(), [](auto &e) {
      return e.get_placeholder_text() ==
             "Pesquisar relação, tipo, elemento da história ou contexto";
    });
    require(search, "Search missing");
    search->set_text(prefix);
    // SearchEntry normally emits after a delay. Exercise its real callback
    // explicitly here; this synchronous scenario does not measure debounce.
    g_signal_emit_by_name(search->gobj(), "search-changed");
    require(count(relations(),
                  [](auto &w) { return w.has_css_class("catalog-card"); }) == 1,
            "Literal filter failed");
    select_card();
    action("Ações da relação selecionada", "Editar relação");
    field<Gtk::Entry>("Notas de contexto").set_text(prefix + " — revisão 🖋");
    modal("Salvar");
    records = narrative.relations(query);
    require(records.size() == 1 && records.front().id == id &&
                records.front().description == prefix + " — revisão 🖋" &&
                records.front().fictional_time_point_id == time_point &&
                records.front().location_entity_id == location.id,
            "Edit lost identity/content/qualifiers");
    action("Ações do tipo de relação", "Remover tipo");
    modal("Remover");
    require(host_.has_dialog() && narrative.relations(query).size() == 1,
            "Used type not protected");
    modal("Fechar");
    planning_.reveal_entity(source.id);
    auto select_detail_relation = [&] {
      require(!host_.has_dialog(), "Source navigation showed an error dialog");
      auto *section = find<Gtk::Expander>(detail(), [](auto &expander) {
        return expander.get_label() == "Relações narrativas";
      });
      require(section, "Relation section missing");
      section->set_expanded(true);
      auto *row = find<Gtk::Button>(detail(), [&](auto &b) {
        return b.get_label().raw().find(target.name) != std::string::npos;
      });
      require(row, "Detail relation missing");
      g_signal_emit_by_name(row->gobj(), "clicked");
    };
    select_detail_relation();
    click(detail(), "Abrir outro elemento da relação");
    require(find<Gtk::Label>(
                detail(),
                [&](auto &l) { return l.get_text().raw() == target.name; }),
            "Counterpart not revealed");
    planning_.reveal_entity(source.id);
    select_detail_relation();
    click(detail(), "Filtrar por outro elemento da relação");
    require(service_.planning_context().related_entity_id == target.id,
            "Counterpart filter lost identity");
    click(planning_, "Limpar contexto");
    require(!service_.planning_context().related_entity_id,
            "Context not cleared");
    click(planning_, "Explorar relações");
    select_card();
    action("Ações da relação selecionada", "Excluir relação");
    auto *acknowledge=find<Gtk::CheckButton>(host_,[](auto&c){return c.get_label()=="Conferi os efeitos desta operação";});
    require(acknowledge&&!acknowledge->get_active()&&acknowledge->get_sensitive(),"Relation impact acknowledgment missing");
    acknowledge->set_active(true);
    click(host_,"Excluir e registrar");
    require(narrative.relations(query).empty(), "Relation deletion failed");
    action("Ações do tipo de relação", "Remover tipo");
    modal("Remover");
    require(!host_.has_dialog(), "Unused type deletion failed");
    for (const auto &entity : {source, target, location})
      narrative.delete_entity(entity.id);
    for (const auto &id : filler_ids)
      narrative.delete_entity(id);
    require(narrative.entity_count() == initial_count &&
                narrative.relation_types().size() == initial_types &&
                narrative.relations().size() == initial_relations,
            "Synthetic cleanup changed original counts");
    service_.close();
    planning_.reset();
  }
  inde::application::ProjectService service_;
  inde::ui::PlanningWorkspace planning_;
  inde::ui::ModalOverlayHost host_;
  int &result_;
  sigc::connection timer_;
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-fixture-writes")
    return 2;
  inde::ui::configure_rendering_backend();
  int result = 1;
  auto app = Gtk::Application::create("io.github.inde.relations-smoke",
                                      Gio::Application::Flags::NON_UNIQUE);
  app->make_window_and_run<RelationsWindow>(
      1, argv, std::filesystem::path(argv[1]), result);
  return result;
}

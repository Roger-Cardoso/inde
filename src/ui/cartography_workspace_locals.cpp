#include "cartography_ui_helpers.hpp"
#include "inde/ui/cartography_workspace.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <set>
#include <sstream>

namespace inde::ui {
namespace geo = project::geo;
using cartography_ui_detail::local_type;
using cartography_ui_detail::number;
using cartography_ui_detail::spin;

void CartographyWorkspace::reveal_entity(const std::string &id) {
  try {
    if (!current_ || preview_)
      return;
    const auto entity = service_.narrative().entity(id);
    if (!entity || entity->entity_type_id != local_type) {
      message_.set_text(
          "Este mapa posiciona elementos da história da categoria Lugar.");
      return;
    }
    selected_local_ = id;
    local_selector_->set_selected(id, entity->name);
    const auto p = service_.cartography().position(current_->id, id);
    if (p) {
      camera_.longitude = p->coordinate.longitude;
      camera_.latitude = p->coordinate.latitude;
      camera_.zoom = std::max({camera_.zoom, 4.0, std::pow(2.0, p->min_level)});
      selection_.set_text(
          entity->name + "\n" +
          (p->approximate ? "Posição aproximada" : "Coordenadas definidas") +
          "\nLon " + number(p->coordinate.longitude, 6) + "° · Latitude " +
          number(p->coordinate.latitude, 6) + "°");
      queue_scene();
    } else
      selection_.set_text(entity->name +
                          " ainda não possui posição neste planeta. Use "
                          "Definir coordenadas ou Posicionar lugar.");
    update_actions();
  } catch (const std::exception &e) {
    error(e);
  }
}
void CartographyWorkspace::place_local(geo::Coordinate at) {
  if (!current_ || preview_ || busy_ || !owner())
    return;
  if (!selected_local_) {
    message_.set_text("Selecione um Local na pesquisa lateral. Crie novos "
                      "Locais no Planejamento.");
    return;
  }
  try {
    const auto id = *selected_local_, world = current_->id;
    const auto previous = service_.cartography().position(world, id);
    auto *d = new OverlayDialog("Posicionar lugar", *owner(), true);
    d->set_secondary_text(
        "Coordenadas no planeta atual. A posição não comprova verdade, "
        "soberania ou validade em uma data ficcional. Substituir coordenadas é "
        "explícito e pode ser desfeito nesta sessão.");
    d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    d->add_button("Salvar posição", Gtk::ResponseType::ACCEPT);
    auto *body = d->get_content_area();
    body->set_margin(16);
    auto *lon = spin(-180, 180, 0.1, at.longitude, 6);
    auto *lat = spin(-90, 90, 0.1, at.latitude, 6);
    auto *approx = Gtk::make_managed<Gtk::CheckButton>("Posição aproximada");
    approx->set_active(previous && previous->approximate);
    auto *importance = spin(1, 100, 5, previous ? previous->importance : 70);
    auto *symbol = Gtk::make_managed<Gtk::ComboBoxText>();
    symbol->append("place", "Local");
    symbol->append("city", "Cidade");
    symbol->append("mountain", "Montanha");
    symbol->set_active_id(previous ? previous->symbol : "place");
    auto *lod = spin(0, 6, 1, previous ? previous->min_level : 0);
    append_labeled_form_field(*body, "Longitude (graus)", *lon);
    append_labeled_form_field(*body, "Latitude (graus)", *lat);
    body->append(*approx);
    append_labeled_form_field(*body, "Símbolo", *symbol);
    append_labeled_form_field(*body, "Importância cartográfica (1–100)",
                              *importance);
    append_labeled_form_field(*body, "Mostrar a partir deste nível de detalhe (0–6)", *lod);
    d->signal_response().connect([this, id, world, previous, lon, lat, approx,
                                  importance, symbol, lod](int r) {
      if (r != Gtk::ResponseType::ACCEPT)
        return;
      try {
        geo::Position p{world,
                        id,
                        {lon->get_value(), lat->get_value()},
                        approx->get_active(),
                        importance->get_value_as_int(),
                        lod->get_value_as_int(),
                        symbol->get_active_id().raw()};
        service_.cartography().change_position(world, id, previous, p);
        const auto after = service_.cartography().position(world, id);
        undo_.push_back({world, id, previous, after});
        if (undo_.size() > 20)
          undo_.erase(undo_.begin());
        redo_.clear();
        update_actions();
        queue_scene();
        message_.set_text("Posição salva. O elemento da história continua sendo o mesmo do Planejamento.");
      } catch (const std::exception &e) {
        error(e);
      }
    });
    d->present();
  } catch (const std::exception &e) {
    error(e);
  }
}
void CartographyWorkspace::unplace_local() {
  if (!current_ || !selected_local_ || !owner())
    return;
  try {
    const auto world = current_->id, id = *selected_local_;
    const auto previous = service_.cartography().position(world, id);
    if (!previous) {
      message_.set_text("Local sem posição neste planeta.");
      return;
    }
    auto *d = new OverlayDialog("Remover posição cartográfica", *owner(), true);
    d->set_secondary_text(
        "O Local e seu conteúdo no Planejamento serão preservados. Só esta "
        "associação ao planeta será removida.");
    d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
    d->add_button("Remover posição", Gtk::ResponseType::ACCEPT);
    d->signal_response().connect([this, world, id, previous](int r) {
      if (r != Gtk::ResponseType::ACCEPT)
        return;
      try {
        service_.cartography().change_position(world, id, previous,
                                               std::nullopt);
        undo_.push_back({world, id, previous, std::nullopt});
        if (undo_.size() > 20)
          undo_.erase(undo_.begin());
        redo_.clear();
        update_actions();
        queue_scene();
        selection_.set_text("Posição removida; Local preservado.");
      } catch (const std::exception &e) {
        error(e);
      }
    });
    d->present();
  } catch (const std::exception &e) {
    error(e);
  }
}
void CartographyWorkspace::history(bool redo) {
  auto &from = redo ? redo_ : undo_;
  auto &to = redo ? undo_ : redo_;
  if (from.empty())
    return;
  const auto edit = from.back();
  try {
    service_.cartography().change_position(edit.planet, edit.entity,
                                           redo ? edit.before : edit.after,
                                           redo ? edit.after : edit.before);
    to.push_back(edit);
    from.pop_back();
    update_actions();
    queue_scene();
    message_.set_text(redo ? "Posição refeita." : "Posição desfeita.");
  } catch (const std::exception &e) {
    error(e);
  }
}
} // namespace inde::ui

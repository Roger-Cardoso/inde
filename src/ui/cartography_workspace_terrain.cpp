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
using cartography_ui_detail::label;
using cartography_ui_detail::number;

void CartographyWorkspace::preview_terrain(geo::Coordinate center) {
  if (!current_ || preview_ || busy_)
    return;
  try {
    terrain_center_ = center;
    const auto id = mode_.get_active_id();
    const auto edit_mode = id == "lower"    ? geo::TerrainEditMode::Lower
                           : id == "smooth" ? geo::TerrainEditMode::Smooth
                                            : geo::TerrainEditMode::Raise;
    const geo::TerrainBrush brush{
        current_->id, center, terrain_radius_.get_value(),
        terrain_strength_.get_value_as_int(), edit_mode};
    const auto started = std::chrono::steady_clock::now();
    terrain_preview_ = service_.cartography().preview_terrain(*current_, brush);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - started)
                          .count();
    message_.set_text("PRÉVIA DE RELEVO — " +
                      std::to_string(terrain_preview_->changed_samples) +
                      " amostras finas · " +
                      std::to_string(terrain_preview_->chunks.size()) +
                      " chunks com delta · " + number(ms) +
                      " ms. A câmera pode mudar; o Projeto ainda não.");
    update_actions();
    queue_scene();
  } catch (const std::exception &e) {
    terrain_preview_.reset();
    update_actions();
    queue_scene();
    error(e);
  }
}
void CartographyWorkspace::apply_terrain_preview() {
  if (!current_ || !terrain_preview_)
    return;
  try {
    current_->terrain_revision = service_.cartography().apply_terrain(
        *terrain_preview_, current_->terrain_revision);
    for (auto &planet : planets_)
      if (planet.id == current_->id)
        planet.terrain_revision = current_->terrain_revision;
    terrain_undo_.push_back(*terrain_preview_);
    if (terrain_undo_.size() > 10)
      terrain_undo_.erase(terrain_undo_.begin());
    terrain_redo_.clear();
    terrain_preview_.reset();
    message_.set_text("Relevo aplicado na revisão " +
                      std::to_string(current_->terrain_revision) +
                      ". Todos os níveis foram recompostos.");
    update_actions();
    queue_scene();
  } catch (const std::exception &e) {
    error(e);
  }
}
void CartographyWorkspace::discard_terrain_preview() {
  terrain_preview_.reset();
  message_.set_text("Prévia de relevo descartada; o Projeto não foi alterado.");
  update_actions();
  queue_scene();
}
void CartographyWorkspace::terrain_history(bool redo) {
  if (!current_ || terrain_preview_)
    return;
  auto &from = redo ? terrain_redo_ : terrain_undo_;
  auto &to = redo ? terrain_undo_ : terrain_redo_;
  if (from.empty())
    return;
  const auto patch = from.back();
  try {
    current_->terrain_revision = service_.cartography().apply_terrain(
        patch, current_->terrain_revision, !redo);
    for (auto &planet : planets_)
      if (planet.id == current_->id)
        planet.terrain_revision = current_->terrain_revision;
    to.push_back(patch);
    from.pop_back();
    message_.set_text(redo ? "Edição de relevo refeita."
                           : "Edição de relevo desfeita por delta.");
    update_actions();
    queue_scene();
  } catch (const std::exception &e) {
    error(e);
  }
}
void CartographyWorkspace::add_terrain_lock() {
  if (!current_ || !terrain_center_)
    return;
  auto *window = owner();
  if (!window)
    return;
  auto *dialog = new OverlayDialog("Bloquear região do terreno", *window, true);
  dialog->set_secondary_text(
      "Edições cujo pincel interseccione esta região serão recusadas. O "
      "bloqueio não altera o relevo.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Bloquear região", Gtk::ResponseType::ACCEPT);
  auto *form = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 8);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text("Região protegida");
  set_accessible_label(*name, "Nome do bloqueio de terreno");
  form->set_margin(16);
  form->append(*Gtk::make_managed<Gtk::Label>("Nome do bloqueio"));
  form->append(*name);
  dialog->get_content_area()->append(*form);
  const auto center = *terrain_center_;
  const auto radius = terrain_radius_.get_value();
  const auto planet_id = current_->id;
  dialog->signal_response().connect(
      [this, dialog, name, center, radius, planet_id](int response) {
        if (response == Gtk::ResponseType::ACCEPT)
          try {
            const geo::TerrainLock lock{project::new_uuid(),
                                        planet_id,
                                        name->get_text().raw(),
                                        center,
                                        radius,
                                        project::utc_now()};
            service_.cartography().add_terrain_lock(lock);
            terrain_preview_.reset();
            refresh_terrain_locks();
            message_.set_text("Região bloqueada. O relevo não foi alterado.");
            update_actions();
            queue_scene();
          } catch (const std::exception &e) {
            error(e);
          }
        dialog->hide();
      });
  dialog->present();
}
void CartographyWorkspace::remove_terrain_lock(const geo::TerrainLock &lock) {
  auto *window = owner();
  if (!window)
    return;
  auto *dialog =
      new OverlayDialog("Remover bloqueio de terreno", *window, true);
  dialog->set_secondary_text(
      "A região voltará a aceitar edições. O relevo atual será preservado.");
  dialog->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  dialog->add_button("Remover bloqueio", Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this, dialog, lock](int response) {
    if (response == Gtk::ResponseType::ACCEPT)
      try {
        service_.cartography().remove_terrain_lock(lock);
        refresh_terrain_locks();
        message_.set_text("Bloqueio removido; terreno preservado.");
        queue_scene();
      } catch (const std::exception &e) {
        error(e);
      }
    dialog->hide();
  });
  dialog->present();
}
void CartographyWorkspace::refresh_terrain_locks() {
  while (auto *child = terrain_lock_list_.get_first_child())
    terrain_lock_list_.remove(*child);
  terrain_locks_.clear();
  if (!current_ || preview_)
    return;
  terrain_locks_ = service_.cartography().terrain_locks(current_->id);
  auto *heading = Gtk::make_managed<Gtk::Label>(
      "Regiões bloqueadas: " + std::to_string(terrain_locks_.size()));
  label(*heading);
  terrain_lock_list_.append(*heading);
  for (const auto &lock : terrain_locks_) {
    auto *button = Gtk::make_managed<Gtk::Button>("Desbloquear: " + lock.name);
    button->set_tooltip_text("Centro " + number(lock.center.longitude, 2) +
                             "°, " + number(lock.center.latitude, 2) +
                             "° · raio " + number(lock.radius_degrees, 1) +
                             "°");
    button->signal_clicked().connect(
        [this, lock] { remove_terrain_lock(lock); });
    terrain_lock_list_.append(*button);
  }
}
} // namespace inde::ui

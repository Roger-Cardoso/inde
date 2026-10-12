#include "inde/ui/author_diagnostics.hpp"
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
void CartographyWorkspace::reveal_removal_use(const project::RemovalUse &use) {
  refresh();
  if (!planets_combo_.set_active_id(use.map_id)) throw std::runtime_error("Mapa de origem não encontrado");
  reveal_entity(use.location_id);
}
namespace geo = project::geo;

CartographyWorkspace::~CartographyWorkspace() {
  worker_.request_stop();
  if (worker_.joinable())
    worker_.join();
  scene_request_.disconnect();
}
Gtk::Window *CartographyWorkspace::owner() {
  return dynamic_cast<Gtk::Window *>(get_root());
}
const geo::Planet *CartographyWorkspace::planet() const {
  return preview_ ? &preview_->planet : (current_ ? &*current_ : nullptr);
}
void CartographyWorkspace::error(const std::exception &e) {
  message_.set_text(std::string{"Cartografia: "} + author::diagnostic(e.what()));
}
void CartographyWorkspace::reset() {
  worker_.request_stop();
  if (worker_.joinable())
    worker_.join();
  scene_request_.disconnect();
  busy_ = false;
  {
    std::lock_guard lock(job_mutex_);
    job_result_.reset();
    job_error_.clear();
  }
  current_.reset();
  preview_.reset();
  planets_.clear();
  camera_ = {};
  scene_ = {};
  selected_local_.reset();
  measure_start_.reset();
  terrain_center_.reset();
  terrain_preview_.reset();
  terrain_locks_.clear();
  undo_.clear();
  redo_.clear();
  terrain_undo_.clear();
  terrain_redo_.clear();
  metrics_.set_text("");
  selection_.set_text("");
  while (auto *child = visible_locals_.get_first_child())
    visible_locals_.remove(*child);
  project_path_.clear();
  service_.cartography().clear_cache();
  refreshing_ = true;
  planets_combo_.remove_all();
  local_selector_->clear_selection();
  refreshing_ = false;
  update_actions();
  canvas_.queue_draw();
}
void CartographyWorkspace::refresh() {
  try {
    const auto path =
        service_.current() ? service_.current()->path().string() : "";
    const bool project_changed = path != project_path_;
    if (project_changed) {
      reset();
      project_path_ = path;
    }
    if (path.empty())
      return;
    planets_ = service_.cartography().planets();
    refreshing_ = true;
    planets_combo_.remove_all();
    for (const auto &p : planets_)
      planets_combo_.append(p.id, p.name);
    if (current_ && !planets_combo_.set_active_id(current_->id))
      current_.reset();
    if (!current_ && !planets_.empty()) {
      current_ = planets_.front();
      planets_combo_.set_active_id(current_->id);
    }
    refreshing_ = false;
    if (project_changed && current_)
      message_.set_text(
          "Planeta carregado do Projeto. Zoom consulta os níveis salvos.");
    local_selector_->refresh();
    refresh_terrain_locks();
    update_actions();
    queue_scene();
  } catch (const std::exception &e) {
    refreshing_ = false;
    error(e);
  }
}
void CartographyWorkspace::update_actions() {
  const bool terrain_preview = terrain_preview_.has_value();
  const bool p = planet() != nullptr,
             editable = p && !preview_ && !busy_ && !terrain_preview;
  preview_bar_.set_visible(busy_ || preview_.has_value());
  accept_button_.set_visible(preview_.has_value());
  discard_button_.set_visible(preview_.has_value());
  cancel_button_.set_visible(busy_);
  export_button_.set_sensitive(p && !busy_ && !terrain_preview);
  delete_map_.set_sensitive(editable);
  place_.set_sensitive(editable && selected_local_.has_value());
  remove_.set_sensitive(editable && selected_local_.has_value());
  locate_.set_sensitive(editable && selected_local_.has_value());
  open_.set_sensitive(selected_local_.has_value() && !preview_);
  undo_button_.set_sensitive(editable && !undo_.empty());
  redo_button_.set_sensitive(editable && !redo_.empty());
  terrain_apply_.set_sensitive(p && !preview_ && !busy_ && terrain_preview);
  terrain_discard_.set_sensitive(terrain_preview);
  terrain_undo_button_.set_sensitive(editable && !terrain_undo_.empty() &&
                                     !terrain_preview);
  terrain_redo_button_.set_sensitive(editable && !terrain_redo_.empty() &&
                                     !terrain_preview);
  terrain_lock_button_.set_sensitive(p && !preview_ && !busy_ &&
                                     terrain_center_.has_value());
  terrain_radius_.set_sensitive(editable && !terrain_preview);
  terrain_strength_.set_sensitive(editable && !terrain_preview);
  mode_.set_sensitive(!terrain_preview);
  new_button_.set_sensitive(service_.current() && !busy_ && !preview_ &&
                            !terrain_preview);
  planets_combo_.set_sensitive(!busy_ && !preview_ && !terrain_preview);
  if (!p)
    message_.set_text("Crie um planeta para começar. O Projeto não depende de "
                      "serviços online.");
}
} // namespace inde::ui

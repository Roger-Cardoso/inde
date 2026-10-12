#include "inde/ui/author_diagnostics.hpp"
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
using cartography_ui_detail::spin;

void CartographyWorkspace::new_planet() {
  if (!owner() || busy_ || preview_)
    return;
  auto *d = new OverlayDialog("Criar planeta cartográfico", *owner(), true);
  d->set_secondary_text(
      "Geração artística local, sem IA. A prévia só será gravada após Aceitar. "
      "C-01 ainda não gera rios, clima ou biomas.");
  d->add_button("Cancelar", Gtk::ResponseType::CANCEL);
  d->add_button("Gerar prévia", Gtk::ResponseType::ACCEPT);
  auto *body = d->get_content_area();
  body->set_margin(16);
  auto *name = Gtk::make_managed<Gtk::Entry>();
  name->set_text("Novo mundo");
  name->set_max_length(120);
  auto *radius = spin(1, 1000000, 100, 6371);
  auto *seed = spin(0, 2147483647, 1, 41852791);
  auto *water = spin(5, 95, 5, 65);
  auto *fragment = spin(1, 8, 1, 3);
  auto *height = spin(100, 30000, 100, 8000);
  auto *depth = spin(100, 30000, 100, 6000);
  append_labeled_form_field(*body, "Nome", *name);
  append_labeled_form_field(*body, "Raio da esfera (km)", *radius);
  append_labeled_form_field(*body, "Seed", *seed);
  append_labeled_form_field(*body, "Água alvo (% da área)", *water);
  append_labeled_form_field(*body, "Fragmentação (1–8)", *fragment);
  append_labeled_form_field(*body, "Elevação máxima (m)", *height);
  append_labeled_form_field(*body, "Profundidade máxima (m)", *depth);
  d->signal_response().connect(
      [this, name, radius, seed, water, fragment, height, depth](int r) {
        if (r != Gtk::ResponseType::ACCEPT)
          return;
        geo::Planet p;
        p.id = project::new_uuid();
        p.name = name->get_text();
        p.radius_m = std::llround(radius->get_value() * 1000);
        p.seed = seed->get_value_as_int();
        p.water_percent = water->get_value_as_int();
        p.fragmentation = fragment->get_value_as_int();
        p.height_m = height->get_value_as_int();
        p.depth_m = depth->get_value_as_int();
        p.created_at = project::utc_now();
        try {
          geo::validate(p);
          generate(std::move(p));
        } catch (const std::exception &e) {
          error(e);
        }
      });
  d->present();
}
void CartographyWorkspace::generate(geo::Planet p) {
  if (worker_.joinable())
    worker_.join();
  busy_ = true;
  job_project_ = project_path_;
  preview_label_.set_text("Gerando terreno em segundo plano…");
  update_actions();
  {
    std::lock_guard lock(job_mutex_);
    job_result_.reset();
    job_error_.clear();
  }
  worker_ = std::jthread([this, p = std::move(p)](std::stop_token stop) {
    try {
      auto result = geo::generate(p, stop);
      std::lock_guard lock(job_mutex_);
      job_result_ = std::move(result);
    } catch (const std::exception &e) {
      std::lock_guard lock(job_mutex_);
      job_error_ = author::diagnostic(e.what());
    }
    completion_.emit();
  });
}
void CartographyWorkspace::generation_finished() {
  if (!busy_)
    return;
  {
    std::lock_guard lock(job_mutex_);
    if (!job_result_ && job_error_.empty())
      return;
  }
  if (worker_.joinable())
    worker_.join();
  busy_ = false;
  std::lock_guard lock(job_mutex_);
  if (worker_.get_stop_token().stop_requested()) {
    job_result_.reset();
    job_error_.clear();
    update_actions();
    message_.set_text("Geração cancelada; nenhum planeta foi gravado.");
    return;
  }
  if (!service_.current() ||
      service_.current()->path().string() != job_project_) {
    job_result_.reset();
    job_error_.clear();
    update_actions();
    return;
  }
  if (!job_error_.empty()) {
    update_actions();
    message_.set_text(job_error_);
    job_error_.clear();
    return;
  }
  preview_ = std::move(job_result_);
  job_result_.reset();
  camera_ = {};
  preview_label_.set_text(
      "PRÉVIA — " + preview_->planet.name +
      " · não salva · 170 chunks / 1024×512 no nível mais fino");
  message_.set_text("Inspecione o terreno. Aceitar cria um novo planeta; "
                    "descartar não altera o Projeto.");
  update_actions();
  queue_scene();
}
void CartographyWorkspace::accept_preview() {
  if (!preview_)
    return;
  try {
    service_.cartography().accept(*preview_);
    current_ = preview_->planet;
    preview_.reset();
    message_.set_text(
        "Planeta salvo no Projeto. Zoom apenas consulta os níveis existentes.");
    refresh();
  } catch (const std::exception &e) {
    error(e);
  }
}
void CartographyWorkspace::discard_preview() {
  preview_.reset();
  camera_ = {};
  update_actions();
  queue_scene();
  message_.set_text("Prévia descartada; nenhum terreno foi gravado.");
}
} // namespace inde::ui

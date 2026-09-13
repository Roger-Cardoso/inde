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
namespace {
constexpr auto local_type = "00000000-0000-4000-9000-000000000002";
void label(Gtk::Label &v) {
  v.set_wrap(true);
  v.set_xalign(0);
}
Gtk::SpinButton *spin(double low, double high, double step, double value,
                      int digits = 0) {
  auto *s = Gtk::make_managed<Gtk::SpinButton>();
  s->set_range(low, high);
  s->set_increments(step, step * 10);
  s->set_digits(digits);
  s->set_value(value);
  return s;
}
std::string number(double v, int digits = 2) {
  std::ostringstream s;
  s << std::fixed << std::setprecision(digits) << v;
  return s.str();
}
} // namespace
CartographyWorkspace::CartographyWorkspace(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), service_(service) {
  set_hexpand(true);
  set_vexpand(true);
  planets_combo_.set_hexpand(true);
  toolbar_.append(planets_combo_);
  toolbar_.append(new_button_);
  toolbar_.append(export_button_);
  append(toolbar_);
  set_accessible_label(planets_combo_, "Planeta cartográfico");
  preview_label_.set_hexpand(true);
  label(preview_label_);
  preview_bar_.append(preview_label_);
  preview_bar_.append(accept_button_);
  preview_bar_.append(discard_button_);
  preview_bar_.append(cancel_button_);
  append(preview_bar_);
  layer_.append("land", "Terras e oceanos");
  layer_.append("relief", "Relevo");
  layer_.append("depth", "Profundidade oceânica");
  layer_.set_active_id("relief");
  rendering_.append("smooth", "Visual suave");
  rendering_.append("raster", "Visual raster");
  rendering_.set_active_id("smooth");
  coastlines_.set_active(true);
  coastlines_.set_tooltip_text(
      "Realça somente o limite terra–água; não cria fronteiras políticas.");
  mode_.append("pan", "Navegar");
  mode_.append("measure", "Medir distância");
  mode_.append("place", "Posicionar Local");
  mode_.append("raise", "Elevar terreno");
  mode_.append("lower", "Rebaixar terreno");
  mode_.append("smooth", "Suavizar terreno");
  mode_.set_active_id("pan");
  set_accessible_label(layer_, "Camada cartográfica");
  set_accessible_label(rendering_, "Aparência do terreno");
  set_accessible_label(mode_, "Ferramenta do mapa");
  set_accessible_label(plus_, "Aproximar mapa");
  set_accessible_label(minus_, "Afastar mapa");
  navigation_.append(minus_);
  navigation_.append(plus_);
  navigation_.append(fit_);
  navigation_.append(layer_);
  navigation_.append(rendering_);
  navigation_.append(coastlines_);
  navigation_.append(mode_);
  append(navigation_);
  canvas_.set_hexpand(true);
  canvas_.set_vexpand(true);
  canvas_.set_focusable(true);
  canvas_.set_size_request(340, 240);
  set_accessible_label(canvas_, "Mapa cartográfico do planeta");
  set_accessible_description(
      canvas_,
      "Arraste para navegar. Use mais e menos ou a roda para zoom. Escolha "
      "visual suave ou raster e ative o contorno de costa quando necessário. "
      "Setas movem a câmera. A lista lateral também seleciona Locais.");
  canvas_.set_draw_func(sigc::mem_fun(*this, &CartographyWorkspace::draw));
  canvas_.signal_resize().connect([this, positioned = false](int, int) mutable {
    if (!positioned && split_.get_width() > 600) {
      positioned = true;
      split_.set_position(split_.get_width() - 360);
    }
    queue_scene();
  });
  split_.set_start_child(canvas_);
  split_.set_end_child(sidebar_scroll_);
  split_.set_resize_start_child(true);
  split_.set_resize_end_child(false);
  split_.set_shrink_start_child(false);
  split_.set_shrink_end_child(false);
  split_.set_position(850);
  split_.set_vexpand(true);
  append(split_);
  sidebar_scroll_.set_policy(Gtk::PolicyType::NEVER,
                             Gtk::PolicyType::AUTOMATIC);
  sidebar_scroll_.set_size_request(260, -1);
  sidebar_scroll_.set_child(sidebar_);
  sidebar_.set_margin(12);
  help_.set_text("C-01 · esfera / equiretangular\nTerreno artístico, local e "
                 "sem IA. Zoom não gera novos dados.\nArraste para navegar; na "
                 "medição, clique em dois pontos.");
  label(help_);
  sidebar_.append(help_);
  terrain_help_.set_text(
      "Escolha um modo de terreno e clique no mapa. A prévia não altera o "
      "Projeto até Aplicar relevo.");
  label(terrain_help_);
  terrain_box_.set_margin(8);
  terrain_box_.append(terrain_help_);
  terrain_radius_.set_range(0.5, 15);
  terrain_radius_.set_increments(0.5, 2);
  terrain_radius_.set_digits(1);
  terrain_radius_.set_value(3);
  terrain_strength_.set_range(50, 4000);
  terrain_strength_.set_increments(50, 500);
  terrain_strength_.set_value(500);
  set_accessible_label(terrain_radius_, "Raio do pincel em graus");
  set_accessible_label(terrain_strength_, "Intensidade do pincel em metros");
  auto *radius_label = Gtk::make_managed<Gtk::Label>("Raio (°)");
  auto *strength_label = Gtk::make_managed<Gtk::Label>("Intensidade (m)");
  terrain_parameters_.append(*radius_label);
  terrain_parameters_.append(terrain_radius_);
  terrain_parameters_.append(*strength_label);
  terrain_parameters_.append(terrain_strength_);
  terrain_box_.append(terrain_parameters_);
  terrain_box_.append(terrain_apply_);
  terrain_box_.append(terrain_discard_);
  terrain_box_.append(terrain_undo_button_);
  terrain_box_.append(terrain_redo_button_);
  terrain_box_.append(terrain_lock_button_);
  terrain_box_.append(terrain_lock_list_);
  terrain_frame_.set_child(terrain_box_);
  sidebar_.append(terrain_frame_);
  local_selector_ = Gtk::make_managed<IncrementalSelector>(
      "Pesquisar Local do Projeto",
      [this](const std::string &search, std::size_t limit) {
        persistence::EntityQuery q;
        q.entity_type_ids = {local_type};
        q.search = search;
        q.limit = std::min(limit, std::size_t{50});
        std::vector<IncrementalSelection> out;
        if (service_.current())
          for (const auto &e : service_.narrative().entities(q))
            out.push_back(
                {e.id, e.name, "Local compartilhado com Planejamento"});
        return out;
      });
  sidebar_.append(*local_selector_);
  sidebar_.append(locate_);
  sidebar_.append(place_);
  sidebar_.append(open_);
  sidebar_.append(remove_);
  label(selection_);
  sidebar_.append(selection_);
  sidebar_.append(undo_button_);
  sidebar_.append(redo_button_);
  label(metrics_);
  metrics_.add_css_class("dim-label");
  sidebar_.append(metrics_);
  sidebar_.append(visible_locals_);
  label(message_);
  message_.set_margin(4);
  append(message_);
  new_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::new_planet));
  accept_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::accept_preview));
  discard_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::discard_preview));
  cancel_button_.signal_clicked().connect([this] {
    worker_.request_stop();
    message_.set_text("Cancelando geração local…");
  });
  export_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::export_svg));
  plus_.signal_clicked().connect([this] { zoom(1.5); });
  minus_.signal_clicked().connect([this] { zoom(1 / 1.5); });
  fit_.signal_clicked().connect([this] {
    camera_ = {};
    queue_scene();
  });
  layer_.signal_changed().connect([this] { queue_scene(); });
  rendering_.signal_changed().connect([this] { canvas_.queue_draw(); });
  coastlines_.signal_toggled().connect([this] { canvas_.queue_draw(); });
  mode_.signal_changed().connect([this] {
    measure_start_.reset();
    update_actions();
  });
  terrain_radius_.signal_value_changed().connect([this] {
    if (!terrain_preview_)
      canvas_.queue_draw();
  });
  planets_combo_.signal_changed().connect([this] {
    if (refreshing_)
      return;
    const auto id = planets_combo_.get_active_id().raw();
    auto it = std::find_if(planets_.begin(), planets_.end(),
                           [&](const auto &p) { return p.id == id; });
    current_ =
        it == planets_.end() ? std::nullopt : std::optional<geo::Planet>{*it};
    camera_ = {};
    undo_.clear();
    redo_.clear();
    terrain_preview_.reset();
    terrain_center_.reset();
    terrain_undo_.clear();
    terrain_redo_.clear();
    measure_start_.reset();
    selected_local_.reset();
    local_selector_->clear_selection();
    refresh_terrain_locks();
    queue_scene();
    update_actions();
  });
  local_selector_->signal_selection_changed().connect([this] {
    selected_local_ = local_selector_->selected_id();
    update_actions();
  });
  locate_.signal_clicked().connect([this] {
    if (selected_local_)
      reveal_entity(*selected_local_);
  });
  open_.signal_clicked().connect([this] {
    if (selected_local_)
      entity_source_.emit(*selected_local_);
  });
  place_.signal_clicked().connect(
      [this] { place_local({camera_.longitude, camera_.latitude}); });
  remove_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::unplace_local));
  undo_button_.signal_clicked().connect([this] { history(false); });
  redo_button_.signal_clicked().connect([this] { history(true); });
  terrain_apply_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::apply_terrain_preview));
  terrain_discard_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::discard_terrain_preview));
  terrain_undo_button_.signal_clicked().connect(
      [this] { terrain_history(false); });
  terrain_redo_button_.signal_clicked().connect(
      [this] { terrain_history(true); });
  terrain_lock_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &CartographyWorkspace::add_terrain_lock));
  auto scroll = Gtk::EventControllerScroll::create();
  scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
  scroll->signal_scroll().connect(
      [this](double, double dy) {
        zoom(dy > 0 ? 1 / 1.2 : 1.2);
        return true;
      },
      false);
  canvas_.add_controller(scroll);
  auto drag = Gtk::GestureDrag::create();
  drag->set_button(1);
  drag->signal_drag_begin().connect([this](double, double) {
    drag_camera_ = camera_;
    dragged_ = false;
  });
  drag->signal_drag_update().connect([this](double dx, double dy) {
    const auto mode = mode_.get_active_id();
    if (mode == "measure" || mode == "place" || !planet())
      return;
    if (std::abs(dx) + std::abs(dy) > 4)
      dragged_ = true;
    const auto v =
        application::map_window(drag_camera_, std::max(1, canvas_.get_width()),
                                std::max(1, canvas_.get_height()));
    camera_.longitude =
        geo::normalize_longitude(drag_camera_.longitude - dx / v.scale);
    camera_.latitude =
        std::clamp(drag_camera_.latitude + dy / v.scale, -90.0, 90.0);
    queue_scene();
  });
  canvas_.add_controller(drag);
  auto pointer = Gtk::GestureClick::create();
  pointer->set_button(1);
  pointer->signal_released().connect([this](int, double x, double y) {
    if (!dragged_)
      click(x, y);
  });
  canvas_.add_controller(pointer);
  auto keys = Gtk::EventControllerKey::create();
  keys->signal_key_pressed().connect(
      [this](guint key, guint, Gdk::ModifierType) {
        if (key == GDK_KEY_plus || key == GDK_KEY_equal) {
          zoom(1.5);
          return true;
        }
        if (key == GDK_KEY_minus) {
          zoom(1 / 1.5);
          return true;
        }
        const double step = 15 / camera_.zoom;
        if (key == GDK_KEY_Left)
          camera_.longitude =
              geo::normalize_longitude(camera_.longitude - step);
        else if (key == GDK_KEY_Right)
          camera_.longitude =
              geo::normalize_longitude(camera_.longitude + step);
        else if (key == GDK_KEY_Up)
          camera_.latitude = std::min(90.0, camera_.latitude + step);
        else if (key == GDK_KEY_Down)
          camera_.latitude = std::max(-90.0, camera_.latitude - step);
        else
          return false;
        queue_scene();
        return true;
      },
      false);
  canvas_.add_controller(keys);
  completion_.connect(
      sigc::mem_fun(*this, &CartographyWorkspace::generation_finished));
  update_actions();
}
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
  message_.set_text(std::string{"Cartografia: "} + e.what());
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
      job_error_ = e.what();
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
void CartographyWorkspace::zoom(double factor) {
  if (!planet())
    return;
  camera_.zoom = std::clamp(camera_.zoom * factor, 1.0, 64.0);
  queue_scene();
}
void CartographyWorkspace::queue_scene() {
  if (scene_request_.connected())
    return;
  scene_request_ = Glib::signal_timeout().connect(
      [this] {
        rebuild_scene();
        return false;
      },
      20);
}
void CartographyWorkspace::rebuild_scene() {
  try {
    const auto *p = planet();
    if (!p) {
      scene_ = {};
      canvas_.queue_draw();
      return;
    }
    const int w = std::max(1, canvas_.get_width()),
              h = std::max(1, canvas_.get_height());
    const auto layer =
        layer_.get_active_id() == "land"    ? application::MapLayer::LandWater
        : layer_.get_active_id() == "depth" ? application::MapLayer::OceanDepth
                                            : application::MapLayer::Relief;
    const auto started = std::chrono::steady_clock::now();
    if (preview_) {
      const int level = application::map_level(*p, camera_, w, h);
      std::set<geo::ChunkKey> keys;
      for (auto b : application::map_window(camera_, w, h).ranges)
        for (auto k : geo::visible_chunks(b, level))
          keys.insert(k);
      std::vector<geo::TerrainChunk> chunks;
      for (const auto &t : preview_->chunks)
        if (keys.contains(t.key))
          chunks.push_back(t);
      scene_ =
          application::build_map_scene(*p, camera_, w, h, layer, chunks, {});
    } else
      scene_ = service_.cartography().scene(
          *p, camera_, w, h, layer,
          terrain_preview_ ? &*terrain_preview_ : nullptr);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - started)
                          .count();
    metrics_.set_text(
        "Zoom " + number(camera_.zoom, 1) + "× · LOD " +
        std::to_string(scene_.level) + "/" + std::to_string(p->detail) + "\n" +
        std::to_string(scene_.active_chunks) + " chunks ativos · " +
        std::to_string(scene_.loaded_chunks) + " lidos\n" +
        std::to_string(scene_.cells.size()) + " células · " +
        std::to_string(scene_.markers.size()) + " Locais\nCache " +
        number(scene_.cache_bytes / 1024.0, 0) + " KiB · render ~" +
        number(scene_.render_bytes / 1024.0, 0) + " KiB · cena " + number(ms) +
        " ms" +
        (camera_.zoom > 8 ? "\nLimite de detalhe disponível: zoom adicional "
                            "não cria terreno."
                          : "") +
        (scene_.limited ? "\nVista limitada pelo orçamento cartográfico."
                        : "") +
        (scene_.missing_chunks
             ? "\nATENÇÃO: chunks ausentes; não foram regenerados."
             : ""));
    while (auto *child = visible_locals_.get_first_child())
      visible_locals_.remove(*child);
    auto *heading = Gtk::make_managed<Gtk::Label>("Locais da vista (até 30)");
    label(*heading);
    visible_locals_.append(*heading);
    for (std::size_t i = 0;
         i < std::min(std::size_t{30}, scene_.markers.size()); ++i) {
      const auto &m = scene_.markers[i];
      auto *b = Gtk::make_managed<Gtk::Button>();
      auto *l = Gtk::make_managed<Gtk::Label>(m.local.name);
      l->set_ellipsize(Pango::EllipsizeMode::END);
      l->set_max_width_chars(24);
      b->set_child(*l);
      b->signal_clicked().connect(
          [this, id = m.local.position.entity_id, name = m.local.name] {
            selected_local_ = id;
            local_selector_->set_selected(id, name);
            reveal_entity(id);
          });
      visible_locals_.append(*b);
    }
    canvas_.queue_draw();
  } catch (const std::exception &e) {
    scene_ = {};
    canvas_.queue_draw();
    error(e);
  }
}
void CartographyWorkspace::draw(const Cairo::RefPtr<Cairo::Context> &cr, int w,
                                int h) {
  cr->set_source_rgb(0.06, 0.09, 0.12);
  cr->paint();
  if (!planet()) {
    cr->set_source_rgb(0.8, 0.84, 0.9);
    cr->set_font_size(20);
    cr->move_to(30, h / 2.0);
    cr->show_text("Seu mundo começa com um planeta.");
    return;
  }
  cr->save();
  cr->rectangle(0, 0, w, h);
  cr->clip();
  if (!scene_.terrain_tiles.empty()) {
    const auto filter = rendering_.get_active_id() == "raster"
                            ? Cairo::SurfacePattern::Filter::NEAREST
                            : Cairo::SurfacePattern::Filter::BILINEAR;
    for (auto &tile : scene_.terrain_tiles) {
      auto surface = Cairo::ImageSurface::create(
          reinterpret_cast<unsigned char *>(tile.argb.data()),
          Cairo::Surface::Format::ARGB32, tile.side, tile.side, tile.side * 4);
      auto pattern = Cairo::SurfacePattern::create(surface);
      pattern->set_filter(filter);
      cr->save();
      constexpr double seam_overlap = 0.75;
      cr->set_antialias(Cairo::ANTIALIAS_NONE);
      cr->rectangle(tile.clip_x - seam_overlap, tile.clip_y - seam_overlap,
                    tile.clip_width + 2 * seam_overlap,
                    tile.clip_height + 2 * seam_overlap);
      cr->clip();
      cr->set_antialias(Cairo::ANTIALIAS_DEFAULT);
      cr->translate(tile.paint_x, tile.paint_y);
      cr->scale(tile.sample_pixels, tile.sample_pixels);
      cr->set_source(pattern);
      cr->paint();
      cr->restore();
    }
  } else {
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    for (const auto &r : scene_.cells) {
      cr->set_source_rgb(r.color.red, r.color.green, r.color.blue);
      cr->rectangle(r.x, r.y, r.width + 0.5, r.height + 0.5);
      cr->fill();
    }
  }
  cr->set_antialias(Cairo::ANTIALIAS_DEFAULT);
  if (coastlines_.get_active()) {
    cr->set_source_rgba(0.98, 0.86, 0.65, 0.78);
    cr->set_line_width(1.25);
    for (const auto &line : scene_.coastlines) {
      cr->move_to(line.x1, line.y1);
      cr->line_to(line.x2, line.y2);
    }
    cr->stroke();
  }
  for (const auto &m : scene_.markers) {
    cr->set_source_rgb(1, 0.89, 0.64);
    if (m.local.position.symbol == "mountain") {
      cr->move_to(m.x, m.y - 7);
      cr->line_to(m.x - 6, m.y + 5);
      cr->line_to(m.x + 6, m.y + 5);
      cr->close_path();
    } else if (m.local.position.symbol == "city")
      cr->rectangle(m.x - 5, m.y - 5, 10, 10);
    else
      cr->arc(m.x, m.y, 5, 0, 2 * std::numbers::pi);
    cr->fill_preserve();
    cr->set_source_rgb(0.10, 0.13, 0.18);
    cr->set_line_width(2);
    cr->stroke();
    if (m.local.position.approximate) {
      cr->set_source_rgb(1, 0.89, 0.64);
      cr->set_dash(std::vector<double>{2, 3}, 0);
      cr->arc(m.x, m.y, 10, 0, 2 * std::numbers::pi);
      cr->stroke();
      cr->unset_dash();
    }
    if (m.label) {
      cr->set_font_size(12);
      cr->move_to(m.x + 10, m.y - 5);
      cr->text_path(m.label_text);
      cr->set_source_rgb(0.06, 0.09, 0.12);
      cr->set_line_width(3);
      cr->stroke_preserve();
      cr->set_source_rgb(1, 0.96, 0.84);
      cr->fill();
    }
  }
  const auto view = application::map_window(camera_, w, h);
  const auto ring = [&](geo::Coordinate center, double radius, double red,
                        double green, double blue) {
    const double x = w / 2.0 + geo::normalize_longitude(center.longitude -
                                                        camera_.longitude) *
                                   view.scale;
    const double y =
        h / 2.0 + (camera_.latitude - center.latitude) * view.scale;
    cr->set_source_rgba(red, green, blue, 0.9);
    cr->set_line_width(2);
    cr->set_dash(std::vector<double>{5, 4}, 0);
    cr->arc(x, y, radius * view.scale, 0, 2 * std::numbers::pi);
    cr->stroke();
    cr->unset_dash();
  };
  for (const auto &lock : terrain_locks_)
    ring(lock.center, lock.radius_degrees, 1.0, 0.55, 0.35);
  if (terrain_center_)
    ring(*terrain_center_, terrain_radius_.get_value(), 0.45, 0.9, 1.0);
  cr->restore();
}
void CartographyWorkspace::click(double x, double y) {
  if (!planet())
    return;
  canvas_.grab_focus();
  const auto point = application::map_coordinate(camera_, canvas_.get_width(),
                                                 canvas_.get_height(), x, y);
  if (!point)
    return;
  if (mode_.get_active_id() == "measure") {
    if (!measure_start_) {
      measure_start_ = point;
      message_.set_text("Primeiro ponto definido. Clique no segundo ponto.");
    } else {
      const double km =
          geo::distance_m(*measure_start_, *point, planet()->radius_m) / 1000.0;
      message_.set_text("Distância sobre a esfera: " + number(km) +
                        " km. Não é distância de rota nem tempo de viagem.");
      measure_start_.reset();
    }
    return;
  }
  if (mode_.get_active_id() == "place") {
    place_local(*point);
    return;
  }
  if (mode_.get_active_id() == "raise" || mode_.get_active_id() == "lower" ||
      mode_.get_active_id() == "smooth") {
    preview_terrain(*point);
    return;
  }
  for (const auto &m : scene_.markers)
    if (std::hypot(x - m.x, y - m.y) < 12) {
      selected_local_ = m.local.position.entity_id;
      local_selector_->set_selected(*selected_local_, m.local.name);
      reveal_entity(*selected_local_);
      return;
    }
  selection_.set_text("Longitude " + number(point->longitude, 4) +
                      "°\nLatitude " + number(point->latitude, 4) + "°");
}
void CartographyWorkspace::reveal_entity(const std::string &id) {
  try {
    if (!current_ || preview_)
      return;
    const auto entity = service_.narrative().entity(id);
    if (!entity || entity->entity_type_id != local_type) {
      message_.set_text(
          "A cartografia inicial posiciona entidades do tipo Local.");
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
          "\nLon " + number(p->coordinate.longitude, 6) + "° · Lat " +
          number(p->coordinate.latitude, 6) + "°");
      queue_scene();
    } else
      selection_.set_text(entity->name +
                          " ainda não possui posição neste planeta. Use "
                          "Definir coordenadas ou Posicionar Local.");
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
    auto *d = new OverlayDialog("Posicionar Local", *owner(), true);
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
    append_labeled_form_field(*body, "Visível a partir do LOD (0–6)", *lod);
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
        message_.set_text("Posição salva. A entidade continua sendo a mesma do "
                          "Planejamento.");
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
void CartographyWorkspace::export_svg() {
  if (!planet() || !owner())
    return;
  try {
    rebuild_scene();
    const auto data = application::map_svg(
        scene_, planet()->name + (preview_ ? " — prévia" : ""),
        rendering_.get_active_id() != "raster", coastlines_.get_active());
    auto dialog = Gtk::FileChooserNative::create(
        "Exportar vista cartográfica", *owner(), Gtk::FileChooser::Action::SAVE,
        "Exportar", "Cancelar");
    dialog->set_current_name("mapa-inde.svg");
    dialog->signal_response().connect([this, dialog, data](int r) {
      if (r == Gtk::ResponseType::ACCEPT)
        try {
          auto file = dialog->get_file();
          if (!file)
            throw std::runtime_error("Destino inválido");
          std::string etag;
          file->replace_contents(data, "", etag, false,
                                 Gio::File::CreateFlags::NONE);
          message_.set_text(
              "Vista exportada como SVG. O planeta continua no Projeto.");
        } catch (const std::exception &e) {
          error(e);
        }
    });
    dialog->show();
  } catch (const std::exception &e) {
    error(e);
  }
}
} // namespace inde::ui

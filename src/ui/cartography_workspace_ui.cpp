#include "cartography_ui_helpers.hpp"
#include "inde/ui/cartography_workspace.hpp"
#include "inde/project/manifest.hpp"
#include "inde/ui/accessibility.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/project_operations.hpp"
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
using cartography_ui_detail::local_type;

CartographyWorkspace::CartographyWorkspace(application::ProjectService &service)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8), service_(service) {
  set_hexpand(true);
  set_vexpand(true);
  planets_combo_.set_hexpand(true);
  toolbar_.append(planets_combo_);
  toolbar_.append(new_button_);
  toolbar_.append(export_button_);
  toolbar_.append(delete_map_);
  delete_map_.set_tooltip_text("Conferir exclusão do mapa selecionado; Locais do Projeto serão preservados");
  delete_map_.signal_clicked().connect([this] {
    if (!current_ || preview_ || busy_ || terrain_preview_ || !owner())
      return;
    confirm_project_removal(*owner(), service_, project::RemovalKind::CartographicMap,
                            current_->id, [this] {
                              reset();
                              refresh();
                              message_.set_text("Mapa excluído; Locais preservados. Reversão disponível no histórico do Projeto.");
                            });
  });
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
  rendering_.append("raster", "Visual em pixels");
  rendering_.set_active_id("smooth");
  coastlines_.set_active(true);
  coastlines_.set_tooltip_text(
      "Realça somente o limite terra–água; não cria fronteiras políticas.");
  mode_.append("pan", "Navegar");
  mode_.append("measure", "Medir distância");
  mode_.append("place", "Posicionar lugar");
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
      "Arraste para navegar. Use mais e menos ou a roda para zoom. Escolha visual suave ou em pixels e ative o contorno de costa quando necessário. Setas movem a câmera. A lista lateral também seleciona Locais.");
  canvas_.set_draw_func(sigc::mem_fun(*this, &CartographyWorkspace::draw));
  signal_application_theme_changed().connect(sigc::track_object([this] { canvas_.queue_draw(); }, *this));
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
  help_.set_text("Arraste para explorar o mapa. Para medir uma distância, "
                 "escolha Medir e clique em dois pontos.");
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
      [this](guint key, guint, Gdk::ModifierType modifiers) {
        const auto mask=static_cast<unsigned>(modifiers)&(GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_ALT_MASK|GDK_SUPER_MASK|GDK_META_MASK|GDK_HYPER_MASK);
        if(mask!=0 && !(mask==GDK_SHIFT_MASK && key==GDK_KEY_plus))return false;
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
} // namespace inde::ui

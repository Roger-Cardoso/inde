#pragma once
#include "inde/application/project_service.hpp"
#include "inde/ui/incremental_selector.hpp"
#include <gtkmm.h>
#include <mutex>
#include <thread>

namespace inde::ui {
class CartographyWorkspace final : public Gtk::Box {
public:
  explicit CartographyWorkspace(application::ProjectService &);
  ~CartographyWorkspace() override;
  void refresh();
  void reset();
  void reveal_entity(const std::string &);
  sigc::signal<void(const std::string &)> &signal_entity_source_requested() {
    return entity_source_;
  }

private:
  void new_planet();
  void generate(project::geo::Planet);
  void generation_finished();
  void accept_preview();
  void discard_preview();
  void queue_scene();
  void rebuild_scene();
  void draw(const Cairo::RefPtr<Cairo::Context> &, int, int);
  void click(double, double);
  void zoom(double);
  void place_local(project::geo::Coordinate);
  void unplace_local();
  void history(bool redo);
  void preview_terrain(project::geo::Coordinate);
  void apply_terrain_preview();
  void discard_terrain_preview();
  void terrain_history(bool redo);
  void add_terrain_lock();
  void remove_terrain_lock(const project::geo::TerrainLock &);
  void refresh_terrain_locks();
  void export_svg();
  void error(const std::exception &);
  void update_actions();
  Gtk::Window *owner();
  const project::geo::Planet *planet() const;
  application::ProjectService &service_;
  std::string project_path_;
  std::vector<project::geo::Planet> planets_;
  std::optional<project::geo::Planet> current_;
  std::optional<project::geo::GeneratedPlanet> preview_;
  application::MapCamera camera_, drag_camera_;
  application::MapScene scene_;
  std::optional<project::geo::Coordinate> measure_start_;
  std::optional<project::geo::Coordinate> terrain_center_;
  std::optional<project::geo::TerrainPatch> terrain_preview_;
  std::vector<project::geo::TerrainLock> terrain_locks_;
  std::optional<std::string> selected_local_;
  bool refreshing_{}, busy_{}, dragged_{};
  struct Edit {
    std::string planet, entity;
    std::optional<project::geo::Position> before, after;
  };
  std::vector<Edit> undo_, redo_;
  std::vector<project::geo::TerrainPatch> terrain_undo_, terrain_redo_;
  Gtk::Box toolbar_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::ComboBoxText planets_combo_;
  Gtk::Button new_button_{"Criar planeta"}, export_button_{"Exportar SVG"};
  Gtk::Box preview_bar_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label preview_label_;
  Gtk::Button accept_button_{"Aceitar prévia"},
      discard_button_{"Descartar prévia"}, cancel_button_{"Cancelar geração"};
  Gtk::Box navigation_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button minus_{"−"}, plus_{"+"}, fit_{"Ver planeta"};
  Gtk::ComboBoxText layer_, rendering_, mode_;
  Gtk::CheckButton coastlines_{"Contorno de costa"};
  Gtk::Button undo_button_{"Desfazer posição"}, redo_button_{"Refazer"};
  Gtk::Paned split_{Gtk::Orientation::HORIZONTAL};
  Gtk::DrawingArea canvas_;
  Gtk::ScrolledWindow sidebar_scroll_;
  Gtk::Box sidebar_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Label help_, selection_, metrics_, message_;
  IncrementalSelector *local_selector_{};
  Gtk::Button locate_{"Encontrar no planeta"}, place_{"Definir coordenadas"},
      open_{"Abrir no Planejamento"}, remove_{"Remover posição"};
  Gtk::Frame terrain_frame_{"Editar relevo · C-02"};
  Gtk::Box terrain_box_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Label terrain_help_;
  Gtk::SpinButton terrain_radius_, terrain_strength_;
  Gtk::Box terrain_parameters_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::Button terrain_apply_{"Aplicar relevo"},
      terrain_discard_{"Descartar prévia"},
      terrain_undo_button_{"Desfazer relevo"},
      terrain_redo_button_{"Refazer relevo"},
      terrain_lock_button_{"Bloquear área do pincel"};
  Gtk::Box terrain_lock_list_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Box visible_locals_{Gtk::Orientation::VERTICAL, 4};
  sigc::connection scene_request_;
  Glib::Dispatcher completion_;
  std::mutex job_mutex_;
  std::optional<project::geo::GeneratedPlanet> job_result_;
  std::string job_error_, job_project_;
  std::jthread worker_;
  sigc::signal<void(const std::string &)> entity_source_;
};
} // namespace inde::ui

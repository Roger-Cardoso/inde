#pragma once

#include "inde/application/project_service.hpp"
#include "inde/application/timeline_projection.hpp"
#include "inde/ui/timeline_view.hpp"

#include <gtkmm.h>
#include <optional>
#include <vector>

namespace inde::ui {

class GraphsWorkspace final : public Gtk::Box {
public:
  explicit GraphsWorkspace(application::ProjectService &service);

  void refresh();
  void reset();
  void toggle_inspector();
  [[nodiscard]] sigc::signal<void(const Glib::ustring &)> &
  signal_status_message() {
    return signal_status_message_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_entity_source_requested() {
    return signal_entity_source_requested_;
  }
  [[nodiscard]] sigc::signal<void(const std::string &)> &
  signal_time_point_source_requested() {
    return signal_time_point_source_requested_;
  }

private:
  static constexpr std::size_t query_limit_ = 500;
  static constexpr std::size_t agenda_row_limit_ = 300;

  void build_ui();
  void refresh_context_controls();
  void refresh_axis_points();
  void update_window_controls();
  void store_shared_context_from_controls();
  void refresh_snapshot();
  void apply_filters();
  void update_lens();
  void update_situation();
  void update_agenda();
  void reset_inspector();
  void update_zoom(double value);
  void open_selection_source();
  void on_timeline_painted(double milliseconds);
  void show_selection(application::TimelineVisualKind kind,
                      const std::string &id);
  void show_failure(const std::exception &error);

  application::ProjectService &service_;
  Gtk::Box header_{Gtk::Orientation::HORIZONTAL, 10};
  Gtk::Label title_{"Linha do tempo ficcional"};
  Gtk::Label axis_label_{"Eixo:"};
  Gtk::ComboBoxText axis_combo_;
  Gtk::Button refresh_button_{"Atualizar"};
  Gtk::Box context_controls_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::ScrolledWindow context_controls_scroll_;
  Gtk::Label work_label_{"Obra:"};
  Gtk::ComboBoxText work_combo_;
  Gtk::Label window_label_{"Recorte:"};
  Gtk::ComboBoxText window_combo_;
  Gtk::ComboBoxText start_point_combo_;
  Gtk::ComboBoxText end_point_combo_;
  Gtk::Button clear_window_button_{"Limpar período"};
  Gtk::Button clear_context_button_{"Limpar contexto"};
  Gtk::Label lens_label_{"Lente:"};
  Gtk::ComboBoxText lens_combo_;
  Gtk::Box controls_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::ScrolledWindow controls_scroll_;
  Gtk::Entry search_;
  Gtk::ToggleButton events_filter_{"Acontecimentos visíveis"};
  Gtk::ToggleButton presences_filter_{"Presenças visíveis"};
  Gtk::Label zoom_title_{"Zoom:"};
  Gtk::Button zoom_out_button_{"−"};
  Gtk::Label zoom_label_{"100%"};
  Gtk::Button zoom_in_button_{"+"};
  Gtk::Button zoom_reset_button_{"Restaurar"};
  Gtk::Paned split_{Gtk::Orientation::HORIZONTAL};
  Gtk::Stack lens_stack_;
  Gtk::ScrolledWindow timeline_scroll_;
  TimelineView timeline_;
  Gtk::ScrolledWindow situation_scroll_;
  Gtk::Box situation_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Label situation_title_{"Situação temporal"};
  Gtk::Label situation_context_;
  Gtk::Label situation_state_;
  Gtk::ListBox situation_list_;
  Gtk::ScrolledWindow agenda_scroll_;
  Gtk::Box agenda_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Label agenda_title_{"Agenda cronológica"};
  Gtk::Label agenda_context_;
  Gtk::ListBox agenda_list_;
  Gtk::ScrolledWindow inspector_scroll_;
  Gtk::Box inspector_{Gtk::Orientation::VERTICAL, 10};
  Gtk::Label inspector_title_{"Inspetor temporal"};
  Gtk::Label inspector_kind_;
  Gtk::Label inspector_name_;
  Gtk::Label inspector_position_;
  Gtk::Label inspector_description_;
  Gtk::Label inspector_reason_;
  Gtk::Button open_source_button_{"Abrir origem no Planejamento"};
  Gtk::Label state_;
  Gtk::Label paint_metric_;
  std::vector<project::FictionalTimeAxis> axes_;
  std::vector<project::Work> works_;
  std::vector<project::FictionalTimePoint> axis_points_;
  std::optional<application::TemporalViewSnapshot> view_snapshot_;
  std::optional<application::TimelineSnapshot> snapshot_;
  std::optional<std::string> selected_source_entity_id_;
  std::optional<std::string> selected_source_point_id_;
  bool refreshing_axis_{};
  bool refreshing_context_{};
  bool inspector_visible_{};
  std::size_t paint_samples_{};
  double last_paint_ms_{};
  sigc::signal<void(const Glib::ustring &)> signal_status_message_;
  sigc::signal<void(const std::string &)> signal_entity_source_requested_;
  sigc::signal<void(const std::string &)> signal_time_point_source_requested_;
};

} // namespace inde::ui

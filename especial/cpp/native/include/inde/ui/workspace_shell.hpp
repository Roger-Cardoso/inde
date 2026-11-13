#pragma once
#include <gtkmm.h>
#include <string_view>
namespace inde::ui {
enum class WorkspaceId {
  Catalog,
  Planning,
  Writing,
  Editorial,
  Graphs,
  Cartography,
  Review,
  Comic
};
[[nodiscard]] std::string_view workspace_name(WorkspaceId) noexcept;
class WorkspaceShell final : public Gtk::Box {
public:
  WorkspaceShell();
  void add_workspace(Gtk::Widget &, WorkspaceId);
  void show(WorkspaceId);
  void refresh_vocabulary();
  Gtk::Widget &navigation_region() { return navigation_; }
  WorkspaceId current() const noexcept { return current_; }
  void set_focus_mode(bool focus) { navigation_scroll_.set_visible(!focus); }

private:
  friend struct ComicWorkspaceProbe;
  Gtk::Box navigation_{Gtk::Orientation::HORIZONTAL, 4},
      create_choices_{Gtk::Orientation::VERTICAL, 4};
  Gtk::ScrolledWindow navigation_scroll_;
  Gtk::MenuButton create_button_;
  Gtk::Popover create_popover_;
  Gtk::ToggleButton writing_button_{"Escrita"}, comic_button_{"Comic"},
      planning_button_{"Planejar"}, review_button_{"Revisar"},
      editorial_button_{"Manuscrito"}, graphs_button_{"Cronologia"},
      maps_button_{"Mapas"}, works_button_{"Obras"};
  Gtk::Stack stack_;
  WorkspaceId current_{WorkspaceId::Writing};
};
} // namespace inde::ui

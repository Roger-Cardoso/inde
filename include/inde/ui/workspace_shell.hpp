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
  Review
};

[[nodiscard]] std::string_view workspace_name(WorkspaceId workspace) noexcept;

class WorkspaceShell final : public Gtk::Box {
public:
    WorkspaceShell();

    void add_workspace(Gtk::Widget& widget, WorkspaceId workspace);
    void show(WorkspaceId workspace);
    void refresh_vocabulary();
    Gtk::Widget &navigation_region() { return navigation_; }
    [[nodiscard]] WorkspaceId current() const noexcept { return current_; }
    void set_focus_mode(bool focused) { navigation_.set_visible(!focused); }

private:
    Gtk::Box navigation_{Gtk::Orientation::HORIZONTAL, 4};
    Gtk::ToggleButton planning_button_{"Planejar"};
    Gtk::ToggleButton writing_button_{"Escrever"};
    Gtk::ToggleButton review_button_{"Revisar"};
    Gtk::MenuButton more_button_;
    Gtk::Label location_;
    Gtk::Stack stack_;
    WorkspaceId current_{WorkspaceId::Writing};
};

} // namespace inde::ui

#pragma once

#include <gtkmm.h>
#include <string_view>

namespace inde::ui {

enum class WorkspaceId { Catalog, Planning, Writing, Editorial, Graphs };

[[nodiscard]] std::string_view workspace_name(WorkspaceId workspace) noexcept;

class WorkspaceShell final : public Gtk::Box {
public:
    WorkspaceShell();

    void add_workspace(Gtk::Widget& widget, WorkspaceId workspace);
    void show(WorkspaceId workspace);
    [[nodiscard]] WorkspaceId current() const noexcept { return current_; }

private:
    Gtk::Box navigation_{Gtk::Orientation::HORIZONTAL, 4};
    Gtk::ToggleButton catalog_button_{"Catálogo"};
    Gtk::ToggleButton planning_button_{"Planejamento"};
    Gtk::ToggleButton writing_button_{"Escrita"};
    Gtk::ToggleButton editorial_button_{"Editorial"};
    Gtk::ToggleButton graphs_button_{"Gráficos"};
    Gtk::Stack stack_;
    WorkspaceId current_{WorkspaceId::Editorial};
};

} // namespace inde::ui

#include "inde/ui/workspace_shell.hpp"

namespace inde::ui {

std::string_view workspace_name(WorkspaceId workspace) noexcept {
  switch (workspace) {
  case WorkspaceId::Catalog:
    return "catalog";
  case WorkspaceId::Planning:
    return "planning";
  case WorkspaceId::Writing:
    return "writing";
  case WorkspaceId::Editorial:
    return "editorial";
  case WorkspaceId::Graphs:
    return "graphs";
  }
  return "editorial";
}

WorkspaceShell::WorkspaceShell() : Gtk::Box(Gtk::Orientation::VERTICAL, 12) {
  set_vexpand(true);
  set_hexpand(true);
  add_css_class("workspace-page");
  navigation_.add_css_class("workspace-switcher");
  navigation_.set_halign(Gtk::Align::CENTER);
  navigation_.set_margin_top(6);
  navigation_.append(catalog_button_);
  navigation_.append(planning_button_);
  navigation_.append(writing_button_);
  navigation_.append(editorial_button_);
  navigation_.append(graphs_button_);
  planning_button_.set_group(catalog_button_);
  writing_button_.set_group(catalog_button_);
  editorial_button_.set_group(catalog_button_);
  graphs_button_.set_group(catalog_button_);

  catalog_button_.set_action_name("win.show-catalog");
  planning_button_.set_action_name("win.show-planning");
  writing_button_.set_action_name("win.show-writing");
  editorial_button_.set_action_name("win.show-editorial");
  graphs_button_.set_action_name("win.show-graphs");

  stack_.set_vexpand(true);
  stack_.set_hexpand(true);
  // Workspaces ocultos não participam da geometria da janela. Em especial,
  // páginas editoriais longas são roláveis e não devem elevar o tamanho
  // mínimo do shell acima da área de trabalho disponível.
  stack_.set_hhomogeneous(false);
  stack_.set_vhomogeneous(false);
  stack_.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
  append(navigation_);
  append(stack_);
}

void WorkspaceShell::add_workspace(Gtk::Widget &widget, WorkspaceId workspace) {
  stack_.add(widget, std::string(workspace_name(workspace)));
}

void WorkspaceShell::show(WorkspaceId workspace) {
  current_ = workspace;
  stack_.set_visible_child(std::string(workspace_name(workspace)));
  catalog_button_.set_active(workspace == WorkspaceId::Catalog);
  planning_button_.set_active(workspace == WorkspaceId::Planning);
  writing_button_.set_active(workspace == WorkspaceId::Writing);
  editorial_button_.set_active(workspace == WorkspaceId::Editorial);
  graphs_button_.set_active(workspace == WorkspaceId::Graphs);
}

} // namespace inde::ui

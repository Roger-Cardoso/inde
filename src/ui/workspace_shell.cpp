#include "inde/ui/workspace_shell.hpp"
#include "inde/ui/catalog_vocabulary.hpp"

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
  case WorkspaceId::Cartography:
    return "cartography";
  case WorkspaceId::Review:
    return "review";
  }
  return "editorial";
}

WorkspaceShell::WorkspaceShell() : Gtk::Box(Gtk::Orientation::VERTICAL, 12) {
  set_vexpand(true);
  set_hexpand(true);
  add_css_class("workspace-page");
  navigation_.add_css_class("workspace-switcher");
  navigation_.append(writing_button_);
  navigation_.append(planning_button_);
  navigation_.append(review_button_);
  planning_button_.set_group(writing_button_);
  review_button_.set_group(writing_button_);
  more_button_.set_label("Navegar");
  refresh_vocabulary();
  more_button_.set_tooltip_text("Manuscrito, linha temporal, lugares e obras");
  navigation_.append(more_button_);
  location_.set_hexpand(true);
  location_.set_halign(Gtk::Align::END);
  location_.add_css_class("dim-label");
  navigation_.append(location_);

  planning_button_.set_action_name("win.show-planning");
  writing_button_.set_action_name("win.show-writing");
  review_button_.set_action_name("win.show-review");

  stack_.set_vexpand(true);
  stack_.set_hexpand(true);
  // Workspaces ocultos não participam da geometria da janela. Em especial,
  // páginas editoriais longas são roláveis e não devem elevar o tamanho
  // mínimo do shell acima da área de trabalho disponível.
  stack_.set_hhomogeneous(false);
  stack_.set_vhomogeneous(false);
  stack_.set_transition_type(Gtk::StackTransitionType::NONE);
  append(navigation_);
  append(stack_);
  writing_button_.set_active(true);
}

void WorkspaceShell::add_workspace(Gtk::Widget &widget, WorkspaceId workspace) {
  stack_.add(widget, std::string(workspace_name(workspace)));
}

void WorkspaceShell::show(WorkspaceId workspace) {
  current_ = workspace;
  stack_.set_visible_child(std::string(workspace_name(workspace)));
  planning_button_.set_active(workspace == WorkspaceId::Planning);
  writing_button_.set_active(workspace == WorkspaceId::Writing);
  review_button_.set_active(workspace == WorkspaceId::Review);
  location_.set_text(workspace == WorkspaceId::Catalog ? author::catalog_name() :
      workspace == WorkspaceId::Editorial ? "Manuscrito" :
      workspace == WorkspaceId::Graphs ? "Cronologia" :
      workspace == WorkspaceId::Cartography ? "Lugares e mapas" : "");
}

void WorkspaceShell::refresh_vocabulary() {
  const auto more = Gio::Menu::create();
  more->append("Manuscrito", "win.show-editorial");
  more->append("Cronologia", "win.show-graphs");
  more->append("Lugares e mapas", "win.show-cartography");
  more->append(author::catalog_name(), "win.show-catalog");
  more_button_.set_menu_model(more);
  if (current_ == WorkspaceId::Catalog) location_.set_text(author::catalog_name());
}

} // namespace inde::ui

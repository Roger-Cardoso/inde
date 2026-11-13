#include "inde/ui/workspace_shell.hpp"
#include "inde/ui/accessibility.hpp"
namespace inde::ui {
std::string_view workspace_name(WorkspaceId id) noexcept {
  switch (id) {
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
  case WorkspaceId::Comic:
    return "comic";
  }
  return "writing";
}
WorkspaceShell::WorkspaceShell() : Gtk::Box(Gtk::Orientation::VERTICAL, 12) {
  set_hexpand(true);
  set_vexpand(true);
  add_css_class("workspace-page");
  navigation_.add_css_class("workspace-switcher");
  create_button_.set_label("Criar");
  create_button_.set_tooltip_text("Escrita e Comic");
  set_accessible_label(create_button_, "Criar — Escrita ou Comic");
  create_choices_.set_margin(8);
  create_choices_.append(writing_button_);
  create_choices_.append(comic_button_);
  create_popover_.set_child(create_choices_);
  create_button_.set_popover(create_popover_);
  navigation_.append(create_button_);
  navigation_.append(planning_button_);
  navigation_.append(review_button_);
  navigation_.append(editorial_button_);
  navigation_.append(graphs_button_);
  navigation_.append(maps_button_);
  navigation_.append(works_button_);
  for (auto *b :
       {&comic_button_, &planning_button_, &review_button_, &editorial_button_,
        &graphs_button_, &maps_button_, &works_button_})
    b->set_group(writing_button_);
  for (auto [button, action] :
       std::vector<std::pair<Gtk::ToggleButton *, const char *>>{
           {&writing_button_, "win.show-writing"},
           {&comic_button_, "win.show-comic"},
           {&planning_button_, "win.show-planning"},
           {&review_button_, "win.show-review"},
           {&editorial_button_, "win.show-editorial"},
           {&graphs_button_, "win.show-graphs"},
           {&maps_button_, "win.show-cartography"},
           {&works_button_, "win.show-catalog"}})
    button->set_action_name(action);
  writing_button_.signal_clicked().connect(
      [this] { create_popover_.popdown(); });
  comic_button_.signal_clicked().connect([this] { create_popover_.popdown(); });
  navigation_scroll_.set_policy(Gtk::PolicyType::AUTOMATIC,
                                Gtk::PolicyType::NEVER);
  navigation_scroll_.set_propagate_natural_height(true);
  navigation_scroll_.set_child(navigation_);
  append(navigation_scroll_);
  stack_.set_hexpand(true);
  stack_.set_vexpand(true);
  stack_.set_hhomogeneous(false);
  stack_.set_vhomogeneous(false);
  stack_.set_transition_type(Gtk::StackTransitionType::NONE);
  append(stack_);
}
void WorkspaceShell::add_workspace(Gtk::Widget &w, WorkspaceId id) {
  stack_.add(w, std::string(workspace_name(id)));
}
void WorkspaceShell::show(WorkspaceId id) {
  current_ = id;
  stack_.set_visible_child(std::string(workspace_name(id)));
  for (auto [button, workspace] :
       std::vector<std::pair<Gtk::ToggleButton *, WorkspaceId>>{
           {&writing_button_, WorkspaceId::Writing},
           {&comic_button_, WorkspaceId::Comic},
           {&planning_button_, WorkspaceId::Planning},
           {&review_button_, WorkspaceId::Review},
           {&editorial_button_, WorkspaceId::Editorial},
           {&graphs_button_, WorkspaceId::Graphs},
           {&maps_button_, WorkspaceId::Cartography},
           {&works_button_, WorkspaceId::Catalog}})
    button->set_active(workspace == id);
  const bool creating = id == WorkspaceId::Writing || id == WorkspaceId::Comic;
  if (creating)
    create_button_.add_css_class("suggested-action");
  else
    create_button_.remove_css_class("suggested-action");
  create_button_.set_tooltip_text(id == WorkspaceId::Comic ? "Criar · Comic"
                                  : id == WorkspaceId::Writing
                                      ? "Criar · Escrita"
                                      : "Escrita e Comic");
}
void WorkspaceShell::refresh_vocabulary() { /* Compact labels follow the agreed
                                               navigation; vocabulary inside
                                               each workspace is unchanged. */
}
} // namespace inde::ui

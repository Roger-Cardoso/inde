#pragma once

#include <gtkmm.h>

#include <functional>
#include <vector>

namespace inde::ui {

struct ContextMenuAction {
  Glib::ustring label;
  std::function<void()> activate;
  Glib::ustring icon_name;
  bool enabled{true};
  bool destructive{false};
  bool separator_before{false};
};

// Instala um menu acessível por clique secundário sobre o alvo. O mesmo modelo
// pode ser usado por um MenuButton de overflow para acesso por teclado/toque.
void attach_context_menu(Gtk::Widget &target,
                         std::vector<ContextMenuAction> actions);
Gtk::Popover *create_action_popover(std::vector<ContextMenuAction> actions);
void configure_overflow_button(Gtk::MenuButton &button,
                               std::vector<ContextMenuAction> actions,
                               const Glib::ustring &tooltip = "Mais ações");

} // namespace inde::ui

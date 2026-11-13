#include "inde/ui/main_window.hpp"
namespace inde::ui {
bool MainWindow::guard_comic_project_change() {
  if (comic_workspace_.can_leave())
    return true;
  if (service_.current())
    show_workspace(WorkspaceId::Comic);
  status_bar_.set_text("Conclua a operação ou salve/descarte as alterações em "
                       "Comic antes de fechar ou trocar o Projeto.");
  return false;
}
} // namespace inde::ui

#pragma once

#include "inde/application/project_service.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/ui/cartography_workspace.hpp"
#include "inde/ui/editorial_structure_workspace.hpp"
#include "inde/ui/editorial_workspace.hpp"
#include "inde/ui/graphs_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/proofreading_controller.hpp"
#include "inde/ui/workspace_shell.hpp"
#include "inde/ui/writing_workspace.hpp"

#include <gtkmm.h>
#include <optional>

namespace inde::ui {

class MainWindow final : public Gtk::ApplicationWindow {
public:
  explicit MainWindow(
      std::optional<std::filesystem::path> initial_project = std::nullopt);

private:
  bool on_close_request() override;
  void build_ui();
  void refresh_recents();
  void update_project_view();
  void create_project();
  void open_project();
  void open_project_at(const std::filesystem::path &path);
  void save_project();
  void save_project_as();
  void close_project();
  void toggle_sidebar();
  void toggle_inspector();
  void refresh_workspace(WorkspaceId workspace);
  void show_workspace(WorkspaceId workspace);
  void setup_actions();
  void setup_menus();
  void show_dictionaries();
  void show_about();
  void show_error(const Glib::ustring &title, const std::exception &error);

  application::ProjectService service_;
  persistence::JsonDictionaryStore dictionary_store_;
  persistence::EnchantSpellingProvider spelling_provider_;
  application::ProofreadingService proofreading_service_;
  ProofreadingController proofreading_controller_;
  ModalOverlayHost modal_host_;
  Gtk::Box root_{Gtk::Orientation::VERTICAL};
  Gtk::HeaderBar header_;
  Gtk::Box header_heading_{Gtk::Orientation::VERTICAL};
  Gtk::Label header_title_{"INDE"};
  Gtk::Label header_subtitle_{"Ambiente de desenvolvimento narrativo"};
  Gtk::PopoverMenuBar menu_bar_;
  Gtk::MenuButton application_menu_;
  Gtk::Button sidebar_toggle_{"Filtros"};
  Gtk::Button inspector_toggle_{"Painel lateral"};
  Gtk::Button new_button_{"Novo"};
  Gtk::Button open_button_{"Abrir"};
  Gtk::Button save_button_{"Salvar"};
  Gtk::Button save_as_button_{"Salvar cópia"};
  Gtk::Button close_button_{"Fechar"};
  Gtk::Stack stack_;
  Gtk::Box welcome_{Gtk::Orientation::VERTICAL, 12};
  Gtk::Label welcome_title_{"INDE"};
  Gtk::Label welcome_subtitle_{"Integrated Narrative Development Environment"};
  Gtk::Label recent_title_{"Projetos recentes"};
  Gtk::ListBox recent_list_;
  Gtk::Box project_view_{Gtk::Orientation::VERTICAL, 12};
  WorkspaceShell workspace_shell_;
  PlanningWorkspace planning_workspace_{service_};
  // CatalogWorkspace is kept in editorial_workspace.hpp for compatibility
  // with the existing implementation; it is exposed as its own workspace.
  EditorialWorkspace catalog_workspace_{service_};
  WritingWorkspace writing_workspace_{service_};
  GraphsWorkspace graphs_workspace_{service_};
  CartographyWorkspace cartography_workspace_{service_};
  EditorialStructureWorkspace editorial_workspace_{service_};
  Gtk::Label status_bar_{"Pronto"};
};

} // namespace inde::ui

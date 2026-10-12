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
#include "inde/ui/review_workspace.hpp"
#include "inde/ui/keyboard_commands.hpp"

#include <gtkmm.h>
#include <optional>

namespace inde::ui {

class MainWindow final : public Gtk::ApplicationWindow {
public:
  explicit MainWindow(
      std::optional<std::filesystem::path> initial_project = std::nullopt);
  ~MainWindow() override;
  void navigate_removal_use(const project::OperationPreview &, const project::RemovalUse &, std::function<void()> completed);

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
  void setup_keyboard();
  void show_dictionaries();
  void show_about();
  void show_error(const Glib::ustring &title, const std::exception &error);
  void set_focus_mode(bool focused);
  void remember_context();
  [[nodiscard]] ViewState capture_context();
  void restore_context(const ViewState &);
  void manage_workspaces();
  void show_project_operations();
  void return_to_removal();
  struct RemovalReturn {
    ViewState context;
    project::OperationPreview preview;
    std::function<void()> completed;
  };
  std::vector<RemovalReturn> removal_returns_;
  Gtk::Button removal_return_{"Voltar à exclusão"};
  std::optional<ViewState> builder_return_context_;
  Gtk::Button builder_return_{"Voltar ao planejamento"};
  void update_builder_return();
  void restore_pending_view(WorkspaceId);
  bool restoring_context_{};
  std::string restored_project_id_;
  std::string context_warning_;
  std::map<std::string, ViewState> pending_views_;
  sigc::connection context_save_;

  sigc::connection maximize_idle_;
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
  Gtk::MenuButton appearance_menu_;
  Gtk::Button commands_button_{"Comandos"};
  Gtk::ToggleButton focus_button_{"Foco"};
  bool focused_{};
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
  ReviewWorkspace review_workspace_{service_};
  GraphsWorkspace graphs_workspace_{service_};
  CartographyWorkspace cartography_workspace_{service_};
  EditorialStructureWorkspace editorial_workspace_{service_};
  Gtk::Label status_bar_{"Pronto"};
  std::unique_ptr<OverlayDialog> operations_dialog_;
  std::unique_ptr<KeyboardCommands> keyboard_;
};

} // namespace inde::ui

#pragma once
#include "inde/experimental/lab_source.hpp"
#include "inde/experimental/text_blocks.hpp"
#include "inde/ui/keyboard_commands.hpp"
#include <gtkmm.h>
#include <functional>
#include <memory>
namespace inde::experimental {
class BlocksLabWindow : public Gtk::ApplicationWindow {
public:
  explicit BlocksLabWindow(const std::filesystem::path &copy);
  ~BlocksLabWindow() override;
private:
  struct State { TextBlocksLab lab; ReadingProfile profile; std::string title{"Minha montagem"}; } state_;
  struct Editor {
    std::string block, expected;
    bool dirty{}, writable{};
    Gtk::Frame frame;
    Gtk::Box box{Gtk::Orientation::VERTICAL, 6}, buttons{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::Label caption;
    Gtk::TextView text;
    Gtk::ScrolledWindow scroll;
    Gtk::Button apply{"Aplicar versão"}, discard{"Descartar rascunho"}, close{"Fechar painel"};
  };
  LabSource source_;
  project::Document source_document_;
  std::map<std::string, project::Document> sources_;
  std::map<std::string, std::size_t> source_references_;
  std::vector<project::Document> examples_;
  std::vector<State> undo_, redo_;
  std::vector<std::unique_ptr<Editor>> editors_;
  std::vector<std::string> library_ids_, occurrence_ids_;
  std::string selected_, library_selected_;
  std::size_t source_page_{};
  bool rendering_{}, changed_{};
  std::function<void()> pending_;
  std::optional<MaterializedDocument> prepared_;
  Gtk::Box root_{Gtk::Orientation::VERTICAL, 8}, top_{Gtk::Orientation::HORIZONTAL, 6},
      confirmation_{Gtk::Orientation::HORIZONTAL, 8}, confirmation_buttons_{Gtk::Orientation::HORIZONTAL, 6},
      source_box_{Gtk::Orientation::VERTICAL, 6}, library_box_{Gtk::Orientation::VERTICAL, 6},
      montage_box_{Gtk::Orientation::VERTICAL, 6}, montage_actions_{Gtk::Orientation::HORIZONTAL, 6},
      read_box_{Gtk::Orientation::VERTICAL, 6}, output_box_{Gtk::Orientation::VERTICAL, 6},
      source_actions_{Gtk::Orientation::HORIZONTAL, 6}, library_actions_{Gtk::Orientation::HORIZONTAL, 6},
      library_actions2_{Gtk::Orientation::HORIZONTAL, 6}, layer_actions_{Gtk::Orientation::HORIZONTAL, 6},
      sidebar_box_{Gtk::Orientation::VERTICAL, 6};
  Gtk::Paned horizontal_{Gtk::Orientation::HORIZONTAL}, vertical_{Gtk::Orientation::VERTICAL};
  Gtk::Notebook sidebar_, preview_tabs_;
  Gtk::Grid editor_grid_;
  Gtk::Label heading_{"Montagem de texto — trechos, versões e usos dos blocos"}, notice_, confirmation_text_, focus_info_,
      edit_heading_{"Edição de trechos · rascunhos independentes da leitura"}, status_, read_info_;
  Gtk::Entry name_, title_;
  Gtk::ComboBoxText documents_, versions_, alternatives_, layers_;
  Gtk::ListBox library_, occurrences_;
  Gtk::TextView source_text_, read_text_, map_text_, impact_text_, output_text_, warnings_text_;
  Gtk::ScrolledWindow source_scroll_, library_scroll_, library_page_scroll_, montage_scroll_, editors_scroll_, read_scroll_, map_scroll_, impact_scroll_, output_scroll_, warnings_scroll_;
  Gtk::Box editor_box_{Gtk::Orientation::VERTICAL, 6};
  Gtk::ComboBoxText editor_selector_;
  std::size_t active_editor_{};
  bool selecting_editor_{};
  Gtk::ToggleButton organization_toggle_{"Organização"};
  Gtk::MenuButton theme_menu_;
  Gtk::Popover theme_popover_;
  Gtk::Box theme_choices_{Gtk::Orientation::VERTICAL, 4};
  Gtk::Button light_theme_{"Claro"}, dark_theme_{"Escuro"}, system_theme_{"Seguir sistema"};
  Gtk::Button previous_{"Anteriores"}, next_{"Próximos"}, extract_{"Extrair seleção"}, whole_{"Usar Documento inteiro"},
      open_{"Abrir versão"}, reuse_{"Reutilizar bloco"}, rename_{"Nomear bloco"}, rename_variant_{"Nomear alternativa"},
      split_{"Separar seleção em bloco"}, cursor_split_{"Dividir no cursor"}, repeat_{"Reutilizar uso do bloco"},
      pin_{"Manter versão escolhida"}, follow_{"Acompanhar versão atual"}, fork_{"Tornar independente"},
      ref_{"Converter em referência"}, origin_{"Ver origem"}, up_{"Mover acima"}, down_{"Mover abaixo"},
      hide_{"Ocultar/mostrar uso do bloco"}, remove_{"Retirar da montagem"}, alternative_{"Criar alternativa"},
      promote_{"Transformar em bloco reutilizável"}, assign_layer_{"Incluir/retirar da camada"}, new_layer_{"Criar camada"},
      rename_layer_{"Nomear camada"}, layer_visibility_{"Mostrar/ocultar camada"},
      undo_button_{"Desfazer montagem"}, redo_button_{"Refazer montagem"}, reset_{"Nova montagem"},
      prepare_{"Preparar novo Documento"}, publish_{"Criar Documento na cópia"},
      confirm_{"Confirmar operação"}, cancel_{"Cancelar"};
  Gtk::CheckButton acknowledge_{"Li os avisos de conservação e aceito criar esta cópia independente"};
  void wire_commands();
  void install_keyboard();
  void source_page();
  void load_source();
  void render();
  void render_versions();
  void render_editors();
  void show_editor(std::size_t);
  void select_occurrence(const std::string &, bool open = true);
  void open_editor(const std::string &, const std::string &);
  void apply_editor(Editor &);
  void invalidate_output();
  void prepare_output();
  void ask(const std::string &, std::function<void()>);
  void attempt(const std::function<void()> &);
  bool command(const std::string &, const std::function<void(State &)> &);
  [[nodiscard]] std::size_t dirty_count() const;
  [[nodiscard]] std::string version_label(const std::string &, const std::string &) const;
  [[nodiscard]] std::string occurrence_label(const TextPlacement &, std::size_t) const;
  [[nodiscard]] std::string origin_title(const std::string &) const;
  Gtk::Button commands_button_{"Comandos"};
  std::unique_ptr<ui::KeyboardCommands> keyboard_;
};
}

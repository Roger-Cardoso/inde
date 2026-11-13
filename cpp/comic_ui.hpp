#pragma once
#include "comic_service.hpp"
#include "inde/ui/keyboard_commands.hpp"
#include "inde/ui/view_state.hpp"
#include <chrono>
#include <gtkmm.h>
namespace inde::ui {
class ComicWorkspace final : public Gtk::Box {
public:
  explicit ComicWorkspace(publication::Service &);
  ~ComicWorkspace() override;
  void refresh();
  bool can_leave();
  bool can_switch();
  bool composing_input() const { return composing_; }
  void toggle_inspector();
  std::vector<Gtk::Widget *> keyboard_regions();
  void install_keyboard(KeyboardCommands &);
  void activate();
  void reset();
  void save_changes();
  void undo();
  void redo();
  ViewState capture_view() const;
  void restore_view(const ViewState &);

private:
  friend struct ComicWorkspaceProbe;
  void guard(const std::function<void()> &);
  void changed();
  void checkpoint(const std::string &group = {});
  void reset_history();
  void update_history_buttons();
  void sync_sources();
  void update_zoom();
  static comic::Book lightweight(const comic::Book &);
  std::string signature() const;
  void fill();
  void fill_panel();
  void fill_element();
  void update();
  void save();
  void generate();
  void preview();
  void choose_asset(bool font);
  void draw(const Cairo::RefPtr<Cairo::Context> &, int, int);
  comic::Panel *panel();
  comic::Element *element();
  publication::Service &publication_;
  comic::Service service_;
  comic::Book book_;
  std::string project_id_, saved_signature_, edit_group_;
  std::vector<comic::Book> undo_, redo_;
  std::map<std::string, std::string> asset_data_;
  std::chrono::steady_clock::time_point last_edit_{};
  bool restoring_{}, composing_{};
  sigc::connection ime_end_;
  double zoom_factor_{};
  bool binding_{}, dirty_{}, dragging_{}, resizing_{};
  double drag_w_{}, drag_h_{};
  int panel_index_{}, element_index_{};
  double drag_x_{}, drag_y_{}, canvas_left_{}, canvas_top_{}, canvas_width_{1},
      canvas_height_{1};
  comic::Position guide_{};
  std::optional<comic::Prepared> prepared_;
  std::filesystem::path output_;
  GSubprocess *job_{}, *preview_job_{};
  std::shared_ptr<int> lifetime_{std::make_shared<int>(0)};
  Glib::RefPtr<Gtk::FileChooserNative> file_;
  Glib::RefPtr<Gdk::Pixbuf> image_;
  Gtk::Paned library_split_{Gtk::Orientation::HORIZONTAL},
      inspector_split_{Gtk::Orientation::HORIZONTAL};
  Gtk::Notebook inspector_;
  Gtk::Box library_{Gtk::Orientation::VERTICAL, 8},
      library_actions_{Gtk::Orientation::VERTICAL, 4},
      book_form_{Gtk::Orientation::VERTICAL, 8},
      panel_form_{Gtk::Orientation::VERTICAL, 8},
      publication_form_{Gtk::Orientation::VERTICAL, 8},
      canvas_tools_{Gtk::Orientation::HORIZONTAL, 6};
  Gtk::ScrolledWindow panel_list_scroll_, canvas_scroll_, book_scroll_,
      panel_scroll_, publication_scroll_;
  Gtk::ListBox panel_list_;
  Gtk::Label heading_{"Comic"}, location_;
  Gtk::ComboBoxText zoom_;
  Gtk::Button undo_button_{"Desfazer"}, redo_button_{"Refazer"}, panel_up_{"↑"},
      panel_down_{"↓"};
  Gtk::ColorButton ink_, bubble_background_;
  Gtk::SpinButton ratio_;
  Gtk::Box top_{Gtk::Orientation::HORIZONTAL, 6},
      actions_{Gtk::Orientation::HORIZONTAL, 6},
      body_{Gtk::Orientation::HORIZONTAL, 12},
      left_{Gtk::Orientation::VERTICAL, 8},
      form_{Gtk::Orientation::VERTICAL, 8},
      element_actions_{Gtk::Orientation::HORIZONTAL, 4};
  Gtk::ComboBoxText books_, works_, sites_, panels_, elements_, kind_, shape_,
      direction_, align_, valign_, animation_, fonts_, documents_, layout_,
      font_role_;
  Gtk::Entry title_, slug_, language_, panel_title_, alt_, text_language_,
      speaker_, gloss_;
  Gtk::TextView text_;
  Gtk::ScrolledWindow form_scroll_, text_scroll_;
  Gtk::DrawingArea canvas_;
  Gtk::Grid geometry_;
  Gtk::SpinButton x_, y_, w_, h_, size_;
  Gtk::Label status_,
      help_{"Arraste um elemento. O centro do quadro oferece encaixe visual. A "
            "prévia web é a referência para fontes incorporadas e rolagem."};
  Gtk::CheckButton translate_{"Permitir tradução do texto"},
      snap_{"Encaixar no centro"}, removed_{"Mostrar retirados"};
  Gtk::Button create_{"Novo comic"}, demo_{"Exemplo de comic"},
      save_{"Salvar comic"}, discard_{"Descartar alterações"},
      retire_{"Retirar / restaurar"}, add_panel_{"＋ Quadro"},
      remove_panel_{"Remover quadro"}, add_element_{"＋ Texto"},
      remove_element_{"Remover texto"}, image_button_{"Importar arte"},
      font_button_{"Importar fonte"},
      copy_document_{"Copiar texto do Documento"}, generate_{"Preparar comic"},
      preview_{"Abrir prévia"};
};
} // namespace inde::ui

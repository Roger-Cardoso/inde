#pragma once
#include "inde/application/project_service.hpp"
#include "inde/ui/incremental_selector.hpp"
#include "inde/ui/view_state.hpp"
#include <gtkmm.h>
namespace inde::ui {
class KeyboardCommands;
class CompositionPage final : public Gtk::Box {
public:
  explicit CompositionPage(application::ProjectService &);
  ~CompositionPage() override;
  void refresh();
  void flush();
  void reset();
  ViewState capture_view();
  void restore_view(const ViewState &);
  void reveal_occurrence(const std::string &composition, const std::string &occurrence);
  void install_keyboard(KeyboardCommands &);
  sigc::signal<void()> &signal_narrative_uses() {return narrative_uses_signal_;}
  sigc::signal<void()> &signal_back() { return back_signal_; }
  sigc::signal<void(const std::string &)> &signal_open_document() {
    return open_signal_;
  }

private:
  application::ProjectService &service_;
  std::optional<project::StoredComposition> current_;
  std::optional<project::Document> source_;
  std::optional<application::PreparedComposition> prepared_;
  std::string occurrence_, block_, expected_;
  std::size_t offset_{};
  bool loading_{}, dirty_{}, ime_{};
  sigc::connection debounce_, position_restore_;
  sigc::connection edited_, version_changed_;
  sigc::signal<void()> back_signal_;
  sigc::signal<void()> narrative_uses_signal_;
  sigc::signal<void(const std::string &)> open_signal_;
  Gtk::Box header_{Gtk::Orientation::HORIZONTAL, 6},
      queue_{Gtk::Orientation::VERTICAL, 8},
      order_{Gtk::Orientation::VERTICAL, 8},
      edit_{Gtk::Orientation::VERTICAL, 8},
      reading_{Gtk::Orientation::VERTICAL, 8},
      import_{Gtk::Orientation::VERTICAL, 8},
      output_{Gtk::Orientation::VERTICAL, 8},
      origin_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Label title_{"Montagens de texto"}, message_, hint_, edit_info_, reading_info_,
      output_info_, origin_info_;
  Gtk::Button back_{"Documentos"}, all_{"Todas as Montagens de texto"},
      create_{"Criar Montagem de texto"}, previous_{"Anteriores"}, next_{"Próximas"},
      add_{"Importar Documento…"}, read_{"Ler Montagem de texto"},
      edit_back_{"Voltar à ordem"}, apply_{"Usar esta versão na montagem"},
      discard_{"Descartar rascunho"}, reading_back_{"Voltar à ordem"},
      import_back_{"Voltar à ordem"}, import_whole_{"Importar inteiro"},
      import_selection_{"Importar seleção"}, output_back_{"Voltar à ordem"},
      materialize_{"Criar Documento independente"},
      export_{"Exportar leitura…"}, origin_back_{"Voltar à ordem"};
  Gtk::MenuButton more_;
  Gtk::MenuButton reading_profile_;
  Gtk::Expander output_details_{"Avisos, origens e histórico"};
  Gtk::ScrolledWindow output_details_scroll_, edit_info_scroll_;
  Gtk::Label output_losses_;
  Gtk::Entry name_;
  Gtk::ComboBoxText versions_;
  Gtk::Stack pages_;
  Gtk::ListBox compositions_, occurrences_;
  Gtk::ScrolledWindow queue_scroll_, order_scroll_, edit_scroll_,
      reading_scroll_, import_scroll_, output_scroll_, origin_scroll_;
  Gtk::TextView editor_, reading_text_, import_text_, output_text_,
      origin_text_;
  Gtk::CheckButton acknowledged_{
      "Conferi a leitura e os avisos de conservação desta saída"};
  IncrementalSelector *sources_{};
  void attempt(const std::function<void()> &);
  void open(const std::string &);
  void show_order();
  void show_editor(const std::string &);
  void display_version();
  void show_reading();
  void show_import();
  void import_source(bool whole);
  void show_origin();
  void prepare();
  void export_reading();
  void reload();
  void mutate(const std::function<void(project::Composition &)> &);
  void move(int);
  void split();
  void classify(bool variant);
  void occurrence_command(const std::string &);
  void confirm_draft(bool apply);
};
} // namespace inde::ui

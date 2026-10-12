#pragma once
#include "inde/application/project_service.hpp"
#include "inde/application/work_production.hpp"
#include "inde/ui/view_state.hpp"
#include <gtkmm.h>
#include <mutex>
#include <thread>
namespace inde::ui {
class KeyboardCommands;
class ProductionPage final : public Gtk::Box {
public:
  explicit ProductionPage(application::ProjectService &);
  ~ProductionPage() override;
  void set_work(const std::optional<std::string> &);
  void reset();
  void install_keyboard(KeyboardCommands &);
  ViewState capture_view() const;
  void restore_view(const ViewState &);

private:
  void render_units();
  void choose_texts(const std::string &);
  void prepare();
  void export_snapshot();
  Gtk::Window *owner();
  application::ProjectService &service_;
  std::optional<std::string> work_id_;
  std::string project_id_, structure_id_;
  std::vector<project::StructuralNode> nodes_;
  application::ProductionChoices choices_;
  std::shared_ptr<const application::WorkProduction> prepared_;
  std::size_t offset_{};
  Gtk::Stack pages_;
  Gtk::Box plan_{Gtk::Orientation::VERTICAL, 8},
      preview_{Gtk::Orientation::VERTICAL, 8};
  Gtk::Box controls_{Gtk::Orientation::HORIZONTAL, 8},
      formats_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::ComboBoxText profile_;
  Gtk::Entry author_;
  Gtk::CheckButton docx_{"DOCX"}, epub_{"EPUB"}, pdf_{"PDF"};
  Gtk::Label hint_{
      "Escolha explicitamente os textos. A ordem das unidades vem da estrutura "
      "ativa; títulos e separadores são gerados na saída."},
      message_, summary_;
  Gtk::ScrolledWindow units_scroll_, preview_scroll_, losses_scroll_;
  Gtk::Box units_{Gtk::Orientation::VERTICAL, 8},
      navigation_{Gtk::Orientation::HORIZONTAL, 8},
      preview_actions_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Button previous_{"Unidades anteriores"}, next_{"Próximas unidades"},
      prepare_{"Preparar prévia"}, back_{"Voltar à seleção"},
      export_{"Escolher destino e exportar"}, cancel_{"Cancelar exportação"};
  Gtk::TextView text_;
  Gtk::Expander details_{"Textos incluídos, itens omitidos e limitações da exportação"};
  Gtk::Label losses_;
  Gtk::CheckButton acknowledged_{"Conferi a seleção e as perdas desta saída"};
  std::jthread worker_;
  std::mutex result_mutex_;
  std::string result_;
  bool result_ready_{}, result_ok_{};
  sigc::connection delivery_;
};
} // namespace inde::ui

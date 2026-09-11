#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <gtkmm.h>

namespace inde::ui {

// Um seletor de referência, não uma fonte de dados: a função fornecedora deve
// buscar apenas uma página pequena a cada texto informado pelo autor.
struct IncrementalSelection {
  std::string id;
  std::string label;
  std::string detail;
};

class IncrementalSelector final : public Gtk::Box {
public:
  using Provider = std::function<std::vector<IncrementalSelection>(
      const std::string &search, std::size_t limit)>;

  explicit IncrementalSelector(std::string placeholder, Provider provider,
                               std::string empty_message =
                                   "Nenhum resultado encontrado");

  [[nodiscard]] std::optional<std::string> selected_id() const;
  void set_selected(std::string id, std::string label);
  void clear_selection();
  void refresh();
  [[nodiscard]] sigc::signal<void()> &signal_selection_changed() {
    return signal_selection_changed_;
  }

private:
  static constexpr std::size_t result_limit_ = 50;

  void refresh_results();
  void choose(const IncrementalSelection &selection);

  Provider provider_;
  Gtk::SearchEntry search_;
  Gtk::Label selected_{"Nenhuma escolha"};
  Gtk::ScrolledWindow results_scroll_;
  Gtk::ListBox results_;
  std::optional<std::string> selected_id_;
  sigc::signal<void()> signal_selection_changed_;
};

} // namespace inde::ui

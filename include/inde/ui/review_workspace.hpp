#pragma once
#include "inde/application/project_service.hpp"
#include <gtkmm.h>
#include "inde/application/editorial_review.hpp"
#include "inde/ui/view_state.hpp"
#include <future>
#include <functional>
namespace inde::ui {
class KeyboardCommands;
// A paginated, cancellable view of existing comments and diagnostics.
class ReviewWorkspace final : public Gtk::Box {
public:
  explicit ReviewWorkspace(application::ProjectService &);
  ~ReviewWorkspace() override;
  void refresh();
  void install_keyboard(KeyboardCommands &);
  void reset();
  void set_change_handler(std::function<project::Document(const project::Document &,const project::Document &,const std::string &)> handler) { change_=std::move(handler); }
  [[nodiscard]] ViewState capture_view() const;
  void restore_view(const ViewState &);
  sigc::signal<void(const std::string &)> &signal_open_document_requested() { return open_; }
private:
  void load_page();
  bool scan_next();
  void show_document(const std::string &);
  void decide(const project::Document &,const project::Document &,const std::string &);
  void status_dialog(const project::Document &);
  void suggestion_dialog(const project::Document &,const project::TextAnnotation &,bool);
  void comparison_dialog(const project::Document &);
  void revision_dialog(const project::Document &);
  void compare(const project::Document &,const project::Document &);
  void finish_comparison();
  void render_comparison();
  void color_comparison();
  void fail(const std::string &,const std::exception &);
  application::ProjectService &service_;
  Gtk::Box header_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Label title_{"Revisar"}, summary_;
  Gtk::Button refresh_{"Atualizar"}, previous_{"Anterior"}, next_{"Próxima"};
  Gtk::ComboBoxText filter_;
  Gtk::ScrolledWindow scroll_;
  Gtk::ListBox list_;
  Gtk::Box footer_{Gtk::Orientation::HORIZONTAL, 8};
  Gtk::Stack pages_;
  Gtk::Box queue_{Gtk::Orientation::VERTICAL,8}, detail_{Gtk::Orientation::VERTICAL,8};
  Gtk::ScrolledWindow detail_scroll_;
  Gtk::Box detail_body_{Gtk::Orientation::VERTICAL,10};
  Gtk::SearchEntry search_;
  std::string selected_document_, comparison_left_,comparison_right_;
  Gtk::Box comparison_{Gtk::Orientation::VERTICAL,8}, comparison_header_{Gtk::Orientation::HORIZONTAL,8};
  Gtk::Button comparison_back_{"Voltar à revisão"}, comparison_previous_{"Diferença anterior"}, comparison_next_{"Próxima diferença"};
  Gtk::CheckButton differences_only_{"Somente diferenças"};
  Gtk::Label comparison_info_;
  Gtk::ScrolledWindow comparison_scroll_;
  Gtk::TextView comparison_text_;
  std::optional<application::DocumentComparison> difference_;
  std::optional<project::Document> comparison_left_source_,comparison_right_source_;
  std::vector<int> difference_offsets_;
  std::size_t difference_index_{};
  std::future<application::DocumentComparison> comparison_future_;
  std::stop_source comparison_stop_;
  sigc::connection comparison_delivery_, theme_changed_;
  bool refreshing_controls_{};
  std::size_t item_offset_{};
  std::function<project::Document(const project::Document &,const project::Document &,const std::string &)> change_;
  std::vector<project::DocumentSummary> documents_;
  std::vector<project::AnnotationTypeDefinition> types_;
  std::size_t offset_{}, total_{}, position_{}, found_{};
  std::string project_id_;
  sigc::connection scan_;
  sigc::signal<void(const std::string &)> open_;
};
}

#pragma once
#include "publication_service.hpp"
#include <gtkmm.h>
#include <gio/gio.h>
#include <functional>
namespace inde::ui {
class PublicationPage final: public Gtk::Box {
public:
  explicit PublicationPage(publication::Service &);
  ~PublicationPage() override;
  void refresh();
  bool can_leave();
private:
  friend struct PublicationPageProbe;
  friend struct ComicWorkspaceProbe;
  struct Job;
  void guard(const std::function<void()> &);
  void changed();
  bool clean();
  void load_site(const std::string &);
  void load_edition(const std::string &);
  void fill_site();void fill_edition();void fill_sources();void fill_catalog();
  void save_catalog();void select_catalog(const std::string &,bool);void move_catalog(const std::string &,int);
  void save_site();void save_edition();void prepare(bool catalog=false);void preview();
  void confirm(const std::string &,std::function<void()>);
  void generated(bool,const std::string &);
  void stop_preview();
  publication::Service &service_;
  publication::Site site_;
  publication::Edition edition_;
  std::string project_id_,site_id_,edition_id_;
  bool binding_{},dirty_{};
  int dirty_page_{};
  std::optional<publication::Prepared> prepared_;
  std::filesystem::path output_;
  std::shared_ptr<Job> job_;
  std::shared_ptr<Job> preview_job_;
  std::string preview_id_;
  std::filesystem::path preview_control_;
  std::function<void()> confirmation_;
  Gtk::Box controls_{Gtk::Orientation::VERTICAL,12},toolbar_{Gtk::Orientation::HORIZONTAL,8},
    site_buttons_{Gtk::Orientation::HORIZONTAL,6},edition_buttons_{Gtk::Orientation::HORIZONTAL,6},
    site_form_{Gtk::Orientation::VERTICAL,10},edition_form_{Gtk::Orientation::VERTICAL,10},
    catalog_form_{Gtk::Orientation::VERTICAL,10},catalog_box_{Gtk::Orientation::VERTICAL,8},
    source_box_{Gtk::Orientation::VERTICAL,6},entity_box_{Gtk::Orientation::VERTICAL,6},
    bottom_{Gtk::Orientation::HORIZONTAL,8};
  Gtk::Notebook notebook_;
  Gtk::Grid site_grid_,edition_grid_;
  Gtk::ScrolledWindow site_scroll_,edition_scroll_,catalog_scroll_;
  Gtk::Label heading_{"Publicação web"},intro_{"Site, Edições e comics pertencem a este Projeto. A escrita de novels continua nos Documentos do INDE."},status_,confirm_label_;
  Gtk::CheckButton removed_{"Mostrar retirados"};
  Gtk::ComboBoxText sites_,editions_,theme_,destination_,work_,structure_;
  Gtk::Entry name_,author_,description_,origin_,base_path_,repository_,branch_,title_,slug_;
  Gtk::Button new_site_{"Novo Site"},save_site_{"Salvar Site"},remove_site_{"Retirar Site"},
    new_edition_{"Nova Edição"},save_edition_{"Salvar Edição"},remove_edition_{"Retirar Edição"},
    save_catalog_{"Salvar catálogo"},prepare_site_{"Preparar Site"},prepare_{"Gerar Edição"},preview_button_{"Abrir prévia"},discard_{"Descartar alterações"},
    reload_{"Recarregar"},confirm_yes_{"Confirmar"},confirm_no_{"Cancelar"};
};
}

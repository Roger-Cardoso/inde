#include "publication_ui.hpp"
#include <algorithm>
#include <map>
namespace inde::ui {
void PublicationPage::fill_catalog(){
  binding_=true;while(auto *child=catalog_box_.get_first_child())catalog_box_.remove(*child);
  save_catalog_.set_sensitive(!site_id_.empty()&&!site_.removed);prepare_site_.set_sensitive(!site_id_.empty()&&!site_.removed);
  if(site_id_.empty()){binding_=false;return;}
  const auto editions=service_.editions(site_id_,true);std::map<std::string,publication::Edition> by_id;
  std::vector<std::string> order=site_.catalog;for(const auto &e:editions){by_id.emplace(e.id,e);if(!e.removed&&std::find(order.begin(),order.end(),e.id)==order.end())order.push_back(e.id);}
  auto *summary=Gtk::make_managed<Gtk::Label>(std::to_string(site_.catalog.size())+" Obra(s) selecionada(s)");summary->set_xalign(0);catalog_box_.append(*summary);
  for(const auto &id:order){
    const auto selected=std::find(site_.catalog.begin(),site_.catalog.end(),id);const auto index=static_cast<std::size_t>(selected-site_.catalog.begin());const bool included=selected!=site_.catalog.end();
    const auto item=by_id.find(id);const bool available=item!=by_id.end()&&!item->second.removed;
    std::string caption=available?item->second.title+" · /"+item->second.slug+"/":"Edição indisponível · "+id;
    if(included)caption=std::to_string(index+1)+". "+caption;
    auto *row=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,6);auto *check=Gtk::make_managed<Gtk::CheckButton>(caption);check->set_hexpand(true);check->set_active(included);check->set_sensitive(!site_.removed&&(available||included));
    if(auto *label=dynamic_cast<Gtk::Label*>(check->get_child())){label->set_wrap(true);label->set_xalign(0);}
    check->signal_toggled().connect([this,check,id]{if(!binding_)guard([&]{select_catalog(id,check->get_active());});});row->append(*check);
    auto *up=Gtk::make_managed<Gtk::Button>("Subir");auto *down=Gtk::make_managed<Gtk::Button>("Descer");
    up->set_sensitive(included&&index>0&&!site_.removed);down->set_sensitive(included&&index+1<site_.catalog.size()&&!site_.removed);
    up->set_tooltip_text("Mover esta Obra para cima no catálogo");down->set_tooltip_text("Mover esta Obra para baixo no catálogo");
    up->signal_clicked().connect([this,id]{guard([&]{move_catalog(id,-1);});});down->signal_clicked().connect([this,id]{guard([&]{move_catalog(id,1);});});
    row->append(*up);row->append(*down);catalog_box_.append(*row);
  }
  binding_=false;
}
void PublicationPage::select_catalog(const std::string &id,bool included){
  std::erase(site_.catalog,id);if(included)site_.catalog.push_back(id);changed();
  Glib::signal_idle().connect_once(sigc::track_object([this]{fill_catalog();},*this));
}
void PublicationPage::move_catalog(const std::string &id,int delta){
  const auto it=std::find(site_.catalog.begin(),site_.catalog.end(),id);if(it==site_.catalog.end())return;
  const auto target=(it-site_.catalog.begin())+delta;if(target<0||target>=static_cast<std::ptrdiff_t>(site_.catalog.size()))return;
  std::iter_swap(it,site_.catalog.begin()+target);changed();Glib::signal_idle().connect_once(sigc::track_object([this]{fill_catalog();},*this));
}
void PublicationPage::save_catalog(){
  if(notebook_.get_current_page()!=2 || (dirty_&&dirty_page_!=2))throw std::runtime_error("Abra a aba Catálogo para salvar suas escolhas");
  site_=service_.update_catalog(site_id_,site_.revision,site_.catalog);dirty_=false;refresh();notebook_.set_current_page(2);status_.set_text("Catálogo salvo · "+std::to_string(site_.catalog.size())+" Obra(s)");
}
}

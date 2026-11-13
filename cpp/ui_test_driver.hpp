#pragma once
#include "publication_ui.hpp"
namespace inde::ui {
struct PublicationPageProbe {
 static void draft_site(PublicationPage &p){
  p.name_.set_text("Biblioteca da estação");p.author_.set_text("Autoria de demonstração");p.description_.set_text("Histórias para descobrir — edição INDE");p.base_path_.set_text("/biblioteca");
  if(p.can_leave())throw std::runtime_error("O formulário sujo permitiu saída");
 }
 static void commit_site(PublicationPage &p){if(p.name_.get_text()!=Glib::ustring("Biblioteca da estação"))throw std::runtime_error("Reabertura perdeu o formulário");p.save_site();}
 static void site(PublicationPage &p){draft_site(p);commit_site(p);}
 static void edition(PublicationPage &p){
  p.notebook_.set_current_page(1);p.title_.set_text("Edição de leitura");p.slug_.set_text("estacao-inde");
  p.work_.set_active(0);
  for(auto *c=p.source_box_.get_first_child();c;c=c->get_next_sibling())if(auto *check=dynamic_cast<Gtk::CheckButton*>(c))check->set_active(true);
  for(auto *c=p.entity_box_.get_first_child();c;c=c->get_next_sibling())if(auto *check=dynamic_cast<Gtk::CheckButton*>(c))check->set_active(true);
  p.save_edition();
 }
 static void recover(PublicationPage &p){
  const auto site=p.site_id_,edition=p.edition_id_;
  const auto click=[](Gtk::Button &button){g_signal_emit_by_name(button.gobj(),"clicked");};
  click(p.remove_site_);click(p.confirm_yes_);if(p.service_.site(site).removed)throw std::runtime_error("Site em uso foi retirado pela UI");
  click(p.remove_edition_);click(p.confirm_yes_);p.removed_.set_active(true);
  if(p.edition_id_!=edition||!p.edition_.removed)throw std::runtime_error("Retirada da Edição não ficou recuperável na UI");
  click(p.remove_site_);click(p.confirm_yes_);if(!p.site_.removed)throw std::runtime_error("Site não foi retirado pela UI");
  click(p.remove_site_);click(p.confirm_yes_);click(p.remove_edition_);click(p.confirm_yes_);
  if(p.site_id_!=site||p.edition_id_!=edition||p.site_.removed||p.edition_.removed)throw std::runtime_error("Restauração na UI perdeu identidade");
 }
 static void generate(PublicationPage &p){p.prepare();if(!p.job_)throw std::runtime_error("Geração não iniciou");if(p.can_leave())throw std::runtime_error("Geração permitiu saída");}
 static bool done(PublicationPage &p){if(p.job_)return false;if(!p.preview_button_.get_sensitive())throw std::runtime_error(p.status_.get_text());return true;}
 static void catalog_draft(PublicationPage &p){
  p.notebook_.set_current_page(2);if(p.site_.catalog.size()!=2)throw std::runtime_error("A demo não tem duas escolhas");
  const auto target=p.site_.catalog[1];const auto title=p.service_.edition(target).title;
  Gtk::Button *button=nullptr;
  for(auto *row=p.catalog_box_.get_first_child();row;row=row->get_next_sibling()){
    auto *box=dynamic_cast<Gtk::Box*>(row);if(!box)continue;auto *check=dynamic_cast<Gtk::CheckButton*>(box->get_first_child());
    if(check&&check->get_label().raw().find(title)!=std::string::npos)button=dynamic_cast<Gtk::Button*>(check->get_next_sibling());
  }
  if(!button)throw std::runtime_error("Botão de reordenação ausente");
  g_signal_emit_by_name(button->gobj(),"clicked");
  if(p.site_.catalog.front()!=target||p.can_leave())throw std::runtime_error("Reordenação não ficou pendente");
  if(p.service_.site(p.site_id_).catalog==p.site_.catalog)throw std::runtime_error("Reordenação salvou implicitamente");
 }
 static void catalog_commit(PublicationPage &p){
  const auto expected=p.site_.catalog;g_signal_emit_by_name(p.save_catalog_.gobj(),"clicked");
  if(p.service_.site(p.site_id_).catalog!=expected)throw std::runtime_error("Salvar catálogo perdeu a ordem");
 }
 static void catalog_generate(PublicationPage &p){p.prepare(true);if(!p.job_)throw std::runtime_error("Geração do Site não iniciou");}
 static std::filesystem::path catalog_output(PublicationPage &p){return p.output_;}
 static void preview(PublicationPage &p){p.preview();}
 static std::filesystem::path preview_control(PublicationPage &p){return p.preview_control_;}
 static void show_site(PublicationPage &p){p.notebook_.set_current_page(0);}
 static void back(PublicationPage &p){p.refresh();if(p.site_id_.empty()||p.edition_id_.empty())throw std::runtime_error("Reabrir perdeu seleção persistida");p.notebook_.set_current_page(1);}
};
}

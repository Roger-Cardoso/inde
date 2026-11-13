#include "publication_ui.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <csignal>
namespace inde::ui {
namespace {
void field(Gtk::Grid &grid,int row,const char *text,Gtk::Widget &input){
  auto *label=Gtk::make_managed<Gtk::Label>(text);label->set_halign(Gtk::Align::START);
  label->set_mnemonic_widget(input);grid.attach(*label,0,row);input.set_hexpand(true);grid.attach(input,1,row);
}
void label(Gtk::Box &box,const std::string &text){auto *l=Gtk::make_managed<Gtk::Label>(text);l->set_wrap(true);l->set_xalign(0);box.append(*l);}
std::filesystem::path resources(){const auto *p=std::getenv("INDE_PUBLICACAO_RECURSOS");if(!p || !*p)throw std::runtime_error("Abra pelo lançador Abrir-INDE-especial.sh para localizar o leitor incluído.");return std::filesystem::path(p);}
GSubprocess *spawn(const std::vector<std::string> &args,GSubprocessFlags flags){
  std::vector<const char*> argv;for(const auto &a:args)argv.push_back(a.c_str());argv.push_back(nullptr);
  GError *error=nullptr;auto *child=g_subprocess_newv(argv.data(),flags,&error);
  if(!child){std::string message=error?error->message:"Falha ao iniciar o leitor";g_clear_error(&error);throw std::runtime_error(message);}return child;
}
}
struct PublicationPage::Job{
  PublicationPage *owner{};GSubprocess *process{};
  ~Job(){if(process)g_object_unref(process);}
};
PublicationPage::PublicationPage(publication::Service &s):Gtk::Box(Gtk::Orientation::VERTICAL,10),service_(s){
  set_size_request(580,420);set_margin(12);heading_.add_css_class("title-2");heading_.set_xalign(0);
  intro_.set_wrap(true);intro_.set_xalign(0);append(heading_);append(intro_);append(controls_);
  sites_.set_hexpand(true);toolbar_.append(sites_);toolbar_.append(removed_);toolbar_.append(reload_);controls_.append(toolbar_);
  controls_.append(notebook_);controls_.set_vexpand(true);notebook_.set_vexpand(true);
  site_grid_.set_column_spacing(16);site_grid_.set_row_spacing(8);
  theme_.append("violeta","Violeta");theme_.append("ambar","Âmbar");theme_.append("verde","Verde");
  destination_.append("local","Local");destination_.append("pacote","Pacote");destination_.append("github","GitHub (configuração)");
  field(site_grid_,0,"Nome do Site",name_);field(site_grid_,1,"Autoria pública",author_);field(site_grid_,2,"Descrição",description_);
  field(site_grid_,3,"Tema",theme_);field(site_grid_,4,"Origem HTTPS (opcional)",origin_);field(site_grid_,5,"Caminho base (ex.: /biblioteca)",base_path_);
  field(site_grid_,6,"Destino",destination_);field(site_grid_,7,"Repositório (dono/nome)",repository_);field(site_grid_,8,"Branch",branch_);
  site_form_.append(site_grid_);label(site_form_,"Os destinos preparam arquivos locais. Autenticação, push e implantação ainda não estão disponíveis.");
  for(auto *b:{&new_site_,&save_site_,&remove_site_})site_buttons_.append(*b);
  site_form_.append(site_buttons_);
  site_scroll_.set_min_content_height(330);site_scroll_.set_child(site_form_);site_scroll_.set_policy(Gtk::PolicyType::NEVER,Gtk::PolicyType::AUTOMATIC);notebook_.append_page(site_scroll_,"Site");
  editions_.set_hexpand(true);edition_form_.append(editions_);edition_grid_.set_column_spacing(16);edition_grid_.set_row_spacing(8);
  field(edition_grid_,0,"Nome da Edição",title_);field(edition_grid_,1,"Endereço da Obra",slug_);field(edition_grid_,2,"Obra do Projeto",work_);field(edition_grid_,3,"Estrutura editorial",structure_);
  edition_form_.append(edition_grid_);label(edition_form_,"Documentos incluídos — seleção explícita. A ordem vem da estrutura editorial; os números dos endereços são reservados ao salvar.");
  label(edition_form_,"A Edição usa a redação corrente e integral de cada Documento. Ocultar trechos no editor não os retira da publicação; alternativas guardadas permanecem privadas. Confira o texto integral antes de preparar.");
  edition_form_.append(source_box_);label(edition_form_,"Personagens publicados — selecione os usados nas anotações e na perspectiva.");edition_form_.append(entity_box_);
  for(auto *b:{&new_edition_,&save_edition_,&remove_edition_})edition_buttons_.append(*b);
  edition_form_.append(edition_buttons_);
  edition_scroll_.set_min_content_height(330);edition_scroll_.set_child(edition_form_);edition_scroll_.set_policy(Gtk::PolicyType::NEVER,Gtk::PolicyType::AUTOMATIC);notebook_.append_page(edition_scroll_,"Edição e fontes");
  label(catalog_form_,"Escolha uma Edição de cada Obra e ajuste a ordem. Somente as Edições marcadas entram no Site. Um catálogo vazio gera uma biblioteca vazia.");
  catalog_form_.append(catalog_box_);catalog_form_.append(save_catalog_);
  catalog_scroll_.set_min_content_height(330);catalog_scroll_.set_policy(Gtk::PolicyType::NEVER,Gtk::PolicyType::AUTOMATIC);catalog_scroll_.set_child(catalog_form_);notebook_.append_page(catalog_scroll_,"Catálogo do Site");
  save_catalog_.signal_clicked().connect([this]{guard([&]{save_catalog();});});
  prepare_site_.signal_clicked().connect([this]{guard([&]{prepare(true);});});
  prepare_site_.set_visible(false);
  bottom_.append(prepare_site_);bottom_.append(prepare_);bottom_.append(preview_button_);bottom_.append(discard_);append(bottom_);
  confirm_label_.set_wrap(true);confirm_label_.set_xalign(0);append(confirm_label_);append(confirm_yes_);append(confirm_no_);
  confirm_label_.set_visible(false);confirm_yes_.set_visible(false);confirm_no_.set_visible(false);
  status_.set_wrap(true);status_.set_xalign(0);status_.set_selectable(true);append(status_);preview_button_.set_sensitive(false);
  for(auto *entry:{&name_,&author_,&description_,&origin_,&base_path_,&repository_,&branch_,&title_,&slug_})entry->signal_changed().connect([this]{changed();});
  theme_.signal_changed().connect([this]{changed();});destination_.signal_changed().connect([this]{changed();});
  sites_.signal_changed().connect([this]{if(binding_)return;guard([&]{const auto id=sites_.get_active_id().raw();if(clean())load_site(id);else{binding_=true;sites_.set_active_id(site_id_);binding_=false;}});});
  editions_.signal_changed().connect([this]{if(binding_)return;guard([&]{const auto id=editions_.get_active_id().raw();if(clean())load_edition(id);else{binding_=true;editions_.set_active_id(edition_id_);binding_=false;}});});
  work_.signal_changed().connect([this]{if(binding_)return;guard([&]{edition_.work_id=work_.get_active_id();edition_.structure_id.clear();edition_.choices.clear();changed();fill_sources();});});
  structure_.signal_changed().connect([this]{if(binding_)return;guard([&]{edition_.structure_id=structure_.get_active_id();edition_.choices.clear();changed();fill_sources();});});
  reload_.signal_clicked().connect([this]{guard([&]{if(clean())refresh();});});
  removed_.signal_toggled().connect([this]{if(!binding_)guard([&]{if(clean())refresh();else{binding_=true;removed_.set_active(!removed_.get_active());binding_=false;}});});
  discard_.signal_clicked().connect([this]{guard([&]{dirty_=false;refresh();status_.set_text("Alterações do formulário descartadas. Os registros salvos permanecem.");});});
  new_site_.signal_clicked().connect([this]{guard([&]{if(!clean())return;site_={};site_id_.clear();edition_id_.clear();edition_={};binding_=true;sites_.set_active(-1);binding_=false;fill_site();fill_edition();changed();});});
  new_edition_.signal_clicked().connect([this]{guard([&]{if(!clean())return;if(site_id_.empty()||site_.removed)throw std::runtime_error("Salve ou restaure primeiro um Site.");edition_={};edition_.site_id=site_id_;edition_id_.clear();binding_=true;editions_.set_active(-1);binding_=false;fill_edition();changed();notebook_.set_current_page(1);});});
  save_site_.signal_clicked().connect([this]{guard([&]{save_site();});});save_edition_.signal_clicked().connect([this]{guard([&]{save_edition();});});
  remove_site_.signal_clicked().connect([this]{guard([&]{if(!clean()||site_id_.empty())return;confirm(site_.removed?"Restaurar este Site?":"Retirar este Site? Retire antes suas Edições ativas. O registro será recuperável.",[this]{auto s=site_.removed?service_.restore_site(site_id_,site_.revision):service_.remove_site(site_id_,site_.revision);refresh();status_.set_text(s.removed?"Site retirado; disponível em Mostrar retirados.":"Site restaurado.");});});});
  remove_edition_.signal_clicked().connect([this]{guard([&]{if(!clean()||edition_id_.empty())return;confirm(edition_.removed?"Restaurar esta Edição?":"Retirar esta Edição? Os Documentos do INDE permanecem e a Edição será recuperável.",[this]{auto e=edition_.removed?service_.restore_edition(edition_id_,edition_.revision):service_.remove_edition(edition_id_,edition_.revision);refresh();status_.set_text(e.removed?"Edição retirada; disponível em Mostrar retirados.":"Edição restaurada.");});});});
  confirm_yes_.signal_clicked().connect([this]{auto fn=std::move(confirmation_);confirm_label_.set_visible(false);confirm_yes_.set_visible(false);confirm_no_.set_visible(false);controls_.set_sensitive(true);bottom_.set_sensitive(true);guard(fn);});
  confirm_no_.signal_clicked().connect([this]{confirmation_={};confirm_label_.set_visible(false);confirm_yes_.set_visible(false);confirm_no_.set_visible(false);controls_.set_sensitive(true);bottom_.set_sensitive(true);});
  notebook_.signal_switch_page().connect([this](Gtk::Widget*,guint page){prepare_.set_visible(page!=2);prepare_site_.set_visible(page==2);if(dirty_&&!binding_&&int(page)!=dirty_page_){status_.set_text("Salve ou descarte as alterações antes de trocar de aba.");Glib::signal_idle().connect_once(sigc::track_object([this]{binding_=true;notebook_.set_current_page(dirty_page_);binding_=false;},*this));}});
  prepare_.signal_clicked().connect([this]{guard([&]{prepare();});});preview_button_.signal_clicked().connect([this]{guard([&]{preview();});});
}
PublicationPage::~PublicationPage(){if(job_){job_->owner=nullptr;g_subprocess_force_exit(job_->process);}stop_preview();}
void PublicationPage::guard(const std::function<void()> &fn){try{if(fn)fn();}catch(const std::exception &e){status_.set_text(e.what());}}
void PublicationPage::changed(){if(!binding_){if(!dirty_)dirty_page_=notebook_.get_current_page();dirty_=true;prepared_.reset();preview_button_.set_sensitive(false);status_.set_text("Há alterações no formulário. Salve ou descarte antes de sair, trocar a seleção ou gerar.");}}
bool PublicationPage::clean(){if(dirty_){status_.set_text("Salve ou descarte as alterações do formulário primeiro.");return false;}return true;}
bool PublicationPage::can_leave(){if(job_){status_.set_text("Aguarde a geração terminar.");return false;}if(confirmation_){status_.set_text("Confirme ou cancele a operação pendente.");return false;}return clean();}
void PublicationPage::confirm(const std::string &message,std::function<void()> fn){confirmation_=std::move(fn);confirm_label_.set_text(message);confirm_label_.set_visible(true);confirm_yes_.set_visible(true);confirm_no_.set_visible(true);controls_.set_sensitive(false);bottom_.set_sensitive(false);}
void PublicationPage::refresh(){
  const auto *p=service_.source().current();if(!p)throw std::runtime_error("Abra um Projeto primeiro.");
  if(project_id_!=p->manifest().project_id){project_id_=p->manifest().project_id;site_id_.clear();edition_id_.clear();}
  binding_=true;sites_.remove_all();const auto sites=service_.sites(removed_.get_active());
  for(const auto &s:sites)sites_.append(s.id,s.name+(s.removed?" — retirado":""));
  const auto selected=std::find_if(sites.begin(),sites.end(),[&](const auto &s){return s.id==site_id_;});
  const auto id=selected!=sites.end()?selected->id:sites.empty()?std::string{}:sites.front().id;
  sites_.set_active_id(id);binding_=false;load_site(id);status_.set_text(id.empty()?"Preencha a configuração do Site para começar.":"Escolha uma Edição ou crie outra para preparar a publicação.");
}
void PublicationPage::load_site(const std::string &id){
  site_id_=id;site_=id.empty()?publication::Site{}:service_.site(id);fill_site();
  binding_=true;editions_.remove_all();const auto editions=id.empty()?std::vector<publication::Edition>{}:service_.editions(id,removed_.get_active());
  for(const auto &e:editions)editions_.append(e.id,e.title+(e.removed?" — retirada":""));
  const auto selected=std::find_if(editions.begin(),editions.end(),[&](const auto &e){return e.id==edition_id_;});
  const auto eid=selected!=editions.end()?selected->id:editions.empty()?std::string{}:editions.front().id;editions_.set_active_id(eid);binding_=false;load_edition(eid);
}
void PublicationPage::fill_site(){binding_=true;name_.set_text(site_.name);author_.set_text(site_.author);description_.set_text(site_.description);theme_.set_active_id(site_.theme);origin_.set_text(site_.origin);base_path_.set_text(site_.base_path);destination_.set_active_id(site_.destination);repository_.set_text(site_.repository);branch_.set_text(site_.branch);site_grid_.set_sensitive(!site_.removed);save_site_.set_sensitive(!site_.removed);remove_site_.set_sensitive(!site_id_.empty());remove_site_.set_label(site_.removed?"Restaurar Site":"Retirar Site");binding_=false;dirty_=false;}
void PublicationPage::load_edition(const std::string &id){edition_id_=id;edition_=id.empty()?publication::Edition{}:service_.edition(id);edition_.site_id=site_id_;fill_edition();prepared_.reset();preview_button_.set_sensitive(false);}
void PublicationPage::fill_edition(){binding_=true;title_.set_text(edition_.title);slug_.set_text(edition_.slug);work_.remove_all();for(const auto &w:service_.source().catalog().works)work_.append(w.id,w.title);if(!edition_.work_id.empty())work_.set_active_id(edition_.work_id);edition_grid_.set_sensitive(!site_id_.empty()&&!site_.removed&&!edition_.removed);source_box_.set_sensitive(!site_.removed&&!edition_.removed);entity_box_.set_sensitive(!site_.removed&&!edition_.removed);save_edition_.set_sensitive(!site_.removed&&!edition_.removed);remove_edition_.set_sensitive(!edition_id_.empty());prepare_.set_sensitive(!edition_id_.empty()&&!site_.removed&&!edition_.removed);remove_edition_.set_label(edition_.removed?"Restaurar Edição":"Retirar Edição");binding_=false;fill_sources();fill_catalog();dirty_=false;}
void PublicationPage::fill_sources(){
  binding_=true;structure_.remove_all();if(!edition_.work_id.empty()){
    const auto structures=service_.source().structures().editorial_structures(edition_.work_id);for(const auto &s:structures)structure_.append(s.id,s.name);
    if(edition_.structure_id.empty()&&!structures.empty())edition_.structure_id=structures.front().id;
    structure_.set_active_id(edition_.structure_id);
  }
  while(auto *child=source_box_.get_first_child())source_box_.remove(*child);
  while(auto *child=entity_box_.get_first_child())entity_box_.remove(*child);
  if(!edition_.work_id.empty()){
    auto nodes=service_.source().structural_nodes_for_work(edition_.work_id);std::erase_if(nodes,[&](const auto &n){return n.structure_id!=edition_.structure_id;});
    const auto docs=service_.source().writing().documents();std::size_t displayed=0;
    for(const auto &node:application::production_order(nodes))for(const auto &d:docs)if(d.editorial_node_id==node.id){
      if(++displayed>200)continue;
      auto *check=Gtk::make_managed<Gtk::CheckButton>(node.title+" · "+d.title);
      const auto chosen=edition_.choices.find(node.id);check->set_active(chosen!=edition_.choices.end()&&std::find(chosen->second.documents.begin(),chosen->second.documents.end(),d.id)!=chosen->second.documents.end());
      check->signal_toggled().connect([this,check,node_id=node.id,id=d.id]{if(binding_)return;auto &choice=edition_.choices[node_id];choice.heading=true;std::erase(choice.documents,id);if(check->get_active())choice.documents.push_back(id);if(choice.documents.empty())edition_.choices.erase(node_id);changed();});source_box_.append(*check);
    }
    if(displayed>200)label(source_box_,"A lista mostra os primeiros 200 Documentos. Escolhas já salvas fora da lista são preservadas.");
  }
  const auto types=service_.source().narrative().entity_types();
  for(const auto &entity:service_.source().narrative().entities())if(std::any_of(types.begin(),types.end(),[&](const auto &t){return t.id==entity.entity_type_id&&t.key=="character";})){
    auto *check=Gtk::make_managed<Gtk::CheckButton>(entity.name);check->set_active(std::find(edition_.entities.begin(),edition_.entities.end(),entity.id)!=edition_.entities.end());
    check->signal_toggled().connect([this,check,id=entity.id]{if(binding_)return;std::erase(edition_.entities,id);if(check->get_active())edition_.entities.push_back(id);changed();});entity_box_.append(*check);
  }
  binding_=false;
}
void PublicationPage::save_site(){
  if(notebook_.get_current_page()!=0 || (dirty_ && dirty_page_!=0))throw std::runtime_error("Abra a aba Site para salvar sua configuração.");
  auto s=site_;s.name=name_.get_text();s.author=author_.get_text();s.description=description_.get_text();s.theme=theme_.get_active_id();s.origin=origin_.get_text();s.base_path=base_path_.get_text();s.destination=destination_.get_active_id();s.repository=repository_.get_text();s.branch=branch_.get_text();
  s=site_id_.empty()?service_.create_site(s):service_.update_site(s,site_.revision);site_id_=s.id;dirty_=false;refresh();status_.set_text("Site salvo no Projeto · revisão "+std::to_string(s.revision));
}
void PublicationPage::save_edition(){
  if(notebook_.get_current_page()!=1 || (dirty_ && dirty_page_!=1))throw std::runtime_error("Abra a aba Edição para salvar suas escolhas.");
  auto e=edition_;e.title=title_.get_text();e.slug=slug_.get_text();e.site_id=site_id_;e.work_id=work_.get_active_id();e.structure_id=structure_.get_active_id();
  e=edition_id_.empty()?service_.create_edition(e):service_.update_edition(e,edition_.revision);edition_id_=e.id;dirty_=false;refresh();notebook_.set_current_page(1);status_.set_text("Edição salva · revisão "+std::to_string(e.revision));
}
void PublicationPage::prepare(bool catalog){
  if(!clean())return;
  if(!catalog && edition_id_.empty())throw std::runtime_error("Crie e salve uma Edição primeiro.");
  const auto root=resources();prepared_=catalog?service_.prepare_catalog(site_id_,site_.revision):service_.prepare(edition_id_,site_.revision,edition_.revision);const auto input=service_.materialize(*prepared_);
  output_=service_.source().current()->path()/"outputs"/"web"/prepared_->id/"site";
  const auto flags=GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_PIPE);
  job_=std::make_shared<Job>();job_->owner=this;
  try{job_->process=spawn({"python3",(root/"publicacao.py").string(),"--gerar-edicao",input.string(),"--destino",output_.string()},flags);}catch(...){job_.reset();throw;}
  controls_.set_sensitive(false);bottom_.set_sensitive(false);preview_button_.set_sensitive(false);status_.set_text("Gerando o leitor incluído a partir do retrato preparado…");
  auto *context=new std::shared_ptr<Job>(job_);
  g_subprocess_communicate_utf8_async(job_->process,nullptr,nullptr,[](GObject *object,GAsyncResult *result,gpointer data){
    std::unique_ptr<std::shared_ptr<Job>> holder(static_cast<std::shared_ptr<Job>*>(data));auto job=*holder;
    gchar *out=nullptr,*err=nullptr;GError *error=nullptr;const bool communicated=g_subprocess_communicate_utf8_finish(G_SUBPROCESS(object),result,&out,&err,&error);
    const bool ok=communicated&&g_subprocess_get_successful(G_SUBPROCESS(object));std::string message=error?error->message:err&&*err?err:out?out:"";
    g_free(out);g_free(err);g_clear_error(&error);if(job->owner)job->owner->generated(ok,message);
  },context);
}
void PublicationPage::generated(bool ok,const std::string &message){job_.reset();controls_.set_sensitive(true);bottom_.set_sensitive(true);bool input_ok=false;if(prepared_){std::ifstream input(service_.source().current()->path()/"data/publicacao-retratos"/(prepared_->id+".json"),std::ios::binary);const std::string content{std::istreambuf_iterator<char>(input),{}};input_ok=publication::hash(content)==prepared_->payload_hash;}
  const bool current=ok&&input_ok&&prepared_&&service_.is_current(*prepared_);preview_button_.set_sensitive(current);status_.set_text(current?std::string(prepared_->catalog?"Site gerado e conferido. Pacote público: ":"Edição gerada e conferida. Pacote público: ")+(output_.parent_path()/"site-publico.zip").string():ok?"A origem mudou durante a geração. Prepare novamente; a saída anterior não é apresentada como atual.":"Geração recusada: "+message);}
void PublicationPage::stop_preview(){if(preview_job_){preview_job_->owner=nullptr;g_subprocess_send_signal(preview_job_->process,SIGTERM);preview_job_.reset();}preview_id_.clear();}
void PublicationPage::preview(){
  if(!prepared_||!service_.is_current(*prepared_))throw std::runtime_error("A origem mudou. Prepare novamente.");
  const auto script=(resources()/"publicacao.py").string();
  if(preview_job_&&preview_id_==prepared_->id){
    auto *opener=spawn({"python3",script,"--reabrir-previa",preview_control_.string()},G_SUBPROCESS_FLAGS_NONE);g_object_unref(opener);
    status_.set_text("A prévia desta geração foi reutilizada. Diagnóstico do navegador: "+(output_.parent_path()/"navegador.log").string());return;
  }
  stop_preview();preview_id_=prepared_->id;preview_control_=output_.parent_path()/"previa-servidor.json";
  preview_job_=std::make_shared<Job>();preview_job_->owner=this;
  try{preview_job_->process=spawn({"python3",script,"--servir-edicao",output_.string(),"--controle-previa",preview_control_.string()},G_SUBPROCESS_FLAGS_NONE);}catch(...){preview_job_.reset();throw;}
  auto *context=new std::shared_ptr<Job>(preview_job_);
  g_subprocess_wait_async(preview_job_->process,nullptr,[](GObject *object,GAsyncResult *result,gpointer data){
    std::unique_ptr<std::shared_ptr<Job>> holder(static_cast<std::shared_ptr<Job>*>(data));auto job=*holder;GError *error=nullptr;g_subprocess_wait_finish(G_SUBPROCESS(object),result,&error);g_clear_error(&error);
    if(job->owner&&job->owner->preview_job_==job){job->owner->preview_job_.reset();job->owner->preview_id_.clear();}
  },context);
  status_.set_text("Prévia local iniciada. Novos cliques reutilizam esta geração. Diagnóstico do navegador: "+(output_.parent_path()/"navegador.log").string());
}
}

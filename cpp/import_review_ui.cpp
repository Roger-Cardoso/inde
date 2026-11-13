#include "inde/ui/main_window.hpp"
#include "inde/ui/application_theme.hpp"
#include "snapshot_model.hpp"
#include <gio/gio.h>
#include <fstream>
#include <iostream>
#include <algorithm>
void capture_window(Gtk::Widget &,const std::filesystem::path &);
using namespace inde;using namespace inde::publication;namespace fs=std::filesystem;
namespace {
V read(const fs::path &p){if(fs::file_size(p)>64*1024*1024)throw std::runtime_error("Conferência excede 64 MiB");std::ifstream f(p);if(!f)throw std::runtime_error("Conferência não encontrada");return persistence::json::parse(std::string{std::istreambuf_iterator<char>(f),{}});}
std::string describe(const V &report,const char *kind,const char *label){const auto &c=report.at("candidates").at(kind);std::string text=std::string(label)+"\n"+fs::path(report.at("sources").at(kind).at("path").as_string()).filename().string()+"\n\n";
      if(!c.at("content").is_null()){
        const auto &content=c.at("content");text+=content.at("text").as_string()+"\n\nRecursos conferidos\n";
        text+="Origem do capítulo: "+content.at("chapter_id").as_string()+"\n";
        std::map<std::string,std::string> names;for(const auto &entity:content.at("entities").as_array()){const auto id=entity.at("id").as_string();names[id]=entity.at("name").as_string();text+="Personagem: "+names[id]+" · "+id+"\n";}
        for(const auto &annotation:content.at("annotations").as_array()){text+=(annotation.at("kind").as_string()=="dialogue"?"Fala de ":"Pensamento de ")+names[annotation.at("target").as_string()]+" · "+annotation.at("id").as_string()+" · caracteres "+std::to_string(annotation.at("start").as_integer()+1)+"–"+std::to_string(annotation.at("end").as_integer())+"\n";}
        for(const auto &scene:content.at("scenes").as_array())text+="Cena: "+scene.at("id").as_string()+" · caracteres "+std::to_string(scene.at("start").as_integer()+1)+"–"+std::to_string(scene.at("end").as_integer())+"\n";
        for(const auto &format:content.at("formatting").as_array()){const auto style=format.at("style").as_string();text+=(style=="bold"?"Negrito":style=="italic"?"Itálico":"Sublinhado")+std::string(" · caracteres ")+std::to_string(format.at("start").as_integer()+1)+"–"+std::to_string(format.at("end").as_integer())+"\n";}
        if(!content.at("perspective").is_null())text+="Perspectiva: terceira pessoa limitada · "+names[content.at("perspective").at("entity").as_string()]+"\n";
      }
      if(!c.at("issues").as_array().empty())text+="\nRecursos que impedem esta conversão\n";
      for(const auto &issue:c.at("issues").as_array()){text+="• "+issue.as_string()+"\n";}return text;}
class ImportReview final: public Gtk::Window {
public:
  ImportReview(const fs::path &report,bool smoke):report_path_(report),report_(read(report)),smoke_(smoke){
    ui::install_application_theme();add_css_class("inde-window");set_title("INDE · Conferir importação do Literature");set_default_size(1120,800);set_child(root_);root_.set_margin(18);
    heading_.set_xalign(0);heading_.add_css_class("title-2");root_.append(heading_);intro_.set_wrap(true);intro_.set_xalign(0);root_.append(intro_);
    work_mode_=report_.at("protocol").as_string()=="inde.literature.work-review";
    if(work_mode_){heading_.set_text("Conferir Obra importada");intro_.set_text("Escolha a fonte de cada capítulo. A conversão cria um Projeto, uma Obra e uma Edição com as identidades públicas preservadas.");for(const auto &item:report_.at("chapters").as_array())chapters_.append(item.at("id").as_string(),item.at("title").as_string()+" · URL capitulo-"+std::to_string(item.at("number").as_integer()));root_.append(chapters_);identity_.set_xalign(0);identity_.set_wrap(true);identity_.set_text("Identidade pública: "+report_.at("work").at("id").as_string()+" · /"+report_.at("work").at("slug").as_string()+"/ · próximo número reservado: "+std::to_string(report_.at("work").at("next_number").as_integer()));root_.append(identity_);}
    pane_.set_start_child(left_scroll_);pane_.set_end_child(right_scroll_);pane_.set_vexpand(true);root_.append(pane_);
    for(auto *view:{&left_,&right_}){view->set_editable(false);view->set_cursor_visible(false);view->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);view->set_left_margin(12);view->set_right_margin(12);}
    left_scroll_.set_child(left_);right_scroll_.set_child(right_);

    choice_.append("html","Usar HTML aplicado");choice_.append("autoria","Usar autoria salva");choice_label_.set_xalign(0);root_.append(choice_label_);root_.append(choice_);
    title_.set_placeholder_text("Nome da nova Obra");title_.set_text("Obra importada do Literature");title_label_.set_xalign(0);root_.append(title_label_);root_.append(title_);
    target_.set_placeholder_text("Pasta nova para o resultado");target_.set_text((report_path_.parent_path()/"resultado-importado").string());target_label_.set_xalign(0);root_.append(target_label_);root_.append(target_);
    actions_.append(convert_);actions_.append(open_);root_.append(actions_);convert_.set_sensitive(false);open_.set_sensitive(false);
    status_.set_wrap(true);status_.set_selectable(true);status_.set_xalign(0);root_.append(status_);
    if(work_mode_){title_label_.set_text("Obra selecionada");title_.set_text(report_.at("work").at("title").as_string());title_.set_sensitive(false);convert_.set_label("Criar Projeto e Edição");chapters_.signal_changed().connect([this]{try{refresh_chapter();}catch(const std::exception &e){status_.set_text(e.what());convert_.set_sensitive(false);}});chapters_.set_active(0);}else refresh_chapter();
    choice_.signal_changed().connect([this]{if(loading_)return;const auto id=choice_.get_active_id().raw();if(work_mode_&&!current_chapter_.empty())choices_[current_chapter_]=id;try{update_choice();}catch(const std::exception &e){status_.set_text(e.what());convert_.set_sensitive(false);}});
    convert_.signal_clicked().connect([this]{try{begin();}catch(const std::exception &e){status_.set_text(e.what());chapters_.set_sensitive(true);choice_.set_sensitive(true);title_.set_sensitive(!work_mode_);target_.set_sensitive(true);convert_.set_sensitive(true);}});
    open_.signal_clicked().connect([this]{try{spawn_editor();}catch(const std::exception &e){status_.set_text(e.what());}});
    if(smoke_)Glib::signal_timeout().connect_once([this]{try{if(work_mode_){for(int i=0;i<static_cast<int>(report_.at("chapters").as_array().size());++i){chapters_.set_active(i);choice_.set_active_id(i%2?"html":"autoria");}}else choice_.set_active_id("autoria");begin();}catch(const std::exception &error){std::cerr<<error.what()<<'\n';hide();}},300);
  }
  ~ImportReview()override{if(job_){job_->owner=nullptr;g_subprocess_force_exit(job_->process);}}
  int result()const{return result_;}
private:
  struct Job{ImportReview *owner{};GSubprocess *process{};~Job(){if(process)g_object_unref(process);}};
  bool on_close_request()override{if(job_){status_.set_text("Aguarde a conversão terminar. As fontes originais permanecem preservadas.");return true;}return Gtk::Window::on_close_request();}
  void refresh_chapter(){
    loading_=true;V review=report_;
    if(work_mode_){current_chapter_=chapters_.get_active_id().raw();const auto &entries=report_.at("chapters").as_array();const auto i=std::find_if(entries.begin(),entries.end(),[&](const auto &v){return v.at("id").as_string()==current_chapter_;});if(i==entries.end()){loading_=false;return;}review=read(report_path_.parent_path()/i->at("review").as_string());}
    current_review_=review;left_.get_buffer()->set_text(describe(review,"html","HTML APLICADO"));right_.get_buffer()->set_text(describe(review,"autoria","AUTORIA SALVA"));
    if(work_mode_){const auto chosen=choices_.find(current_chapter_);if(chosen==choices_.end())choice_.set_active(-1);else choice_.set_active_id(chosen->second);}loading_=false;update_choice();
  }
  void update_choice(){
    const auto id=choice_.get_active_id().raw();bool selected=!id.empty()&&current_review_.at("candidates").at(id).at("convertible").as_boolean();
    if(work_mode_){if(!current_chapter_.empty()&&!id.empty())ready_choices_[current_chapter_]=current_review_.at("candidates").at(id).at("convertible").as_boolean();selected=choices_.size()==report_.at("chapters").as_array().size();for(const auto &[key,value]:choices_)selected=selected&&!value.empty()&&ready_choices_[key];}
    convert_.set_sensitive(selected&&!job_&&!converted_);std::string status=current_review_.at("equal").as_boolean()?"As fontes são equivalentes. Escolha explicitamente qual usar.":"Há diferenças de texto ou recursos. Confira os dois lados antes de escolher.";
    if(!id.empty()&&!current_review_.at("candidates").at(id).at("convertible").as_boolean())status="Esta fonte contém recursos sem tradutor; a conversão está bloqueada.";
    if(work_mode_){status+=" Escolhas registradas: "+std::to_string(choices_.size())+" de "+std::to_string(report_.at("chapters").as_array().size())+" capítulos.";}status_.set_text(status);
  }
  void run(const std::vector<std::string> &args,int step){
    std::vector<const char*> argv;for(const auto &a:args)argv.push_back(a.c_str());argv.push_back(nullptr);GError *error=nullptr;
    job_=std::make_shared<Job>();job_->owner=this;job_->process=g_subprocess_newv(argv.data(),GSubprocessFlags(G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_PIPE),&error);
    if(!job_->process){const std::string message=error?error->message:"Falha ao iniciar conversão";g_clear_error(&error);job_.reset();throw std::runtime_error(message);}
    auto *context=new std::pair<std::shared_ptr<Job>,int>(job_,step);
    g_subprocess_communicate_utf8_async(job_->process,nullptr,nullptr,[](GObject *o,GAsyncResult *r,gpointer data){std::unique_ptr<std::pair<std::shared_ptr<Job>,int>> holder(static_cast<std::pair<std::shared_ptr<Job>,int>*>(data));auto job=holder->first;gchar *out=nullptr,*err=nullptr;GError *error=nullptr;const bool ok=g_subprocess_communicate_utf8_finish(G_SUBPROCESS(o),r,&out,&err,&error)&&g_subprocess_get_successful(G_SUBPROCESS(o));const std::string message=error?error->message:ok?(out?out:""):(err?err:"");g_free(out);g_free(err);g_clear_error(&error);if(job->owner)job->owner->completed(ok,message,holder->second);},context);
  }
  void begin(){
    const auto *resources=std::getenv("INDE_PUBLICACAO_RECURSOS");if(!resources)throw std::runtime_error("Abra pelo lançador de importação.");resources_=resources;
    const auto selected=choice_.get_active_id().raw();if(selected.empty())throw std::runtime_error("Escolha uma das fontes primeiro.");
    if(title_.get_text().empty()||target_.get_text().empty())throw std::runtime_error("Preencha Obra e destino novo.");
    target_path_=fs::absolute(target_.get_text().raw());plan_=report_path_.parent_path()/("plano-"+project::new_uuid()+".json");
    convert_.set_sensitive(false);chapters_.set_sensitive(false);choice_.set_sensitive(false);title_.set_sensitive(false);target_.set_sensitive(false);status_.set_text("Revalidando as duas fontes e preparando a escolha…");
    if(work_mode_){if(choices_.size()!=report_.at("chapters").as_array().size())throw std::runtime_error("Escolha a fonte de todos os capítulos.");O choices;for(const auto &[key,value]:choices_)choices.emplace(key,value);const auto choice_file=report_path_.parent_path()/("escolhas-"+project::new_uuid()+".json");{std::ofstream output(choice_file);output<<persistence::json::serialize(choices,true);output.close();if(!output)throw std::runtime_error("Falha ao guardar as escolhas.");}run({"python3",(resources_/"importar_obra.py").string(),"--conferencia",report_path_.string(),"--escolhas",choice_file.string(),"--saida",plan_.string()},1);return;}
    run({"python3",(resources_/"importacao.py").string(),"--plano",report_path_.string(),"--escolha",selected,"--saida",plan_.string(),"--titulo",title_.get_text().raw()},1);
  }
  void completed(bool ok,const std::string &message,int step){
    job_.reset();if(!ok){status_.set_text(message);chapters_.set_sensitive(true);choice_.set_sensitive(true);title_.set_sensitive(!work_mode_);target_.set_sensitive(true);convert_.set_sensitive(true);if(smoke_){std::cerr<<message;hide();}return;}
    if(step==1){status_.set_text("Criando o Projeto pelos serviços nativos do INDE…");try{run({(resources_/"build-nativo/inde_web_bridge").string(),work_mode_?"--importar-obra-literature":"--importar-literature",plan_.string(),target_path_.string()},2);}catch(const std::exception &e){status_.set_text(e.what());chapters_.set_sensitive(true);choice_.set_sensitive(true);title_.set_sensitive(!work_mode_);target_.set_sensitive(true);convert_.set_sensitive(true);if(smoke_)hide();}return;}
    converted_=true;chapters_.set_sensitive(true);project_=target_path_/"Importado.inde";status_.set_text("Projeto criado. Texto, marcações suportadas e mapa de identidades foram preservados. A continuidade das identidades públicas ainda precisa de reconciliação antes de gerar o Site.");open_.set_sensitive(true);if(work_mode_)status_.set_text("Projeto, Obra e Edição criados. Os IDs públicos, as rotas e as reservas de URL foram vinculados à Edição. Abra no INDE e use Publicação para conferir e preparar o Site. Dados do navegador dependem também da mesma origem.");
    if(smoke_){result_=0;std::cout<<(work_mode_?"PUB-03B GTK: escolhas por capítulo, revalidação, Projeto e Edição com identidades públicas OK\n":"PUB-03A GTK: escolha explícita, revalidação e Projeto criado pelos serviços nativos OK\n");Glib::signal_timeout().connect_once([this]{capture();hide();},300);}
  }
  void spawn_editor(){const auto executable=fs::read_symlink("/proc/self/exe").string();const std::string path=project_.string();const char *argv[]={executable.c_str(),path.c_str(),nullptr};GError *error=nullptr;auto *process=g_subprocess_newv(argv,G_SUBPROCESS_FLAGS_NONE,&error);if(!process){std::string message=error->message;g_error_free(error);throw std::runtime_error(message);}g_object_unref(process);}
  void capture();
  fs::path report_path_,resources_,target_path_,plan_,project_;V report_;bool smoke_{},work_mode_{},loading_{},converted_{};V current_review_;std::string current_chapter_;std::map<std::string,std::string> choices_;std::map<std::string,bool> ready_choices_;int result_{1};std::shared_ptr<Job> job_;
  Gtk::Box root_{Gtk::Orientation::VERTICAL,10},actions_{Gtk::Orientation::HORIZONTAL,8};Gtk::Label heading_{"Conferir importação"},intro_{"Compare o HTML aplicado com a autoria salva. A conversão cria uma nova Obra em um Projeto novo; nomes iguais não fundem entidades."},status_,choice_label_{"Fonte para a conversão"},title_label_{"Nome da nova Obra"},target_label_{"Pasta nova para o Projeto"};Gtk::ComboBoxText chapters_;Gtk::Label identity_;Gtk::Paned pane_;Gtk::ScrolledWindow left_scroll_,right_scroll_;Gtk::TextView left_,right_;Gtk::ComboBoxText choice_;Gtk::Entry title_,target_;Gtk::Button convert_{"Criar Projeto importado"},open_{"Abrir no INDE"};
};
void ImportReview::capture(){if(const auto *path=std::getenv("INDE_IMPORTACAO_CAPTURA")){capture_window(*this,path);}}
}
int run_import_review(const Glib::RefPtr<Gtk::Application> &app,const fs::path &report,bool smoke){std::unique_ptr<ImportReview> window;app->signal_activate().connect([&]{try{window=std::make_unique<ImportReview>(report,smoke);app->add_window(*window);window->signal_hide().connect([&]{app->quit();});window->present();}catch(const std::exception &error){std::cerr<<error.what()<<"\n";app->quit();}});app->run();return window?(smoke?window->result():0):1;}

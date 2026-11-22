#include "object_native_ui.hpp"
#include "object_composite_pilot.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include <webkit/webkit.h>
#include <fstream>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
namespace inde::ui {
namespace {
using V=persistence::json::Value;
struct Runtime {std::mutex mutex;std::unique_ptr<objects::pilot::Session> session;};
struct Lifetime {bool alive=true;bool draft=false;unsigned pending=0;};
struct Work {
  std::shared_ptr<Runtime> runtime;std::shared_ptr<Lifetime> life;
  V input;std::string output;
  WebKitScriptMessageReply *reply;JSCContext *context;
  ~Work(){webkit_script_message_reply_unref(reply);g_object_unref(context);}
};
void private_directory(const std::filesystem::path &p){
  if(::mkdir(p.c_str(),0700)!=0 && errno!=EEXIST)throw std::runtime_error("Não foi possível criar a pasta dos documentos compostos.");
  struct stat st{};if(::lstat(p.c_str(),&st)!=0||!S_ISDIR(st.st_mode)||st.st_uid!=::geteuid()||(st.st_mode&077)!=0)throw std::runtime_error("A pasta dos documentos compostos precisa ser privada e não pode ser um link.");
}
void asset(WebKitURISchemeRequest *request,gpointer){
  const std::string path=webkit_uri_scheme_request_get_path(request);
  auto name=path=="/"?std::string("index.html"):path.substr(1);
  const bool allowed=name=="index.html"||name=="app.js"||name=="native.js"||name=="style.css"||name=="sample.png"||name=="vendor/katex/katex.min.js"||name=="vendor/katex/katex.min.css"||(name.starts_with("vendor/katex/fonts/")&&name.ends_with(".woff2")&&name.find("..") == std::string::npos&&name.find('/',19)==std::string::npos);
  try{
    if(!allowed)throw std::runtime_error("Recurso indisponível");
    auto assets=std::filesystem::path(INDE_OBJECT_UI_DIR);
    std::error_code error;const auto executable=std::filesystem::read_symlink("/proc/self/exe",error);
    const auto installed=executable.parent_path()/"../share/inde/objects";
    if(!error && std::filesystem::is_regular_file(installed/"index.html"))assets=installed;
    std::ifstream f(assets/name,std::ios::binary);if(!f)throw std::runtime_error("Recurso local ausente");
    std::string bytes((std::istreambuf_iterator<char>(f)),{});
    if(name=="index.html"){
      const auto head=bytes.find("<head>");bytes.insert(head+6,"<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' blob:; font-src 'self'; connect-src 'self'; object-src 'none'; frame-src 'none'; base-uri 'none'\">");
    }
    const char *mime=name.ends_with(".js")?"text/javascript":name.ends_with(".css")?"text/css":name.ends_with(".woff2")?"font/woff2":name.ends_with(".png")?"image/png":"text/html";
    auto *stream=g_memory_input_stream_new_from_data(g_memdup2(bytes.data(),bytes.size()),static_cast<gssize>(bytes.size()),g_free);
    webkit_uri_scheme_request_finish(request,stream,static_cast<gint64>(bytes.size()),mime);g_object_unref(stream);
  }catch(const std::exception &e){auto *error=g_error_new_literal(G_IO_ERROR,G_IO_ERROR_NOT_FOUND,e.what());webkit_uri_scheme_request_finish_error(request,error);g_error_free(error);}
}
}
struct ObjectDocumentWindow::Impl {
  Gtk::Box root{Gtk::Orientation::VERTICAL,8};Gtk::Label status;
  Gtk::Widget *widget{};WebKitWebView *view{};WebKitWebContext *context{};
  std::filesystem::path project_path;
  bool closing=false;
  std::shared_ptr<Runtime> runtime=std::make_shared<Runtime>();
  std::shared_ptr<Lifetime> life=std::make_shared<Lifetime>();
  Impl(application::ProjectService &service,ObjectDocumentWindow &window,const std::string &document){
    const auto *project=service.current();if(!project)throw std::runtime_error("Abra um projeto e selecione um documento na Escrita.");
    if(document.empty())throw std::runtime_error("Selecione um documento na Escrita antes de abrir Objetos.");
    project_path=project->path();
    const auto doc=service.writing().document(document);if(!doc)throw std::runtime_error("O documento selecionado não está disponível.");
    if(document.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=std::string::npos)throw std::runtime_error("Identidade de documento inválida.");
    const auto folder=project->path()/"documents"/"composed";private_directory(folder);
    try{runtime->session=std::make_unique<objects::pilot::Session>(folder/document,project->manifest().project_id,*doc);}
    catch(const std::exception &e){if(std::string(e.what())=="native_document_source_changed")throw std::runtime_error("O texto de origem mudou na Escrita desde a criação desta composição. A composição existente foi preservada. A reconciliação dessas versões ainda precisa ser implementada.");throw;}
    window.set_title("INDE · Objetos · "+doc->title);window.set_default_size(1240,820);window.set_size_request(820,560);window.set_child(root);
    status.set_text("Documento: "+doc->title+" · As operações confirmadas são recuperáveis. Use Salvar documento para confirmar a edição.");status.set_wrap(true);status.set_margin(8);root.append(status);status.set_visible(false);
    context=webkit_web_context_new();auto *security=webkit_web_context_get_security_manager(context);webkit_security_manager_register_uri_scheme_as_secure(security,"inde");webkit_security_manager_register_uri_scheme_as_cors_enabled(security,"inde");webkit_web_context_register_uri_scheme(context,"inde",asset,nullptr,nullptr);
    view=WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,"web-context",context,nullptr));
    auto *settings=webkit_web_view_get_settings(view);webkit_settings_set_enable_html5_database(settings,FALSE);webkit_settings_set_enable_html5_local_storage(settings,FALSE);
    auto *manager=webkit_web_view_get_user_content_manager(view);
    g_signal_connect(manager,"script-message-with-reply-received::inde",G_CALLBACK((+[](WebKitUserContentManager *,JSCValue *value,WebKitScriptMessageReply *reply,gpointer data)->gboolean{
      auto &self=*static_cast<Impl*>(data);
      try{
        if(std::string(webkit_web_view_get_uri(self.view)).rfind("inde://ui/",0)!=0)throw std::runtime_error("Origem recusada");
        gchar *raw=jsc_value_to_json(value,0);if(!raw)throw std::runtime_error("Pedido inválido");std::string bytes(raw);g_free(raw);if(bytes.size()>12*1024*1024)throw std::runtime_error("Pedido excedido");
        auto input=persistence::json::parse(bytes);
        if(input.at("op").as_string()=="native_draft"){
          self.life->draft=input.at("value").as_boolean();if(!self.life->draft)self.status.set_visible(false);auto *ok=jsc_value_new_boolean(jsc_value_get_context(value),TRUE);webkit_script_message_reply_return_value(reply,ok);g_object_unref(ok);return TRUE;
        }
        ++self.life->pending;
        auto *work=new Work{self.runtime,self.life,std::move(input),{},webkit_script_message_reply_ref(reply),JSC_CONTEXT(g_object_ref(jsc_value_get_context(value)))};
        auto *task=g_task_new(nullptr,nullptr,+[](GObject *,GAsyncResult *result,gpointer){
          auto *t=G_TASK(result);auto *w=static_cast<Work*>(g_task_get_task_data(t));--w->life->pending;
          if(w->life->alive){auto *output=jsc_value_new_from_json(w->context,w->output.c_str());webkit_script_message_reply_return_value(w->reply,output);g_object_unref(output);}
          else webkit_script_message_reply_return_error_message(w->reply,"Documento fechado");
        },nullptr);
        g_task_set_task_data(task,work,+[](gpointer p){delete static_cast<Work*>(p);});
        g_task_run_in_thread(task,+[](GTask *t,gpointer,gpointer data,GCancellable *){
          auto &w=*static_cast<Work*>(data);try{std::lock_guard lock(w.runtime->mutex);w.output=objects::pilot::encode_json(w.runtime->session->request(w.input));}catch(const std::exception &e){w.output=objects::pilot::encode_json(V::Object{{"accepted",false},{"error",e.what()}});}g_task_return_boolean(t,TRUE);
        });g_object_unref(task);return TRUE;
      }catch(const std::exception &e){webkit_script_message_reply_return_error_message(reply,e.what());return TRUE;}
    })),this);
    if(!webkit_user_content_manager_register_script_message_handler_with_reply(manager,"inde",nullptr))throw std::runtime_error("Falha na comunicação do editor");
    g_signal_connect(view,"decide-policy",G_CALLBACK((+[](WebKitWebView *,WebKitPolicyDecision *decision,WebKitPolicyDecisionType type,gpointer)->gboolean{
      if(type==WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION){auto *a=webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));const std::string uri=webkit_uri_request_get_uri(webkit_navigation_action_get_request(a));if(uri.rfind("inde://ui/",0)!=0){webkit_policy_decision_ignore(decision);return TRUE;}}return FALSE;
    })),nullptr);
    widget=Glib::wrap(GTK_WIDGET(view));widget->set_hexpand(true);widget->set_vexpand(true);root.append(*widget);webkit_web_view_load_uri(view,"inde://ui/");
  }
  ~Impl(){life->alive=false;if(view){g_signal_handlers_disconnect_by_data(webkit_web_view_get_user_content_manager(view),this);root.remove(*widget);}if(context)g_object_unref(context);}
};
ObjectDocumentWindow::ObjectDocumentWindow(application::ProjectService &service,Gtk::Window &parent,const std::string &document):impl_(std::make_unique<Impl>(service,*this,document)){set_transient_for(parent);set_application(parent.get_application());set_modal(true);}
ObjectDocumentWindow::~ObjectDocumentWindow()=default;
void ObjectDocumentWindow::save_for_project(const std::filesystem::path &project){
  if(project!=impl_->project_path)return;
  if(impl_->life->draft||impl_->life->pending||impl_->closing)throw std::runtime_error("Conclua a edição de Objetos antes de salvar o projeto.");
  std::lock_guard lock(impl_->runtime->mutex);auto &session=*impl_->runtime->session;
  const auto state=session.request(V::Object{{"op","view"}}).at("state");
  const auto result=session.request(V::Object{{"op","save"},{"expected",state.at("revision")}});
  if(!result.at("accepted").as_boolean())throw std::runtime_error("Não foi possível confirmar a composição por objetos: "+result.at("error").as_string());
}

bool ObjectDocumentWindow::on_close_request(){
  if(impl_->life->draft||impl_->life->pending){impl_->status.set_text("Aplique ou cancele a edição aberta antes de fechar. Seu trabalho confirmado já está protegido.");impl_->status.set_visible(true);return true;}
  if(impl_->closing)return true;
  impl_->closing=true;
  struct CloseCheck{ObjectDocumentWindow *window;std::shared_ptr<Lifetime> life;};
  auto *data=new CloseCheck{this,impl_->life};
  // The synchronous cached notification may still be in transit. Ask the live
  // editor before closing, so opening a draft and immediately closing is safe.
  webkit_web_view_evaluate_javascript(impl_->view,"typeof hasDrafts === 'function' && (busy || hasDrafts() || state?.persistence_status === 'uncertain')",-1,nullptr,nullptr,nullptr,+[](GObject *object,GAsyncResult *result,gpointer raw){
    std::unique_ptr<CloseCheck> d(static_cast<CloseCheck*>(raw));GError *error=nullptr;
    auto *value=webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object),result,&error);
    const bool draft=!value||jsc_value_to_boolean(value);if(value)g_object_unref(value);if(error)g_error_free(error);
    if(!d->life->alive)return;
    auto &self=*d->window;self.impl_->closing=false;
    if(draft||d->life->draft||d->life->pending){self.impl_->status.set_text("Aplique ou cancele a edição aberta antes de fechar. Seu trabalho confirmado já está protegido.");self.impl_->status.set_visible(true);}
    else self.set_visible(false);
  },data);
  return true;
}
}

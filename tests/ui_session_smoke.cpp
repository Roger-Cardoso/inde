// Opt-in physical IM-context and endurance QA, on disposable fixtures only.
#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/project/document_state.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "ui/widget_probe.hpp"
#include <gdk/x11/gdkx.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

namespace {
using namespace inde;
using namespace inde::test::ui;
using Clock=std::chrono::steady_clock;
std::size_t rss() {
  std::ifstream input("/proc/self/statm"); std::size_t total{},resident{};
  input>>total>>resident; return resident*static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
}
class SessionWindow final : public Gtk::ApplicationWindow {
public:
  SessionWindow(std::filesystem::path fixture,unsigned duration,int &result)
      : writing_(service_),duration_(duration),result_(result) {
    service_.open(fixture);
    auto d=service_.writing().create_document("QA IME e sessão longa");
    id_=d.id; d.content="Base Á🙂 — "; d.formatting={{project::DocumentTextStyle::Bold,0,4}};
    d.anchors={{project::new_uuid(),"Âncora de QA",0,4}};
    base_=service_.writing().update_document(d);
    host_.set_content(writing_); set_child(host_); set_default_size(1280,900);
    writing_.install_actions(*this); writing_.refresh(); writing_.reveal_document(id_);
    editor_=find<Gtk::TextView>(writing_,[](auto &v){return v.get_editable();}); require(editor_,"No editor");
    g_signal_connect(editor_->gobj(),"preedit-changed",G_CALLBACK(+[](GtkTextView *,gchar *text,gpointer data){
      auto *self=static_cast<SessionWindow *>(data); self->preedit_=text && *text;
      ++self->preedit_events_;
    }),this);
    writing_.signal_status_message().connect([this](const Glib::ustring &s){message_=s.raw();});
    started_=Clock::now();
    timer_=Glib::signal_timeout().connect([this]{
      try { return step(); }
      catch(const std::exception &e) {std::cerr<<"FAIL session: "<<e.what()<<std::endl;hide();return false;}
    },50);
  }
  ~SessionWindow() override {timer_.disconnect();}
private:
  void keys(std::initializer_list<std::string> keys) {
    auto surface=get_surface(); require(GDK_IS_X11_SURFACE(surface->gobj()),"Private X11 required");
    std::vector<std::string> command{"xdotool","windowfocus","--sync",
        std::to_string(gdk_x11_surface_get_xid(surface->gobj())),"key","--delay","60"};
    command.insert(command.end(),keys);
    require(!keys_pid_,"Overlapping physical key batches");
    Glib::spawn_async({},command,Glib::SpawnFlags::SEARCH_PATH|Glib::SpawnFlags::DO_NOT_REAP_CHILD,{},&keys_pid_);
  }
  bool label(const std::string &text) {
    if (!host_.has_dialog()) return false;
    return find<Gtk::Label>(host_,[&](auto &l){return l.get_text().raw().find(text)!=std::string::npos;});
  }
  void recovery() {
    auto *button=find<Gtk::Button>(writing_,[](auto &b){return b.get_label().raw().starts_with("Recuperação");});
    require(button,"No recovery control"); g_signal_emit_by_name(button->gobj(),"clicked");
    click(host_,"Retenção e exportação…");
  }
  void select_closed() {
    auto *choice=find<Gtk::CheckButton>(host_,[](auto &c){
      return c.get_label()=="Selecionar para exportação e limpeza confirmada" &&
             c.get_sensitive() && c.get_parent() &&
             find<Gtk::Label>(*c.get_parent(),[](auto &l){return l.get_text().raw().find("QA fechado")!=std::string::npos;});
    });
    require(choice,"Closed retention entry not selectable"); choice->set_active(true);
  }
  bool step() {
    require(Clock::now()-started_<std::chrono::seconds(duration_+180),"Session watchdog exceeded");
    auto buffer=editor_->get_buffer();
    if (!get_mapped()) return true;
    // Wait for the actual key batch, not just its first preedit signal. Under
    // instrumentation Escape could otherwise race still-arriving hex digits.
    if (keys_pid_) {
      int status{}; const auto finished=::waitpid(keys_pid_,&status,WNOHANG);
      if (!finished) return true;
      require(finished==keys_pid_ && WIFEXITED(status) && WEXITSTATUS(status)==0,"Physical key batch failed");
      Glib::spawn_close_pid(keys_pid_);keys_pid_=0;
    }
    if (phase_==0) {
      editor_->grab_focus(); buffer->place_cursor(buffer->end());
      buffer->insert(buffer->end(),"prévio");
      before_=buffer->get_text().raw(); keys({"ctrl+shift+u"}); phase_=1;
    } else if (phase_==1) {
      if (!preedit_) return true;
      require(buffer->get_text().raw()==before_,"Preedit mutated authoritative text");
      bool rejected=false;try{writing_.flush_changes();}catch(const std::exception &){rejected=true;}
      require(rejected,"Save/context barrier ignored active native IME");
      keys({"e","1"}); wait_=Clock::now(); phase_=2;
    } else if (phase_==2) {
      if (Clock::now()-wait_<std::chrono::milliseconds(1400)) return true;
      require(preedit_ && buffer->get_text().raw()==before_,"Preedit leaked or ended unexpectedly");
      require(service_.writing().document(id_)->content==base_.content,"Autosave wrote during preedit");
      keys({"Return"}); phase_=3;
    } else if (phase_==3) {
      if (preedit_ || buffer->get_text().raw()!=before_+"á") return true;
      writing_.flush_changes();
      click(writing_,"⮐"); require(buffer->get_text().raw()==before_,"IME commit swallowed preceding typing");
      click(writing_,"⮑"); require(buffer->get_text().raw()==before_+"á","IME redo lost Unicode");
      before_=buffer->get_text().raw(); editor_->grab_focus(); keys({"ctrl+shift+u","1","f","6","4","2"}); phase_=4;
    } else if (phase_==4) {
      if (!preedit_) return true;
      keys({"Escape"}); phase_=5;
    } else if (phase_==5) {
      if (preedit_) return true;
      require(buffer->get_text().raw()==before_,"Canceled composition inserted provisional text");
      writing_.flush_changes();
      require(preedit_events_>=4,"Native input-context path not exercised");
      std::cout<<"PASS: physical GTK IMContextSimple preedit, save barrier, Unicode commit, isolated undo/redo, cancellation\n"<<std::flush;
      // Public store seeds only disposable QA journals, never author's data.
      auto base=*service_.writing().document(id_);
      for (const bool clean : {true,false}) {
        auto state=base; state.title=clean?"QA fechado":"QA pendente";
        auto baseline=state; if (!clean) state.content+=" rascunho";
        const auto session=project::new_uuid();
        project::RecoveryRecord r{service_.current()->manifest().project_id,session,1,0,{state,0,0},baseline,"pending",{}, {}};
        {persistence::RecoveryLease lease(*service_.current(),session); store_.checkpoint(*service_.current(),r);}
        if (clean) closed_=session; else pending_=session;
      }
      recovery(); phase_=6;
    } else if (phase_==6) {
      if (!label("Retenção e exportação de ponto de recuperaçãos")) return true;
      auto *days=find<Gtk::ComboBoxText>(host_,[](auto &c){return c.get_active_id()=="30";});
      require(days,"Default 30-day retention absent");
      days->set_active_id("0"); click(host_,"Atualizar prévia"); phase_=7;
    } else if (phase_==7) {
      if (!label("Retenção e exportação de ponto de recuperaçãos")) return true;
      select_closed(); click(host_,"Revisar limpeza dos selecionados");
      require(label("Confirmar exportação e limpeza?"),"Destructive action lacks separate confirmation");
      click(host_,"Cancelar"); require(store_.load(*service_.current(),closed_).has_value(),"Cancel removed checkpoint");
      recovery(); phase_=8;
    } else if (phase_==8) {
      if (!label("Retenção e exportação de ponto de recuperaçãos")) return true;
      auto *days=find<Gtk::ComboBoxText>(host_,[](auto &c){return c.get_active_id()=="30";});
      require(days,"Retention control missing"); days->set_active_id("0");
      click(host_,"Atualizar prévia"); phase_=9;
    } else if (phase_==9) {
      if (!label("Retenção e exportação de ponto de recuperaçãos")) return true;
      select_closed(); click(host_,"Revisar limpeza dos selecionados");
      click(host_,"Confirmar exportação e limpeza"); phase_=10;
    } else if (phase_==10) {
      if (store_.load(*service_.current(),closed_)) return true;
      require(store_.load(*service_.current(),pending_)->status=="pending","UI purge consumed pending evidence");
      std::cout<<"PASS: production GTK retention preview, default protection, cancel, explicit export/purge, pending preservation\n"<<std::flush;
      auto d=service_.writing().create_document("QA 100 mil palavras — endurance");
      long_id_=d.id;
      for (int i=0;i<12500;++i) d.content+="Água caminho voz mundo noite história amanhã relato. 🙂\n";
      d.formatting={{project::DocumentTextStyle::Bold,0,4}};
      d=application::TextSemanticService::create(d,project::builtin_annotation_types().front(),0,4,{});
      d.semantics.annotations.front().origin="imported";
      d.semantics.annotations.front().source="QA endurance — fixture sintética";
      semantic_id_=d.semantics.annotations.front().id;
      expected_=d.content; d=service_.writing().update_document(d);
      writing_.reveal_document(long_id_); editor_->grab_focus();
      auto *lens=find<Gtk::ToggleButton>(writing_,[](auto &b){return b.get_label()=="Realçar marcações";});
      require(lens,"No semantic lens in long-session fixture");lens->set_active(true);
      endurance_=wait_=Clock::now(); initial_rss_=rss(); peak_rss_=initial_rss_;
      std::cout<<"ENDURANCE_START duration_seconds="<<duration_<<" text_bytes="<<expected_.size()
               <<" rss_bytes="<<initial_rss_<<std::endl; phase_=11;
    } else if (phase_==11) {
      if (Clock::now()-endurance_>=std::chrono::seconds(duration_)) {
        writing_.flush_changes();
        const auto persisted=*service_.writing().document(long_id_);
        require(persisted.content==expected_,"Final persisted text differs");
        require(persisted.semantics.annotations.size()==1 && persisted.semantics.annotations.front().id==semantic_id_ &&
                !persisted.formatting.empty(),"Long-session save lost authorial layers");
        writing_.reset(); service_.writing().delete_document(long_id_);service_.writing().delete_document(id_);
        service_.close(); result_=0;
        std::cout<<"PASS: endurance seconds="<<std::chrono::duration<double>(Clock::now()-endurance_).count()
                 <<" cycles="<<cycles_<<" reopen="<<reopens_<<" initial_rss_bytes="<<initial_rss_
                 <<" peak_rss_bytes="<<peak_rss_<<" final_rss_bytes="<<rss()<<std::endl;
        hide();return false;
      }
      if (Clock::now()-wait_<std::chrono::seconds(2)) return true;
      prior_=expected_; const auto start=Clock::now();
      buffer->insert(buffer->end()," — á🙂"); expected_+=" — á🙂";
      std::cout<<"SESSION_INSERT elapsed_ms="<<std::chrono::duration<double,std::milli>(Clock::now()-start).count()<<'\n';
      wait_=Clock::now(); phase_=12;
    } else if (phase_==12) {
      if (Clock::now()-wait_<std::chrono::milliseconds(1300)) return true;
      const auto start=Clock::now(); writing_.flush_changes();
      require(service_.writing().document(long_id_)->content==expected_,"Autosave lost latest edit");
      click(writing_,"⮐"); require(buffer->get_text().raw()==prior_,"Long-session undo diverged");
      click(writing_,"⮑"); require(buffer->get_text().raw()==expected_,"Long-session redo diverged");
      writing_.flush_changes(); ++cycles_;
      if (cycles_%20==0) {
        click(writing_,"← Voltar"); writing_.reveal_document(long_id_); ++reopens_;
        require(buffer->get_text().raw()==expected_,"Reopen lost content");
      }
      const auto memory=rss();peak_rss_=std::max(peak_rss_,memory);
      require(memory<=initial_rss_+128*1024*1024,"RSS growth exceeded conservative 128 MiB guard");
      std::cout<<"SESSION_CYCLE n="<<cycles_<<" elapsed_ms="
               <<std::chrono::duration<double,std::milli>(Clock::now()-start).count()
               <<" rss_bytes="<<memory<<std::endl;
      phase_=11;
    }
    return true;
  }
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  persistence::DocumentRecoveryStore store_;
  Gtk::TextView *editor_{};
  project::Document base_;
  std::string id_,long_id_,semantic_id_,closed_,pending_,before_,expected_,prior_,message_;
  Clock::time_point started_,wait_,endurance_;
  sigc::connection timer_;
  unsigned duration_,phase_{},preedit_events_{},cycles_{},reopens_{};
  std::size_t initial_rss_{},peak_rss_{};
  bool preedit_{};
  Glib::Pid keys_pid_{};
  int &result_;
};
}
int main(int argc,char **argv) {
  if (argc!=4 || std::string(argv[3])!="--allow-fixture-writes") {
    std::cerr<<"Usage: inde_ui_session_smoke disposable.inde seconds --allow-fixture-writes\n";return 2;
  }
  if (!std::getenv("INDE_CLIPBOARD_TEST_ISOLATED")) {std::cerr<<"Private display/config required\n";return 2;}
  inde::ui::configure_rendering_backend();
  auto app=Gtk::Application::create("io.github.inde.sessionqa",Gio::Application::Flags::NON_UNIQUE);
  int result=1;
  SessionWindow *window{};
  app->signal_activate().connect([&]{
    inde::ui::install_application_theme();
    window=new SessionWindow(argv[1],static_cast<unsigned>(std::stoul(argv[2])),result);
    app->add_window(*window);window->present();
  });
  app->run(1,argv);delete window;return result;
}

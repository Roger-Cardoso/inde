#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/project/document_state.hpp"
#include "ui/widget_probe.hpp"
#include <chrono>
#include <iostream>

namespace {
using namespace inde;
using namespace inde::test::ui;
using S = application::TextSemanticService;
using K = project::SemanticFieldKind;
class Window final : public Gtk::ApplicationWindow {
public:
  Window(const std::filesystem::path &fixture, int &result): writing_(service_),result_(result) {
    service_.open(fixture);
    auto types=service_.writing().semantic_types();
    auto narration=*project::find_annotation_type(types,"inde.semantic.narration",1);
    auto perspective=*project::find_annotation_type(types,"inde.semantic.perspective",1);
    auto target=service_.writing().create_document("QA alvo entre Documentos");
    target.content="Água 🙂 alvo situado.";
    for (int i=0;i<25;++i) target=S::create(target,perspective,0,6,
        {{"person",{K::Enum,{"third_person"}}},{"focalization",{K::Enum,{"external"}}}});
    target=service_.writing().update_document(target); target_doc_=target.id;
    auto source=service_.writing().create_document("QA origem entre Documentos");
    source.content="Água 🙂 narração autoral."; source.perspective="Texto livre legado 🙂";
    source=S::create(source,narration,0,6,{}); source=service_.writing().update_document(source);
    source_doc_=source.id;source_annotation_=source.semantics.annotations.front().id;
    persistence::SemanticUseQuery q;q.document_id=target_doc_;q.type_key=perspective.key;q.offset=24;q.limit=1;
    target_annotation_=service_.writing().semantic_uses(q).rows.front().annotation_id;
    host_.set_content(writing_);set_child(host_);set_default_size(1280,900);
    writing_.install_actions(*this);writing_.refresh();writing_.reveal_document(source_doc_);
    editor_=find<Gtk::TextView>(writing_,[](auto &v){return v.get_editable();}); require(editor_,"No editor");
    timer_=Glib::signal_timeout().connect([this]{
      try{return step();}catch(const std::exception &e){std::cerr<<"FAIL semantic increment GTK phase="<<phase_<<": "<<e.what()<<std::endl;hide();return false;}
    },70);
  }
  ~Window() override {timer_.disconnect();}
private:
  void choose(const std::string &id) {
    auto *combo=find<Gtk::ComboBoxText>(host_,[&](auto &c){
      for(auto *p=c.get_parent();p;p=p->get_parent()) if(p->has_css_class("modal-body")) {
        // Probing set_active_id on the wrong combo can clear its selection and
        // emit changed. Inspect IDs first; exercise only the intended control.
        if(combo_has_id(c,id))return c.set_active_id(id);
        return false;
      }
      return false;
    });require(combo,"No modal option: "+id);
  }
  void open(const std::string &id) {
    click(writing_,"Marcações do texto");
    auto &explorer=*pages(writing_,"semantic-layers").get_child_by_name("semantic-layers");
    auto *b=find<Gtk::Button>(explorer,[&](auto &b){return b.has_css_class("entity-card") && b.get_tooltip_text().raw().find(id)!=std::string::npos;});
    require(b,"Missing annotation card: "+id);g_signal_emit_by_name(b->gobj(),"clicked");
  }
  project::Document saved(const std::string &id) {writing_.flush_changes();return *service_.writing().document(id);}
  void relation_flow() {
    open(source_annotation_);click(host_,"Relações desta anotação");click(host_,"Nova relação explícita…");choose("project");
  }
  void last_target() {
    // Rendering/rebuild is deferred; a periodic timer is not an idle barrier.
    if(!find<Gtk::Label>(host_,[](auto &l){return l.get_text().raw().starts_with("25 alvo(s)");}))return;
    auto *b=find<Gtk::Button>(host_,[&](auto &b){return b.get_tooltip_text().raw()=="Revisar alvo: "+target_annotation_;});
    if(!b){
      if(!find<Gtk::Button>(host_,[](auto &v){return v.get_label()=="Próximos alvos";})) {
        auto *body=find<Gtk::Box>(host_,[](auto &v){return v.has_css_class("modal-body");});
        if(body) count(*body,[](auto &v){
          if(auto *l=dynamic_cast<Gtk::Label*>(&v))std::cerr<<"MODAL_LABEL "<<l->get_text()<<"\n";
          if(auto *c=dynamic_cast<Gtk::ComboBoxText*>(&v))std::cerr<<"MODAL_COMBO "<<c->get_active_id()<<"\n";
          if(auto *b=dynamic_cast<Gtk::Button*>(&v))std::cerr<<"MODAL_BUTTON "<<b->get_label()<<" tooltip="<<b->get_tooltip_text()<<"\n";
          return false;
        });
        std::cerr<<"EXPECTED_TARGET "<<target_annotation_<<" pages="<<pages_<<"\n";
      }
      click(host_,"Próximos alvos");++pages_;return;
    }
    g_signal_emit_by_name(b->gobj(),"clicked");++phase_;
  }
  bool step() {
    require(std::chrono::steady_clock::now()-started_<std::chrono::seconds(60),"Timeout");
    if(phase_==0) {
      if(!get_mapped())return true;
      require(!find<Gtk::Button>(writing_,[](auto &b){return b.get_label()=="Busca semântica";}),"Redundant search button remains");
      auto b=editor_->get_buffer();b->select_range(b->get_iter_at_offset(0),b->get_iter_at_offset(6));
      click(writing_,"Marcar Seleção");choose("inde.semantic.perspective");click(host_,"Continuar");
      choose("third_person");choose("internal");click(host_,"Salvar anotação");
      auto d=saved(source_doc_);require(d.semantics.annotations.size()==2 && d.perspective=="Texto livre legado 🙂","Perspective overwritten legacy");
      perspective_annotation_=d.semantics.annotations.back().id;++phase_;
    } else if(phase_==1) {
      auto *lens=find<Gtk::ToggleButton>(writing_,[](auto &b){return b.get_label()=="Realçar marcações";});require(lens,"No lens");lens->set_active(true);
      auto b=editor_->get_buffer();b->place_cursor(b->get_iter_at_offset(1));
      bool found=false;for(auto &tag:b->get_iter_at_offset(1).get_tags())
        if(tag->property_name().get_value()=="inde-semantic-inde.semantic.perspective") {
          require(tag->property_background_set().get_value() && !tag->property_foreground_set().get_value(),"Perspective lens changes glyphs");found=true;
        }
      require(found,"Perspective background missing");
      auto *tip=GTK_TOOLTIP(g_object_new(GTK_TYPE_TOOLTIP,nullptr));gboolean shown=false;
      g_signal_emit_by_name(editor_->gobj(),"query-tooltip",0,0,TRUE,tip,&shown);g_object_unref(tip);require(shown,"No perspective tooltip");
      click(writing_,"⮐");require(saved(source_doc_).semantics.annotations.size()==1,"Perspective undo failed");
      click(writing_,"⮑");require(saved(source_doc_).semantics.annotations.size()==2,"Perspective redo failed");
      relation_flow();click(host_,"Cancelar");
      require(saved(source_doc_).semantics.relations.empty(),"Canceled pending picker applied relation");
      relation_flow();++phase_;
    } else if(phase_==2) {if(!project_picker_ready())return true;choose("inde.semantic.perspective");++phase_;}
    else if(phase_==3) {last_target();}
    else if(phase_==4) {
      require(pages_>=2,"Target pagination not exercised");
      click(host_,"Cancelar");require(saved(source_doc_).semantics.relations.empty(),"Cancel applied relation");
      relation_flow();++phase_;
    } else if(phase_==5) {if(!project_picker_ready())return true;choose("inde.semantic.perspective");++phase_;}
    else if(phase_==6) {last_target();}
    else if(phase_==7) {
      // Race: target changed after the explicit preview, before confirmation.
      auto target=*service_.writing().document(target_doc_);target.perspective="Changed during preview";
      service_.writing().update_document(target);click(host_,"Confirmar relação explícita");
      require(saved(source_doc_).semantics.relations.empty(),"Stale target accepted");
      require(find<Gtk::Label>(host_,[](auto &l){return l.get_text().raw().find("Alvo mudou")!=std::string::npos;}),"Stale target unexplained");
      click(host_,"Fechar");relation_flow();++phase_;
    } else if(phase_==8) {if(!project_picker_ready())return true;choose("inde.semantic.perspective");++phase_;}
    else if(phase_==9) {last_target();}
    else if(phase_==10) {
      choose("inde.semantic.callback");
      auto *body=find<Gtk::Box>(host_,[](auto &v){return v.has_css_class("modal-body");});require(body,"No preview body");
      auto *note=find<Gtk::Entry>(*body,[](auto &e){return GTK_IS_ENTRY(e.gobj()) && e.get_placeholder_text()=="Nota autoral da relação (opcional)";});
      require(note,"No relation note");note->set_text("Retomada confirmada pelo autor 🙂");
      click(host_,"Confirmar relação explícita");
      auto d=saved(source_doc_);require(d.semantics.relations.size()==1,"Cross-document relation missing");
      require(d.semantics.relations.front().target_annotation_id==target_annotation_ &&
              d.semantics.relations.front().notes=="Retomada confirmada pelo autor 🙂","Explicit endpoints/note lost");
      click(writing_,"Voltar ao texto");click(writing_,"⮐");require(saved(source_doc_).semantics.relations.empty(),"Relation undo failed");
      click(writing_,"⮑");require(saved(source_doc_).semantics.relations.size()==1,"Relation redo failed");
      open(source_annotation_);click(host_,"Relações desta anotação");click(host_,"Abrir anotação relacionada");
      click(host_,"Relações desta anotação");
      require(find<Gtk::Label>(host_,[](auto &l){return l.get_text().raw().find("Entrada externa")!=std::string::npos;}),"Incoming backlink missing");
      click(host_,"Abrir origem da relação");click(host_,"Relações desta anotação");click(host_,"Desvincular relação");
      require(saved(source_doc_).semantics.relations.empty(),"Explicit unrelate failed");++phase_;
    } else {
      click(writing_,"Voltar ao texto");click(writing_,"⮐");
      require(saved(source_doc_).semantics.relations.size()==1,"Unrelate undo failed");
      click(writing_,"⮑");require(saved(source_doc_).semantics.relations.empty(),"Unrelate redo failed");
      auto before=saved(source_doc_);writing_.reset();writing_.refresh();writing_.reveal_document(source_doc_);
      require(project::same_document_state(before,saved(source_doc_)),"Reopen lost perspective/relations");
      writing_.reset();service_.writing().delete_document(source_doc_);service_.writing().delete_document(target_doc_);
      service_.close();result_=0;
      std::cout<<"PASS: semantic increment GTK perspective creation/lens/tooltip/legacy independence, "
                  "explicit project target pagination, cancel, stale-target guard, cross-document "
                  "confirmation/notes, undo/redo, both-end navigation/backlinks, unrelate/reopen\n"<<std::flush;
      hide();return false;
    }
    return true;
  }
  application::ProjectService service_;ui::WritingWorkspace writing_;ui::ModalOverlayHost host_;
  bool project_picker_ready() {
    return find<Gtk::Label>(host_,[](auto &l){return l.get_text().raw().starts_with("Outros Documentos do Projeto —");})!=nullptr;
  }
  Gtk::TextView *editor_{};sigc::connection timer_;int &result_;
  int phase_{},pages_{};
  std::string source_doc_,source_annotation_,target_doc_,target_annotation_,perspective_annotation_;
  std::chrono::steady_clock::time_point started_{std::chrono::steady_clock::now()};
};
}
int main(int argc,char **argv) {
  if(argc!=3 || std::string(argv[2])!="--allow-fixture-writes" || !std::getenv("INDE_CLIPBOARD_TEST_ISOLATED"))return 2;
  ui::configure_rendering_backend();auto app=Gtk::Application::create("io.github.inde.semantic2qa",Gio::Application::Flags::NON_UNIQUE);
  int result=1;Window *window{};
  app->signal_activate().connect([&]{ui::install_application_theme();window=new Window(argv[1],result);app->add_window(*window);window->present();});
  app->run(1,argv);delete window;return result;
}

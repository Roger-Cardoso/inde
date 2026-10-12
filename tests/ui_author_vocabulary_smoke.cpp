#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/main_window.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/author_vocabulary_dialog.hpp"
#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/catalog_vocabulary.hpp"
#include "inde/project/document_state.hpp"
#include "ui/widget_probe.hpp"
#include <chrono>
#include <iostream>
using namespace inde;
using namespace inde::test::ui;
class VocabularyWindow final : public Gtk::ApplicationWindow {
public:
  VocabularyWindow(const std::filesystem::path &path, int &result)
      : writing_(service_), planning_(service_), catalog_(service_), result_(result) {
    service_.open(path);
    old_types_ = service_.writing().semantic_types();
    baseline_ = service_.writing().create_document("Entidade, âncora e composição — título autoral preservado");
    baseline_.content = "Água 🙂 e texto que não será traduzido.";
    baseline_ = service_.writing().update_document(baseline_);
    add_css_class("inde-window"); set_default_size(1000, 720);
    stack_.set_hhomogeneous(false); stack_.set_vhomogeneous(false);
    stack_.add(writing_, "writing"); stack_.add(planning_, "planning"); stack_.add(catalog_, "catalog");
    host_.set_content(stack_); set_child(host_);
    writing_.install_actions(*this); planning_.install_actions(*this); catalog_.install_actions(*this);
    writing_.refresh(); planning_.refresh(); catalog_.refresh();
    writing_.reveal_document(baseline_.id);
    timer_ = Glib::signal_timeout().connect([this] {
      try { return step(); }
      catch (const std::exception &e) { std::cerr << "FAIL vocabulary phase=" << phase_ << ": " << e.what() << std::endl; hide(); return false; }
    }, 550);
  }
  ~VocabularyWindow() override { timer_.disconnect(); }
private:
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::PlanningWorkspace planning_;
  ui::EditorialWorkspace catalog_;
  Gtk::Stack stack_;
  ui::ModalOverlayHost host_;
  int &result_; int phase_{};
  sigc::connection timer_;
  project::Document baseline_;
  std::vector<project::AnnotationTypeDefinition> old_types_;
  std::chrono::steady_clock::time_point started_{std::chrono::steady_clock::now()};
  Gtk::Box &modal() {
    auto *v = find<Gtk::Box>(host_, [](auto &b) {return b.get_mapped() && b.has_css_class("modal-card");});
    require(v, "Modal missing"); return *v;
  }
  Gtk::Widget &field(const std::string &name, std::size_t index=0) {
    Gtk::Widget *answer{}; std::size_t matched{};
    count(modal(), [&](auto &v) {
      if (auto *l=dynamic_cast<Gtk::Label *>(&v);l && l->get_text().raw()==name && matched++ == index)
        answer=l->get_mnemonic_widget();
      return false;
    });
    // GTK removes the collapsed Expander content from allocated children.
    // Inspect its explicit child without expanding a technical field merely
    // to assert that the generated default exists.
    if (!answer && index==0)
      find<Gtk::Expander>(modal(), [&](auto &e) {
        auto *child=e.get_child();if (!child) return false;
        auto *label=find<Gtk::Label>(*child,[&](auto &l){return l.get_text().raw()==name && l.get_mnemonic_widget();});
        if(label) answer=label->get_mnemonic_widget();
        return answer!=nullptr;
      });
    require(answer, "Field missing: "+name); return *answer;
  }
  void entry(const std::string &name, const std::string &value, std::size_t i=0) {
    auto *v=dynamic_cast<Gtk::Entry *>(&field(name,i));require(v,"Not an entry");v->set_text(value);
  }
  Gtk::Expander &expand(Gtk::Widget &root,const std::string &name) {
    auto *e=find<Gtk::Expander>(root,[&](auto &v){return v.get_label().raw()==name;});require(e,"Expander missing: "+name);e->set_expanded(true);return *e;
  }
  void open_personal() {
    expand(writing_,"Tipos de marcação personalizados");
    click(writing_,"Criar tipo de marcação personalizado");
  }
  void require_bounded() {
    auto &m=modal();graphene_point_t p{},q{};
    require(gtk_widget_compute_point(GTK_WIDGET(m.gobj()),GTK_WIDGET(gobj()),&p,&q),"Unallocated modal");
    require(q.x>=0 && q.y>=0 && q.x+m.get_width()<=get_width()+1 && q.y+m.get_height()<=get_height()+1,"Modal escapes viewport");
  }
  bool step() {
    require(std::chrono::steady_clock::now()-started_<std::chrono::seconds(75),"Timeout");
    if (!get_mapped()) return true;
    switch (phase_) {
    case 0: click(writing_,"Marcações do texto"); break;
    case 1: open_personal(); break;
    case 2: {
      require_bounded();
      auto *e=find<Gtk::Expander>(modal(),[](auto &v){return v.get_label()=="Editor técnico (opcional)";});
      require(e && !e->get_expanded(),"Technical editor not collapsed");
      click(modal(),"Criar tipo de marcação"); break;
    }
    case 3:
      require(service_.writing().semantic_types().size()==old_types_.size(),"Invalid definition applied");
      entry("Nome da marcação","Tensão ♥"); entry("Nome do campo","Intensidade");
      dynamic_cast<Gtk::ComboBoxText &>(field("Conteúdo do campo")).set_active_id("enum");
      entry("Opções","Alta; Alta"); click(modal(),"Criar tipo de marcação"); break;
    case 4:
      require(service_.writing().semantic_types().size()==old_types_.size(),"Duplicate choices applied");
      require(dynamic_cast<Gtk::Entry &>(field("Nome da marcação")).get_text()=="Tensão ♥","Input lost after refusal");
      require(find<Gtk::Label>(modal(),[](auto &l){return l.get_text().raw().find("evite opções repetidas")!=std::string::npos;}),"Technical validation leaked into normal form");
      entry("Opções","Baixa; Média; Alta"); click(modal(),"Adicionar campo"); entry("Nome do campo","Nota",1);
      std::cout<<"SNAPSHOT: vocabulary-personal-form"<<std::endl; break;
    case 5: click(modal(),"Retirar este campo"); break;
    case 6: {
      require(dynamic_cast<Gtk::Entry &>(field("Nome do campo")).get_text()=="Nota","Removed wrong field");
      expand(modal(),"Editor técnico (opcional)");
      click(modal(),"Conferir definição gerada pelo formulário");
      auto *text=find<Gtk::TextView>(modal(),[](auto &v){return v.get_editable();});require(text,"JSON editor missing");
      const auto source=text->get_buffer()->get_text();
      entry("Descrição","Mudança após gerar definição");
      click(modal(),"Conferir definição gerada pelo formulário");
      require(text->get_buffer()->get_text()==source,"Manual draft overwritten");
      auto *manual=find<Gtk::CheckButton>(modal(),[](auto &v){return v.get_label()=="Usar a definição do editor técnico ao criar";});
      require(manual,"Manual mode absent");manual->set_active(true);manual->set_active(false);
      require(text->get_buffer()->get_text()==source,"Mode switch overwrote draft");
      click(modal(),"Cancelar"); break;
    }
    case 7:
      require(service_.writing().semantic_types().size()==old_types_.size(),"Canceled definition applied");open_personal();break;
    case 8:
      entry("Nome da marcação","Ritmo ♥");entry("Nome do campo","Tensão");
      dynamic_cast<Gtk::ComboBoxText &>(field("Conteúdo do campo")).set_active_id("enum");
      entry("Opções"," Baixa ; Média ; Alta ");click(modal(),"Criar tipo de marcação");break;
    case 9: {
      const auto types=service_.writing().semantic_types();require(types.size()==old_types_.size()+1,"Personal type missing");
      auto t=std::find_if(types.begin(),types.end(),[](auto &v){return v.label=="Ritmo ♥";});
      require(t!=types.end() && !t->system && t->fields.front().values==std::vector<std::string>{"Baixa","Média","Alta"},"Incorrect personal definition");
      click(writing_,"Voltar ao texto");stack_.set_visible_child("planning");
      auto *menu=find<Gtk::MenuButton>(planning_,[](auto &v){return v.get_tooltip_text()=="Gerenciar categorias de elemento";});require(menu,"Category menu missing");menu->popup();
      click(*menu->get_popover(),"Nova categoria");break;
    }
    case 10: {
      require_bounded(); auto *e=find<Gtk::Expander>(modal(),[](auto &v){return v.get_label()=="Identificador técnico (opcional)";});
      require(e&&!e->get_expanded(),"Key is not optional/collapsed");
      require(dynamic_cast<Gtk::Entry &>(field("Identificador técnico")).get_text().raw().starts_with("personal-"),"ID not generated");
      entry("Nome da categoria","Entidade pessoal");std::cout<<"SNAPSHOT: vocabulary-category-form"<<std::endl;break;
    }
    case 11: click(modal(),"Criar");break;
    case 12:
      require(!service_.narrative().entity_types().empty(),"Categories missing");
      stack_.set_visible_child("catalog");click(catalog_,"Filtros");break;
    case 13: {
      auto *c=find<Gtk::ComboBoxText>(catalog_,[](auto &v){return combo_has_id(v,"collection");});require(c,"Nonfiction option missing");c->set_active_id("collection");
      require(find<Gtk::Label>(catalog_,[](auto &l){return l.get_text()=="Obras e conjuntos de obras";}),"Catalog label unchanged");
      click(catalog_,"Nova obra");break;
    }
    case 14:
      require(find<Gtk::Label>(modal(),[](auto &l){return l.get_text()=="Conjunto de obras";}),"Work setup inconsistent with preference");
      require_bounded();std::cout<<"SNAPSHOT: vocabulary-nonfiction-setup"<<std::endl;break;
    case 15: click(modal(),"Cancelar");ui::show_author_vocabulary(*this);break;
    case 16: {
      auto *search=find<Gtk::SearchEntry>(modal(),[](auto &){return true;});require(search,"Glossary search absent");search->set_text("âncora");break;
    }
    case 17:
      require(find<Gtk::Label>(modal(),[](auto &l){return l.get_text()=="Trecho marcado";}),"Old-term alias not searchable");
      require_bounded();std::cout<<"SNAPSHOT: vocabulary-glossary"<<std::endl;break;
    case 18: {
      click(modal(),"Fechar");writing_.flush_changes();
      const auto current=service_.writing().document(baseline_.id);require(current&&project::same_document_state(*current,baseline_),"Vocabulary changed authored document");
      const auto types=service_.writing().semantic_types();for (auto &old:old_types_) {
        const auto *t=project::find_annotation_type(types,old.key,old.version);require(t&&*t==old,"Built-in definition changed");
      }
      const auto categories=service_.narrative().entity_types();require(std::any_of(categories.begin(),categories.end(),[](auto &c){return c.name=="Entidade pessoal";}),"Personal name translated");
      stack_.set_visible_child("planning");pages(planning_,"relations").set_visible_child("relations");break;
    }
    case 19: {
      auto *menu=find<Gtk::MenuButton>(planning_,[](auto &v){return v.get_tooltip_text()=="Gerenciar tipos de relação";});require(menu,"Relation menu missing");menu->popup();
      click(*menu->get_popover(),"Novo tipo");break;
    }
    case 20:
      require_bounded();entry("Como se lê de um elemento para o outro","protege");entry("Como se lê no sentido contrário","é protegido por");
      require(dynamic_cast<Gtk::Entry &>(field("Identificador técnico")).get_text().raw().starts_with("personal-"),"Relation ID not generated");
      std::cout<<"SNAPSHOT: vocabulary-relation-form"<<std::endl;break;
    case 21: click(modal(),"Criar");break;
    case 22: {
      const auto types=service_.narrative().relation_types();
      require(std::any_of(types.begin(),types.end(),[](auto &t){return t.name=="protege"&&t.inverse_name=="é protegido por"&&t.directionality==project::RelationDirectionality::Directed;}),"Relation meaning changed");
      std::cout<<"PASS: vocabulary form, optional IDs, validation, cancellation, technical draft, nonfiction, glossary, directed relation and unchanged authored data"<<std::endl;
      result_=0;close();return false;
    }
    }
    ++phase_;return true;
  }
};
int main(int argc,char **argv) {
  if(argc!=3||std::string(argv[2])!="--allow-fixture-writes")return 2;
  ui::configure_rendering_backend();auto app=Gtk::Application::create("io.github.inde.vocabulary-smoke",Gio::Application::Flags::NON_UNIQUE);
  int result=1;VocabularyWindow *window{};
  app->signal_activate().connect([&]{ui::install_application_theme();window=new VocabularyWindow(argv[1],result);app->add_window(*window);window->present();});
  char *args[]={argv[0],nullptr};app->run(1,args);delete window;return result;
}

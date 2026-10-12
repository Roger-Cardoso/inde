// Read/navigate existing authorial documents, only in an isolated snapshot.
#include "../src/ui/rendering_policy.hpp"
#include "inde/ui/application_theme.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/project/document_state.hpp"
#include "ui/widget_probe.hpp"
#include <iostream>

namespace {
using namespace inde;
using namespace inde::test::ui;
class ReviewWindow final : public Gtk::ApplicationWindow {
public:
  ReviewWindow(std::filesystem::path fixture,int &result):writing_(service_),result_(result) {
    service_.open(fixture);docs_=service_.writing().documents();
    host_.set_content(writing_);set_child(host_);set_default_size(1280,900);
    writing_.install_actions(*this);writing_.refresh();
    timer_=Glib::signal_timeout().connect([this] {
      try {return step();}
      catch (const std::exception &e) {std::cerr<<"FAIL review snapshot: "<<e.what()<<std::endl;hide();return false;}
    },80);
  }
  ~ReviewWindow() override {timer_.disconnect();}
private:
  bool step() {
    if (index_==docs_.size()) {
      writing_.reset();service_.close();result_=0;
      std::cout<<"PASS: existing review snapshot documents="<<docs_.size()
               <<" annotations="<<annotations_<<" readable, lenses/layers/project search/reset, exact persisted states unchanged\n"<<std::flush;
      hide();return false;
    }
    const auto &d=docs_[index_];
    if (!phase_) {
      writing_.reveal_document(d.id);
      auto *editor=find<Gtk::TextView>(writing_,[](auto &v){return v.get_editable();});
      require(editor && editor->get_buffer()->get_text().raw()==d.content,"Existing document text differs on load");
      auto *lens=find<Gtk::ToggleButton>(writing_,[](auto &b){return b.get_label()=="Realçar marcações";});
      require(lens,"No lens");lens->set_active(true);
      annotations_+=d.semantics.annotations.size();click(writing_,"Marcações do texto");phase_=1;
    } else if (phase_==1) {
      require(pages(writing_,"semantic-layers").get_visible_child_name()=="semantic-layers","Layers missing");
      require(!find<Gtk::Button>(writing_,[](auto &b){return b.get_label()=="Busca semântica";}),"Redundant search button");
      auto &explorer=*pages(writing_,"semantic-layers").get_child_by_name("semantic-layers");
      auto *scope=find<Gtk::ComboBoxText>(explorer,[](auto &c){return c.get_active_id()=="document" || c.get_active_id()=="project";});
      require(scope,"No integrated scope");scope->set_active_id("project");phase_=2;
    } else {
      require(pages(writing_,"semantic-layers").get_visible_child_name()=="semantic-layers","Search not integrated");
      click(writing_,"Voltar ao texto");writing_.flush_changes();
      const auto persisted=service_.writing().document(d.id);
      require(persisted && project::same_document_state(*persisted,d),"Read/navigation changed persisted authorial state");
      writing_.reset();phase_=0;++index_;writing_.refresh();
    }
    return true;
  }
  application::ProjectService service_;
  ui::WritingWorkspace writing_;
  ui::ModalOverlayHost host_;
  std::vector<project::Document> docs_;
  sigc::connection timer_;
  std::size_t index_{},annotations_{};
  unsigned phase_{};
  int &result_;
};
}
int main(int argc,char **argv) {
  if (argc!=2 || !std::getenv("INDE_CLIPBOARD_TEST_ISOLATED")) {
    std::cerr<<"Usage: isolated snapshot.inde, private display/config required\n";return 2;
  }
  ui::configure_rendering_backend();
  auto app=Gtk::Application::create("io.github.inde.reviewqa",Gio::Application::Flags::NON_UNIQUE);
  int result=1;ReviewWindow *window{};
  app->signal_activate().connect([&] {
    ui::install_application_theme();window=new ReviewWindow(argv[1],result);app->add_window(*window);window->present();
  });
  app->run(1,argv);delete window;return result;
}

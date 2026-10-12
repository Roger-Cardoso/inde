#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/application/document_export.hpp"
#include "inde/project/manifest.hpp"
namespace inde::ui {
void WritingWorkspace::export_document() {
  auto *owner=dynamic_cast<Gtk::Window *>(get_root());
  if(!owner || !service_.current() || !current_document_)return;
  try {
    flush_changes();
    const auto document=service_.writing().document(current_document_->id);
    if(!document)throw std::runtime_error("Documento não encontrado para exportar");
    const auto prepared=std::make_shared<const application::DocumentExport>(
      application::prepare_document_export(service_.current()->manifest().project_id,*document));
    const auto source=std::filesystem::weakly_canonical(service_.current()->path());
    auto *dialog=new OverlayDialog("Exportar Documento — prévia",*owner,true);
    dialog->set_secondary_text("Snapshot independente para leitura: HTML + TXT UTF-8 + relatório de perdas. O Documento e suas camadas continuam no INDE. Não é exportação da Obra completa nem backup.");
    dialog->add_button("Cancelar",Gtk::ResponseType::CANCEL);
    dialog->add_button("Escolher destino e exportar",Gtk::ResponseType::ACCEPT);
    auto *body=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,8);body->set_margin(12);
    auto *title=Gtk::make_managed<Gtk::Label>(prepared->title);title->set_wrap(true);title->add_css_class("heading");body->append(*title);
    auto *scroll=Gtk::make_managed<Gtk::ScrolledWindow>();scroll->set_min_content_height(160);scroll->set_max_content_height(220);scroll->set_propagate_natural_height(true);
    auto *preview=Gtk::make_managed<Gtk::TextView>();preview->set_editable(false);preview->set_cursor_visible(false);preview->set_wrap_mode(Gtk::WrapMode::WORD_CHAR);preview->get_buffer()->set_text(prepared->text);
    scroll->set_child(*preview);body->append(*scroll);
    auto *losses=Gtk::make_managed<Gtk::Label>();losses->set_wrap(true);losses->set_xalign(0);
    std::string text="Limites desta saída:\n";for(const auto &line:prepared->limitations)text+="• "+line+"\n";
    losses->set_text(text);body->append(*losses);dialog->get_content_area()->append(*body);
    dialog->signal_response().connect([this,dialog,owner,prepared,source](int response) {
      dialog->hide();if(response!=Gtk::ResponseType::ACCEPT)return;
      auto chooser=Gtk::FileChooserNative::create("Escolher pasta para a nova exportação",*owner,Gtk::FileChooser::Action::SELECT_FOLDER,"Exportar","Cancelar");
      chooser->signal_response().connect(sigc::track_object([this,chooser,prepared,source](int r) {
        if(r!=Gtk::ResponseType::ACCEPT || !chooser->get_file())return;
        try {
          const auto parent=std::filesystem::canonical(chooser->get_file()->get_path());
          const auto mismatch=std::mismatch(source.begin(),source.end(),parent.begin(),parent.end());
          if(mismatch.first==source.end())throw std::runtime_error("Escolha uma pasta fora do Projeto INDE");
          const auto destination=parent/("INDE-exportacao-"+project::new_uuid());
          application::write_document_export(*prepared,destination);
          signal_status_message_.emit("Exportação conferida: "+destination.string());
          auto *done=new OverlayDialog("Documento exportado",*dynamic_cast<Gtk::Window *>(get_root()),true);
          done->set_secondary_text("HTML, TXT e relatorio.json gravados e conferidos em:\n"+destination.string()+"\nO relatório registra a identidade, os hashes e as perdas deste snapshot.");
          done->add_button("Fechar",Gtk::ResponseType::CLOSE);done->signal_response().connect([done](int){done->hide();});done->present();
        } catch(const std::exception &error){show_error("Não foi possível exportar o Documento",error);}
      },*this));
      chooser->show();
    });
    dialog->present();
  } catch(const std::exception &error){show_error("Não foi possível preparar a exportação",error);}
}
}

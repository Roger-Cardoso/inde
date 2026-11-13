#include "inde/ui/main_window.hpp"
#include "publication_ui.hpp"
namespace inde::ui {
void MainWindow::show_publication(){
  if(!service_.current()||modal_host_.has_dialog())return;
  if(!writing_workspace_.publication_ready()){
    status_bar_.set_text("Aguarde o salvamento do Documento ou use Salvar antes de abrir Publicação. Nenhum salvamento foi solicitado pelo painel.");return;
  }
  try{
    if(!publication_page_)publication_page_=std::make_unique<PublicationPage>(publication_service_);
    if(publication_page_->can_leave())publication_page_->refresh();
    status_bar_.set_text("Publicação web");
    present_publication();
  }catch(const std::exception &error){show_error("Publicação web",error);}
}
void MainWindow::present_publication(){
  if(!publication_page_||modal_host_.has_dialog())return;
  g_object_set_data(G_OBJECT(modal_host_.gobj()),"inde-publication-layout",GINT_TO_POINTER(1));
  publication_token_=modal_host_.present_dialog("Publicação","",*publication_page_,
    {{"Voltar ao INDE",Gtk::ResponseType::CLOSE,false}},[this](int){
      publication_token_=0;
      g_object_set_data(G_OBJECT(modal_host_.gobj()),"inde-publication-layout",nullptr);
      if(publication_page_->can_leave()){status_bar_.set_text("Publicação web fechada");return;}
      // The host removes the emitting footer first. The form is owned by this
      // window and is reattached on idle without refreshing or discarding it.
      publication_retry_=Glib::signal_idle().connect(sigc::track_object([this]{
        publication_retry_.disconnect();present_publication();return false;
      },*this));
    });
  modal_host_.queue_allocate();
}
}

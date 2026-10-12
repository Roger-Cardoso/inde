#include "inde/ui/composition_page.hpp"
#include "inde/ui/keyboard_commands.hpp"
#include "inde/ui/overlay_dialog.hpp"

namespace inde::ui {
void CompositionPage::confirm_draft(bool apply) {
  flush();
  if(!current_ || block_.empty() || !current_->value.drafts.contains(block_))return;
  auto *owner=dynamic_cast<Gtk::Window *>(get_root());if(!owner)return;
  auto expected=*current_;
  const auto block=block_, occurrence=occurrence_;
  std::size_t live{},pinned{};
  for(const auto &use:current_->value.blocks.placements())if(use.block_id==block) {
    if(use.pinned_revision)++pinned;else if(use.mode==experimental::PlacementMode::Live)++live;
  }
  auto *dialog=new OverlayDialog(apply ? "Usar esta versão na montagem" : "Descartar rascunho",*owner);
  dialog->set_secondary_text(apply ? "A nova versão afetará "+std::to_string(live)+" usos vivos deste bloco. "+std::to_string(pinned)+" usos fixados conservam suas versões. Salvar o projeto não equivale a esta aplicação." : "Apenas o rascunho deste bloco será descartado. O bloco, as origens e as versões publicadas na montagem permanecem.");
  dialog->add_button("Manter rascunho",Gtk::ResponseType::CANCEL);
  dialog->add_button(apply ? "Confirmar nova versão" : "Descartar rascunho",Gtk::ResponseType::REJECT);
  dialog->signal_response().connect([this,dialog,expected,block,occurrence,apply](int response) {
    if(response==Gtk::ResponseType::REJECT)attempt([&] {
      if(!current_ || current_->revision!=expected.revision || block_!=block)throw std::runtime_error("A montagem mudou; confirme novamente no contexto atual.");
      mutate([&](auto &c) {if(apply){auto d=c.drafts.at(block);c.blocks.revise(block,d.expected_revision,d.text);}c.drafts.erase(block);});
      show_editor(occurrence);
    });
    dialog->hide(); // OverlayDialog owns deferred disposal after response.
  });
  dialog->present();
}
void CompositionPage::install_keyboard(KeyboardCommands &keys) {
  auto scope=[this](Gtk::Widget *focus){return get_mapped() && focus_inside(focus,*this) && !ime_;};
  auto nontext=[this,scope](Gtk::Widget *focus){return scope(focus) && focus_inside(focus,occurrences_) && !text_input_focus(focus);};
  auto ready=[this]{return current_ ? std::string{} : "Abra uma montagem primeiro.";};
  auto add=[&](std::string id,std::string label,std::string key,std::function<void()> run,bool list=false){
    keys.add({"composition."+id,std::move(label),"Montagens de texto",std::move(key),{},list ? std::function<bool(Gtk::Widget *)>(nontext) : scope,ready,[this,run]{attempt(run);}});
  };
  add("output","Preparar leitura e saída da montagem…","<Control><Shift>e",[this]{prepare();});
  add("apply","Usar versão — revisar impacto e confirmar…","<Control>Return",[this]{confirm_draft(true);});
  add("discard","Descartar rascunho — pedir confirmação…","",[this]{confirm_draft(false);});
  add("read","Ler montagem","",[this]{show_reading();});
  add("import","Importar documento…","Insert",[this]{show_import();},true);
  add("reuse","Reutilizar uso — mesmo bloco compartilhado","<Control>d",[this]{occurrence_command("repeat");},true);
  add("remove","Retirar uso do bloco…","Delete",[this]{occurrence_command("remove");},true);
  add("up","Mover uso para cima na ordem da montagem","<Alt><Shift>Up",[this]{move(-1);},true);
  add("down","Mover uso para baixo na ordem da montagem","<Alt><Shift>Down",[this]{move(1);},true);
  add("reload","Recarregar montagem — revisar rascunhos…","",[this]{reload();});
}
}

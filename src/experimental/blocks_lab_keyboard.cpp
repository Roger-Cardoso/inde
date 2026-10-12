#include "inde/experimental/blocks_lab_window.hpp"

namespace inde::experimental {
void BlocksLabWindow::install_keyboard() {
  top_.append(commands_button_);
  keyboard_=std::make_unique<ui::KeyboardCommands>(*this,commands_button_,[this]{return bool(pending_);},[this](const auto &s){status_.set_text(s);});
  auto any=[](Gtk::Widget *){return true;};
  keyboard_->add({"lab.palette","Buscar comandos do laboratório","Laboratório","<Control><Shift>p",{},any,{},[this]{keyboard_->show();}});
  keyboard_->add({"lab.help","Teclado e ajuda do laboratório","Laboratório","F1",{},any,{},[this]{keyboard_->show(true);}});
  keyboard_->add({"lab.keyboard","Personalizar teclado do laboratório","Laboratório","<Control>F1",{},any,{},[this]{keyboard_->show(true);}});
  commands_button_.signal_clicked().connect([this]{keyboard_->show();});
  auto add=[this](std::string id,Gtk::Button &button,std::string key="",Gtk::Widget *region=nullptr) {
    keyboard_->add({"lab."+id,button.get_label(),"Laboratório de montagens",std::move(key),{},
      [region](auto *f){return !ui::text_input_focus(f) && (!region || ui::focus_inside(f,*region));},
      [&button]{return button.get_mapped() && button.get_sensitive() ? std::string{} : "Disponível na página e seleção correspondentes.";},
      [&button]{g_signal_emit_by_name(button.gobj(),"clicked");}});
  };
  add("rename-block",rename_,"F2",&library_);
  add("open-version",open_,"Return",&library_);
  add("reuse-block",reuse_,"",&library_);
  add("extract",extract_);add("whole-source",whole_);
  add("split-selection",split_);add("split-cursor",cursor_split_);
  add("reuse-use",repeat_,"<Control>d",&occurrences_);
  add("pin",pin_);add("follow",follow_);add("fork",fork_);add("reference",ref_);add("origin",origin_);
  add("move-up",up_,"<Alt><Shift>Up",&occurrences_);
  add("move-down",down_,"<Alt><Shift>Down",&occurrences_);
  add("hide",hide_);add("remove",remove_,"Delete",&occurrences_);
  add("alternative",alternative_);add("rename-alternative",rename_variant_);add("promote",promote_);
  add("assign-layer",assign_layer_);add("new-layer",new_layer_);add("rename-layer",rename_layer_);add("layer-visibility",layer_visibility_);
  add("undo-montage",undo_button_,"<Control>z",&occurrences_);
  add("redo-montage",redo_button_,"<Control><Shift>z",&occurrences_);
  add("new-montage",reset_);add("prepare-document",prepare_,"<Control><Shift>e");
  add("publish",publish_);add("previous-source",previous_,"<Alt>Page_Up");add("next-source",next_,"<Alt>Page_Down");
  add("light",light_theme_);add("dark",dark_theme_);add("system",system_theme_);
  keyboard_->regions([this]{return std::vector<Gtk::Widget *>{&top_,&sidebar_,&editors_scroll_,&montage_scroll_,&preview_tabs_};});
  auto keys=Gtk::EventControllerKey::create();keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  keys->signal_key_pressed().connect([this](guint key,guint,Gdk::ModifierType mods){
    if(pending_ && key==GDK_KEY_Escape && ui::shortcut_modifiers(mods)==0 && !ui::input_composition_active(get_focus())) {
      g_signal_emit_by_name(cancel_.gobj(),"clicked");return true;
    }
    return false;
  },false);add_controller(keys);
}
}

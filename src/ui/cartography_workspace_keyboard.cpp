#include "inde/ui/cartography_workspace.hpp"
#include "inde/ui/keyboard_commands.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "cartography_ui_helpers.hpp"

namespace inde::ui {
void CartographyWorkspace::install_keyboard(KeyboardCommands &keys) {
  if(auto *window=dynamic_cast<Gtk::ApplicationWindow *>(get_root()))window->add_action("map-context",[this]{sidebar_scroll_.set_visible(!sidebar_scroll_.get_visible());});
  auto canvas = [this](Gtk::Widget *focus) { return get_mapped() && focus_inside(focus,canvas_); };
  auto map = [this] { return planet() && !busy_ ? std::string{} : "Abra um mapa e aguarde a geração."; };
  auto add = [&](std::string id,std::string label,std::string chord,std::function<void()> run,bool all=false) {
    keys.add({"map."+id,std::move(label),"Lugares e mapas",std::move(chord),{},all ? std::function<bool(Gtk::Widget *)>([this](auto *f) { return get_mapped() && focus_inside(f,*this); }) : canvas,map,std::move(run)});
  };
  add("fit","Ver planeta inteiro","<Control>0",[this] { camera_={}; queue_scene(); });
  add("zoom-in","Ampliar mapa","<Control>plus",[this] { zoom(1.5); });
  add("zoom-out","Reduzir mapa","<Control>minus",[this] { zoom(1/1.5); });
  add("coordinates","Definir posição por coordenadas…","",[this] { place_local({camera_.longitude,camera_.latitude}); },true);
  add("measure","Medir entre duas coordenadas…","",[this] { coordinate_action(true); },true);
  add("brush","Definir centro do pincel por coordenadas…","",[this] { coordinate_action(false); },true);
  add("undo","Desfazer no modo atual do mapa","<Control>z",[this] {
    auto mode=mode_.get_active_id(); if(mode=="place") history(false); else if(mode=="raise" || mode=="lower" || mode=="smooth") terrain_history(false);
  });
  add("redo","Refazer no modo atual do mapa","<Control><Shift>z",[this] {
    auto mode=mode_.get_active_id(); if(mode=="place") history(true); else if(mode=="raise" || mode=="lower" || mode=="smooth") terrain_history(true);
  });
  add("output","Preparar exportação SVG…","<Control><Shift>e",[this] { export_svg(); },true);
  for(const auto &[mode,label] : std::vector<std::pair<std::string,std::string>>{{"pan","Navegar"},{"measure","Medir"},{"place","Posicionar lugar"},{"raise","Elevar relevo"},{"lower","Rebaixar relevo"},{"smooth","Suavizar relevo"}})
    add("mode-"+mode,"Modo de mapa: "+label,"",[this,mode] { mode_.set_active_id(mode); },true);
}
void CartographyWorkspace::coordinate_action(bool measure) {
  if(!planet() || busy_ || !owner())return;
  auto *dialog=new OverlayDialog(measure ? "Medir entre coordenadas" : "Centro do pincel",*owner());
  dialog->set_secondary_text(measure ? "Distância sobre a esfera; não é rota nem duração de viagem." : "Escolha um modo de relevo. Este comando apenas prepara a prévia; aplicar exige confirmação.");
  auto spin=[&](const std::string &name,double value,double min,double max) {
    auto *label=Gtk::make_managed<Gtk::Label>(name); label->set_xalign(0);dialog->get_content_area()->append(*label);
    auto *field=Gtk::make_managed<Gtk::SpinButton>(); field->set_range(min,max);field->set_increments(.1,1);field->set_digits(6);field->set_value(value);dialog->get_content_area()->append(*field);return field;
  };
  auto *lon=spin("Longitude inicial",camera_.longitude,-180,180),*lat=spin("Latitude inicial",camera_.latitude,-90,90);
  auto *lon2=measure ? spin("Longitude final",camera_.longitude,-180,180) : nullptr;
  auto *lat2=measure ? spin("Latitude final",camera_.latitude,-90,90) : nullptr;
  dialog->add_button("Cancelar",Gtk::ResponseType::CANCEL);
  dialog->add_button(measure ? "Medir" : "Preparar prévia",Gtk::ResponseType::ACCEPT);
  dialog->signal_response().connect([this,dialog,measure,lon,lat,lon2,lat2](int response) {
    if(response==Gtk::ResponseType::ACCEPT && planet()) {
      project::geo::Coordinate first{lon->get_value(),lat->get_value()};
      if(measure) message_.set_text("Distância sobre a esfera: "+cartography_ui_detail::number(project::geo::distance_m(first,{lon2->get_value(),lat2->get_value()},planet()->radius_m)/1000)+" km. Não é rota nem tempo de viagem.");
      else if(mode_.get_active_id()=="raise" || mode_.get_active_id()=="lower" || mode_.get_active_id()=="smooth")preview_terrain(first);
      else message_.set_text("Escolha Elevar, Rebaixar ou Suavizar antes de definir o centro do pincel.");
    }
    dialog->hide(); // OverlayDialog owns deferred disposal after response.
  });
  dialog->present();
}
}

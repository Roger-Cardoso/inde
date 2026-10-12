#include "inde/ui/keyboard_commands.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/narrative_structure_workspace.hpp"
#include "inde/ui/review_workspace.hpp"
#include "inde/ui/editorial_workspace.hpp"
#include "inde/ui/editorial_structure_workspace.hpp"
#include "inde/ui/structural_editor.hpp"
#include "inde/ui/overlay_dialog.hpp"
#include "inde/ui/temporal_context.hpp"
#include "inde/ui/production_page.hpp"

namespace inde::ui {
void PlanningWorkspace::install_keyboard(KeyboardCommands &keys) {
  narrative_structure_workspace_.install_keyboard(keys);
  temporal_library_.install_keyboard(keys);
  auto scope=[this](Gtk::Widget *f){return get_mapped() && focus_inside(f,*this) && !focus_inside(f,narrative_structure_workspace_) && !focus_inside(f,temporal_library_);};
  auto add=[&](std::string id,std::string label,std::string chord,std::function<void()> run,std::string page="",bool nontext=false){
    Gtk::Widget *target=nullptr;
    if(chord=="Delete" || chord=="<Alt>Return") {
      if(page=="explorer")target=&entity_list_;
      else if(page=="relations")target=&relation_card_list_;
      else if(page=="time")target=&point_list_;
    }
    keys.add({"planning."+id,std::move(label),"Personagens, mundo, relações e tempo",std::move(chord),{},
      [this,scope,page,nontext,target](auto *f){return scope(f) && (page.empty() || page_stack_.get_visible_child_name().raw()==page) && (!nontext || !text_input_focus(f)) && (!target || focus_inside(f,*target));},
      [this]{return service_.current() ? std::string{} : "Abra um projeto primeiro.";},[this,run]{try{run();}catch(const std::exception &e){show_error("Comando de planejamento",e);}}});
  };
  add("back","Voltar no planejamento","<Alt>Left",[this]{go_back_in_navigation();});
  add("forward","Avançar no planejamento","<Alt>Right",[this]{go_forward_in_navigation();});
  add("world","Personagens e mundo","",[this]{show_explorer_page();});
  add("relations","Explorar relações","",[this]{show_relations_page();});
  add("story-time","Tempo da história e eixos","",[this]{show_time_page();});
  add("time-systems","Calendários, relógios e referências","",[this]{show_temporal_library();});
  add("plans","Plano narrativo","",[this]{show_narrative_page();});
  add("find-element","Pesquisar elementos","<Control>f",[this]{toggle_navigation();search_.grab_focus();},"explorer");
  add("find-relation","Pesquisar relações","<Control>f",[this]{toggle_relation_filters();relation_search_.grab_focus();},"relations");
  add("create-element","Novo elemento da história…","Insert",[this]{create_entity();},"explorer",true);
  add("edit-element","Editar elemento selecionado…","",[this]{edit_selected_entity();},"explorer",true);
  add("remove-element","Pedir exclusão do elemento…","",[this]{delete_selected_entity();},"explorer",true);
  add("create-category","Nova categoria de elemento…","",[this]{create_entity_type();});
  add("edit-category","Editar categoria…","",[this]{edit_entity_type();});
  add("remove-category","Pedir exclusão da categoria…","",[this]{delete_entity_type();});
  add("aliases","Nomes alternativos do elemento…","",[this]{show_aliases();},"detail");
  add("element-time","Datas do elemento selecionado…","<Control><Shift>t",[this]{if(selected_entity_id_ && owner_window())show_temporal_context(*owner_window(),service_,"entity",*selected_entity_id_);},"detail");
  add("relation-time","Datas da relação selecionada…","<Control><Shift>t",[this]{if(selected_relation_id_ && owner_window())show_temporal_context(*owner_window(),service_,"relation",*selected_relation_id_);},"relations",true);
  add("create-relation","Nova relação…","Insert",[this]{create_relation();},"relations",true);
  add("edit-relation","Editar relação selecionada…","",[this]{edit_relation();},"relations",true);
  add("remove-relation","Pedir exclusão da relação…","",[this]{delete_relation();},"relations",true);
  add("create-relation-type","Novo tipo de relação…","",[this]{create_relation_type();});
  add("edit-relation-type","Editar tipo de relação…","",[this]{edit_relation_type();});
  add("remove-relation-type","Pedir exclusão do tipo de relação…","",[this]{delete_relation_type();});
  add("previous-page","Página anterior de elementos","<Alt>Page_Up",[this]{previous_page();},"explorer",true);
  add("next-page","Próxima página de elementos","<Alt>Page_Down",[this]{next_page();},"explorer",true);
  add("refresh-world","Atualizar resultados de elementos","F5",[this]{refresh_entities();},"explorer",true);
  add("refresh-relations","Atualizar resultados de relações","F5",[this]{refresh_relation_explorer();},"relations",true);
  add("new-axis","Novo eixo temporal…","",[this]{create_time_axis();},"time");
  add("edit-axis","Editar eixo temporal…","",[this]{edit_time_axis();},"time");
  add("remove-axis","Pedir exclusão do eixo temporal…","",[this]{delete_time_axis();},"time");
  add("new-point","Novo marco temporal…","Insert",[this]{create_time_point();},"time",true);
  add("edit-point","Editar marco selecionado…","<Alt>Return",[this]{edit_time_point();},"time",true);
  add("remove-point","Pedir exclusão do marco…","Delete",[this]{delete_time_point();},"time",true);
  add("new-calendar","Novo calendário e relógio…","",[this]{create_calendar_definition();});
  add("calendar","Consultar calendário do eixo…","",[this]{show_calendar_month();},"time");
}
void NarrativeStructureWorkspace::install_keyboard(KeyboardCommands &keys) {
  auto scope=[this](Gtk::Widget *f){return get_mapped() && focus_inside(f,*this);};
  auto add=[&](std::string id,std::string label,std::string chord,std::function<void()> run,std::string page="",bool nontext=false){
    Gtk::Widget *target=nullptr;
    if(page=="library" && (chord=="Delete" || chord=="<Control>d" || chord=="<Control><Shift>c"))target=&structures_list_;
    if(chord=="<Alt><Shift>Up" || chord=="<Alt><Shift>Down")target=&units_list_;
    keys.add({"plan."+id,std::move(label),"Plano narrativo",std::move(chord),{},
      [this,scope,page,nontext,target](auto *f){return scope(f) && (page.empty() || stack_.get_visible_child_name().raw()==page) && (!nontext || !text_input_focus(f)) && (!target || focus_inside(f,*target));},
      [this]{return selected_work_id().empty() ? "Crie ou escolha uma obra. Use “Começar minha Obra” nesta página." : std::string{};},
      [this,run]{try{run();}catch(const std::exception &e){show_error(e);}}});
  };
  add("new","Novo plano vazio…","Insert",[this]{create_blank();},"library",true);
  add("models","Abrir modelos reutilizáveis","",[this]{models_popover_.popup();},"library");
  add("duplicate","Duplicar plano selecionado…","<Control>d",[this]{duplicate_selected(false);},"library",true);
  add("derive","Derivar plano selecionado…","",[this]{duplicate_selected(true);},"library");
  add("compare","Comparar plano com outros…","<Control><Shift>c",[this]{if(selected_structure_id_)start_comparison_selection(*selected_structure_id_);},"library",true);
  add("compare-selected","Comparar alvos escolhidos","<Control>Return",[this]{if(!selection_source_.empty())confirm_comparison_selection();},"library",true);
  add("cancel-comparison","Cancelar seleção de comparação","Escape",[this]{if(!selection_source_.empty())cancel_comparison_selection();},"library",true);
  add("activate","Ativar plano selecionado…","",[this]{activate_selected();},"library");
  add("save-model","Salvar plano como modelo…","",[this]{capture_model();},"library");
  add("remove","Pedir exclusão do plano…","Delete",[this]{delete_selected();},"library",true);
  add("new-line","Nova linha narrativa…","",[this]{add_line();},"detail");
  add("new-unit","Nova parte narrativa…","Insert",[this]{add_unit();},"detail",true);
  add("edit-line","Editar linha selecionada…","",[this]{edit_line();},"detail");
  add("edit-unit","Editar parte selecionada…","<Alt>Return",[this]{edit_unit();},"unit",true);
  add("remove-line","Pedir exclusão da linha…","",[this]{delete_line();},"detail");
  add("remove-unit","Pedir exclusão da parte…","Delete",[this]{delete_unit();},"unit",true);
  add("write","Escolher alvo de escrita para a parte…","<Control>Return",[this]{choose_text_target();},"unit",true);
  add("continuity","Consultar continuidade — apenas avisos","F8",[this]{show_continuity();},"unit");
  add("continuity-plan","Consultar continuidade do plano","F8",[this]{show_continuity();},"detail");
  add("up","Mover parte na ordem editável escolhida","<Alt><Shift>Up",[this]{move_unit(-1);},"detail",true);
  add("down","Mover parte na ordem editável escolhida","<Alt><Shift>Down",[this]{move_unit(1);},"detail",true);
  add("location","Escolher lugar da parte…","",[this]{choose_location();},"unit");
  add("unit-time","Datas da parte selecionada…","<Control><Shift>t",[this]{if(selected_unit_id_ && owner_window())show_temporal_context(*owner_window(),service_,"unit",*selected_unit_id_);},"unit");
  add("line-time","Datas da linha selecionada…","<Control><Shift>t",[this]{if(selected_line_id_ && owner_window())show_temporal_context(*owner_window(),service_,"line",*selected_line_id_);},"detail",true);
  add("relation","Escolher relação da parte…","",[this]{choose_relation();},"unit");
  add("dependency-scope","Escolher a ordem à qual se aplica a regra…","",[this]{choose_dependency_scope();},"detail");
  add("new-link","Novo vínculo narrativo…","",[this]{add_link();},"detail");
  add("remove-link","Desvincular alvo selecionado…","",[this]{delete_link();},"detail");
  add("roles","Papéis na história","",[this]{show_roles();});
  add("new-role","Novo papel na história…","Insert",[this]{create_role();},"roles",true);
  add("assign-role","Definir papel…","",[this]{assign_role();},"roles");
  add("remove-role","Pedir exclusão do papel…","",[this]{delete_role();},"roles");
  add("remove-assignment","Retirar atribuição de papel…","",[this]{delete_assignment();},"roles");
}
void ReviewWorkspace::install_keyboard(KeyboardCommands &keys) {
  auto add=[&](std::string id,std::string label,std::string chord,std::function<void()> run,std::string page=""){
    keys.add({"review."+id,std::move(label),"Revisar",std::move(chord),{},
      [this,page](auto *f){return get_mapped() && focus_inside(f,*this) && (page.empty() || pages_.get_visible_child_name().raw()==page);}, {},
      [this,run]{try{run();}catch(const std::exception &e){fail("Comando de revisão",e);}}});
  };
  add("search","Pesquisar fila de revisão","<Control>f",[this]{search_.grab_focus();},"queue");
  add("refresh","Atualizar revisão","F5",[this]{refresh();});
  add("previous-page","Página anterior de revisão","<Alt>Page_Up",[this]{if(offset_>=50){offset_-=50;load_page();}},"queue");
  add("next-page","Próxima página de revisão","<Alt>Page_Down",[this]{if(offset_+50<total_){offset_+=50;load_page();}},"queue");
  add("compare","Comparar documento…","<Control><Shift>c",[this]{if(auto d=service_.writing().document(selected_document_))comparison_dialog(*d);},"detail");
  add("open-writing","Abrir documento na escrita","<Control>Return",[this]{if(!selected_document_.empty())open_.emit(selected_document_);},"detail");
  add("next-difference","Próxima diferença","F3",[this]{if(!difference_offsets_.empty()){difference_index_=(difference_index_+1)%difference_offsets_.size();auto iter=comparison_text_.get_buffer()->get_iter_at_offset(difference_offsets_[difference_index_]);comparison_text_.scroll_to(iter,.1);}},"comparison");
  add("previous-difference","Diferença anterior","<Shift>F3",[this]{if(!difference_offsets_.empty()){difference_index_=(difference_index_+difference_offsets_.size()-1)%difference_offsets_.size();auto iter=comparison_text_.get_buffer()->get_iter_at_offset(difference_offsets_[difference_index_]);comparison_text_.scroll_to(iter,.1);}},"comparison");
  add("state","Registrar estado editorial…","",[this]{if(auto d=service_.writing().document(selected_document_))status_dialog(*d);},"detail");
  add("revision","Criar revisão independente…","",[this]{if(auto d=service_.writing().document(selected_document_))revision_dialog(*d);},"detail");
}
void EditorialWorkspace::install_keyboard(KeyboardCommands &keys) {
  auto add=[&](std::string id,std::string label,std::string chord,std::function<void()> run,Gtk::Widget *list=nullptr){
    keys.add({"catalog."+id,std::move(label),"Obras e universos",std::move(chord),{},
      [this,list](auto *f){return get_mapped() && focus_inside(f,list ? *list : *this) && !text_input_focus(f);}, {},
      [this,run]{try{run();}catch(const std::exception &e){show_error("Comando do catálogo",e);}}});
  };
  add("find","Pesquisar catálogo","<Control>f",[this]{if(!filters_revealer_.get_reveal_child())toggle_navigation();catalog_search_.grab_focus();});
  add("new-universe","Novo universo…","Insert",[this]{create_intellectual_property();},&ip_list_);
  add("new-work","Nova obra…","Insert",[this]{create_work();},&work_list_);
  add("edit-universe","Editar universo selecionado…","F2",[this]{if(selected_ip_id_)edit_intellectual_property(*selected_ip_id_);},&ip_list_);
  add("edit-work","Editar obra selecionada…","F2",[this]{if(selected_work_id_)edit_work(*selected_work_id_);},&work_list_);
  add("remove-universe","Pedir exclusão do universo…","Delete",[this]{if(selected_ip_id_)delete_intellectual_property(*selected_ip_id_);},&ip_list_);
  add("remove-work","Pedir exclusão da obra…","Delete",[this]{if(selected_work_id_)delete_work(*selected_work_id_);},&work_list_);
  add("refresh","Atualizar catálogo","F5",[this]{refresh_catalog();});
}
void EditorialStructureWorkspace::install_keyboard(KeyboardCommands &keys) {
  structural_editor_.install_keyboard(keys);
  production_.install_keyboard(keys);
  auto add=[&](std::string id,std::string label,std::string chord,std::function<void()> run,std::string page=""){
    auto *target=(page=="library" && (chord=="Delete" || chord=="<Control>d")) ? &structures_list_ : nullptr;
    keys.add({"manuscript."+id,std::move(label),"Manuscrito",std::move(chord),{},
      [this,page,target](auto *f){return get_mapped() && focus_inside(f,*this) && !text_input_focus(f) && (page.empty() || pages_.get_visible_child_name().raw()==page) && (!target || focus_inside(f,*target));}, {},
      [this,run]{try{run();}catch(const std::exception &e){show_error(e);}}});
  };
  add("produce","Preparar produção da obra…","<Control><Shift>e",[this]{show_production();});
  add("new","Nova estrutura de manuscrito…","Insert",[this]{create_blank_structure();},"library");
  add("models","Aplicar modelo de manuscrito…","",[this]{instantiate_model();},"library");
  add("duplicate","Duplicar estrutura de manuscrito…","<Control>d",[this]{duplicate_structure(false);},"library");
  add("derive","Derivar estrutura de manuscrito…","",[this]{duplicate_structure(true);},"library");
  add("activate","Ativar estrutura de manuscrito…","",[this]{activate_structure();},"library");
  add("save-model","Salvar estrutura como modelo…","",[this]{capture_model();},"library");
  add("remove","Pedir exclusão da estrutura de manuscrito…","Delete",[this]{delete_structure();},"library");
}
void StructuralEditor::install_keyboard(KeyboardCommands &keys) {
  auto add=[&](std::string id,std::string label,std::string chord,std::function<void()> run){
    keys.add({"editorial-tree."+id,std::move(label),"Árvore do manuscrito",std::move(chord),{},
      [this](auto *f){return get_mapped() && focus_inside(f,structure_list_) && !text_input_focus(f);}, {},
      [this,run]{try{run();}catch(const std::exception &e){show_error("Comando de manuscrito",e);}}});
  };
  add("new-root","Novo item no nível principal…","Insert",[this]{create_structural_node({});});
  add("new-child","Novo item dentro do selecionado…","<Shift>Insert",[this]{if(selected_structural_node_id_)create_structural_node(selected_structural_node_id_);});
  add("edit","Editar item selecionado…","F2",[this]{if(selected_structural_node_id_)edit_structural_node(*selected_structural_node_id_);});
  add("properties","Propriedades do item selecionado…","<Alt>Return",[this]{if(selected_structural_node_id_)edit_structural_node(*selected_structural_node_id_);});
  add("duplicate","Duplicar ramo do manuscrito…","<Control>d",[this]{duplicate_selected_branch();});
  add("remove","Pedir exclusão do item do manuscrito…","Delete",[this]{if(selected_structural_node_id_)delete_structural_node(*selected_structural_node_id_);});
  add("find","Pesquisar estrutura do manuscrito","<Control>f",[this]{set_overlay_revealer_open(filters_revealer_,true);structure_search_.grab_focus();});
}
void TemporalLibraryPage::install_keyboard(KeyboardCommands &keys) {
  auto scope=[this](Gtk::Widget *f){
    for(auto *w=f;w;w=w->get_parent())if(g_object_get_data(G_OBJECT(w->gobj()),"inde-crud-draft"))return false;
    return get_mapped() && focus_inside(f,*this);
  };
  keys.add({"time.new-system","Novo calendário e relógio…","Tempo","",{},scope,{},[this]{edit_calendar();}});
  keys.add({"time.usage","Consultar onde calendários e relógios são usados","Tempo","",{},scope,{},[this]{show_usage();}});
  keys.add({"time.links","Vínculos de datas com a história…","Tempo","",{},scope,{},[this]{date_links();}});
  keys.add({"time.save-draft","Salvar calendário e relógio em edição","Tempo","<Control>Return",{},scope,
    [this]{return save_.get_mapped() && save_.get_sensitive() ? std::string{} : "Abra um calendário ou relógio em edição primeiro.";},
    [this]{g_signal_emit_by_name(save_.gobj(),"clicked");}});
}
void ProductionPage::install_keyboard(KeyboardCommands &keys) {
  auto scope=[this](Gtk::Widget *f){return get_mapped() && focus_inside(f,*this);};
  auto unavailable=[this]{return prepare_.get_mapped() && prepare_.get_sensitive() ? std::string{} : "Escolha a obra e os textos na seleção de produção antes de preparar.";};
  keys.add({"production.prepare","Preparar prévia da obra, sem exportar","Produção","<Control>Return",{},scope,unavailable,[this]{prepare();}});
  keys.add({"production.output","Preparar saída da obra","Produção","<Control><Shift>e",{},scope,unavailable,[this]{prepare();}});
  keys.add({"production.export","Escolher destino e exportar — após conferir avisos…","Produção","",{},scope,
    [this]{return export_.get_mapped() && export_.get_sensitive() ? std::string{} : "Prepare a prévia e confira as perdas antes de exportar.";},[this]{export_snapshot();}});
}
}

#include "inde/ui/author_diagnostics.hpp"
#include "inde/ui/writing_workspace.hpp"
#include "inde/ui/planning_workspace.hpp"
#include "inde/ui/editorial_structure_workspace.hpp"
#include "inde/ui/ui_preferences.hpp"
#include <glib.h>
#include <cmath>
namespace inde::ui {
namespace {
ViewState ids(const std::vector<std::string> &values) {
  ViewState::Array result;for(const auto &v:values)result.emplace_back(v);return result;
}
std::vector<std::string> ids(const ViewState &v) {
  if(v.as_array().size()>2048)throw std::runtime_error("Muitos filtros no contexto");
  std::vector<std::string> result;
  for(const auto &s:v.as_array()) {auto id=view_id(s);if(!id)throw std::runtime_error("Filtro sem identidade");result.push_back(*id);}
  return result;
}
void selection(IncrementalSelector *selector,const std::optional<std::string> &id) {
  if(!selector)return;
  if(id)selector->set_selected(*id,"Referência salva");else selector->clear_selection();
}
constexpr const char *pending_scroll_key="inde-restored-view-scroll";
double captured_scroll(const Gtk::ScrolledWindow &scroll) {
  const auto *pending=static_cast<const double*>(g_object_get_data(G_OBJECT(const_cast<Gtk::ScrolledWindow &>(scroll).gobj()),pending_scroll_key));
  return pending ? *pending : scroll.get_vadjustment()->get_value();
}
void apply_scroll(Gtk::Widget &owner,Gtk::ScrolledWindow &scroll) {
  Glib::signal_timeout().connect(sigc::track_object([&scroll,attempts=20]() mutable {
    const auto *pending=static_cast<const double*>(g_object_get_data(G_OBJECT(scroll.gobj()),pending_scroll_key));
    if(!pending)return false;
    const auto value=*pending;
    const auto a=scroll.get_vadjustment();
    if((!scroll.get_mapped() || a->get_upper()-a->get_page_size()<value) && --attempts>0)return true;
    a->set_value(std::clamp(value,a->get_lower(),std::max(a->get_lower(),a->get_upper()-a->get_page_size())));
    g_object_set_data(G_OBJECT(scroll.gobj()),pending_scroll_key,nullptr);
    return false;
  },owner),50);
}
void later_scroll(Gtk::Widget &owner,Gtk::ScrolledWindow &scroll,double value) {
  g_object_set_data_full(G_OBJECT(scroll.gobj()),pending_scroll_key,new double(value),[](gpointer p){delete static_cast<double*>(p);});
  if(scroll.get_mapped())apply_scroll(owner,scroll);
  else {
    auto connection=std::make_shared<sigc::connection>();
    *connection=scroll.signal_map().connect(sigc::track_object([&owner,&scroll,connection] {
      connection->disconnect();apply_scroll(owner,scroll);
    },owner));
  }
}
}
ViewState WritingWorkspace::capture_view() {
  const auto buffer=editor_.get_buffer();
  return ViewState::Object{{"document",view_id(current_document_ ? std::optional(current_document_->id):std::nullopt)},
    {"cursor",static_cast<std::int64_t>(buffer->get_iter_at_mark(buffer->get_insert()).get_offset())},
    {"bound",static_cast<std::int64_t>(buffer->get_iter_at_mark(buffer->get_selection_bound()).get_offset())},
    {"scroll",scroll_state(current_document_ ? editor_scroll_.get_vadjustment()->get_value():captured_scroll(library_scroll_))},
    {"search",search_.get_text().raw()},{"group",view_id(current_group_id_)},
    {"group_filter",group_filter_.get_active_id().raw()},{"placement",placement_filter_.get_active_id().raw()},
    {"purpose",purpose_filter_.get_active_id().raw()},{"perspective",perspective_filter_.get_text().raw()},
    {"revisions",revisions_filter_.get_active()},{"entity",view_id(entity_filter_ ? entity_filter_->selected_id():std::nullopt)},
    {"writing_page",page_stack_.get_visible_child_name().raw()},{"composition",composition_page_.capture_view()}};
}
void WritingWorkspace::restore_view(const ViewState &v) {
  const auto doc=view_id(v.at("document"));const auto cursor=view_offset(v.at("cursor")),bound=view_offset(v.at("bound"));
  const auto scroll=view_scroll(v.at("scroll"));const auto group=view_id(v.at("group")),entity=view_id(v.at("entity"));
  const auto search=v.at("search").as_string(),placement=v.at("placement").as_string(),purpose=v.at("purpose").as_string();
  const auto group_filter=v.at("group_filter").as_string(),perspective=v.at("perspective").as_string();
  const auto revisions=v.at("revisions").as_boolean();
  position_restore_.disconnect();
  refreshing_library_controls_=true;
  current_group_id_=group;
  search_.set_text(search);
  if(!group_filter_.set_active_id(group_filter))group_filter_.set_active_id("all");
  if(!placement_filter_.set_active_id(placement))placement_filter_.set_active_id("all");
  if(!purpose_filter_.set_active_id(purpose))purpose_filter_.set_active_id("all");
  perspective_filter_.set_text(perspective);revisions_filter_.set_active(revisions);
  selection(entity_filter_,entity);
  refreshing_library_controls_=false;
  if(v.as_object().contains("writing_page") && v.at("writing_page").as_string()=="compositions") {
    flush_changes();
    try {
      composition_page_.restore_view(v.at("composition"));
      page_stack_.set_visible_child("compositions");
    } catch(const std::exception &e) {
      show_library();
      signal_status_message_.emit(std::string("Contexto da montagem de texto indisponível: ")+author::diagnostic(e.what()));
    }
    return;
  }
  if(doc && service_.writing().document(*doc)) {
    if(current_document_id()!=*doc)open_document(*doc);
    if(current_document_id()!=*doc)throw std::runtime_error("Não foi possível restaurar o Documento");
    position_restore_.disconnect();
    const auto buffer=editor_.get_buffer();const auto count=static_cast<std::size_t>(buffer->get_char_count());
    buffer->select_range(buffer->get_iter_at_offset(static_cast<int>(std::min(cursor,count))),
                         buffer->get_iter_at_offset(static_cast<int>(std::min(bound,count))));
    edit_session_.update_selection({std::min(cursor,count),std::min(bound,count)});
    position_restore_=Glib::signal_timeout().connect(sigc::track_object([this,scroll,cursor,bound,id=*doc,attempts=60,selection_restored=false,stable=0,upper=-1.0,page=-1.0]() mutable {
      if(current_document_id()==id) {
        if(ime_active_ || --attempts<=0)return false;
        const auto a=editor_scroll_.get_vadjustment();
        // GtkTextView lays out a freshly reopened buffer incrementally. Do not
        // clamp a valid saved scroll to zero before its first allocation.
        if(scroll>0 && (!editor_.get_mapped() || a->get_upper()-a->get_page_size()<scroll) && attempts>40)
          return true;
        if(!editor_.get_mapped())return false;
        if(!editor_.has_focus()) {editor_.grab_focus();return true;}
        // The initial focus/primary-selection handoff can collapse a selection
        // placed before the window is mapped. Restore it after that handoff.
        if(!selection_restored) {
          const auto buffer=editor_.get_buffer();const auto count=static_cast<std::size_t>(buffer->get_char_count());
          restoring_undo_=true;
          buffer->select_range(buffer->get_iter_at_offset(static_cast<int>(std::min(cursor,count))),
                               buffer->get_iter_at_offset(static_cast<int>(std::min(bound,count))));
          restoring_undo_=false;
          edit_session_.update_selection({std::min(cursor,count),std::min(bound,count)});
          selection_restored=true;
          // Restoring selection may itself occupy a full frame on a long
          // Document. Apply the available reading position now, not only in a
          // later timer that may lose to the next UI action. GTK can still
          // queue scroll-to-insert; the settling loop below reapplies the
          // reading position after those later focus/layout requests.
          a->set_value(std::clamp(scroll,a->get_lower(),std::max(a->get_lower(),a->get_upper()-a->get_page_size())));
          return true;
        }
        const auto target=std::clamp(scroll,a->get_lower(),std::max(a->get_lower(),a->get_upper()-a->get_page_size()));
        // Reopen can queue more than one focus/layout scroll. A pair of timer
        // writes is not evidence that the reading position has settled: require
        // the actual adjustment and its geometry to remain stable across ticks.
        if(std::abs(a->get_value()-target)<0.5 && upper==a->get_upper() && page==a->get_page_size())++stable;
        else stable=0;
        upper=a->get_upper();page=a->get_page_size();a->set_value(target);
        return stable<8;
      }
      return false;
    // Layout of a long buffer can make several default-priority UI sources
    // ready at once. Settle the available position before those sources read
    // or save it; the callback remains bounded and yields between 50 ms ticks.
    },*this),50,Glib::PRIORITY_HIGH);
  } else {
    show_library();later_scroll(*this,library_scroll_,scroll);
    if(doc)signal_status_message_.emit("Documento do contexto não existe mais; Biblioteca aberta");
  }
}
void WritingWorkspace::remember_document_position() {
  position_save_.disconnect();
  if(!current_document_ || !service_.current() || loading_editor_ || page_stack_.get_visible_child_name()!="editor")return;
  const auto key="position-"+current_document_->id;
  set_ui_preference(project_preference_group(service_.current()->manifest().project_id),key,serialize_view(capture_view()));
}
void WritingWorkspace::reveal_usage_offset(std::size_t offset) {
  position_restore_.disconnect();
  if (!current_document_) throw std::runtime_error("Documento de origem indisponível");
  const auto buffer = editor_.get_buffer();
  const auto point = static_cast<int>(std::min(offset, static_cast<std::size_t>(buffer->get_char_count())));
  buffer->place_cursor(buffer->get_iter_at_offset(point));
  edit_session_.update_selection({static_cast<std::size_t>(point), static_cast<std::size_t>(point)});
  editor_.scroll_to(buffer->get_insert());
  editor_.grab_focus();
}
void WritingWorkspace::open_removal_use_tools(const project::RemovalUse &use) {
  if (!use.annotation_id.empty()) show_semantic_annotation(use.annotation_id);
  else if (use.table == "document_text_references") show_text_references();
  else if (use.table == "documents") edit_document_organization();
  else if (use.table == "document_entity_references") {
    tools_revealer_.set_reveal_child(true);
    select_entity_reference(use.record_id);
  }
}
void WritingWorkspace::restore_document_position() {
  if(!current_document_ || !service_.current())return;
  const auto data=ui_preference(project_preference_group(service_.current()->manifest().project_id),"position-"+current_document_->id);
  if(data.empty())return;
  try {
    const auto v=parse_view_state(data);
    if(view_id(v.at("document"))==current_document_->id) {
      auto position=capture_view().as_object();
      for(const auto *key:{"cursor","bound","scroll"})position[key]=v.at(key);
      position["writing_page"]="editor";
      restore_view(position);
    }
  }
  catch(const std::exception &) { signal_status_message_.emit("Posição salva indisponível; Documento preservado"); }
}
void WritingWorkspace::schedule_position_save() {
  if(loading_editor_ || restoring_undo_ || !current_document_)return;
  position_save_.disconnect();
  position_save_=Glib::signal_timeout().connect(sigc::track_object([this] {
    // Preferences never contain text, formatting or semantic authority.
    // Unmapping/unrealizing a window can collapse the primary selection; the close
    // barrier already saved the real position before this teardown.
    if(editor_.get_mapped())remember_document_position();
    return false;
  },*this),1000);
}
ViewState PlanningWorkspace::capture_view() const {
  const auto &c=service_.planning_context();
  return ViewState::Object{{"page",page_stack_.get_visible_child_name().raw()},
    {"builder",narrative_structure_workspace_.capture_view()},
    {"entity",view_id(selected_entity_id_)},{"point",view_id(selected_time_point_id_)},
    {"search",c.search},{"work",view_id(c.work_id)},{"types",ids(c.entity_type_ids)},{"relation_types",ids(c.relation_type_ids)},
    {"related",view_id(c.related_entity_id)},{"axis",view_id(c.fictional_axis_id)},{"time",view_id(c.fictional_time_point_id)},
    {"start",view_id(c.fictional_window_start_time_point_id)},{"end",view_id(c.fictional_window_end_time_point_id)},
    {"node",view_id(c.editorial_node_id)},{"document",view_id(c.document_id)},{"referenced",c.require_document_reference},
    {"offset",static_cast<std::int64_t>(c.offset)},{"scroll",scroll_state(captured_scroll(entity_scroll_))},
    {"relation_search",relation_search_.get_text().raw()},{"relation_type",relation_type_filter_.get_active_id().raw()},
    {"relation_entity",view_id(relation_entity_filter_->selected_id())},{"relation_time",view_id(relation_time_point_filter_->selected_id())},
    {"relation_location",view_id(relation_location_filter_->selected_id())},{"relation_cause",view_id(relation_cause_filter_->selected_id())},
    {"relation_scroll",scroll_state(captured_scroll(relation_scroll_))}};
}
void PlanningWorkspace::restore_view(const ViewState &v) {
  NavigationState state;auto &c=state.context;
  state.page_name=v.at("page").as_string();
  if(state.page_name!="explorer" && state.page_name!="detail" && state.page_name!="time" &&
      state.page_name!="relations" && state.page_name!="narrative" &&
      state.page_name!="temporal-library")throw std::runtime_error("Área de Planejar inválida");
  c.search=v.at("search").as_string();c.work_id=view_id(v.at("work"));
  c.entity_type_ids=ids(v.at("types"));c.relation_type_ids=ids(v.at("relation_types"));
  c.related_entity_id=view_id(v.at("related"));c.fictional_axis_id=view_id(v.at("axis"));
  c.fictional_time_point_id=view_id(v.at("time"));c.fictional_window_start_time_point_id=view_id(v.at("start"));
  c.fictional_window_end_time_point_id=view_id(v.at("end"));c.editorial_node_id=view_id(v.at("node"));
  c.document_id=view_id(v.at("document"));c.require_document_reference=v.at("referenced").as_boolean();
  c.offset=view_offset(v.at("offset"));c.limit=page_size_;
  state.entity_id=view_id(v.at("entity"));state.time_point_id=view_id(v.at("point"));
  const auto scroll=view_scroll(v.at("scroll"));
  const auto relation_search=v.at("relation_search").as_string(),relation_type=v.at("relation_type").as_string();
  const auto relation_entity=view_id(v.at("relation_entity")),relation_time=view_id(v.at("relation_time")),
    relation_location=view_id(v.at("relation_location")),relation_cause=view_id(v.at("relation_cause"));
  const auto relation_scroll=view_scroll(v.at("relation_scroll"));
  if(state.entity_id && !service_.narrative().entity(*state.entity_id)) {state.entity_id.reset();state.page_name="explorer";}
  if(state.time_point_id && !service_.planning().time_point(*state.time_point_id))state.time_point_id.reset();
  try { restore_navigation_state(std::move(state)); }
  catch(...) { restoring_navigation_=refreshing_context_=refreshing_facets_=false;throw; }
  relation_search_.set_text(relation_search);
  if(!relation_type_filter_.set_active_id(relation_type))relation_type_filter_.set_active_id("all");
  selection(relation_entity_filter_,relation_entity);selection(relation_time_point_filter_,relation_time);
  selection(relation_location_filter_,relation_location);selection(relation_cause_filter_,relation_cause);
  if(page_stack_.get_visible_child_name()=="relations")refresh_relation_explorer();
  later_scroll(*this,relation_scroll_,relation_scroll);
  later_scroll(*this,entity_scroll_,scroll);
  const auto builder=v.as_object().find("builder");
  if(builder!=v.as_object().end() && page_stack_.get_visible_child_name()=="narrative")narrative_structure_workspace_.restore_view(builder->second);
}
ViewState StructuralEditor::capture_view() {
  capture_structure_view_state();
  ViewState::Array expanded;
  if(selected_work_id_)for(const auto &id:structure_view_states_[*selected_work_id_].expanded_node_ids)expanded.emplace_back(id);
  return ViewState::Object{{"node",view_id(selected_structural_node_id_)},{"detail",page_stack_.get_visible_child_name()=="detail"},
    {"search",structure_search_.get_text().raw()},{"type",structural_type_filter_.get_active_id().raw()},
    {"status",status_filter_.get_active_id().raw()},{"entities",ids(selected_entity_type_ids_)},
    {"expanded",std::move(expanded)},{"scroll",scroll_state(captured_scroll(structure_scroll_))}};
}
void StructuralEditor::restore_view(const ViewState &v) {
  const auto node=view_id(v.at("node"));const auto detail=v.at("detail").as_boolean();
  const auto search=v.at("search").as_string(),type=v.at("type").as_string(),status=v.at("status").as_string();
  const auto entities=ids(v.at("entities")),expanded=ids(v.at("expanded"));const auto scroll=view_scroll(v.at("scroll"));
  refreshing_filters_=true;structure_filter_=search;structure_search_.set_text(search);
  if(!structural_type_filter_.set_active_id(type))structural_type_filter_.set_active_id("all");
  if(!status_filter_.set_active_id(status))status_filter_.set_active_id("all");
  selected_entity_type_ids_=entities;refreshing_filters_=false;
  if(selected_work_id_) { auto &s=structure_view_states_[*selected_work_id_];s.expanded_node_ids={expanded.begin(),expanded.end()};s.scroll_value=scroll;s.initialized=true; }
  rendered_work_id_.reset(); // Do not recapture the old tree over the requested state.
  refresh();
  if(node && detail)reveal_node(*node);else {selected_structural_node_id_=node;show_structure_page();}
  later_scroll(*this,structure_scroll_,scroll);
}
ViewState EditorialStructureWorkspace::capture_view() {
  return ViewState::Object{{"work",view_id(selected_work_id_)},{"page",pages_.get_visible_child_name().raw()},
    {"tab",library_views_.get_visible_child_name().raw()},{"structure",view_id(selected_structure_id_)},
    {"model",view_id(selected_model_id_)},{"editor",structural_editor_.capture_view()},{"production",production_.capture_view()}};
}
void EditorialStructureWorkspace::restore_view(const ViewState &v) {
  const auto work=view_id(v.at("work"));const auto page=v.at("page").as_string(),tab=v.at("tab").as_string();
  const auto structure=view_id(v.at("structure")),model=view_id(v.at("model"));
  if((page!="editor" && page!="library" && page!="production") || (tab!="models" && tab!="alternatives"))throw std::runtime_error("Contexto do Manuscrito inválido");
  selected_work_id_=work;refresh();
  show_editor();structural_editor_.restore_view(v.at("editor"));
  if(page=="library") { selected_structure_id_=structure;selected_model_id_=model;show_library();library_views_.set_visible_child(tab); }
  const auto production=v.as_object().find("production");
  if(production!=v.as_object().end() && view_id(production->second.at("work")))production_.restore_view(production->second);
  if(page=="production")show_production();
}
}

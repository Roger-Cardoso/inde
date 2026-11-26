#include "inde/application/project_service.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/project/document_state.hpp"
#include "object_composite_pilot.hpp"
#include <glib.h>
#include <algorithm>
#include <iostream>
using namespace inde;
using V=persistence::json::Value;
using Session=objects::pilot::Session;
void require(bool yes,const char *m) {if(!yes)throw std::runtime_error(m);}
V view(Session &s){return s.request(V::Object{{"op","view"}}).at("state");}
V op(Session &s,const std::string &name,V::Object args={}){
 args["op"]=name;args["expected"]=view(s).at("revision");auto r=s.request(args);
 if(!r.at("accepted").as_boolean())throw std::runtime_error(name+": "+r.at("error").as_string());
 return r.at("state");
}
std::vector<std::string> order(const V &s,const char *key,const std::string &omit={}){
 std::vector<std::string> r;for(const auto &x:s.at(key).as_array())if(x.as_string()!=omit)r.push_back(x.as_string());return r;
}
int main(){
 const auto root=std::filesystem::temp_directory_path()/("inde-insert-"+project::new_uuid());
 std::filesystem::create_directories(root);g_setenv("XDG_CONFIG_HOME",(root/"config").c_str(),true);
 try{
 application::ProjectService service;service.create(root/"Teste.inde","Inserção");
 auto d=service.writing().create_document("Documento");d.content="Um.\nDois.\nTrês.";d.paragraphs=project::identify_paragraphs(d.content);d=service.writing().update_document(d);
 const auto bytes=persistence::encode_document_snapshot(d);
 const auto folder=root/"Teste.inde/documents/composed"/d.id;
 std::filesystem::create_directories(folder.parent_path());
 V saved;std::string t1,t2,p0,p2;
 {Session s(folder,service.current()->manifest().project_id,d,objects::pilot::DocumentBinding::Live);
 auto state=view(s);p0=state.at("paragraphs").as_array()[0].at("id").as_string();p2=state.at("paragraphs").as_array()[2].at("id").as_string();
 op(s,"set_context",{{"context","structure"}});
 auto table=[&](const std::string &before,const std::string &author){return op(s,"create_table_sized",{{"before",before},{"author_before",author},{"title","Tabela"},{"header",true},{"rows",2},{"columns",2}});};
 t1=table("","").at("selection").as_string();t2=table("","").at("selection").as_string();
 state=op(s,"place_beside",{{"id",t2},{"other",t1},{"side","left"}});
 op(s,"set_context",{{"context","composition"}});
 state=op(s,"arrange_objects",{{"id",t2},{"other",t1},{"side","left"},{"first",0.35},{"second",0.65},{"gap",24}});
 require(order(state,"author_order")!=order(state,"visual_order"),"Fixture did not diverge");
 op(s,"set_context",{{"context","structure"}});
 const auto original=state;
 state=table(t2,p0);const auto inserted=state.at("selection").as_string();
 require(order(state,"author_order",inserted)==order(original,"author_order"),"Reading order overwritten");
 require(order(state,"visual_order",inserted)==order(original,"visual_order"),"Visual order overwritten");
 require(order(state,"author_order").front()==inserted,"Explicit reading anchor ignored");
 auto visual=order(state,"visual_order");auto it=std::find(visual.begin(),visual.end(),inserted);
 require(it!=visual.end() && std::next(it)!=visual.end() && *std::next(it)==t2,"Visual anchor ignored");
 const auto rows=persistence::json::serialize(original.at("rows"));
 const auto before_image=state;
 state=op(s,"import",{{"before",t2},{"author_before",p2},{"data","iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEUlEQVR4nGNwaDjwH4QZYAwAVPQJ/Q4itk8AAAAASUVORK5CYII="},{"caption","PNG"},{"alt",""},{"decorative",true}});
 const auto image=state.at("selection").as_string();
 require(order(state,"author_order",image)==order(before_image,"author_order"),"PNG changed reading order");
 require(order(state,"visual_order",image)==order(before_image,"visual_order"),"PNG changed visual order");
 op(s,"undo");state=view(s);

 require(persistence::json::serialize(state.at("rows"))==rows,"Pair geometry changed");
 auto assert_refused=[&](V::Object a){a["op"]="create_table_sized";a["expected"]=view(s).at("revision");a["title"]="Invalid";a["header"]=false;a["rows"]=2;a["columns"]=2;const auto before=view(s);auto r=s.request(a);require(!r.at("accepted").as_boolean(),"Invalid anchor accepted");require(view(s).at("revision").as_integer()==before.at("revision").as_integer(),"Refusal mutated state");};
 assert_refused({{"before",t1},{"author_before",p0}}); // second side of a visual row
 assert_refused({{"before",""},{"author_before","foreign"}});
 op(s,"undo");require(order(view(s),"author_order")==order(original,"author_order"),"Insertion undo failed");op(s,"redo");
 state=op(s,"create_equation",{{"before",t2},{"author_before",p2},{"source","x^2"},{"mode","inline"},{"label","Equação"}});
 require(persistence::json::serialize(state.at("rows"))==rows,"Equation disturbed pair");
 state=op(s,"create_formula",{{"before",""},{"author_before",p0},{"name","HP"},{"source","declara HP = 100\nresultado HP"},{"table",""},{"cell",""},{"prefix",""},{"suffix",""}});
 require(state.at("formulas").as_array().size()==1,"Formula missing");
 const auto formula=state.at("selection").as_string();op(s,"remove",{{"id",formula}});op(s,"restore_removed",{{"id",formula},{"before",t2},{"author_before",p0}});
 // Continue canonical text editing without resetting the visual arrangement.
 const auto changed=application::replace_selection({s.current_document(),{}},0,0,"Continuar ").document;
 s.synchronize_document(changed);
 require(s.current_document().content.starts_with("Continuar "),"Text edit failed");
 require(persistence::json::serialize(view(s).at("rows"))==rows,"Text edit lost pair");
 saved=op(s,"save");d=s.current_document();
 require(persistence::encode_document_snapshot(*service.writing().document(d.id))==bytes,"Session modified canonical owner");
 }
 {Session s(folder,service.current()->manifest().project_id,d,objects::pilot::DocumentBinding::Live);
 auto state=view(s);require(order(state,"author_order")==order(saved,"author_order"),"Reopen reading mismatch");require(order(state,"visual_order")==order(saved,"visual_order"),"Reopen visual mismatch");require(persistence::json::serialize(state.at("rows"))==persistence::json::serialize(saved.at("rows")),"Reopen pair mismatch");}
 service.close();std::filesystem::remove_all(root);std::cout<<"Native independent insertion destinations, pair conservation, undo/redo, continued text and restart OK\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<"\nEvidence: "<<root<<"\n";return 1;}
}

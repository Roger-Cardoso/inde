#include "object_composite_pilot.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
namespace fs=std::filesystem;
using V=inde::persistence::json::Value;
using Session=inde::objects::pilot::Session;
void check(bool yes,const char *message){if(!yes)throw std::runtime_error(message);}
V view(Session &s){return s.request(V::Object{{"op","view"}}).at("state");}
V op(Session &s,const std::string &name,V::Object args={}){args["op"]=name;args["expected"]=view(s).at("revision");auto r=s.request(args);if(!r.at("accepted").as_boolean())throw std::runtime_error(r.at("error").as_string());return r.at("state");}
int main(){try{
 char pattern[]="/tmp/inde-native-objects-XXXXXX";const auto *name=::mkdtemp(pattern);check(name,"temporary directory");const fs::path root=name;::setenv("XDG_CONFIG_HOME",(root/"config").c_str(),1);
 inde::application::ProjectService service;service.create(root/"Autor.inde","Autor");auto d=service.writing().create_document("Matemática e história");d.content="Uma viagem.\nA equação acompanha a escrita 🙂.";d.paragraphs=inde::project::identify_paragraphs(d.content);d.formatting={{inde::project::DocumentTextStyle::Bold,0,3,{}}};d=service.writing().update_document(d);
 const auto original=inde::persistence::encode_document_snapshot(d);const auto owner=service.current()->manifest().project_id;const auto parent=service.current()->path()/"documents/composed";fs::create_directory(parent);fs::permissions(parent,fs::perms::owner_all);const auto folder=parent/d.id;std::string id;
 {
  Session s(folder,owner,d);auto state=view(s);check(state.at("native_document").as_boolean(),"native flag");check(state.at("source_document").as_string()==d.id,"wrong document");check(state.at("document_title").as_string()==d.title,"wrong title");
  state=op(s,"create_equation",{{"before",""},{"source","\\frac{a}{b}"},{"mode","block"},{"label","Fração"}});id=state.at("selection").as_string();op(s,"save");op(s,"set_context",{{"context","text"}});op(s,"edit_equation",{{"id",id},{"source","x^2+y^2"},{"mode","inline"},{"label","Distância"}});
 }
 check(inde::persistence::encode_document_snapshot(*service.writing().document(d.id))==original,"owner source changed");
 {
  Session s(folder,owner,d);auto state=view(s);check(state.at("dirty").as_boolean(),"unsaved state not recovered");check(state.at("equations").as_array()[0].at("source").as_string()=="x^2+y^2","source lost");op(s,"set_context",{{"context","structure"}});op(s,"remove",{{"id",id}});op(s,"save");op(s,"restore_removed",{{"id",id},{"before",""}});op(s,"save");
 }
 service.save_as(root/"Copia.inde");{
  Session s(root/"Copia.inde/documents/composed"/d.id,service.current()->manifest().project_id,*service.writing().document(d.id));check(view(s).at("equations").as_array().size()==1,"save as lost objects");
 }
 auto changed=*service.writing().document(d.id);changed.content+=" Novo texto.";changed.paragraphs=inde::project::identify_paragraphs(changed.content);changed=service.writing().update_document(changed);bool refused=false;
 try{Session s(root/"Copia.inde/documents/composed"/d.id,service.current()->manifest().project_id,changed);}catch(const std::exception &e){refused=std::string(e.what())=="native_document_source_changed";}check(refused,"source drift accepted");
 bool pilot_refused=false;try{Session s(folder,inde::objects::pilot::Profile::P2D2);}catch(const std::exception &){pilot_refused=true;}check(pilot_refused,"pilot accepts real project path");
 inde::persistence::SqliteDatabase db(root/"Copia.inde/data/project.sqlite3");check(db.query_integer("SELECT MAX(version) FROM schema_migrations")==37,"canonical schema changed");check(db.query_text("PRAGMA integrity_check")=="ok","canonical integrity");service.close();
 // The test owns this newly created temporary tree only.
 fs::remove_all(root);std::cout<<"Native document binding, source preservation, recovery, save-as, stale refusal and schema37: OK\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}

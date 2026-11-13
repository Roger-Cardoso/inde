#include "publication_service.hpp"
#include <regex>
#include <set>
#include <algorithm>
namespace inde::publication {
namespace {
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::string text(const V &v,const char *key){return v.at(key).as_string();}
void ident(const std::string &value){require(std::regex_match(value,std::regex("[A-Za-z0-9][A-Za-z0-9_-]{0,199}")),"Identidade pública inválida");}
void slug(const std::string &value){const std::set<std::string> reserved{"home","assets","css-global","js-global","site","publicado","scripts","tests","backend","exports","autoria","obra"};require(value.size()<=100 && std::regex_match(value,std::regex("[a-z0-9]+(-[a-z0-9]+)*")) && !reserved.contains(value),"Endereço público inválido ou reservado");}
void audit(persistence::SqliteDatabase &db,const Edition &before,const Edition &after){auto q=db.prepare("INSERT INTO history(kind,object_id,before_payload,after_payload,created_at) VALUES('bind-literature',?,?,?,?)");q.bind(1,after.id);q.bind(2,persistence::json::serialize(encode(before),false));q.bind(3,persistence::json::serialize(encode(after),false));q.bind(4,project::utc_now());q.run();}
}
void Service::validate_public_identity(const Edition &e)const{
 const auto &identity=e.public_identity;
 require(identity.as_object().size()==10 && identity.at("version").as_integer()==1,"Mapa público incompatível");ident(text(identity,"work"));slug(text(identity,"folder"));slug(text(identity,"slug"));require(text(identity,"slug")==e.slug,"O endereço desta Edição está vinculado à identidade importada");
 std::set<std::string> aliases;for(const auto &a:identity.at("aliases").as_array()){slug(a.as_string());require(a.as_string()!=e.slug && aliases.insert(a.as_string()).second,"Alias público repetido");}
 const auto next=identity.at("next_number").as_integer();require(next>=1 && next<=1000000 && e.next_number>=next,"Contador público importado inválido");
 for(const auto *kind:{"documents","entities","anchors","annotations"}){
  std::set<std::string> native_ids,public_ids;
  for(const auto &entry:identity.at(kind).as_array()){
   const auto native=text(entry,"native"),source=text(entry,"source");ident(native);ident(source);
   const auto context=(std::string(kind)=="anchors"||std::string(kind)=="annotations")?text(entry,"document")+":":"";
   require(native_ids.insert(context+native).second && public_ids.insert(context+source).second,"Identidade pública repetida no mesmo contexto");
   if(std::string(kind)=="documents"){const auto n=entry.at("number").as_integer();require(entry.as_object().size()==3 && n>=1 && n<next && e.public_numbers.contains(native) && e.public_numbers.at(native)==n,"A reserva de URL importada não corresponde ao Documento");}
   else require(entry.as_object().size()==(std::string(kind)=="entities"?2:3),"Campos do mapa público incompatíveis");
  }
 }
}
Edition Service::bind_literature_identity(const std::string &id,std::int64_t expected,const V &identity){
 auto db=writable_database();persistence::SqliteTransaction transaction(*db);auto before=edition(id),e=before;
 require(before.revision==expected && expected==1 && !before.removed && before.public_identity.is_null() && preparations(id).empty(),"Vincule identidades somente a uma Edição nova e sem preparações");
 e.public_identity=identity;e.public_numbers.clear();e.next_number=identity.at("next_number").as_integer();
 std::set<std::string> mapped_documents,mapped_entities,selected_documents;
 for(const auto &[node,choice]:e.choices)for(const auto &doc:choice.documents)selected_documents.insert(doc);
 for(const auto &entry:identity.at("documents").as_array()){
  const auto native=text(entry,"native");require(selected_documents.contains(native) && mapped_documents.insert(native).second,"O mapa público aponta para um Documento fora da seleção");e.public_numbers.emplace(native,entry.at("number").as_integer());
 }
 require(mapped_documents==selected_documents,"O mapa público precisa cobrir todos os Documentos importados");
 for(const auto &entry:identity.at("entities").as_array()){const auto native=text(entry,"native");require(std::find(e.entities.begin(),e.entities.end(),native)!=e.entities.end() && mapped_entities.insert(native).second,"Personagem público fora da seleção");}
 require(mapped_entities==std::set<std::string>(e.entities.begin(),e.entities.end()),"O mapa público precisa cobrir os Personagens importados");
 for(const auto *kind:{"anchors","annotations"})for(const auto &entry:identity.at(kind).as_array()){
  const auto doc_id=text(entry,"document");require(mapped_documents.contains(doc_id),"Contexto do mapa fora da Obra");const auto doc=source_.writing().document(doc_id);require(bool(doc),"Documento do mapa ausente");
  const auto native=text(entry,"native");if(std::string(kind)=="anchors")require(std::any_of(doc->anchors.begin(),doc->anchors.end(),[&](const auto &a){return a.id==native;}),"Âncora pública ausente");
  else require(std::any_of(doc->semantics.annotations.begin(),doc->semantics.annotations.end(),[&](const auto &a){return a.id==native;}),"Marcação pública ausente");
 }
 validate_public_identity(e);validate_edition(e,false);
 for(const auto &other:editions(e.site_id,true))if(other.id!=e.id){std::set<std::string> routes{other.slug};if(!other.public_identity.is_null())for(const auto &alias:other.public_identity.at("aliases").as_array())routes.insert(alias.as_string());require(!routes.contains(e.slug),"Endereço público já reservado");for(const auto &alias:e.public_identity.at("aliases").as_array())require(!routes.contains(alias.as_string()),"Alias público já reservado");}
 ++e.revision;
 auto q=db->prepare("UPDATE editions SET revision=?,payload=? WHERE id=?");q.bind(1,e.revision);q.bind(2,persistence::json::serialize(encode(e),false));q.bind(3,e.id);q.run();audit(*db,before,e);transaction.commit();return e;
}
}

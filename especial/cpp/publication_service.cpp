#include "publication_service.hpp"
#include "inde/project/manifest.hpp"
#include <glib.h>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <regex>
#include <set>

namespace inde::publication {
namespace {
using DB=persistence::SqliteDatabase;
using TX=persistence::SqliteTransaction;
std::string dump(const V &v){return persistence::json::serialize(v,false);}
std::string text(const V &v,const char *key){return v.at(key).as_string();}
void require(bool value,const std::string &message){if(!value)throw std::runtime_error(message);}
void bounded(const std::string &s,std::size_t limit,const char *label,bool required=false){
  require(s.size()<=limit*4 && g_utf8_validate(s.data(),s.size(),nullptr) && g_utf8_strlen(s.data(),s.size())<=static_cast<glong>(limit),std::string(label)+": texto inválido ou muito extenso");
  require(!required || s.find_first_not_of(" \n\r\t")!=std::string::npos,std::string("Preencha ")+label);
}
Site load_site(DB &db,const std::string &id){
  auto q=db.prepare("SELECT payload,revision,removed FROM sites WHERE id=?");q.bind(1,id);
  require(q.step(),"Site não encontrado");auto value=persistence::json::parse(q.column_text(0));
  const auto schema=db.query_text("SELECT value FROM metadata WHERE key='schema'");
  require((schema=="1" && value.as_object().size()==12) || ((schema=="2" || schema=="3") && value.as_object().size()==13 && value.as_object().contains("catalog")),"O Site não corresponde à versão da extensão");
  auto s=site_from_json(value);
  require(s.id==id && s.revision==q.column_integer(1) && s.removed==bool(q.column_integer(2)),"Registro de Site inconsistente");return s;
}
Edition load_edition(DB &db,const std::string &id){
  auto q=db.prepare("SELECT payload,revision,removed,site_id FROM editions WHERE id=?");q.bind(1,id);
  require(q.step(),"Edição não encontrada");auto payload=persistence::json::parse(q.column_text(0));const auto schema=db.query_text("SELECT value FROM metadata WHERE key='schema'");require(payload.as_object().size()==(schema=="3"?13:12),"A Edição não corresponde à versão da extensão");auto e=edition_from_json(payload);
  require(e.id==id && e.revision==q.column_integer(1) && e.removed==bool(q.column_integer(2)) && e.site_id==q.column_text(3),"Registro de Edição inconsistente");return e;
}
void audit(DB &db,const std::string &kind,const std::string &id,const V &before,const V &after){
  auto q=db.prepare("INSERT INTO history(kind,object_id,before_payload,after_payload,created_at) VALUES(?,?,?,?,?)");
  q.bind(1,kind);q.bind(2,id);q.bind(3,dump(before));q.bind(4,dump(after));q.bind(5,project::utc_now());q.run();
}
void write_site(DB &db,const Site &s){
  auto q=db.prepare("UPDATE sites SET revision=?,removed=?,payload=? WHERE id=?");
  q.bind(1,s.revision);q.bind(2,std::int64_t(s.removed));q.bind(3,dump(encode(s)));q.bind(4,s.id);q.run();
}
void write_edition(DB &db,const Edition &e){
  auto q=db.prepare("UPDATE editions SET revision=?,removed=?,payload=? WHERE id=?");
  q.bind(1,e.revision);q.bind(2,std::int64_t(e.removed));q.bind(3,dump(encode(e)));q.bind(4,e.id);q.run();
}
std::string catalog_hash(const V &payload){
  auto value=payload.as_object();A items;
  for(const auto &item:value.at("editions").as_array()){
    auto copy=item.as_object();auto snapshot=copy.at("snapshot").as_object();snapshot.erase("prepared_at");snapshot.erase("omissions");copy["snapshot"]=snapshot;items.emplace_back(copy);
  }
  value["editions"]=items;return hash(dump(value));
}
std::string source_hash(const V &snapshot){auto value=snapshot.as_object();value.erase("prepared_at");return hash(dump(value));}
std::set<std::string> public_routes(const Edition &edition){std::set<std::string> out{edition.slug};if(!edition.public_identity.is_null())for(const auto &alias:edition.public_identity.at("aliases").as_array())out.insert(alias.as_string());return out;}
bool routes_overlap(const Edition &a,const Edition &b){const auto other=public_routes(b);for(const auto &route:public_routes(a))if(other.contains(route))return true;return false;}
std::vector<Edition> read_editions(DB &db,const std::string &site,bool removed){
  auto q=db.prepare(removed?"SELECT id FROM editions WHERE site_id=? ORDER BY rowid":"SELECT id FROM editions WHERE site_id=? AND removed=0 ORDER BY rowid");q.bind(1,site);
  std::vector<Edition> out;while(q.step())out.push_back(load_edition(db,q.column_text(0)));return out;
}
}
V encode(const Site &s){return O{{"id",s.id},{"name",s.name},{"author",s.author},{"description",s.description},
  {"theme",s.theme},{"origin",s.origin},{"base_path",s.base_path},{"destination",s.destination},
  {"repository",s.repository},{"branch",s.branch},{"revision",s.revision},{"removed",s.removed},{"catalog",strings(s.catalog)}};}
Site site_from_json(const V &v){
  require(v.as_object().size()==12 || (v.as_object().size()==13 && v.as_object().contains("catalog")),"Formato de Site incompatível");Site s;
  s.id=text(v,"id");s.name=text(v,"name");s.author=text(v,"author");s.description=text(v,"description");
  s.theme=text(v,"theme");s.origin=text(v,"origin");s.base_path=text(v,"base_path");s.destination=text(v,"destination");
  s.repository=text(v,"repository");s.branch=text(v,"branch");s.revision=v.at("revision").as_integer();s.removed=v.at("removed").as_boolean();
  if(v.as_object().contains("catalog"))for(const auto &id:v.at("catalog").as_array())s.catalog.push_back(id.as_string());
  return s;
}
V encode(const Edition &e){
  A choices;O numbers;
  for(const auto &[id,c]:e.choices)choices.emplace_back(O{{"node",id},{"heading",c.heading},{"documents",strings(c.documents)}});
  for(const auto &[id,n]:e.public_numbers)numbers.emplace(id,n);
  return O{{"id",e.id},{"site_id",e.site_id},{"title",e.title},{"work_id",e.work_id},{"structure_id",e.structure_id},
    {"slug",e.slug},{"choices",choices},{"entities",strings(e.entities)},{"public_numbers",numbers},
    {"next_number",e.next_number},{"revision",e.revision},{"removed",e.removed},{"public_identity",e.public_identity}};
}
Edition edition_from_json(const V &v){
  require(v.as_object().size()==12 || v.as_object().size()==13,"Formato de Edição incompatível");Edition e;if(v.as_object().contains("public_identity"))e.public_identity=v.at("public_identity");
  e.id=text(v,"id");e.site_id=text(v,"site_id");e.title=text(v,"title");e.work_id=text(v,"work_id");e.structure_id=text(v,"structure_id");e.slug=text(v,"slug");
  for(const auto &item:v.at("choices").as_array()){
    application::ProductionChoice c;c.heading=item.at("heading").as_boolean();for(const auto &d:item.at("documents").as_array())c.documents.push_back(d.as_string());
    require(e.choices.emplace(text(item,"node"),c).second,"Unidade duplicada na seleção");
  }
  for(const auto &id:v.at("entities").as_array())e.entities.push_back(id.as_string());
  for(const auto &[id,n]:v.at("public_numbers").as_object())e.public_numbers.emplace(id,n.as_integer());
  e.next_number=v.at("next_number").as_integer();e.revision=v.at("revision").as_integer();e.removed=v.at("removed").as_boolean();return e;
}
const project::Project &Service::project()const{require(source_.current(),"Abra um Projeto INDE para gerenciar publicação");return *source_.current();}
std::filesystem::path Service::database_path()const{return project().path()/"data/publicacao.sqlite3";}
std::unique_ptr<DB> Service::database(bool create)const{
  const auto path=database_path();const bool existed=std::filesystem::exists(path);
  if(!existed && !create)return {};
  require(!std::filesystem::is_symlink(path),"A extensão de publicação não aceita banco por vínculo simbólico");
  auto db=std::make_unique<DB>(path);
  if(!existed){
    TX tx(*db);
    db->execute("CREATE TABLE IF NOT EXISTS metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS sites(id TEXT PRIMARY KEY,revision INTEGER NOT NULL,removed INTEGER NOT NULL,payload TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS editions(id TEXT PRIMARY KEY,site_id TEXT NOT NULL REFERENCES sites(id),revision INTEGER NOT NULL,removed INTEGER NOT NULL,payload TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS preparations(id TEXT PRIMARY KEY,site_id TEXT NOT NULL REFERENCES sites(id),edition_id TEXT NOT NULL REFERENCES editions(id),site_revision INTEGER NOT NULL,edition_revision INTEGER NOT NULL,source_hash TEXT NOT NULL,payload_hash TEXT NOT NULL,payload TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS history(sequence INTEGER PRIMARY KEY AUTOINCREMENT,kind TEXT NOT NULL,object_id TEXT NOT NULL,before_payload TEXT NOT NULL,after_payload TEXT NOT NULL,created_at TEXT NOT NULL);");
    auto q=db->prepare("INSERT OR IGNORE INTO metadata(key,value) VALUES('project_id',?)");q.bind(1,project().manifest().project_id);q.run();
    db->execute("CREATE TABLE catalog_preparations(id TEXT PRIMARY KEY,site_id TEXT NOT NULL REFERENCES sites(id),site_revision INTEGER NOT NULL,source_hash TEXT NOT NULL,payload_hash TEXT NOT NULL,payload TEXT NOT NULL);");
    db->execute("INSERT OR IGNORE INTO metadata(key,value) VALUES('schema','3')");tx.commit();
  }
  const auto schema=db->query_text("SELECT value FROM metadata WHERE key='schema'");
  require(schema=="1" || schema=="2" || schema=="3","Versão da extensão de publicação incompatível");
  require(db->query_text("SELECT value FROM metadata WHERE key='project_id'")==project().manifest().project_id,"A publicação pertence a outro Projeto. A cópia exige adoção explícita");
  return db;
}
void Service::upgrade(DB &db)const{
  const auto schema=db.query_text("SELECT value FROM metadata WHERE key='schema'");if(schema=="3")return;
  require(schema=="1" || schema=="2","Versão da extensão incompatível");
  const auto directory=project().path()/"data/publicacao-recuperacao";std::filesystem::create_directories(directory);
  const auto backup=directory/("schema"+schema+"-"+project::new_uuid()+".sqlite3");
  {auto q=db.prepare("VACUUM INTO ?");q.bind(1,backup.string());q.run();}
  {DB copy(backup);require(copy.query_text("PRAGMA integrity_check")=="ok","A cópia de recuperação da extensão não passou na conferência");}
  TX tx(db);require(db.query_text("SELECT value FROM metadata WHERE key='schema'")==schema,"A extensão mudou durante a atualização");
  std::vector<Site> all;{auto q=db.prepare("SELECT id FROM sites");while(q.step())all.push_back(load_site(db,q.column_text(0)));}
  std::vector<Edition> editions;{auto q=db.prepare("SELECT id FROM editions");while(q.step())editions.push_back(load_edition(db,q.column_text(0)));}
  if(schema=="1")db.execute("CREATE TABLE catalog_preparations(id TEXT PRIMARY KEY,site_id TEXT NOT NULL REFERENCES sites(id),site_revision INTEGER NOT NULL,source_hash TEXT NOT NULL,payload_hash TEXT NOT NULL,payload TEXT NOT NULL)");
  for(const auto &site:all){write_site(db,site);}for(const auto &edition:editions){write_edition(db,edition);}
  db.execute("UPDATE metadata SET value='3' WHERE key='schema'");
  audit(db,"migrate-extension",project().manifest().project_id,O{{"schema",schema}},O{{"schema","3"},{"backup",backup.filename().string()}});tx.commit();
}
std::unique_ptr<DB> Service::writable_database(bool create)const{
  auto db=database(create);require(bool(db),"Crie um Site primeiro");upgrade(*db);return db;
}
void Service::validate_catalog(const Site &s)const{
  require(s.catalog.size()<=100,"O catálogo deste laboratório aceita até 100 Obras; nada foi truncado");
  std::set<std::string> ids,works,slugs;
  for(const auto &id:s.catalog){
    const auto e=edition(id);
    require(e.site_id==s.id && !e.removed && ids.insert(id).second,"Edição do catálogo ausente, retirada, repetida ou de outro Site");
    require(works.insert(e.work_id).second,"Escolha apenas uma Edição de cada Obra no catálogo");
    require(slugs.insert(e.slug).second,"Endereços públicos repetidos no catálogo");if(!e.public_identity.is_null())for(const auto &alias:e.public_identity.at("aliases").as_array())require(slugs.insert(alias.as_string()).second,"Alias público já utilizado no catálogo");
  }
}
Site Service::update_catalog(const std::string &id,std::int64_t expected,const std::vector<std::string> &editions){
  auto owner=site(id);owner.catalog=editions;return update_site(owner,expected);
}
V Service::capture_catalog(const Site &s)const{
  require(!s.removed,"Restaure o Site antes de preparar");validate_catalog(s);A items;
  for(const auto &id:s.catalog){const auto e=edition(id);items.emplace_back(O{{"edition",encode(e)},{"snapshot",capture(e,s)}});}
  bool bound=false;for(const auto &item:items)bound=bound||item.at("snapshot").at("version").as_integer()==2;return O{{"protocol","inde.web.catalog"},{"version",bound?2:1},{"site",encode(s)},{"editions",items}};
}
Prepared Service::prepare_catalog(const std::string &id,std::int64_t expected){
  const auto owner=site(id);require(owner.revision==expected,"O Site mudou. Recarregue antes de preparar");const auto payload=capture_catalog(owner);
  Prepared p{project::new_uuid(),id,{},catalog_hash(payload),{},dump(payload),expected,0,true};p.payload_hash=hash(p.payload);
  require(p.payload.size()<=64*1024*1024,"Retrato do catálogo excede 64 MiB; nada foi truncado");
  auto db=writable_database();TX tx(*db);
  require(load_site(*db,id).revision==expected && catalog_hash(capture_catalog(owner))==p.source_hash,"O catálogo ou suas fontes mudaram durante a preparação");
  auto q=db->prepare("INSERT INTO catalog_preparations VALUES(?,?,?,?,?,?)");q.bind(1,p.id);q.bind(2,id);q.bind(3,expected);q.bind(4,p.source_hash);q.bind(5,p.payload_hash);q.bind(6,p.payload);q.run();
  audit(*db,"prepare-catalog",id,V{},O{{"preparation",p.id},{"payload_hash",p.payload_hash}});tx.commit();return p;
}
std::vector<Site> Service::sites(bool removed)const{
  auto db=database();if(!db)return {};
  auto q=db->prepare(removed?"SELECT id FROM sites ORDER BY rowid":"SELECT id FROM sites WHERE removed=0 ORDER BY rowid");
  std::vector<Site> result;while(q.step())result.push_back(load_site(*db,q.column_text(0)));return result;
}
std::vector<Edition> Service::editions(const std::string &id,bool removed)const{auto db=database();if(!db)return {};return read_editions(*db,id,removed);}
Site Service::site(const std::string &id)const{auto db=database();require(bool(db),"Nenhum Site cadastrado");return load_site(*db,id);}
Edition Service::edition(const std::string &id)const{auto db=database();require(bool(db),"Nenhuma Edição cadastrada");return load_edition(*db,id);}
void Service::validate_site(const Site &s)const{
  bounded(s.name,500,"nome do Site",true);bounded(s.author,500,"autoria",true);bounded(s.description,500,"apresentação",true);
  require(s.theme=="violeta" || s.theme=="verde" || s.theme=="ambar","Tema inválido");
  require(s.destination=="local" || s.destination=="pacote" || s.destination=="github","Destino inválido");
  require(s.base_path.empty() || std::regex_match(s.base_path,std::regex("(/[a-zA-Z0-9_-]+)+")),"Caminho base inválido; use /biblioteca ou deixe vazio");
  if(!s.origin.empty()){
    GError *error=nullptr;auto *uri=g_uri_parse(s.origin.c_str(),G_URI_FLAGS_NONE,&error);
    const bool valid=uri && std::string(g_uri_get_scheme(uri)?g_uri_get_scheme(uri):"")=="https" && g_uri_get_host(uri) &&
      !g_uri_get_userinfo(uri) && g_uri_get_port(uri)==-1 && std::string(g_uri_get_path(uri)?g_uri_get_path(uri):"").empty() && !g_uri_get_query(uri) && !g_uri_get_fragment(uri);
    if(uri)g_uri_unref(uri);
    if(error)g_error_free(error);
    require(valid,"Origem pública inválida; use https://dominio sem caminho");
  }
  if(s.destination=="github"){
    require(!s.origin.empty(),"Informe a origem pública para preparar GitHub");
    require(std::regex_match(s.repository,std::regex("[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9_.-]+")),"Repositório inválido; use proprietario/nome");
    require(std::regex_match(s.branch,std::regex("[A-Za-z0-9][A-Za-z0-9_./-]{0,100}")) && s.branch.find("..")==std::string::npos && s.branch.find("//")==std::string::npos && s.branch.back()!='/',"Branch inválida");
  }
}
Site Service::create_site(Site s){
  validate_site(s);require(s.catalog.empty(),"Crie o Site antes de escolher suas Edições");auto db=writable_database(true);TX tx(*db);s.id=project::new_uuid();s.revision=1;s.removed=false;
  auto q=db->prepare("INSERT INTO sites(id,revision,removed,payload) VALUES(?,?,0,?)");q.bind(1,s.id);q.bind(2,s.revision);q.bind(3,dump(encode(s)));q.run();
  audit(*db,"create-site",s.id,V{},encode(s));tx.commit();return s;
}
Site Service::update_site(Site s,std::int64_t expected){
  validate_site(s);validate_catalog(s);auto db=writable_database();require(bool(db),"Site não encontrado");TX tx(*db);const auto before=load_site(*db,s.id);
  require(before.revision==expected && !before.removed,"O Site mudou. Recarregue antes de salvar");
  s.revision=expected+1;s.removed=false;validate_catalog(s);write_site(*db,s);audit(*db,"update-site",s.id,encode(before),encode(s));tx.commit();return s;
}
Site Service::change_site_state(const std::string &id,std::int64_t expected,bool removed){
  auto db=writable_database();require(bool(db),"Site não encontrado");TX tx(*db);auto s=load_site(*db,id);const auto before=s;
  require(s.revision==expected && s.removed!=removed,"O Site mudou. Reabra a operação");
  if(removed)require(read_editions(*db,id,false).empty(),"Retire primeiro as Edições deste Site; nenhuma foi apagada");
  s.removed=removed;++s.revision;write_site(*db,s);audit(*db,removed?"remove-site":"restore-site",id,encode(before),encode(s));tx.commit();return s;
}
Site Service::remove_site(const std::string &id,std::int64_t r){return change_site_state(id,r,true);}
Site Service::restore_site(const std::string &id,std::int64_t r){return change_site_state(id,r,false);}
void Service::validate_edition(Edition &e,bool allocate)const{
  bounded(e.title,500,"nome da Edição",true);
  require(std::regex_match(e.slug,std::regex("[a-z0-9]+(-[a-z0-9]+)*")) && e.slug.size()<=100 && e.slug!="home" && e.slug!="obra" && e.slug!="assets" && e.slug!="site" && e.slug!="publicado" && e.slug!="scripts" && e.slug!="css-global" && e.slug!="js-global","Endereço público inválido ou reservado");
  const auto &works=source_.catalog().works;
  require(std::any_of(works.begin(),works.end(),[&](const auto &w){return w.id==e.work_id;}),"A Obra da Edição não existe mais");
  const auto structures=source_.structures().editorial_structures(e.work_id);
  require(std::any_of(structures.begin(),structures.end(),[&](const auto &s){return s.id==e.structure_id;}),"A estrutura da Edição está ausente");
  auto nodes=source_.structural_nodes_for_work(e.work_id);
  std::erase_if(nodes,[&](const auto &n){return n.structure_id!=e.structure_id;});
  std::set<std::string> docs;std::set<std::int64_t> numbers;
  require(e.next_number>=1 && e.next_number<=1000000 && e.public_numbers.size()<=10000,"Reserva de endereços inválida");
  for(const auto &[id,n]:e.public_numbers){require(!id.empty() && n>0 && n<e.next_number && numbers.insert(n).second,"Número público repetido ou não reservado");}
  for(const auto &[id,c]:e.choices){
    const auto node=std::find_if(nodes.begin(),nodes.end(),[&](const auto &n){return n.id==id && n.structure_id==e.structure_id;});
    require(node!=nodes.end(),"Unidade selecionada ausente ou de outra estrutura");
    require(c.heading || !c.documents.empty(),"Seleção vazia; remova explicitamente a unidade");
    for(const auto &doc_id:c.documents){
      const auto doc=source_.writing().document(doc_id);
      require(doc && doc->editorial_node_id==id && docs.insert(doc_id).second,"Documento ausente, duplicado ou fora da unidade selecionada");
      require(allocate || e.public_numbers.contains(doc_id),"Documento sem número público reservado");
    }
  }
  for(const auto &node:application::production_order(nodes)){
    const auto choice=e.choices.find(node.id);if(choice==e.choices.end())continue;
    for(const auto &id:choice->second.documents)if(!e.public_numbers.contains(id))e.public_numbers[id]=e.next_number++;
  }
  if(!e.public_identity.is_null())validate_public_identity(e);
  std::set<std::string> entity_ids;
  for(const auto &id:e.entities)require(entity_ids.insert(id).second && source_.narrative().entity(id).has_value(),"Entidade selecionada ausente ou repetida");
}
Edition Service::create_edition(Edition e){
  e.id=project::new_uuid();e.revision=1;e.removed=false;e.public_identity=V{};e.public_numbers.clear();e.next_number=1;validate_edition(e,true);
  auto db=writable_database();require(bool(db),"Crie um Site primeiro");TX tx(*db);require(!load_site(*db,e.site_id).removed,"Restaure primeiro o Site");
  for(const auto &other:read_editions(*db,e.site_id,true))require(!routes_overlap(other,e),"Este endereço já pertence a outra Edição do Site");
  auto q=db->prepare("INSERT INTO editions(id,site_id,revision,removed,payload) VALUES(?,?,?,0,?)");q.bind(1,e.id);q.bind(2,e.site_id);q.bind(3,e.revision);q.bind(4,dump(encode(e)));q.run();
  audit(*db,"create-edition",e.id,V{},encode(e));tx.commit();return e;
}
Edition Service::update_edition(Edition e,std::int64_t expected){
  auto db=writable_database();require(bool(db),"Edição não encontrada");TX tx(*db);const auto before=load_edition(*db,e.id);
  require(before.revision==expected && !before.removed && e.site_id==before.site_id,"A Edição mudou. Recarregue antes de salvar");
  const auto owner=load_site(*db,e.site_id);require(!owner.removed,"Restaure primeiro o Site");
  require(before.work_id==e.work_id || std::find(owner.catalog.begin(),owner.catalog.end(),e.id)==owner.catalog.end(),"Retire a Edição do catálogo antes de mudar sua Obra");
  require(before.public_identity.is_null() || before.work_id==e.work_id,"Uma Edição com identidades preservadas não pode trocar de Obra");e.public_identity=before.public_identity;e.public_numbers=before.public_numbers;e.next_number=before.next_number;e.revision=expected+1;e.removed=false;validate_edition(e,true);
  for(const auto &other:read_editions(*db,e.site_id,true))require(other.id==e.id || !routes_overlap(other,e),"Endereço público já utilizado");
  write_edition(*db,e);audit(*db,"update-edition",e.id,encode(before),encode(e));tx.commit();return e;
}
Edition Service::change_edition_state(const std::string &id,std::int64_t expected,bool removed){
  auto db=writable_database();require(bool(db),"Edição não encontrada");TX tx(*db);auto e=load_edition(*db,id);const auto before=e;
  require(e.revision==expected && e.removed!=removed,"A Edição mudou. Reabra a operação");
  if(removed){const auto owner=load_site(*db,e.site_id);require(std::find(owner.catalog.begin(),owner.catalog.end(),id)==owner.catalog.end(),"Retire a Edição do catálogo do Site antes de retirá-la");}
  if(!removed){require(!load_site(*db,e.site_id).removed,"Restaure primeiro o Site");
    for(const auto &other:read_editions(*db,e.site_id,true))require(other.id==e.id || other.slug!=e.slug,"O endereço da Edição já está ocupado");}
  e.removed=removed;++e.revision;write_edition(*db,e);audit(*db,removed?"remove-edition":"restore-edition",id,encode(before),encode(e));tx.commit();return e;
}
Edition Service::remove_edition(const std::string &id,std::int64_t r){return change_edition_state(id,r,true);}
Edition Service::restore_edition(const std::string &id,std::int64_t r){return change_edition_state(id,r,false);}
V Service::capture(const Edition &edition,const Site &s)const{
  const auto imported=project().path()/"data/importacao-literature.json";
  if(std::filesystem::exists(imported)){
    std::ifstream input(imported);const auto receipt=persistence::json::parse(std::string{std::istreambuf_iterator<char>(input),{}});
    require(receipt.at("work_id").as_string()!=edition.work_id || !edition.public_identity.is_null(),"Esta Obra importada ainda precisa reconciliar suas identidades públicas antes de gerar o Site");
  }
  auto e=edition;validate_edition(e,false);require(!e.removed && !s.removed,"Restaure o Site e a Edição antes de preparar");
  const auto &works=source_.catalog().works;const auto w=std::find_if(works.begin(),works.end(),[&](const auto &value){return value.id==e.work_id;});
  std::vector<project::StructuralNode> nodes;for(const auto &n:source_.structural_nodes_for_work(e.work_id))if(n.structure_id==e.structure_id)nodes.push_back(n);
  application::ProductionOptions options;options.author=s.author;options.profile=application::ProductionProfile::Reading;
  const auto production=application::prepare_work_production(project().manifest().project_id,e.structure_id,*w,nodes,source_.writing().documents(),e.choices,options);
  std::vector<project::NarrativeEntity> entities;const auto types=source_.narrative().entity_types();
  for(const auto &id:e.entities){const auto entity=*source_.narrative().entity(id);
    require(std::any_of(types.begin(),types.end(),[&](const auto &t){return t.id==entity.entity_type_id && t.key=="character";}),"Este tipo de entidade ainda não tem projeção web");entities.push_back(entity);}
  auto result=snapshot(production,entities).as_object();A units;
  for(const auto &unit:result.at("units").as_array()){
    auto u=unit.as_object();A documents;
    for(const auto &doc:u.at("documents").as_array()){auto d=doc.as_object();d.emplace("public_number",e.public_numbers.at(text(doc,"id")));documents.emplace_back(d);}
    u["documents"]=documents;units.emplace_back(u);
  }
  result["units"]=units;if(!e.public_identity.is_null()){result["public_identity"]=e.public_identity;result["version"]=2;}return result;
}
Prepared Service::prepare(const std::string &id,std::int64_t expected_site,std::int64_t expected_edition){
  const auto e=edition(id);const auto s=site(e.site_id);
  require(e.revision==expected_edition && s.revision==expected_site,"Site ou Edição mudou. Recarregue antes de preparar");
  const auto captured=capture(e,s);
  const V wrapper=O{{"protocol","inde.web.edition"},{"version",captured.at("version")},{"site",encode(s)},{"edition",encode(e)},{"snapshot",captured}};
  Prepared p{project::new_uuid(),s.id,e.id,source_hash(captured),{},dump(wrapper),s.revision,e.revision};p.payload_hash=hash(p.payload);
  require(p.payload.size()<=16*1024*1024,"Retrato excede 16 MiB; nada foi truncado");
  auto db=writable_database();TX tx(*db);
  require(load_site(*db,s.id).revision==s.revision && load_edition(*db,e.id).revision==e.revision && source_hash(capture(e,s))==p.source_hash,"A fonte mudou durante a preparação; prepare novamente");
  auto q=db->prepare("INSERT INTO preparations VALUES(?,?,?,?,?,?,?,?)");q.bind(1,p.id);q.bind(2,p.site_id);q.bind(3,p.edition_id);q.bind(4,p.site_revision);q.bind(5,p.edition_revision);q.bind(6,p.source_hash);q.bind(7,p.payload_hash);q.bind(8,p.payload);q.run();
  audit(*db,"prepare-edition",id,V{},O{{"preparation",p.id},{"payload_hash",p.payload_hash}});tx.commit();return p;
}
bool Service::is_current(const Prepared &p)const{
  try{if(hash(p.payload)!=p.payload_hash)return false;
    if(p.catalog){
      if(!p.edition_id.empty() || p.edition_revision!=0)return false;
      auto db=database();if(!db || db->query_text("SELECT value FROM metadata WHERE key='schema'")!="3")return false;
      auto q=db->prepare("SELECT site_id,site_revision,source_hash,payload_hash,payload FROM catalog_preparations WHERE id=?");q.bind(1,p.id);
      if(!q.step() || q.column_text(0)!=p.site_id || q.column_integer(1)!=p.site_revision || q.column_text(2)!=p.source_hash || q.column_text(3)!=p.payload_hash || q.column_text(4)!=p.payload)return false;
      const auto owner=site(p.site_id);return !owner.removed && owner.revision==p.site_revision && catalog_hash(capture_catalog(owner))==p.source_hash;
    }
    auto db=database();if(!db)return false;
    auto saved=db->prepare("SELECT payload,source_hash,payload_hash,site_id,edition_id,site_revision,edition_revision FROM preparations WHERE id=?");saved.bind(1,p.id);
    if(!saved.step() || saved.column_text(0)!=p.payload || saved.column_text(1)!=p.source_hash || saved.column_text(2)!=p.payload_hash || saved.column_text(3)!=p.site_id || saved.column_text(4)!=p.edition_id || saved.column_integer(5)!=p.site_revision || saved.column_integer(6)!=p.edition_revision)return false;
    const auto e=edition(p.edition_id);const auto s=site(p.site_id);
    return !e.removed && !s.removed && e.site_id==s.id && e.revision==p.edition_revision && s.revision==p.site_revision && source_hash(capture(e,s))==p.source_hash;
  }catch(...){return false;}
}
std::vector<Prepared> Service::preparations(const std::string &id)const{
  auto db=database();if(!db)return {};auto q=db->prepare("SELECT id,site_id,edition_id,source_hash,payload_hash,payload,site_revision,edition_revision FROM preparations WHERE edition_id=? ORDER BY rowid DESC LIMIT 50");q.bind(1,id);
  std::vector<Prepared> result;while(q.step())result.push_back({q.column_text(0),q.column_text(1),q.column_text(2),q.column_text(3),q.column_text(4),q.column_text(5),q.column_integer(6),q.column_integer(7)});return result;
}
std::filesystem::path Service::materialize(const Prepared &p)const{
  require(is_current(p),"O retrato está desatualizado. Prepare uma nova Edição");
  const auto directory=project().path()/"data/publicacao-retratos";std::filesystem::create_directories(directory);
  const auto path=directory/(p.id+".json");require(!std::filesystem::exists(path),"O retrato já foi materializado; prepare uma nova saída");
  auto *out=std::fopen(path.c_str(),"wbx");require(out,"Não foi possível criar um retrato novo");
  const auto written=std::fwrite(p.payload.data(),1,p.payload.size(),out);const auto closed=std::fclose(out);
  require(written==p.payload.size() && closed==0,"Falha ao gravar o retrato de produção");
  std::ifstream check(path,std::ios::binary);const std::string contents{std::istreambuf_iterator<char>(check),{}};
  require(hash(contents)==p.payload_hash,"O retrato gravado diverge da preparação");return path;
}
void Service::adopt_project_copy(const std::string &previous){
  const auto path=database_path();if(!std::filesystem::exists(path))return;DB db(path);
  const auto schema=db.query_text("SELECT value FROM metadata WHERE key='schema'");
  require((schema=="1" || schema=="2" || schema=="3") && db.query_text("SELECT value FROM metadata WHERE key='project_id'")==previous,"A extensão copiada não corresponde à origem");
  upgrade(db);TX tx(db);
  require(db.query_text("SELECT value FROM metadata WHERE key='project_id'")==previous,"A origem da cópia mudou");
  auto q=db.prepare("UPDATE metadata SET value=? WHERE key='project_id'");q.bind(1,project().manifest().project_id);q.run();
  auto sites_query=db.prepare("SELECT id FROM sites");std::vector<Site> all;while(sites_query.step())all.push_back(load_site(db,sites_query.column_text(0)));
  for(auto &s:all){const auto before=s;s.destination="local";s.repository.clear();s.origin.clear();++s.revision;write_site(db,s);audit(db,"adopt-project-copy",s.id,encode(before),encode(s));}
  tx.commit();
}
}

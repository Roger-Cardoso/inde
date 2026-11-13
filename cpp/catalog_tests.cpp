#include "publication_service.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
using namespace inde;using namespace inde::publication;namespace fs=std::filesystem;
namespace {
std::string read(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void check(bool ok,const char *label){if(!ok)throw std::runtime_error(label);}
template<class F>void rejects(F fn,const char *label){bool failed=false;try{fn();}catch(const std::exception &){failed=true;}check(failed,label);}
void save(const fs::path &p,const std::string &value){std::ofstream f(p);f<<value;f.close();check(bool(f),"Falha ao gravar evidência sintética");}
}
int main(int argc,char **argv){try{
  check(argc==2,"Use a fixture nova de catálogo");const fs::path root=argv[1];setenv("XDG_CONFIG_HOME",(root/"perfil-testes").c_str(),1);
  application::ProjectService source;source.open(root/"Publicacao.inde");Service service(source);
  const auto selection=persistence::json::parse(read(root/"catalogo-selecao.json"));auto site=service.site(selection.at("site").as_string());const auto original=site.catalog;check(original.size()==2,"Catálogo inicial não tem duas Obras");
  auto first=service.edition(original[0]),second=service.edition(original[1]),excluded=service.edition(selection.at("excluded").as_string());
  const auto main_before=hash(read(source.current()->path()/"data/project.sqlite3"));
  auto prepared=service.prepare_catalog(site.id,site.revision);check(service.is_current(prepared),"Preparação de catálogo obsoleta ao nascer");check(prepared.payload.find("SEGREDO-TERCEIRA-OBRA-PUB02B")==std::string::npos,"Catálogo vazou texto excluído");
  save(root/"catalogo-para-gerar.json",prepared.payload);const auto raw=service.materialize(prepared);check(read(raw)==prepared.payload,"Retrato materializado diferente");
  auto tampered=prepared;tampered.payload+=" ";tampered.payload_hash=hash(tampered.payload);check(!service.is_current(tampered),"Catálogo adulterado aceito");
  excluded.title+=" · revisado";excluded=service.update_edition(excluded,excluded.revision);check(service.is_current(prepared),"Cadastro fora do catálogo invalidou seleção sem mudar fontes");
  auto duplicate=first;duplicate.slug="outra-edicao-das-mares";duplicate=service.create_edition(duplicate);
  rejects([&]{service.update_catalog(site.id,site.revision,{first.id,duplicate.id});},"Duas Edições da mesma Obra aceitas");
  rejects([&]{service.update_catalog(site.id,site.revision,{first.id,first.id});},"Edição repetida aceita");
  Site other;other.name="Outro Site";other.author="Outra autoria";other.description="Configuração independente";other=service.create_site(other);
  auto foreign=first;foreign.site_id=other.id;foreign=service.create_edition(foreign);
  rejects([&]{service.update_catalog(site.id,site.revision,{foreign.id});},"Edição de outro Site aceita");
  rejects([&]{service.remove_edition(first.id,first.revision);},"Retirada de Edição em uso no catálogo aceita");
  auto changed_work=first;changed_work.work_id=second.work_id;rejects([&]{service.update_edition(changed_work,first.revision);},"Mudança de Obra em catálogo ativo aceita");
  const auto old_site=site;site=service.update_catalog(site.id,site.revision,{second.id,first.id});check(!service.is_current(prepared),"Reordenação não invalidou retrato");
  rejects([&]{service.update_catalog(site.id,old_site.revision,original);},"Catálogo aceitou revisão obsoleta");
  prepared=service.prepare_catalog(site.id,site.revision);auto payload=persistence::json::parse(prepared.payload);check(payload.at("editions").as_array().front().at("edition").at("id").as_string()==second.id,"Preparação perdeu ordem escolhida");save(root/"catalogo-reordenado.json",prepared.payload);
  const auto numbers=service.edition(first.id).public_numbers;check(numbers==first.public_numbers,"Reordenação mudou números públicos");
  second.title+=" · revista";second=service.update_edition(second,second.revision);check(!service.is_current(prepared),"Revisão selecionada não invalidou retrato");
  site=service.update_catalog(site.id,site.revision,{second.id});first=service.remove_edition(first.id,first.revision);prepared=service.prepare_catalog(site.id,site.revision);save(root/"catalogo-sem-mares.json",prepared.payload);
  first=service.restore_edition(first.id,first.revision);check(service.site(site.id).catalog==std::vector<std::string>{second.id},"Restauração incluiu Obra automaticamente");
  site=service.update_catalog(site.id,site.revision,{});prepared=service.prepare_catalog(site.id,site.revision);save(root/"catalogo-vazio.json",prepared.payload);check(persistence::json::parse(prepared.payload).at("editions").as_array().empty(),"Catálogo vazio não foi respeitado");
  site=service.update_catalog(site.id,site.revision,original);prepared=service.prepare_catalog(site.id,site.revision);
  check(hash(read(source.current()->path()/"data/project.sqlite3"))==main_before,"Catálogo alterou banco autoral");
  source.close();source.open(root/"Publicacao.inde");check(service.site(site.id).catalog==original,"Reabertura perdeu escolhas");
  auto documents=source.writing().documents();const auto private_doc=std::find_if(documents.begin(),documents.end(),[](const auto &d){return d.content=="SEGREDO-TERCEIRA-OBRA-PUB02B";});check(private_doc!=documents.end(),"Fixture privada ausente");
  auto private_change=*private_doc;private_change.content+=" · alteração fora da publicação";static_cast<void>(source.writing().update_document(private_change));check(service.is_current(prepared),"Texto fora do catálogo invalidou retrato");
  const auto nodes=application::production_order(source.structural_nodes_for_work(first.work_id));auto doc=*source.writing().document(first.choices.at(nodes.back().id).documents.front());doc.content+="\nAlteração depois da preparação.";static_cast<void>(source.writing().update_document(doc));check(!service.is_current(prepared),"Texto selecionado não invalidou catálogo");
  const auto previous=source.current()->manifest().project_id;source.save_as(root/"Copia-catalogo.inde");service.adopt_project_copy(previous);check(service.site(site.id).catalog==original,"Cópia perdeu catálogo");check(!service.is_current(prepared),"Cópia manteve preparação atual");source.close();

  source.close();const auto schema2=root/"Schema2.inde";fs::copy(root/"Publicacao.inde",schema2,fs::copy_options::recursive);
  {persistence::SqliteDatabase db(schema2/"data/publicacao.sqlite3");persistence::SqliteTransaction tx(db);auto q=db.prepare("SELECT id,payload FROM editions");std::vector<std::pair<std::string,std::string>> rows;while(q.step()){auto value=persistence::json::parse(q.column_text(1)).as_object();value.erase("public_identity");rows.emplace_back(q.column_text(0),persistence::json::serialize(value,false));}for(const auto &[id,value]:rows){auto u=db.prepare("UPDATE editions SET payload=? WHERE id=?");u.bind(1,value);u.bind(2,id);u.run();}db.execute("UPDATE metadata SET value='2' WHERE key='schema'");tx.commit();}
  source.open(schema2);const auto schema2_before=hash(read(service.database_path())),schema2_main=hash(read(schema2/"data/project.sqlite3"));auto s2=service.site(site.id);const auto e2=service.edition(first.id);check(e2.public_identity.is_null(),"Consulta de schema 2 inventou um mapa");check(hash(read(service.database_path()))==schema2_before&&!fs::exists(schema2/"data/publicacao-recuperacao"),"Consulta de schema 2 migrou ou criou backup");s2.description+=" revisão schema 2";s2=service.update_site(s2,s2.revision);
  {persistence::SqliteDatabase db(service.database_path());check(db.query_text("SELECT value FROM metadata WHERE key='schema'")=="3","Schema 2 não chegou ao schema 3");check(db.query_text("PRAGMA integrity_check")=="ok","Integridade após schema 2");}
  std::vector<fs::path> schema2_backups;for(const auto &entry:fs::directory_iterator(schema2/"data/publicacao-recuperacao"))schema2_backups.push_back(entry.path());check(schema2_backups.size()==1,"Schema 2 não criou exatamente um backup");{persistence::SqliteDatabase backup(schema2_backups.front());check(backup.query_text("SELECT value FROM metadata WHERE key='schema'")=="2","Backup não conserva schema 2");}
  check(hash(read(schema2/"data/project.sqlite3"))==schema2_main&&service.edition(first.id).public_numbers==e2.public_numbers,"Migração do schema 2 mudou autoria ou reservas");source.close();

  // A synthetic schema-1 copy verifies a real read-only opening and migration.
  const auto legacy=root/"Legado.inde";fs::copy(root/"Publicacao.inde",legacy,fs::copy_options::recursive);
  std::string legacy_payload;{
    persistence::SqliteDatabase db(legacy/"data/publicacao.sqlite3");persistence::SqliteTransaction tx(db);
    auto q=db.prepare("SELECT id,payload FROM sites");std::vector<std::pair<std::string,std::string>> rows;
    while(q.step()){auto value=persistence::json::parse(q.column_text(1)).as_object();value.erase("catalog");rows.emplace_back(q.column_text(0),persistence::json::serialize(value,false));}
    for(const auto &[id,value]:rows){auto update=db.prepare("UPDATE sites SET payload=? WHERE id=?");update.bind(1,value);update.bind(2,id);update.run();if(id==site.id)legacy_payload=value;}
    {auto eq=db.prepare("SELECT id,payload FROM editions");std::vector<std::pair<std::string,std::string>> legacy_editions;while(eq.step()){auto value=persistence::json::parse(eq.column_text(1)).as_object();value.erase("public_identity");legacy_editions.emplace_back(eq.column_text(0),persistence::json::serialize(value,false));}for(const auto &[id,value]:legacy_editions){auto u=db.prepare("UPDATE editions SET payload=? WHERE id=?");u.bind(1,value);u.bind(2,id);u.run();}}
    db.execute("DROP TABLE catalog_preparations;DELETE FROM history WHERE kind='prepare-catalog';UPDATE metadata SET value='1' WHERE key='schema'");tx.commit();
  }
  source.open(legacy);const auto before_read=hash(read(service.database_path()));const auto main_legacy=hash(read(legacy/"data/project.sqlite3"));const auto old_preparations=service.preparations(first.id);
  auto old=service.site(site.id);check(old.catalog.empty(),"Leitura legada inventou seleção");check(hash(read(service.database_path()))==before_read,"Consulta migrou extensão implicitamente");check(!fs::exists(legacy/"data/publicacao-recuperacao"),"Consulta criou backup ou migração");
  old.description+=" · atualizada";old=service.update_site(old,old.revision);
  persistence::SqliteDatabase migrated(service.database_path());check(migrated.query_text("SELECT value FROM metadata WHERE key='schema'")=="3","Migração não chegou ao schema 3");check(migrated.query_text("PRAGMA integrity_check")=="ok","Integridade após migração");
  std::vector<fs::path> backups;for(const auto &item:fs::directory_iterator(legacy/"data/publicacao-recuperacao"))backups.push_back(item.path());check(backups.size()==1,"Migração não conservou uma cópia prévia");
  {persistence::SqliteDatabase backup(backups.front());check(backup.query_text("SELECT value FROM metadata WHERE key='schema'")=="1","Backup perdeu schema anterior");auto q=backup.prepare("SELECT payload FROM sites WHERE id=?");q.bind(1,site.id);check(q.step()&&q.column_text(0)==legacy_payload,"Backup perdeu configuração anterior");}
  check(service.edition(first.id).public_numbers==numbers,"Migração alterou reservas");check(service.preparations(first.id).size()==old_preparations.size(),"Migração perdeu preparações anteriores");check(hash(read(legacy/"data/project.sqlite3"))==main_legacy,"Migração alterou banco autoral");
  std::string complete;{auto q=migrated.prepare("SELECT payload FROM sites WHERE id=?");q.bind(1,site.id);check(q.step(),"Site migrado ausente");complete=q.column_text(0);}
  auto incomplete=persistence::json::parse(complete).as_object();incomplete.erase("catalog");
  {auto q=migrated.prepare("UPDATE sites SET payload=? WHERE id=?");q.bind(1,persistence::json::serialize(incomplete,false));q.bind(2,site.id);q.run();}
  const auto invalid_shape=hash(read(service.database_path()));rejects([&]{service.site(site.id);},"Site incompleto tratado como catálogo vazio");check(hash(read(service.database_path()))==invalid_shape,"Leitura de formato incompleto mudou dados");
  {auto q=migrated.prepare("UPDATE sites SET payload=? WHERE id=?");q.bind(1,complete);q.bind(2,site.id);q.run();}
  migrated.execute("UPDATE metadata SET value='99' WHERE key='schema'");const auto unsupported=hash(read(service.database_path()));rejects([&]{service.sites();},"Schema futuro aceito");check(hash(read(service.database_path()))==unsupported,"Recusa de schema futuro mudou dados");
  std::cout<<"PUB-02B: seleção, ordem, vínculos, revisão, fontes, retirada, restauração, cópia, leitura legada e migração com backup OK\n";return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}

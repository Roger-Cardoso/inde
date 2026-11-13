#include "publication_service.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
using namespace inde;
using namespace inde::publication;
namespace fs=std::filesystem;
std::string read(const fs::path &p){std::ifstream f(p,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
void check(bool ok,const char *label){if(!ok)throw std::runtime_error(label);}
template<class F>void rejects(F fn,const char *label){bool failed=false;try{fn();}catch(const std::exception &){failed=true;}check(failed,label);}
int main(int argc,char **argv){try{
  check(argc==2,"Use uma pasta de fixture nova");const fs::path root=argv[1];
  setenv("XDG_CONFIG_HOME",(root/"perfil-teste").c_str(),1);
  application::ProjectService source;source.open(root/"Publicacao.inde");Service service(source);
  check(service.sites().empty()&&!fs::exists(service.database_path()),"Consultar criou extensão");
  const auto base_hash=hash(read(root/"Publicacao.inde/data/project.sqlite3"));
  Site s;s.name="Biblioteca GTK";s.author="Autoria de demonstração";s.description="Edição da sessão viva";s.base_path="/biblioteca";
  s=service.create_site(s);check(s.revision==1,"Revisão inicial de Site");
  auto stale=s;s.name="Biblioteca da estação";s=service.update_site(s,s.revision);
  rejects([&]{service.update_site(stale,stale.revision);},"Site aceitou gravação obsoleta");
  auto selection=persistence::json::parse(read(root/"selecao.json"));
  Edition e;e.site_id=s.id;e.title="Edição de leitura";e.slug="estacao-inde";e.work_id=selection.at("work").as_string();e.structure_id=selection.at("structure").as_string();
  for(const auto &item:selection.at("choices").as_array())for(const auto &d:item.at("documents").as_array())e.choices[item.at("node").as_string()].documents.push_back(d.as_string());
  for(const auto &id:selection.at("entities").as_array())e.entities.push_back(id.as_string());
  e=service.create_edition(e);check(e.public_numbers.size()==2&&e.next_number==3,"Reserva inicial de endereços");
  auto nodes=source.structural_nodes_for_work(e.work_id);std::erase_if(nodes,[&](const auto &n){return n.structure_id!=e.structure_id;});nodes=application::production_order(nodes);
  const auto first=e.choices.at(nodes.front().id).documents.front();check(e.public_numbers.at(first)==1,"Numeração não seguiu a ordem editorial");
  rejects([&]{service.remove_site(s.id,s.revision);},"Site em uso foi retirado");
  auto duplicate=e;duplicate.id.clear();rejects([&]{service.create_edition(duplicate);},"Slug em uso aceito");
  auto prepared=service.prepare(e.id,s.revision,e.revision);check(service.is_current(prepared),"Retrato recém-preparado obsoleto");
  check(prepared.payload.find("SEGREDO-PRIVADO-INDE-PUB01")==std::string::npos,"Retrato vazou Documento privado");
  check(service.preparations(e.id).size()==1,"Histórico de preparações não persistiu");
  auto tampered=prepared;tampered.payload+=" ";check(!service.is_current(tampered),"Retrato adulterado aceito");
  tampered.payload_hash=hash(tampered.payload);check(!service.is_current(tampered),"Retrato externo com hash próprio aceito");
  const auto material=service.materialize(prepared);check(read(material)==prepared.payload,"Materialização diferente do retrato");
  rejects([&]{service.materialize(prepared);},"Materialização sobrescreveu arquivo");
  std::ofstream exported(root/"edicao-para-gerar.json");exported<<prepared.payload;exported.close();
  auto before=e;e.choices.erase(nodes.front().id);e=service.update_edition(e,e.revision);check(!service.is_current(prepared),"Escolhas alteradas não invalidaram retrato");
  e.choices=before.choices;e=service.update_edition(e,e.revision);check(e.public_numbers==before.public_numbers&&e.next_number==3,"Endereços foram reciclados");
  rejects([&]{service.update_edition(before,before.revision);},"Edição aceitou revisão obsoleta");
  auto invalid=e;invalid.choices["inexistente"]={true,{first}};rejects([&]{service.update_edition(invalid,e.revision);},"Referência fora da estrutura aceita");
  e=service.remove_edition(e.id,e.revision);check(service.editions(s.id).empty(),"Retirada não ocultou Edição");
  rejects([&]{service.create_edition(duplicate);},"Endereço de Edição retirada foi reutilizado");
  s=service.remove_site(s.id,s.revision);check(service.sites().empty(),"Retirada não ocultou Site");
  rejects([&]{service.restore_edition(e.id,e.revision);},"Edição restaurada sob Site retirado");
  s=service.restore_site(s.id,s.revision);e=service.restore_edition(e.id,e.revision);check(e.id==before.id&&persistence::json::serialize(encode(e).at("choices"),false)==persistence::json::serialize(encode(before).at("choices"),false),"Restauração não preservou identidade/escolhas");
  check(hash(read(root/"Publicacao.inde/data/project.sqlite3"))==base_hash,"Operações de publicação alteraram o banco autoral");
  source.close();source.open(root/"Publicacao.inde");check(service.edition(e.id).revision==e.revision&&service.site(s.id).id==s.id,"Reabertura perdeu persistência");
  prepared=service.prepare(e.id,s.revision,e.revision);
  auto doc=*source.writing().document(e.choices.at(nodes.back().id).documents.front());doc.content+="\nUma mudança posterior.";static_cast<void>(source.writing().update_document(doc));
  check(!service.is_current(prepared),"Texto alterado não invalidou retrato");
  auto remote=s;remote.origin="https://exemplo.github.io";remote.repository="autoria/biblioteca";remote.destination="github";remote=service.update_site(remote,s.revision);
  const auto previous=source.current()->manifest().project_id;source.save_as(root/"Copia.inde");
  rejects([&]{service.sites();},"Cópia com nova identidade adotada implicitamente");
  service.adopt_project_copy(previous);const auto adopted=service.site(s.id);check(adopted.destination=="local"&&adopted.repository.empty()&&adopted.origin.empty(),"Cópia manteve destino remoto");
  check(!service.is_current(prepared),"Cópia manteve preparação atual");
  source.close();source.open(root/"Publicacao.inde");check(service.site(s.id).repository=="autoria/biblioteca","Adoção da cópia alterou origem");
  persistence::SqliteDatabase extension(service.database_path());check(extension.query_integer("SELECT count(DISTINCT kind) FROM history")==9,"Histórico de operações ausente");
  check(extension.query_text("PRAGMA integrity_check")=="ok","Integridade da extensão");
  e=service.remove_edition(e.id,e.revision);source.writing().delete_document(doc.id);
  e=service.restore_edition(e.id,e.revision);check(!e.removed&&e.choices.size()==2,"Referência ausente impediu recuperar configuração");
  rejects([&]{service.prepare(e.id,service.site(s.id).revision,e.revision);},"Edição com Documento ausente foi preparada");
  std::cout<<"PUB-02: CRUD, conflitos, restauração, reservas, proveniência, cópia, integridade e persistência OK\n";
  return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}

#include "literature_document.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <set>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <linux/fs.h>
using namespace inde;using namespace inde::publication;namespace fs=std::filesystem;
namespace {
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::string text(const V &v,const char *key){return v.at(key).as_string();}
std::string raw(const fs::path &p){require(fs::is_regular_file(p)&&fs::file_size(p)<=64*1024*1024,"Fonte ausente ou maior que 64 MiB");std::ifstream f(p,std::ios::binary);require(bool(f),"Fonte ausente");return {std::istreambuf_iterator<char>(f),{}};}
V read(const fs::path &p){return persistence::json::parse(raw(p));}
bool equal(const V &a,const V &b){return persistence::json::serialize(a,false)==persistence::json::serialize(b,false);}
void write(const fs::path &p,const V &v){std::ofstream f(p);f<<persistence::json::serialize(v,true);f.close();require(bool(f),"Falha ao registrar procedência");}
void verify(const V &entry){
 const auto path=fs::path(text(entry,"review"));const auto bytes=raw(path);require(hash(bytes)==text(entry,"review_sha256"),"A conferência do capítulo mudou");const auto review=persistence::json::parse(bytes);require(text(review,"protocol")=="inde.literature.review"&&review.at("version").as_integer()==1,"Conferência de capítulo incompatível");
 const auto choice=text(entry,"choice");require(choice=="html"||choice=="autoria","Escolha uma fonte por capítulo");const auto &selected=review.at("candidates").at(choice);
 require(selected.at("convertible").as_boolean()&&selected.at("issues").as_array().empty()&&equal(selected.at("content"),entry.at("content"))&&equal(review.at("sources"),entry.at("sources")),"Conteúdo ou diagnóstico do capítulo mudou");
 require(text(entry.at("content"),"chapter_id")==text(entry,"id"),"O ID do capítulo não corresponde à fonte");
 for(const auto &[kind,source]:entry.at("sources").as_object()){
  const auto copy=fs::weakly_canonical(path.parent_path()/text(source,"copy"));require(copy.parent_path()==fs::weakly_canonical(path.parent_path())&&!fs::is_symlink(path.parent_path()/text(source,"copy")),"Cópia fora da conferência");
  require(hash(raw(copy))==text(source,"sha256")&&hash(raw(text(source,"path")))==text(source,"sha256"),"Uma fonte de capítulo mudou");
 }
}
}
void import_literature_work(const fs::path &plan_path,const fs::path &target){
 require(!fs::exists(target),"O destino já existe; nada foi substituído");const auto plan=read(plan_path);require(text(plan,"protocol")=="inde.literature.work-import"&&plan.at("version").as_integer()==1,"Plano de Obra incompatível");
 const auto review_path=fs::path(text(plan,"review"));const auto bytes=raw(review_path);require(hash(bytes)==text(plan,"review_sha256"),"A conferência da Obra mudou");const auto review=persistence::json::parse(bytes);
 require(text(review,"protocol")=="inde.literature.work-review"&&review.at("version").as_integer()==1,"Conferência de Obra incompatível");
 const auto &origin=review.at("source");const auto original=fs::path(text(origin,"path"));const auto copy=fs::weakly_canonical(review_path.parent_path()/text(origin,"copy"));require(copy.parent_path()==fs::weakly_canonical(review_path.parent_path())&&!fs::is_symlink(review_path.parent_path()/text(origin,"copy")),"Manifesto fora da conferência");
 require(hash(raw(original))==text(origin,"sha256")&&hash(raw(copy))==text(origin,"sha256"),"O manifesto de origem mudou");const auto manifest=read(copy);
 require(equal(plan.at("work"),review.at("work"))&&equal(plan.at("site"),review.at("site"))&&equal(plan.at("work"),manifest.at("work"))&&equal(plan.at("site"),manifest.at("site")),"Metadados da Obra foram adulterados");
 const auto &chapters=plan.at("chapters").as_array();require(!chapters.empty()&&chapters.size()<=100&&chapters.size()==review.at("chapters").as_array().size()&&chapters.size()==manifest.at("chapters").as_array().size(),"Quantidade de capítulos incompatível");
 std::map<std::string,V> definitions;std::set<std::string> chapter_ids;std::set<std::int64_t> public_numbers;
 for(std::size_t i=0;i<chapters.size();++i){const auto &entry=chapters[i];const auto &expected=review.at("chapters").as_array()[i];const auto &source=manifest.at("chapters").as_array()[i];
  for(const auto *key:{"id","title","number"})require(equal(entry.at(key),expected.at(key))&&equal(entry.at(key),source.at(key)),"Ordem, número ou identidade do capítulo mudou");
  require(fs::weakly_canonical(text(entry,"review"))==fs::weakly_canonical(review_path.parent_path()/text(expected,"review")),"Conferência de capítulo trocada");
  require(chapter_ids.insert(text(entry,"id")).second&&public_numbers.insert(entry.at("number").as_integer()).second,"Capítulo ou número público repetido");verify(entry);
  for(const auto *kind:{"html","autoria"})require(fs::weakly_canonical(text(entry.at("sources").at(kind),"path"))==fs::weakly_canonical(original.parent_path()/text(source,kind)),"Arquivo de origem trocado");
  for(const auto &entity:entry.at("content").at("entities").as_array()){const auto id=text(entity,"id");if(definitions.contains(id))require(equal(definitions.at(id),entity),"Definições incompatíveis para um Personagem compartilhado");else definitions.emplace(id,entity);}
 }
 require(definitions.size()==plan.at("entities").as_array().size(),"Personagens do plano não correspondem às fontes");
 for(const auto &entity:plan.at("entities").as_array())require(definitions.contains(text(entity,"id"))&&equal(entity,definitions.at(text(entity,"id"))),"Personagem adulterado");
 fs::create_directories(target.parent_path());const auto stage=target.parent_path()/(".importacao-obra-"+project::new_uuid());require(fs::create_directory(stage),"Não foi possível reservar o destino");
 try{
  application::ProjectService service;service.create(stage/"Importado.inde",text(plan.at("work"),"title"));auto work=service.start_work(text(plan.at("work"),"title"),{},"Universo importado");work.synopsis=text(plan.at("work"),"synopsis");service.update_work(work);const auto structure=service.structures().active_editorial(work.id);require(bool(structure),"Estrutura ausente");
  const auto types=service.narrative().entity_types();const auto character=std::find_if(types.begin(),types.end(),[](const auto &t){return t.key=="character";});require(character!=types.end(),"Tipo Personagem ausente");
  A mappings,public_entities,public_documents,public_anchors,public_annotations,receipt_chapters;std::map<std::string,std::string> entities;
  for(const auto &e:plan.at("entities").as_array()){require(text(e,"type")=="character","Tipo de entidade sem tradutor");const auto entity=service.narrative().create_entity(character->id,text(e,"name"),"");static_cast<void>(service.narrative().add_entity_to_work(entity.id,work.id));require(entities.emplace(text(e,"id"),entity.id).second,"Personagem repetido");public_entities.emplace_back(O{{"native",entity.id},{"source",e.at("id")}});mappings.emplace_back(O{{"kind","character"},{"source",e.at("id")},{"native",entity.id}});}
  Edition edition;edition.title="Edição importada do Literature";edition.work_id=work.id;edition.structure_id=structure->id;edition.slug=text(plan.at("work"),"slug");for(const auto &[source,native]:entities)edition.entities.push_back(native);
  for(std::size_t index=0;index<chapters.size();++index){const auto &entry=chapters[index];const auto node=service.create_structural_node(work.id,{},project::StructuralNodeType::Chapter,text(entry,"title"));A local;auto doc=import_literature_document(service,node.id,text(entry,"title"),entry.at("content"),entities,local);edition.choices[node.id].documents.push_back(doc.id);
   public_documents.emplace_back(O{{"native",doc.id},{"source",entry.at("id")},{"number",entry.at("number")}});mappings.emplace_back(O{{"kind","chapter"},{"source",entry.at("id")},{"native",node.id}});
   for(const auto &mapping:local){auto m=mapping.as_object();m["document"]=doc.id;mappings.emplace_back(m);const auto kind=text(mapping,"kind");if(kind=="scene")public_anchors.emplace_back(O{{"document",doc.id},{"native",mapping.at("native")},{"source",mapping.at("source")}});else if(kind=="dialogue"||kind=="thought")public_annotations.emplace_back(O{{"document",doc.id},{"native",mapping.at("native")},{"source",mapping.at("source")}});}
   receipt_chapters.emplace_back(O{{"source",entry.at("id")},{"document",doc.id},{"node",node.id},{"choice",entry.at("choice")},{"sources",entry.at("sources")}});
  }
  service.save();Service publication(service);const auto &configuration=plan.at("site");Site site;site.name=text(configuration,"name");site.author=text(configuration,"author");site.description=text(configuration,"description");site.theme=text(configuration,"theme");site.origin=text(configuration,"origin");site.base_path=text(configuration,"base_path");site=publication.create_site(site);edition.site_id=site.id;edition=publication.create_edition(edition);
  const V identity=O{{"version",1},{"work",plan.at("work").at("id")},{"slug",plan.at("work").at("slug")},{"folder",plan.at("work").at("folder")},{"aliases",plan.at("work").at("aliases")},{"next_number",plan.at("work").at("next_number")},{"documents",public_documents},{"entities",public_entities},{"anchors",public_anchors},{"annotations",public_annotations}};
  edition=publication.bind_literature_identity(edition.id,edition.revision,identity);site=publication.update_catalog(site.id,site.revision,{edition.id});const auto project_id=service.current()->manifest().project_id;service.close();
  const auto data=stage/"Importado.inde/data";fs::create_directory(data/"importacao-fontes");fs::copy_file(copy,data/"importacao-fontes/manifesto-origem.json");
  for(std::size_t i=0;i<chapters.size();++i){const auto &entry=chapters[i];const auto directory=data/"importacao-fontes"/("capitulo-"+std::to_string(i+1));fs::create_directory(directory);for(const auto &[kind,source]:entry.at("sources").as_object()){const auto source_copy=fs::path(text(entry,"review")).parent_path()/text(source,"copy");fs::copy_file(source_copy,directory/text(source,"copy"));require(hash(raw(directory/text(source,"copy")))==text(source,"sha256"),"A cópia mudou durante a conversão");}verify(entry);}
  require(hash(raw(original))==text(origin,"sha256")&&hash(raw(data/"importacao-fontes/manifesto-origem.json"))==text(origin,"sha256"),"O manifesto mudou durante a conversão");
  write(data/"importacao-literature.json",O{{"protocol","inde.literature.receipt"},{"version",2},{"project_id",project_id},{"work_id",work.id},{"site_id",site.id},{"edition_id",edition.id},{"source",origin},{"chapters",receipt_chapters},{"identities",mappings},{"publication","bound-to-edition"}});
  require(syscall(SYS_renameat2,AT_FDCWD,stage.c_str(),AT_FDCWD,target.c_str(),RENAME_NOREPLACE)==0,"O destino apareceu durante a conversão; nada foi substituído");std::cout<<(target/"Importado.inde").string()<<'\n';
 }catch(...){std::error_code ignored;fs::remove_all(stage,ignored);throw;}
}

void prepare_literature_work(const fs::path &path){application::ProjectService source;source.open(path);const auto receipt=read(path/"data/importacao-literature.json");Service publication(source);const auto site=publication.site(text(receipt,"site_id"));const auto prepared=publication.prepare_catalog(site.id,site.revision);std::cout<<publication.materialize(prepared).string()<<'\n';}

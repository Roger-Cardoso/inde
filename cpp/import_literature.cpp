#include "publication_service.hpp"
#include "literature_document.hpp"
#include "inde/application/text_semantic_service.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <linux/fs.h>
using namespace inde;using namespace inde::publication;namespace fs=std::filesystem;
namespace {
std::string read(const fs::path &path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Fonte da importação ausente");return {std::istreambuf_iterator<char>(f),{}};}
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::string str(const V &v,const char *key){return v.at(key).as_string();}
void write(const fs::path &p,const V &v){std::ofstream f(p);f<<persistence::json::serialize(v,true);f.close();require(bool(f),"Falha ao guardar procedência da importação");}
}
project::Document import_literature_document(application::ProjectService &service,const std::string &node_id,const std::string &title,const V &content,const std::map<std::string,std::string> &entities,A &mappings){
    auto document=service.writing().create_document(title,node_id);document.content=str(content,"text");const auto size=project::utf8_character_count(document.content);
    for(const auto &f:content.at("formatting").as_array()){
      const auto style=str(f,"style");require(style=="bold"||style=="italic"||style=="underline","Formatação sem tradutor");document.formatting.push_back({style=="bold"?project::DocumentTextStyle::Bold:style=="italic"?project::DocumentTextStyle::Italic:project::DocumentTextStyle::Underline,static_cast<std::size_t>(f.at("start").as_integer()),static_cast<std::size_t>(f.at("end").as_integer()),{}});
    }
    for(const auto &scene:content.at("scenes").as_array()){
      const auto start=scene.at("start").as_integer(),end=scene.at("end").as_integer();require(start>=0&&end>start&&static_cast<std::size_t>(end)<=size,"Cena fora do texto");const auto id=project::new_uuid();document.anchors.push_back({id,"Cena importada",static_cast<std::size_t>(start),static_cast<std::size_t>(end)});mappings.emplace_back(O{{"kind","scene"},{"source",scene.at("id")},{"native",id}});
    }
    for(const auto &annotation:content.at("annotations").as_array()){
      const auto kind=str(annotation,"kind");require(kind=="dialogue"||kind=="thought","Marcação sem tradutor");const auto *type=project::find_annotation_type(project::builtin_annotation_types(),"inde.semantic."+kind,1);require(type,"Tipo semântico ausente");
      document=application::TextSemanticService::create(document,*type,annotation.at("start").as_integer(),annotation.at("end").as_integer(),{{kind=="dialogue"?"speaker":"thinker",{project::SemanticFieldKind::Entity,{entities.at(str(annotation,"target"))}}}});
      mappings.emplace_back(O{{"kind",kind},{"source",annotation.at("id")},{"native",document.semantics.annotations.back().id}});
    }
    if(!content.at("perspective").is_null()){
      const auto &p=content.at("perspective");document=application::TextSemanticService::create_document_perspective(document,{{"person",{project::SemanticFieldKind::Enum,{str(p,"person")}}},{"focalization",{project::SemanticFieldKind::Enum,{str(p,"focalization")}}},{"viewpoint",{project::SemanticFieldKind::Entity,{entities.at(str(p,"entity"))}}}});
    }
    document=service.writing().update_document(document);mappings.emplace_back(O{{"kind","document"},{"source",content.at("chapter_id")},{"native",document.id}});
    return document;
}
void import_literature(const fs::path &plan_path,const fs::path &target){
  require(!fs::exists(target),"A pasta de destino já existe; nada foi substituído");
  const auto plan=persistence::json::parse(read(plan_path));require(str(plan,"protocol")=="inde.literature.import"&&plan.at("version").as_integer()==1,"Plano de importação incompatível");
  const auto review_path=fs::path(str(plan,"review"));const auto raw_review=read(review_path);require(hash(raw_review)==str(plan,"review_sha256"),"A conferência mudou depois da escolha");const auto review=persistence::json::parse(raw_review);
  const auto choice=str(plan,"choice");require(choice=="html"||choice=="autoria","Escolha uma fonte explicitamente");
  const auto &selected=review.at("candidates").at(choice);require(selected.at("convertible").as_boolean()&&selected.at("issues").as_array().empty(),"A fonte tem recursos ainda sem tradutor");
  require(persistence::json::serialize(selected.at("content"),false)==persistence::json::serialize(plan.at("content"),false),"O conteúdo não corresponde à fonte conferida");
  require(persistence::json::serialize(review.at("sources"),false)==persistence::json::serialize(plan.at("sources"),false),"A procedência do plano mudou");
  std::map<std::string,fs::path> copies;
  for(const auto &[kind,source]:plan.at("sources").as_object()){
    const auto copy=fs::weakly_canonical(review_path.parent_path()/str(source,"copy"));require(copy.parent_path()==fs::weakly_canonical(review_path.parent_path())&&!fs::is_symlink(review_path.parent_path()/str(source,"copy")),"Cópia fora da conferência");
    require(hash(read(copy))==str(source,"sha256")&&hash(read(str(source,"path")))==str(source,"sha256"),"A fonte mudou desde a comparação");copies.emplace(kind,copy);
  }
  const auto title=str(plan,"title");require(!title.empty()&&title.size()<=2000,"Preencha o nome da Obra");
  fs::create_directories(target.parent_path());const auto stage=target.parent_path()/(".importacao-"+project::new_uuid());require(fs::create_directory(stage),"Não foi possível reservar a área de importação");
  try{
    application::ProjectService service;service.create(stage/"Importado.inde",title);const auto work=service.start_work(title,{},"Universo importado");const auto structure=service.structures().active_editorial(work.id);require(bool(structure),"Estrutura editorial ausente");
    const auto node=service.create_structural_node(work.id,{},project::StructuralNodeType::Chapter,title);
    const auto &content=plan.at("content");std::map<std::string,std::string> entities;A mappings;
    mappings.emplace_back(O{{"kind","chapter"},{"source",content.at("chapter_id")},{"native",node.id}});

    const auto types=service.narrative().entity_types();const auto character=std::find_if(types.begin(),types.end(),[](const auto &t){return t.key=="character";});require(character!=types.end(),"Tipo Personagem ausente");
    for(const auto &e:content.at("entities").as_array()){
      require(str(e,"type")=="character","Tipo de entidade sem tradutor");auto entity=service.narrative().create_entity(character->id,str(e,"name"),"");static_cast<void>(service.narrative().add_entity_to_work(entity.id,work.id));require(entities.emplace(str(e,"id"),entity.id).second,"Entidade repetida");mappings.emplace_back(O{{"kind","character"},{"source",e.at("id")},{"native",entity.id}});
    }
    static_cast<void>(import_literature_document(service,node.id,title,content,entities,mappings));service.save();const auto project_id=service.current()->manifest().project_id;service.close();
    const auto data=stage/"Importado.inde/data";fs::create_directory(data/"importacao-fontes");for(const auto &[kind,copy]:copies)fs::copy_file(copy,data/"importacao-fontes"/copy.filename());
    for(const auto &[kind,source]:plan.at("sources").as_object())require(hash(read(data/"importacao-fontes"/str(source,"copy")))==str(source,"sha256")&&hash(read(str(source,"path")))==str(source,"sha256"),"A fonte mudou durante a conversão");
    write(data/"importacao-literature.json",O{{"protocol","inde.literature.receipt"},{"version",1},{"project_id",project_id},{"work_id",work.id},{"choice",choice},{"review_id",review.at("id")},{"sources",plan.at("sources")},{"identities",mappings},{"publication","pending-public-identities"}});
    require(syscall(SYS_renameat2,AT_FDCWD,stage.c_str(),AT_FDCWD,target.c_str(),RENAME_NOREPLACE)==0,"O destino apareceu durante a conversão; nada foi sobrescrito");
    std::cout<<(target/"Importado.inde").string()<<'\n';
  }catch(...){std::error_code ignored;fs::remove_all(stage,ignored);throw;}
}

void inspect_import(const fs::path &path){
  application::ProjectService source;source.open(path);const auto receipt=persistence::json::parse(read(path/"data/importacao-literature.json"));const auto work_id=str(receipt,"work_id");
  const auto &works=source.catalog().works;const auto work=std::find_if(works.begin(),works.end(),[&](const auto &w){return w.id==work_id;});require(work!=works.end(),"Obra importada ausente");const auto structure=source.structures().active_editorial(work_id);require(bool(structure),"Estrutura ausente");
  application::ProductionChoices choices;for(const auto &d:source.writing().documents())if(d.editorial_node_id)choices[*d.editorial_node_id].documents.push_back(d.id);
  const auto production=application::prepare_work_production(source.current()->manifest().project_id,structure->id,*work,source.structural_nodes_for_work(work_id),source.writing().documents(),choices,{});
  auto value=snapshot(production,source.narrative().entities()).as_object();value["import_receipt"]=receipt;
  std::cout<<persistence::json::serialize(value,true)<<'\n';
}

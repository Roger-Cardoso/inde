#include "publication_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <glib.h>
using namespace inde;
using namespace inde::publication;
namespace fs=std::filesystem;
void demo(const fs::path &);
namespace {
V read_json(const fs::path &p){std::ifstream f(p);return persistence::json::parse(std::string{std::istreambuf_iterator<char>(f),{}});}
}
void demo_catalog(const fs::path &root){
  demo(root);application::ProjectService source;source.open(root/"Publicacao.inde");Service publication(source);
  Site site;site.name="Biblioteca de marés e estrelas";site.author="Autoria de demonstração";site.description="Duas Obras, o mesmo Projeto e escolhas explícitas de publicação.";site.base_path="/biblioteca";site=publication.create_site(site);
  const auto selection=read_json(root/"selecao.json");Edition first;first.site_id=site.id;first.title="Marés · edição de leitura";first.slug="estacao-inde";first.work_id=selection.at("work").as_string();first.structure_id=selection.at("structure").as_string();
  for(const auto &c:selection.at("choices").as_array())for(const auto &d:c.at("documents").as_array())first.choices[c.at("node").as_string()].documents.push_back(d.as_string());
  for(const auto &id:selection.at("entities").as_array())first.entities.push_back(id.as_string());
  first=publication.create_edition(first);
  const auto universe=source.catalog().works.front().intellectual_property_id;
  auto create=[&](const std::string &title,const std::string &slug,const std::string &text,bool dialogue){
    auto work=source.start_work(title,universe,"");work.synopsis=dialogue?"Mara visita um observatório onde as estrelas contam o tempo.":"Obra fictícia mantida fora do catálogo para conferir a seleção.";source.update_work(work);
    const auto structure=source.structures().active_editorial(work.id);if(!structure)throw std::runtime_error("Estrutura da amostra ausente");
    const auto node=source.create_structural_node(work.id,{},project::StructuralNodeType::Chapter,dialogue?"A lente":"Anotações reservadas");
    auto document=source.writing().create_document(node.title,node.id);document.content=text;document.anchors.push_back({project::new_uuid(),"Cena do capítulo",0,project::utf8_character_count(text)});
    if(dialogue){
      static_cast<void>(source.narrative().add_entity_to_work(first.entities.front(),work.id));
      const std::string speech="— O céu também tem marés — disse Mara.";const auto start=text.find(speech);if(start==std::string::npos)throw std::runtime_error("Fala da amostra ausente");
      const auto offset=static_cast<std::size_t>(g_utf8_strlen(text.data(),start));
      document=application::TextSemanticService::create(document,*project::find_annotation_type(project::builtin_annotation_types(),"inde.semantic.dialogue",1),offset,offset+project::utf8_character_count(speech),{{"speaker",{project::SemanticFieldKind::Entity,{first.entities.front()}}}});
    }
    document=source.writing().update_document(document);
    Edition edition;edition.site_id=site.id;edition.title=dialogue?"Estrelas · edição de leitura":"Arquivo · fora do catálogo";edition.slug=slug;edition.work_id=work.id;edition.structure_id=structure->id;edition.choices[node.id]={true,{document.id}};
    if(dialogue)edition.entities=first.entities;
    return publication.create_edition(edition);
  };
  const auto second=create("O observatório das estrelas","observatorio-das-estrelas","Mara encontrou a lente apontada para o mar.\n\n— O céu também tem marés — disse Mara.",true);
  const auto excluded=create("Arquivo de bordo","arquivo-de-bordo","SEGREDO-TERCEIRA-OBRA-PUB02B",false);
  source.save();site=publication.update_catalog(site.id,site.revision,{first.id,second.id});
  const auto prepared=publication.prepare_catalog(site.id,site.revision);
  std::ofstream output(root/"catalogo-preparado.json");output<<prepared.payload;output.close();if(!output)throw std::runtime_error("Falha ao gravar catálogo demonstrativo");
  std::ofstream ids(root/"catalogo-selecao.json");ids<<persistence::json::serialize(O{{"site",site.id},{"included",strings({first.id,second.id})},{"excluded",excluded.id}},true);ids.close();
  source.close();std::cout<<(root/"catalogo-preparado.json").string()<<'\n';
}

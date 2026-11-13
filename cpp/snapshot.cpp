#include "snapshot_model.hpp"
// PUB-01: snapshot from the accepted INDE services and WorkProduction.
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/application/work_production.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include <glib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <tuple>
using namespace inde;
using V = persistence::json::Value;
using O = V::Object;
using A = V::Array;
namespace fs = std::filesystem;

using inde::publication::snapshot;
using inde::publication::strings;
std::pair<std::size_t, std::size_t> range(const std::string &text, const std::string &needle) {
  const auto position = text.find(needle);
  if (position == std::string::npos) throw std::runtime_error("Trecho da demonstração não encontrado");
  const auto start = static_cast<std::size_t>(g_utf8_strlen(text.data(), position));
  return {start, start + static_cast<std::size_t>(g_utf8_strlen(needle.data(), needle.size()))};
}
void demo(const fs::path &output) {
  if (fs::exists(output) || !fs::create_directory(output)) throw std::runtime_error("Use uma pasta nova para a demo nativa");
  const auto profile = output / "perfil-local";
  fs::create_directory(profile);
  if (setenv("XDG_CONFIG_HOME", profile.c_str(), 1) != 0) throw std::runtime_error("Não foi possível isolar o perfil da demo");
  application::ProjectService service;
  service.create(output / "Publicacao.inde", "Demonstração INDE Publicação");
  auto work = service.start_work("A estação das marés — núcleo INDE", "", "Universo de demonstração");
  work.synopsis = "Obra sintética criada e salva pelos serviços C++ do INDE, para conferir a publicação web.";
  service.update_work(work);
  auto structure = service.structures().active_editorial(work.id);
  if (!structure) {
    const auto created = service.structures().create_editorial(work.id, "Edição demonstrativa");
    service.structures().activate_editorial(created.id);
    structure = created;
  }
  const auto types = service.narrative().entity_types();
  const auto type = std::find_if(types.begin(), types.end(), [](const auto &t) {return t.key == "character";});
  if (type == types.end()) throw std::runtime_error("Tipo Personagem não encontrado");
  auto mara = service.narrative().create_entity(type->id, "Mara", "Viajante da estação.");
  static_cast<void>(service.narrative().add_entity_to_work(mara.id, work.id));
  application::ProductionChoices choices;
  const std::vector<std::pair<std::string,std::string>> contents{
    {"O farol", "Mara chegou à estação antes do amanhecer.\n\n— Ainda há tempo — disse ela.\n\nTalvez alguém estivesse esperando."},
    {"A porta", "A porta se abriu. Mara atravessou o limiar.\n\nA viagem começava ali."}};
  int number = 0;
  for (const auto &[title, text] : contents) {
    const auto node = service.create_structural_node(work.id, {}, project::StructuralNodeType::Chapter, title);
    auto document = service.writing().create_document(title, node.id);
    document.content = text;
    document.anchors.push_back({project::new_uuid(), "Cena do capítulo", 0, project::utf8_character_count(text)});
    if (++number == 1) {
      const auto &all = project::builtin_annotation_types();
      for (const auto &[key, needle, field] : std::vector<std::tuple<std::string,std::string,std::string>>{
        {"inde.semantic.dialogue", "— Ainda há tempo — disse ela.", "speaker"},
        {"inde.semantic.thought", "Talvez alguém estivesse esperando.", "thinker"}}) {
        const auto [start, end] = range(text, needle);
        document = application::TextSemanticService::create(document, *project::find_annotation_type(all,key,1), start,end,
            {{field, {project::SemanticFieldKind::Entity,{mara.id}}}});
      }
      document = application::TextSemanticService::create_document_perspective(document,
        {{"person", {project::SemanticFieldKind::Enum,{"third_person"}}},
         {"focalization", {project::SemanticFieldKind::Enum,{"internal"}}},
         {"viewpoint", {project::SemanticFieldKind::Entity,{mara.id}}}});
    }
    document = service.writing().update_document(document);
    choices[node.id] = {true,{document.id}};
  }
  auto private_document = service.writing().create_document("Referência privada — excluída da edição");
  private_document.content = "SEGREDO-PRIVADO-INDE-PUB01";
  static_cast<void>(service.writing().update_document(private_document));
  service.save();
  application::ProductionOptions options;
  options.profile = application::ProductionProfile::Reading;
  options.author = "Autoria de demonstração INDE";
  const auto production = application::prepare_work_production(service.current()->manifest().project_id,
    structure->id,work,service.structural_nodes_for_work(work.id),service.writing().documents(),choices,options);
  const auto document = snapshot(production,{mara});
  A selected;
  for (const auto &[node,choice] : choices)
    selected.emplace_back(O{{"node",node},{"documents",strings(choice.documents)}});
  std::ofstream selection(output / "selecao.json");
  selection << persistence::json::serialize(O{{"protocol","inde.web.demo-selection"},{"version",1},
    {"project_id",service.current()->manifest().project_id},{"work",work.id},{"structure",structure->id},
    {"choices",selected},{"entities",A{mara.id}}},true);
  selection.close();
  if (!selection) throw std::runtime_error("Falha ao preservar a seleção explícita da demonstração");
  service.close();
  std::ofstream stream(output / "retrato.json", std::ios::binary);
  stream << persistence::json::serialize(document,true) << '\n';
  stream.close();
  if (!stream) throw std::runtime_error("Falha ao gravar o retrato nativo");
  std::cout << (output / "retrato.json").string() << '\n';
}
void refresh_copy(const fs::path &root) {
  if (!fs::is_regular_file(root / ".copia-para-projecao"))
    throw std::runtime_error("A atualização aceita somente uma cópia marcada pelo laboratório");
  std::ifstream file(root / "selecao.json");
  const std::string raw((std::istreambuf_iterator<char>(file)),{});
  const auto selection = persistence::json::parse(raw);
  if (selection.at("protocol").as_string() != "inde.web.demo-selection" || selection.at("version").as_integer() != 1)
    throw std::runtime_error("Seleção de demonstração incompatível");
  const auto profile = root / "perfil-local";
  if (setenv("XDG_CONFIG_HOME",profile.c_str(),1) != 0) throw std::runtime_error("Falha ao isolar o perfil");
  application::ProjectService service;
  service.open(root / "Publicacao.inde");
  if (service.current()->manifest().project_id != selection.at("project_id").as_string())
    throw std::runtime_error("Identidade do Projeto diverge da seleção");
  const auto &works=service.catalog().works;
  const auto work=std::find_if(works.begin(),works.end(),[&](const auto &w){return w.id==selection.at("work").as_string();});
  if (work==works.end()) throw std::runtime_error("Obra da seleção não encontrada");
  application::ProductionChoices choices;
  for (const auto &item:selection.at("choices").as_array()) {
    application::ProductionChoice choice;
    for (const auto &id:item.at("documents").as_array()) choice.documents.push_back(id.as_string());
    choices.emplace(item.at("node").as_string(),choice);
  }
  std::vector<project::NarrativeEntity> entities;
  for (const auto &id:selection.at("entities").as_array()) {
    const auto entity=service.narrative().entity(id.as_string());
    if(!entity)throw std::runtime_error("Entidade selecionada não encontrada");
    const auto types=service.narrative().entity_types();
    const auto type=std::find_if(types.begin(),types.end(),[&](const auto &t){return t.id==entity->entity_type_id;});
    if(type==types.end() || type->key!="character")throw std::runtime_error("O tipo da entidade selecionada exige uma adaptação explícita");
    entities.push_back(*entity);
  }
  application::ProductionOptions options; options.profile=application::ProductionProfile::Reading;
  const auto production=application::prepare_work_production(service.current()->manifest().project_id,
    selection.at("structure").as_string(),*work,service.structural_nodes_for_work(work->id),service.writing().documents(),choices,options);
  const auto result=snapshot(production,entities);
  service.close();
  std::ofstream output(root / "retrato.json",std::ios::binary|std::ios::trunc);
  output<<persistence::json::serialize(result,true)<<'\n';output.close();
  if(!output)throw std::runtime_error("Falha ao gravar atualização do retrato");
  std::cout<<(root / "retrato.json").string()<<'\n';
}
void demo_catalog(const fs::path &);
void import_literature(const fs::path &,const fs::path &);
void inspect_import(const fs::path &);
void import_literature_work(const fs::path &,const fs::path &);
void prepare_literature_work(const fs::path &);
int main(int argc, char **argv) {
  try {
    if(argc==4 && std::string(argv[1])=="--importar-obra-literature"){import_literature_work(fs::absolute(argv[2]),fs::absolute(argv[3]));return 0;}
    if(argc==4 && std::string(argv[1])=="--importar-literature"){import_literature(fs::absolute(argv[2]),fs::absolute(argv[3]));return 0;}
    if (argc != 3) throw std::runtime_error("Uso: inde_web_bridge --demo PASTA_NOVA | --snapshot-copia COPIA_MARCADA");
    if(std::string(argv[1])=="--demo")demo(fs::absolute(argv[2]));
    else if(std::string(argv[1])=="--preparar-obra-importada")prepare_literature_work(fs::absolute(argv[2]));
    else if(std::string(argv[1])=="--inspecionar-importacao")inspect_import(fs::absolute(argv[2]));
    else if(std::string(argv[1])=="--demo-catalogo")demo_catalog(fs::absolute(argv[2]));
    else if(std::string(argv[1])=="--snapshot-copia")refresh_copy(fs::absolute(argv[2]));
    else throw std::runtime_error("Operação não suportada");
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n'; return 1;
  }
}

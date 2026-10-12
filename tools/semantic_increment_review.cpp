// Explicit synthetic examples for independent human-review copies only.
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-synthetic-examples") return 2;
  try {
    inde::application::ProjectService service; service.open(argv[1]);
    const auto types=service.writing().semantic_types();
    const auto &perspective=*inde::project::find_annotation_type(types,"inde.semantic.perspective",1);
    const auto &marked=*inde::project::find_annotation_type(types,"inde.semantic.marked-passage",1);
    using S=inde::application::TextSemanticService; using K=inde::project::SemanticFieldKind;
    auto target=service.writing().create_document("Semântica 2 — alvo da retomada (exemplo sintético)");
    target.content="Na pedra, uma marca antiga aguardava a chuva.";
    target=S::create(target,marked,0,43,{{"category",{K::Enum,{"clue"}}},
                                      {"note",{K::Text,{"Pista de demonstração; não interpreta texto real."}}}});
    target.semantics.annotations.back().origin="imported";
    target.semantics.annotations.back().source="synthetic-fixture/semantic-increment-2-2026-10-03";
    target=service.writing().update_document(target);
    auto source=service.writing().create_document("Semântica 2 — perspectiva e relações (leia primeiro)");
    source.content="A marca parecia familiar. Talvez a chuva revelasse sua origem.";
    source.perspective="Observação livre do autor — preservada, não convertida em perspectiva tipada.";
    source=S::create(source,perspective,0,24,{{"person",{K::Enum,{"third_person"}}},
                                          {"focalization",{K::Enum,{"internal"}}},
                                          {"note",{K::Text,{"Exemplo situado. Focalizador ainda não declarado: revisão opcional, sem inferência."}}}});
    source.semantics.annotations.back().origin="imported";
    source.semantics.annotations.back().source="synthetic-fixture/semantic-increment-2-2026-10-03";
    source=S::relate(source,source.semantics.annotations.back().id,
                     target.semantics.annotations.back().id,"inde.semantic.callback");
    source.semantics.relations.back().origin="imported";
    source.semantics.relations.back().notes="Retomada sintética entre Documentos, para testar navegação e proteção do alvo.";
    source=service.writing().update_document(source);
    std::cout<<"SYNTHETIC_EXAMPLES source="<<source.id<<" target="<<target.id<<"\n";
    service.close(); return 0;
  } catch(const std::exception &e) {std::cerr<<e.what()<<"\n";return 1;}
}

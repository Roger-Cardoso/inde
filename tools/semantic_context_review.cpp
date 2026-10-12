#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include <iostream>
using namespace inde;
int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-synthetic-examples")
    return 2;
  try {
    application::ProjectService service;
    service.open(argv[1]);
    const std::string title =
        "Editor — perspectiva global e tempo ficcional (exemplo sintético)";
    persistence::DocumentQuery q;
    q.limit = 200;
    for (q.offset = 0;; q.offset += q.limit) {
      auto page = service.writing().documents(q);
      for (const auto &d : page)
        if (d.title == title)
          throw std::runtime_error(
              "Exemplo já existe; nenhuma duplicação autorizada");
      if (page.size() < q.limit)
        break;
    }
    for (const auto &a : service.planning().time_axes())
      if (a.name == "Exemplo sintético — tempo semântico")
        throw std::runtime_error(
            "Eixo de exemplo já existe; nada foi alterado");
    auto types = service.writing().semantic_types();
    auto perspective =
        *project::find_annotation_type(types, "inde.semantic.perspective", 1);
    auto narration =
        *project::find_annotation_type(types, "inde.semantic.narration", 1);
    auto d = service.writing().create_document(title);
    d.content = "Um olhar autoral percorre a história.\nO começo e o fim aqui "
                "são referências explícitas do Planejamento.\nÁgua, palavras e "
                "tonalidades: a cor deste texto é autoral.\n";
    d.perspective = "Descrição livre legada — permanece independente da "
                    "perspectiva tipada.";
    d.formatting = {{project::DocumentTextStyle::Heading2, 0, 35},
                    {project::DocumentTextStyle::Foreground, 0, 35, "#88aaff"}};
    using K = project::SemanticFieldKind;
    using S = application::TextSemanticService;
    d = S::create_document_perspective(
        d,
        {{"person", {K::Enum, {"third_person"}}},
         {"focalization", {K::Enum, {"external"}}},
         {"note",
          {K::Text,
           {"Declaração global feita pelo autor, não extraída do texto."}}}});
    const auto global = d.semantics.annotations.back().id;
    d = S::create(
        d, perspective, 0, 35,
        {{"person", {K::Enum, {"first_person"}}},
         {"focalization", {K::Enum, {"variable"}}},
         {"note",
          {K::Text,
           {"Contexto local; sobreposições permanecem revisáveis."}}}});
    d = S::create(d, narration, 36, 101,
                  {{"kind", {K::Enum, {"transition"}}}});
    const auto local = d.semantics.annotations.back().id;
    auto axis = service.planning().create_time_axis(
        "Exemplo sintético — tempo semântico",
        "Pontos de ordem, sem data, duração ou calendário implícito.");
    auto start =
        service.planning().create_time_point(axis.id, 10, "Início sintético");
    auto middle =
        service.planning().create_time_point(axis.id, 15, "Meio sintético");
    auto end =
        service.planning().create_time_point(axis.id, 20, "Fim sintético");
    d = S::bind_fictional_time(
        d, global, axis.id, start.id, {},
        "Contexto pontual global — exemplo, não fato autoral.");
    d = S::bind_fictional_time(
        d, local, axis.id, start.id, end.id,
        "Período fechado de contexto textual; não cria acontecimento.");
    d = service.writing().update_document(d);
    std::cout << "SYNTHETIC_DOCUMENT " << d.id << "\nSYNTHETIC_AXIS " << axis.id
              << "\nSYNTHETIC_POINTS " << start.id << " " << middle.id << " "
              << end.id
              << "\nPASS: additive explicit synthetic semantic context; no "
                 "prior Document edited\n";
    service.close();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}

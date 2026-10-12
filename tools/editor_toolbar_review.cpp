// Only additive, explicitly synthetic examples in independent review copies.
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 3 || std::string(argv[2]) != "--allow-synthetic-examples")
    return 2;
  try {
    inde::application::ProjectService service;
    service.open(argv[1]);
    const std::string title =
        "Editor — três fileiras e comandos inline (exemplo sintético)";
    inde::persistence::DocumentQuery q;
    q.search = title;
    q.limit = 50;
    for (const auto &s : service.writing().document_summaries(q))
      if (s.title == title)
        throw std::runtime_error(
            "O exemplo já existe; não sobrescrever revisão humana.");
    auto d = service.writing().create_document(title);
    using S = inde::project::DocumentTextStyle;
    const auto add = [&](const std::string &text, S style) {
      const auto start = inde::project::utf8_character_count(d.content);
      d.content += text;
      const auto end = inde::project::utf8_character_count(d.content);
      d.formatting.push_back({style, start, end});
      d.content += '\n';
    };
    add("Título 1 — demonstração editorial", S::Heading);
    add("Título 2", S::Heading2);
    add("Título 3", S::Heading3);
    add("Título 4", S::Heading4);
    add("Título 5", S::Heading5);
    add("Título 6", S::Heading6);
    add("Subtítulo independente do Título 2", S::Subheading);
    add("Citação: exemplo sintético, sem conteúdo de obra real.", S::Quote);
    const auto start = inde::project::utf8_character_count(d.content);
    d.content += "Selecione este parágrafo para testar fonte, cor, tamanho, "
                 "alinhamento e Comentar.\n";
    const auto end = inde::project::utf8_character_count(d.content) - 1;
    d.formatting.push_back({S::FontFamily, start, end, "Serif"});
    d.formatting.push_back({S::FontSize, start, end, "12"});
    d.formatting.push_back({S::Foreground, start, start + 9, "#8dbbff"});
    d.formatting.push_back({S::AlignLeft, start, end});
    d.content += "Em Corpo, digite @nota ou @cena e escolha a sugestão. O "
                 "botão ⌘ gerencia os modelos.\n"
                 "A escolha insere texto literal em uma operação reversível; "
                 "nunca executa scripts.\n"
                 "Todos estes dados são exemplos aditivos apenas nesta cópia "
                 "de revisão.";
    d.inline_commands = {
        {"@", "nota", "Nota editorial", "Nota editorial: "},
        {"@", "cena", "Roteiro de cena", "Objetivo: \nConflito: \nMudança: "}};
    auto types = service.writing().semantic_types();
    const auto *comment =
        inde::project::find_annotation_type(types, "inde.editor.comment", 1);
    if (!comment)
      throw std::runtime_error("Tipo de comentário ausente");
    using K = inde::project::SemanticFieldKind;
    d = inde::application::TextSemanticService::create(
        d, *comment, start, end,
        {{"note",
          {K::Text,
           {"Nota sintética ancorada. Não faz parte do corpo nem estabelece "
            "verdade ficcional."}}},
         {"state", {K::Enum, {"open"}}}});
    d.semantics.annotations.back().origin = "imported";
    d.semantics.annotations.back().source =
        "synthetic-fixture/editor-toolbar-2026-10-03";
    d = service.writing().update_document(d);
    std::cout << "SYNTHETIC_TOOLBAR_EXAMPLE document=" << d.id << "\n";
    service.close();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}

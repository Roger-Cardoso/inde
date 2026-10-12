#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"
#include "inde/project/writing.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "demo_builder.hpp"

namespace inde::demo {

void DemoBuilder::create_catalog() {
    const auto covers = output_ / "assets" / "covers";
    std::filesystem::create_directories(covers);
    {
      std::ofstream image(covers / "atlas-vespera.svg");
      image
          << R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="900" height="1200" viewBox="0 0 900 1200"><defs><linearGradient id="g" x2="1" y2="1"><stop stop-color="#15304f"/><stop offset="1" stop-color="#5a315f"/></linearGradient></defs><rect width="900" height="1200" fill="url(#g)"/><circle cx="640" cy="250" r="155" fill="#d8e8ff" opacity=".82"/><path d="M0 760 Q180 620 360 760 T720 760 T1080 760 V1200 H0Z" fill="#4d9bb7" opacity=".7"/><path d="M0 890 Q210 730 420 890 T840 890 T1260 890 V1200 H0Z" fill="#101c30"/><g fill="none" stroke="#e6d8ae" stroke-width="13" opacity=".85"><path d="M180 760V270h230v490"/><path d="M245 270l50-120 50 120"/><path d="M210 420h170M210 560h170"/></g><text x="70" y="1050" fill="#f5ead0" font-family="serif" font-size="72" font-weight="700">ATLAS DE VÉSPERA</text><text x="74" y="1120" fill="#bdd5e3" font-family="sans-serif" font-size="32">memória, maré e futuros</text></svg>)svg";
    }
    {
      std::ofstream image(covers / "cadernos-meridiano.svg");
      image
          << R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="900" height="1200" viewBox="0 0 900 1200"><rect width="900" height="1200" fill="#172b2b"/><path d="M90 100h720v1000H90z" fill="#d7c99d"/><path d="M145 170h610v860H145z" fill="#243d3b"/><g stroke="#d7c99d" fill="none"><circle cx="450" cy="500" r="210" stroke-width="8"/><path d="M450 220v560M170 500h560M255 305l390 390M645 305L255 695" stroke-width="5"/></g><circle cx="450" cy="500" r="42" fill="#d7c99d"/><text x="190" y="900" fill="#efe6c7" font-family="serif" font-size="64" font-weight="700">CADERNOS</text><text x="205" y="970" fill="#b9d1ca" font-family="sans-serif" font-size="39">DO MERIDIANO</text></svg>)svg";
    }
    const auto saga = service_.create_intellectual_property(
        "Atlas de Véspera", "Uma saga sobre memória, clima e poder",
        "Propriedade intelectual de fantasia científica marítima ambientada em "
        "um arquipélago que prevê o futuro pelas marés. O conjunto investiga "
        "quem controla a memória pública, quanto custa impedir uma catástrofe "
        "e "
        "como uma cidade aprende a escolher futuros que não cabem nos mapas.",
        "assets/covers/atlas-vespera.svg");
    ips_["saga"] = saga.id;

    const auto ensaios = service_.create_intellectual_property(
        "Cadernos do Meridiano", "Documentos de campo do mundo de Véspera",
        "Linha editorial complementar que organiza geografia, ciência, cultura "
        "material e versões contraditórias do arquipélago sem transformar o "
        "dossiê em verdade absoluta do narrador.",
        "assets/covers/cadernos-meridiano.svg");
    ips_["cadernos"] = ensaios.id;

    const auto romance = service_.create_work(
        saga.id, "A Cidade de Vidro e Sal", "Livro I de Atlas de Véspera",
        "Quando as marés começam a devolver lembranças de dias que ainda não "
        "aconteceram, a restauradora de mapas Lia Avelar encontra o registro "
        "de "
        "uma inundação apagada dos arquivos oficiais. Para salvar os bairros "
        "baixos, ela precisa reunir uma tripulação improvável e decidir se o "
        "futuro deve ser revelado, negociado ou destruído.",
        "pt-BR", "Revisão estrutural");
    works_["romance"] = romance.id;

    const auto novela = service_.create_work(
        saga.id, "Cartas do Farol Submerso", "Novela de arquivo",
        "Dezoito anos antes do romance principal, a faroleira Ada Avelar "
        "escreve "
        "cartas para uma filha que talvez nunca as leia enquanto acompanha o "
        "primeiro despertar da máquina de marés.",
        "pt-BR", "Segundo rascunho");
    works_["novela"] = novela.id;

    const auto contos = service_.create_work(
        saga.id, "Mapas para Depois da Tempestade", "Seis histórias de Véspera",
        "Coletânea que acompanha habitantes comuns antes, durante e depois da "
        "crise: uma mensageira, um jardineiro de sal, duas contrabandistas, "
        "uma "
        "criança que ouve boias e o último tipógrafo da cidade.",
        "pt-BR", "Planejamento avançado");
    works_["contos"] = contos.id;

    const auto dossie = service_.create_work(
        ensaios.id, "Bestiário das Correntes Invisíveis",
        "Dossiê editorial e atlas de continuidade",
        "Documento de referência sobre lugares, instituições, tecnologias, "
        "símbolos e disputas de cânone. Cada verbete registra também dúvidas e "
        "pontos que a narrativa deve manter deliberadamente ambíguos.",
        "pt-BR", "Em atualização");
    works_["dossie"] = dossie.id;
  }

void DemoBuilder::create_structures() {
    create_main_structure();
    create_novella_structure();
    create_story_structure();
    create_dossier_structure();
  }

void DemoBuilder::create_main_structure() {
    const auto root = add_node(
        "romance-root", "romance", std::nullopt, StructuralNodeType::Volume,
        "Volume I — A Cidade de Vidro e Sal", "A maré que recorda o amanhã",
        "Estrutura-mãe do romance: descoberta, aliança, ruptura pública e "
        "escolha "
        "coletiva. A progressão externa acompanha a subida das águas; a "
        "interna, "
        "a passagem de Lia do controle solitário à confiança distribuída.",
        "Revisão estrutural");

    const auto prologue = add_node(
        "romance-prologo", "romance", root, StructuralNodeType::Custom,
        "Prólogo — A carta que chegou cedo demais", "Dezoito anos antes",
        "Ada sela numa garrafa o mapa de uma inundação futura e apaga o "
        "próprio "
        "nome do registro do farol.",
        "Revisado", "Prólogo");
    add_node(
        "romance-prologo-a", "romance", prologue, StructuralNodeType::Scene,
        "A luz sob a água", "Ponto de vista: Ada",
        "O farol acende abaixo da linha da maré e projeta ruas que ainda não "
        "foram construídas.",
        "Revisado");
    add_node("romance-prologo-b", "romance", prologue,
             StructuralNodeType::Scene, "O nome raspado",
             "Decisão irreversível",
             "Ada entrega a carta ao mar e remove sua assinatura do livro de "
             "vigília.",
             "Revisado");

    const std::array<const char *, 4> part_titles{
        "Parte I — A maré impossível", "Parte II — O arquivo das águas",
        "Parte III — A guerra dos mapas", "Parte IV — Uma cidade por escolher"};
    const std::array<const char *, 4> part_synopses{
        "O presságio aparece, é desacreditado e força Lia a sair do arquivo.",
        "A equipe descobre que a previsão é também uma máquina política.",
        "Versões rivais do futuro dividem aliados e tornam o segredo público.",
        "A cidade escolhe entre repetir o protocolo ou aceitar um futuro "
        "aberto."};
    const std::array<ChapterSpec, 20> chapters{{
        {"1. Sal no papel",
         "Lia restaura um mapa que amanhece com ruas inundadas.",
         "A mancha azul", "A data de amanhã"},
        {"2. A cidade suspensa",
         "Uma falha nos elevadores de maré isola os bairros baixos.",
         "Cabos em silêncio", "A travessia pelas pontes"},
        {"3. O mensageiro sem sombra",
         "Nael entrega a Lia uma chave deixada por Ada.", "Entrega na chuva",
         "A chave de nácar"},
        {"4. Conselho de pedra",
         "Maíra nega a previsão e ordena o recolhimento dos mapas.",
         "Audiência fechada", "O decreto sem assinatura"},
        {"5. O primeiro desvio",
         "Lia escolhe salvar cópias e foge com Iara pelo mercado flutuante.",
         "Fogo no catálogo", "Barco entre telhados"},
        {"6. Arquivo das Águas",
         "A equipe encontra registros mantidos em tanques de memória salina.",
         "A porta pneumática", "Vozes no reservatório"},
        {"7. O protocolo Nácar",
         "Tomé revela o mecanismo que troca lembranças por previsões.",
         "Manual para esquecer", "O preço da certeza"},
        {"8. Cartografia de ausências",
         "Lia percebe lacunas artificiais na história de sua família.",
         "Retratos sem rosto", "A margem arrancada"},
        {"9. Ilha do Sino", "Caio reativa uma estação meteorológica proibida.",
         "A torre inclinada", "Sete badaladas"},
        {"10. A tempestade ensaiada",
         "Um teste confirma a inundação, mas também mostra uma rota de fuga.",
         "Modelo de vidro", "A rua que sobrevive"},
        {"11. Dois futuros",
         "O Conselho e a Cooperativa publicam mapas incompatíveis.",
         "A praça dividida", "Tinta contra tinta"},
        {"12. Motim das comportas",
         "Bento conduz trabalhadores contra o fechamento dos canais.",
         "Correntes no cais", "A comporta três"},
        {"13. A memória de Ada",
         "Uma gravação explica por que Ada sabotou o primeiro ciclo.",
         "A voz no cobre", "Herança ou aviso"},
        {"14. O mapa vendido",
         "Safira negocia uma cópia com os Navegantes para ganhar tempo.",
         "Mesa de marfim", "A terceira condição"},
        {"15. Queda do observatório",
         "Davi destrói a lente central e torna toda previsão incompleta.",
         "Cúpula em cerco", "Chuva de cristal"},
        {"16. O dia sem maré",
         "As águas param e a cidade interpreta o silêncio como milagre.",
         "Peixes no ar", "A hora suspensa"},
        {"17. Evacuação do Bairro Baixo",
         "Lia coordena rotas que dependem de confiança, não de certeza.",
         "Lista de portas", "A ponte de cordas"},
        {"18. Câmara das Correntes",
         "Os grupos confrontam a máquina e suas memórias sacrificadas.",
         "O coração hidráulico", "Quem paga o amanhã"},
        {"19. A última previsão",
         "Lia recebe um futuro perfeito que exige apagar a revolta.",
         "Véspera intacta", "A recusa"},
        {"20. Atlas aberto",
         "A população desmonta o monopólio e transforma mapas em assembleias.",
         "Mil mãos no papel", "A maré comum"},
    }};

    for (std::size_t part = 0; part < part_titles.size(); ++part) {
      const auto part_id =
          add_node("romance-part-" + std::to_string(part + 1), "romance", root,
                   StructuralNodeType::Part, part_titles[part],
                   "Movimento " + std::to_string(part + 1), part_synopses[part],
                   part < 2 ? "Revisão estrutural" : "Planejamento avançado");
      const auto act_id = add_node("romance-act-" + std::to_string(part + 1),
                                   "romance", part_id, StructuralNodeType::Act,
                                   "Ato " + std::to_string(part + 1),
                                   "Escalada dramática", part_synopses[part]);
      for (std::size_t arc = 0; arc < 2; ++arc) {
        const auto arc_id = add_node(
            "romance-arc-" + std::to_string(part * 2 + arc + 1), "romance",
            act_id, StructuralNodeType::Arc,
            arc == 0 ? "Arco de abertura" : "Arco de reversão",
            "Unidade de cinco capítulos",
            arc == 0 ? "Planta as forças e o custo da escolha desta parte."
                     : "Converte as descobertas em consequência e mudança.");
        const auto first = part * 5 + arc * 3;
        const auto count = arc == 0 ? 3U : 2U;
        for (std::size_t local = 0; local < count; ++local) {
          const auto index = first + local;
          const auto &spec = chapters[index];
          const auto chapter_key =
              "romance-chapter-" + std::to_string(index + 1);
          const auto chapter = add_node(
              chapter_key, "romance", arc_id, StructuralNodeType::Chapter,
              spec.title, "Capítulo " + std::to_string(index + 1),
              spec.synopsis,
              index < 10 ? "Revisão estrutural" : "Planejamento avançado");
          add_node(chapter_key + "-a", "romance", chapter,
                   StructuralNodeType::Scene, spec.scene_a, "Cena de pressão",
                   std::string(spec.synopsis) +
                       " A cena estabelece a pergunta e o obstáculo imediato.",
                   index < 6 ? "Revisado" : "Planejamento");
          add_node(
              chapter_key + "-b", "romance", chapter, StructuralNodeType::Scene,
              spec.scene_b, "Cena de decisão",
              std::string(spec.synopsis) +
                  " A cena termina com uma escolha que altera o próximo passo.",
              index < 6 ? "Revisado" : "Planejamento");
        }
      }
    }

    const auto epilogue =
        add_node("romance-epilogo", "romance", root, StructuralNodeType::Custom,
                 "Epílogo — O mapa das crianças", "Um ano depois",
                 "Crianças redesenham Véspera com caminhos que só existem "
                 "durante a maré "
                 "baixa, encerrando o livro com uma cartografia participativa.",
                 "Planejamento", "Epílogo");
    add_node(
        "romance-epilogo-a", "romance", epilogue, StructuralNodeType::Scene,
        "Margens em branco", "Imagem final",
        "Lia devolve ao arquivo um atlas cujas últimas páginas ficam vazias.");
  }

void DemoBuilder::create_novella_structure() {
    const auto root = add_node(
        "novela-root", "novela", std::nullopt, StructuralNodeType::Volume,
        "Cartas do Farol Submerso", "Dezoito anos antes",
        "Nove cartas organizadas em três marés. O leitor sabe que Ada escreve; "
        "não sabe inicialmente quem censurou, recebeu ou reordenou as cartas.",
        "Segundo rascunho");
    const std::array<const char *, 3> parts{"Primeira maré — Sinais",
                                            "Segunda maré — O preço",
                                            "Terceira maré — O apagamento"};
    const std::array<const char *, 9> letters{
        "Carta 1 — A luz invertida",      "Carta 2 — Os peixes de cobre",
        "Carta 3 — Teu nome no livro",    "Carta 4 — O visitante do Conselho",
        "Carta 5 — Memória em sal",       "Carta 6 — A previsão de Véspera",
        "Carta 7 — Manual de sabotagem",  "Carta 8 — A garrafa",
        "Carta 9 — Se você estiver lendo"};
    for (int part = 0; part < 3; ++part) {
      const auto parent =
          add_node("novela-part-" + std::to_string(part + 1), "novela", root,
                   StructuralNodeType::Part, parts[part], "Três documentos",
                   "A sequência muda o sentido das cartas anteriores e "
                   "estreita as opções "
                   "de Ada.",
                   "Segundo rascunho");
      for (int local = 0; local < 3; ++local) {
        const int index = part * 3 + local;
        const auto key = "novela-chapter-" + std::to_string(index + 1);
        const auto chapter = add_node(
            key, "novela", parent, StructuralNodeType::Chapter, letters[index],
            "Documento " + std::to_string(index + 1),
            "Carta, anotação marginal e evidência material compõem uma versão "
            "parcial do primeiro ciclo da máquina.",
            "Segundo rascunho");
        add_node(key + "-a", "novela", chapter, StructuralNodeType::Section,
                 "Texto preservado", "Voz de Ada",
                 "Trecho contínuo da carta, com emoção contida e observação "
                 "técnica.");
        add_node(
            key + "-b", "novela", chapter, StructuralNodeType::Section,
            "Nota do arquivista", "Camada posterior",
            "Comentário cuja confiabilidade varia e cria tensão documental.");
      }
    }
  }

void DemoBuilder::create_story_structure() {
    const auto root = add_node(
        "contos-root", "contos", std::nullopt, StructuralNodeType::Series,
        "Mapas para Depois da Tempestade", "Coletânea em seis perspectivas",
        "Cada conto funciona sozinho e toca uma consequência lateral do "
        "romance.",
        "Planejamento avançado");
    const std::array<const char *, 6> titles{"A mulher que entregava previsões",
                                             "Jardim para uma água salgada",
                                             "Contrabando de nomes",
                                             "A criança e as boias",
                                             "Tipos móveis",
                                             "O cais depois do fim"};
    for (std::size_t index = 0; index < titles.size(); ++index) {
      const auto key = "conto-" + std::to_string(index + 1);
      const auto story = add_node(
          key, "contos", root, StructuralNodeType::Custom, titles[index],
          "Conto " + std::to_string(index + 1),
          "Narrativa lateral com conflito completo, imagem marítima recorrente "
          "e "
          "uma informação que muda de sentido quando lida junto ao romance.",
          index < 2 ? "Primeiro rascunho" : "Planejamento", "Conto");
      add_node(
          key + "-opening", "contos", story, StructuralNodeType::Section,
          "Antes da mudança", "Abertura",
          "Apresenta trabalho, rotina e uma pequena regra do mundo em ação.");
      add_node(
          key + "-ending", "contos", story, StructuralNodeType::Section,
          "Depois da escolha", "Fecho",
          "Mostra a consequência íntima que as crônicas oficiais ignoram.");
    }
  }

void DemoBuilder::create_dossier_structure() {
    const auto root = add_node(
        "dossie-root", "dossie", std::nullopt, StructuralNodeType::Custom,
        "Bestiário das Correntes Invisíveis", "Edição de continuidade 0.7",
        "Base editorial que distingue fato estabelecido, relato interessado, "
        "hipótese de trabalho e espaço deliberadamente aberto.",
        "Em atualização", "Dossiê");
    const std::array<const char *, 6> sections{
        "Geografia anfíbia",      "Instituições e facções",
        "Tecnologias de memória", "Calendários e marés",
        "Linguagem e símbolos",   "Questões de cânone"};
    for (std::size_t section = 0; section < sections.size(); ++section) {
      const auto parent = add_node(
          "dossie-section-" + std::to_string(section + 1), "dossie", root,
          StructuralNodeType::Section, sections[section], "Três verbetes",
          "Agrupa referências de continuidade e alerta para contradições "
          "úteis.",
          "Em atualização");
      for (int entry = 0; entry < 3; ++entry) {
        add_node(
            "dossie-entry-" + std::to_string(section * 3 + entry + 1), "dossie",
            parent, StructuralNodeType::Custom,
            "Verbete " + std::to_string(section * 3 + entry + 1),
            entry == 0   ? "Estabelecido"
            : entry == 1 ? "Disputado"
                         : "Aberto",
            "Define a regra operacional, registra a fonte interna e aponta "
            "onde uma futura produção pode reinterpretá-la.",
            "Em atualização", "Verbete");
      }
    }
  }

void DemoBuilder::create_narrative_structures() {
    const auto &structures = service_.structures();
    const auto active = structures.active_narrative(works_.at("romance"));
    if (!active)
      throw std::runtime_error("a Obra principal não possui narrativa ativa");

    const auto public_line = service_.structures().create_line(
        active->id, "Crise pública", "Acontecimentos conhecidos pela cidade.");
    const auto memory_line = service_.structures().create_line(
        active->id, "Memória de Lia",
        "Passado recordado fora da ordem linear.");
    const auto council_line = service_.structures().create_line(
        active->id, "Conselho", "Versão institucional e suas omissões.");

    struct UnitSpec {
      const char *designator;
      const char *title;
      const char *event;
      const char *purpose;
      const char *perspective;
    };
    const std::array<UnitSpec, 6> specs{{
        {"1", "O mapa impossível", "ev-mapa", "Incidente incitante", "Lia"},
        {"2", "A memória do farol", "mem-incendio", "Revelar uma lacuna",
         "Lia"},
        {"3", "A aliança improvável", "ev-abertura", "Formar oposição", "Iara"},
        {"4", "O preço do atlas", "ev-camara", "Confrontar a máquina", "Lia"},
        {"5", "A oferta de estabilidade", "ev-oferta", "Alternativa ética",
         "Maíra"},
        {"6", "O atlas aberto", "ev-atlas-publico", "Resolução coletiva",
         "Iara"},
    }};
    std::vector<inde::project::NarrativeUnit> units;
    for (const auto &spec : specs) {
      auto unit = service_.structures().create_unit(
          active->id, spec.designator, spec.title, entities_.at(spec.event),
          spec.purpose, spec.perspective);
      service_.structures().add_unit_to_line(unit.id, public_line.id);
      service_.structures().add_entity_to_unit(
          unit.id, entities_.at(spec.event), "Acontecimento selecionado");
      units.push_back(std::move(unit));
    }
    service_.structures().add_unit_to_line(units[1].id, memory_line.id);
    service_.structures().add_unit_to_line(units[4].id, council_line.id);
    service_.structures().add_unit_to_line(units[5].id, council_line.id);
    for (std::size_t i = 1; i < units.size(); ++i)
      static_cast<void>(service_.structures().create_link(
          active->id, units[i - 1].id, units[i].id,
          inde::project::NarrativeLinkKind::Precedes,
          "Ordem principal de revelação"));
    static_cast<void>(service_.structures().create_link(
        active->id, units[3].id, units[4].id,
        inde::project::NarrativeLinkKind::Alternative,
        "Recusar ou aceitar a previsão controlada"));

    const auto roles = service_.structures().roles();
    const auto role_id = [&](const std::string &name) {
      const auto found =
          std::ranges::find(roles, name, &inde::project::NarrativeRole::name);
      if (found == roles.end())
        throw std::runtime_error("papel narrativo interno ausente: " + name);
      return found->id;
    };
    static_cast<void>(service_.structures().assign_role(
        role_id("Protagonista"), entities_.at("lia"), works_.at("romance"),
        active->id, std::nullopt, "Centro da alternativa narrativa ativa"));
    static_cast<void>(service_.structures().assign_role(
        role_id("Antagonista"), entities_.at("maira"), works_.at("romance"),
        active->id, units[4].id, "Oposição contextual na oferta"));
    static_cast<void>(service_.structures().assign_role(
        role_id("Ponto de vista (PoV)"), entities_.at("lia"),
        works_.at("romance"), active->id, units.front().id,
        "Ponto de vista da abertura"));

    static_cast<void>(service_.structures().capture_narrative_model(
        active->id, "Revelação em linhas cruzadas",
        "Modelo do usuário com linha pública, memória e versão "
        "institucional."));
    static_cast<void>(service_.structures().duplicate_narrative(
        active->id, "Alternativa — Conselho vence", true));

    const auto editorial_models =
        service_.structures().models(inde::project::StructureLayer::Editorial);
    const auto three_acts =
        std::ranges::find(editorial_models, std::string{"three-acts"},
                          &inde::project::StructureModel::key);
    if (three_acts == editorial_models.end())
      throw std::runtime_error("modelo editorial de três atos ausente");
    static_cast<void>(service_.structures().instantiate_editorial(
        works_.at("romance"), three_acts->id,
        "Alternativa editorial — três atos"));
    const auto active_editorial =
        service_.structures().active_editorial(works_.at("romance"));
    if (!active_editorial)
      throw std::runtime_error("a Obra principal não possui editorial ativo");
    static_cast<void>(service_.structures().capture_editorial_model(
        active_editorial->id, "Quatro marés",
        "Modelo editorial capturado da estrutura integral do romance."));
  }

} // namespace inde::demo

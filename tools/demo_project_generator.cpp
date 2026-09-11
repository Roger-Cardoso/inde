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

namespace {

using inde::project::RelationDirectionality;
using inde::project::StructuralNodeType;

struct EntitySpec {
  const char *key;
  const char *name;
  const char *summary;
};

struct ChapterSpec {
  const char *title;
  const char *synopsis;
  const char *scene_a;
  const char *scene_b;
};

struct EventSpec {
  const char *key;
  const char *name;
  const char *summary;
  const char *axis;
  int point;
  const char *location;
  std::array<const char *, 3> participants;
};

class DemoBuilder {
public:
  explicit DemoBuilder(std::filesystem::path output)
      : output_(std::move(output)) {}

  void build() {
    if (std::filesystem::exists(output_))
      throw std::runtime_error("o destino já existe: " + output_.string());

    service_.create(output_,
                    "Atlas de Véspera — Projeto integral de avaliação");
    create_catalog();
    create_structures();
    create_entities();
    create_relations();
    create_timeline();
    create_narrative_structures();
    create_scopes_and_references();
    create_documents();
    organize_documents();
    service_.save();
    audit();
  }

private:
  std::string add_node(const std::string &key, const std::string &work,
                       const std::optional<std::string> &parent,
                       StructuralNodeType type, const std::string &title,
                       const std::string &subtitle, const std::string &synopsis,
                       const std::string &status = "Planejamento",
                       const std::string &custom_type = {}) {
    auto node = service_.create_structural_node(works_.at(work), parent, type,
                                                title, custom_type);
    const auto siblings = service_.structural_nodes_for_work(works_.at(work));
    node.designator = std::to_string(
        std::ranges::count_if(siblings, [&](const auto &candidate) {
          return candidate.parent_id == parent &&
                 candidate.structural_type_id == node.structural_type_id;
        }));
    node.subtitle = subtitle;
    node.synopsis = synopsis;
    node.status = status;
    service_.update_structural_node(node);
    nodes_[key] = node.id;
    return node.id;
  }

  void add_entity(const std::string &type, const EntitySpec &spec) {
    const auto value = service_.narrative().create_entity(
        types_.at(type), spec.name, spec.summary);
    entities_[spec.key] = value.id;
    entity_keys_.push_back(spec.key);
  }

  void add_relation(const std::string &type, const std::string &source,
                    const std::string &target, const std::string &description) {
    static_cast<void>(service_.narrative().create_relation(
        relation_types_.at(type), entities_.at(source), entities_.at(target),
        description));
  }

  static std::pair<std::size_t, std::size_t>
  unicode_range(const std::string &content, const std::string &needle) {
    const auto byte_start = content.find(needle);
    if (byte_start == std::string::npos)
      throw std::runtime_error("trecho documental não encontrado: " + needle);
    const auto start =
        inde::project::utf8_character_count(content.substr(0, byte_start));
    return {start, start + inde::project::utf8_character_count(needle)};
  }

  static void add_format(inde::project::Document &document,
                         inde::project::DocumentTextStyle style,
                         const std::string &needle) {
    const auto [start, end] = unicode_range(document.content, needle);
    document.formatting.push_back({style, start, end});
  }

  static std::string add_anchor(inde::project::Document &document,
                                const std::string &label,
                                const std::string &needle) {
    const auto [start, end] = unicode_range(document.content, needle);
    const auto id = inde::project::new_uuid();
    document.anchors.push_back({id, label, start, end});
    return id;
  }

  void add_document_reference(inde::project::Document &document,
                              const std::string &entity,
                              const std::optional<std::string> &anchor,
                              const std::string &notes) {
    document.entity_references.push_back(
        {inde::project::new_uuid(), entities_.at(entity), anchor, notes});
  }

  inde::project::Document begin_document(const std::string &title,
                                         const std::string &content,
                                         std::optional<std::string> node_key,
                                         std::optional<std::size_t> word_goal) {
    std::optional<std::string> placement;
    if (node_key)
      placement = nodes_.at(*node_key);
    auto document = service_.writing().create_document(title, placement);
    document.content = content;
    document.word_goal = word_goal;
    return document;
  }

  void finish_document(const std::string &key,
                       inde::project::Document document) {
    document = service_.writing().update_document(document);
    documents_[key] = document.id;
  }

  void create_catalog() {
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

  void create_structures() {
    create_main_structure();
    create_novella_structure();
    create_story_structure();
    create_dossier_structure();
  }

  void create_main_structure() {
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

  void create_novella_structure() {
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

  void create_story_structure() {
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

  void create_dossier_structure() {
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

  void create_entities() {
    for (const auto &type : service_.narrative().entity_types())
      types_[type.key] = type.id;
    for (const auto &[key, name, description] :
         std::vector<std::array<const char *, 3>>{
             {"faction", "Facção",
              "Instituição, coletivo ou grupo com agência."},
             {"artifact", "Artefato", "Objeto singular com função narrativa."},
             {"motif", "Motivo",
              "Imagem ou ideia recorrente acompanhada entre obras."}}) {
      types_[key] =
          service_.narrative().create_entity_type(key, name, description).id;
    }

    const std::vector<EntitySpec> characters{
        {"lia", "Lia Avelar",
         "Restauradora de mapas, 31 anos. Precisa trocar o domínio da técnica "
         "pela confiança em outras pessoas."},
        {"iara", "Iara Nóbrega",
         "Arquivista pública e especialista em proveniência; transforma dúvida "
         "metódica em coragem política."},
        {"caio", "Caio Orun",
         "Engenheiro das estações de maré, pragmático, culpado por ter "
         "aperfeiçoado parte do Protocolo Nácar."},
        {"maira", "Maíra Seles",
         "Governadora de Véspera; acredita que esconder o risco evita pânico e "
         "preserva a cidade."},
        {"tome", "Tomé Varga",
         "Antigo cartógrafo do Conselho e mentor de Lia; sabe demais, mas "
         "recorda cada vez menos."},
        {"bento", "Bento Sal",
         "Operador de comportas e organizador do Bairro Baixo; exige que "
         "sobrevivência não seja privilégio técnico."},
        {"safira", "Safira Ko",
         "Negociadora anfíbia que cruza legalidade, contrabando e diplomacia "
         "sem confundir lealdade com obediência."},
        {"davi", "Davi Junco",
         "Astrônomo cético; destrói a lente que ajudou a construir quando "
         "percebe o uso autoritário das previsões."},
        {"nael", "Nael das Boias",
         "Mensageiro adolescente capaz de reconhecer correntes pelo som; "
         "carrega a chave deixada por Ada."},
        {"ada", "Ada Avelar",
         "Faroleira, mãe de Lia e primeira sabotadora documentada da máquina "
         "de marés."},
        {"cora", "Irmã Cora",
         "Médica das balsas e guardiã de histórias orais que o Arquivo recusou "
         "registrar."},
        {"joana", "Joana Lume",
         "Tipógrafa clandestina que imprime mapas públicos em papel solúvel."},
        {"rui", "Rui Candeia",
         "Capitão dos Navegantes Livres; cobra caro, mas mantém cada acordo "
         "até a última palavra."},
        {"yara", "Yara Pontal",
         "Hidróloga do Conselho dividida entre dever institucional e evidência "
         "científica."},
        {"milo", "Milo Ferrugem",
         "Mergulhador de manutenção que encontra memórias cristalizadas no "
         "fundo do reservatório."},
        {"ines", "Inês Varanda",
         "Professora do Bairro Alto que organiza abrigos e confronta a falsa "
         "neutralidade dos mapas."},
        {"sol", "Sol Arari",
         "Criança que conversa com boias meteorológicas e percebe primeiro a "
         "pausa das marés."},
        {"gaspar", "Gaspar Seles",
         "Irmão da governadora e chefe da Guarda Cartográfica; confunde "
         "proteção com controle."},
        {"nara", "Nara Vime",
         "Jardineira que cultiva alimentos em água salobra e lidera a "
         "logística da evacuação."},
        {"orfeu", "Orfeu Manso",
         "Ancião das salinas, testemunha do ciclo anterior, lembrado por todos "
         "de maneira diferente."},
        {"celina", "Celina Raso",
         "Contrabandista de documentos e parceira de Safira, especialista em "
         "rotas que só existem por minutos."},
        {"leon", "Leon Marulho",
         "Repórter da Rádio Ponte; escolhe transmitir incerteza honesta em vez "
         "de uma previsão confortável."}};
    const std::vector<EntitySpec> locations{
        {"vespera", "Véspera",
         "Cidade-arquipélago construída em camadas móveis entre água doce, "
         "oceano e ruínas pré-maré."},
        {"bairro-baixo", "Bairro Baixo",
         "Distrito de palafitas e oficinas, o primeiro a inundar e o último a "
         "ser consultado."},
        {"bairro-alto", "Bairro Alto",
         "Centro administrativo sobre fundações de vidro, protegido por "
         "comportas prioritárias."},
        {"arquivo", "Arquivo das Águas",
         "Reservatórios onde documentos e lembranças são conservados em "
         "soluções salinas."},
        {"farol", "Farol Nacarado",
         "Torre parcialmente submersa que funciona como observatório e "
         "interface da máquina de marés."},
        {"ilha-sino", "Ilha do Sino",
         "Estação meteorológica abandonada; o bronze ressoa antes de mudanças "
         "de pressão."},
        {"salinas", "Salinas de Orfeu",
         "Terraços produtivos e memória viva do primeiro deslocamento "
         "populacional."},
        {"mercado", "Mercado Flutuante",
         "Centenas de plataformas que mudam de bairro conforme vento, preço e "
         "fiscalização."},
        {"observatorio", "Observatório do Meridiano",
         "Cúpula de lentes usada para converter padrões celestes em "
         "probabilidades oceânicas."},
        {"camara", "Câmara das Correntes",
         "Núcleo hidráulico sob a cidade, construído ao redor de um aquífero "
         "sensível à memória."},
        {"ponte-nove", "Ponte Nove",
         "Passagem popular coberta por mensagens, remendos e marcas das "
         "enchentes antigas."},
        {"cais", "Cais das Vozes",
         "Terminal de barcos onde anúncios oficiais disputam espaço com rádio "
         "comunitária."},
        {"jardins", "Jardins de Sal",
         "Estufas comunitárias adaptadas por Nara para irrigação com água "
         "salobra."},
        {"tipografia", "Tipografia Submersa",
         "Oficina clandestina montada dentro de um antigo túnel de drenagem."},
        {"rota-azul", "Rota Azul",
         "Canal navegável por poucos minutos entre duas marés; não aparece nos "
         "mapas oficiais."},
        {"reservatorio", "Reservatório das Sete Portas",
         "Sistema de contenção que distribui água e define quais distritos "
         "serão sacrificados."}};
    const std::vector<EntitySpec> informations{
        {"info-protocolo", "Protocolo Nácar",
         "Procedimento que converte memórias humanas em previsões de alta "
         "precisão."},
        {"info-ciclo", "Registro do ciclo anterior",
         "Prova fragmentária de que Véspera já escolheu esquecer uma "
         "inundação."},
        {"info-carta", "Carta de Ada",
         "Mensagem destinada a Lia com coordenadas, culpa e uma instrução "
         "ambígua."},
        {"info-custo", "Custo cognitivo da máquina",
         "Quanto mais certa a previsão, mais memória coletiva é apagada."},
        {"info-rota", "Janela da Rota Azul",
         "Intervalo de onze minutos que permite retirar pessoas do Bairro "
         "Baixo."},
        {"info-comporta", "Falha da comporta três",
         "Defeito conhecido que o Conselho reclassificou como risco "
         "aceitável."},
        {"info-lente", "Limite da lente meridiana",
         "A lente não prevê decisões tomadas após sua última calibração."},
        {"info-ada", "Paradeiro de Ada",
         "Versões incompatíveis afirmam morte, exílio ou integração à Câmara "
         "das Correntes."},
        {"info-sete", "Significado das sete badaladas",
         "Código meteorológico transformado em senha política pelos "
         "trabalhadores."},
        {"info-mapa", "Mapa da inundação",
         "Projeção com três camadas: provável, evitável e desejada pelo "
         "Conselho."},
        {"info-sal", "Memória cristalizada",
         "Resíduos do processo formam cristais que reproduzem fragmentos sem "
         "contexto."},
        {"info-decreto", "Decreto sem assinatura",
         "Ordem de apreensão cuja autoria foi removida do registro "
         "administrativo."}};
    const std::vector<EntitySpec> objectives{
        {"goal-atlas", "Decifrar o atlas",
         "Separar previsão, propaganda e possibilidade antes da maré máxima."},
        {"goal-evacuar", "Evacuar o Bairro Baixo",
         "Retirar moradores sem entregar ao Conselho uma lista de "
         "dissidentes."},
        {"goal-expor", "Tornar o protocolo público",
         "Publicar mecanismo e custo sem apresentar hipótese como certeza."},
        {"goal-controlar", "Preservar o controle do Conselho",
         "Evitar pânico e manter as comportas sob comando central."},
        {"goal-ada", "Descobrir o destino de Ada",
         "Responder à ausência sem reduzir Ada a pista para a jornada de Lia."},
        {"goal-maquina", "Desligar a máquina",
         "Interromper o ciclo sem provocar colapso hidráulico."},
        {"goal-memoria", "Restituir as memórias",
         "Encontrar modo seguro de devolver fragmentos às comunidades de "
         "origem."},
        {"goal-rota", "Manter a Rota Azul aberta",
         "Coordenar barcos, vento e comportas durante onze minutos."},
        {"goal-arquivo", "Salvar o Arquivo das Águas",
         "Preservar documentos sem priorizá-los sobre vidas humanas."},
        {"goal-pacto", "Criar um pacto cartográfico",
         "Substituir o monopólio preditivo por revisão pública e plural."}};
    const std::vector<EntitySpec> factions{
        {"fac-conselho", "Conselho das Cartas",
         "Governo técnico que administra previsão, comportas e circulação de "
         "mapas."},
        {"fac-cooperativa", "Cooperativa da Maré Baixa",
         "Rede de trabalhadores, barqueiros e cuidadoras do distrito "
         "ameaçado."},
        {"fac-navegantes", "Navegantes Livres",
         "Aliança de capitães que reconhece contratos próprios e fronteiras "
         "móveis."},
        {"fac-guardas", "Guarda Cartográfica",
         "Força encarregada de controlar cópias, rotas e instrumentos de "
         "medição."},
        {"fac-arquivistas", "Círculo dos Arquivistas",
         "Profissionais divididos entre custódia institucional e acesso "
         "público."},
        {"fac-radio", "Rádio Ponte",
         "Emissora comunitária transmitida por cabos, boias e alto-falantes "
         "recuperados."},
        {"fac-jardineiros", "Jardineiros de Sal",
         "Comunidades que desenvolvem agricultura anfíbia e logística "
         "alimentar."},
        {"fac-tipografos", "Oficina das Margens",
         "Tipógrafos e encadernadores que replicam documentos censurados."}};
    const std::vector<EntitySpec> artifacts{
        {"art-chave", "Chave de nácar",
         "Chave biométrica e mecânica deixada por Ada; responde às lembranças "
         "de Lia."},
        {"art-atlas", "Atlas de vidro",
         "Conjunto de lâminas cartográficas que muda conforme luz, sal e "
         "observador."},
        {"art-lente", "Lente meridiana",
         "Óptica central do observatório, calibrada com registros apagados."},
        {"art-garrafa", "Garrafa azul",
         "Recipiente da carta de Ada, marcado por duas datas de fabricação."},
        {"art-radio", "Transmissor Ponte",
         "Rádio portátil capaz de usar as boias como repetidores."},
        {"art-caderno", "Caderno de Tomé",
         "Notas que perdem palavras à medida que o autor perde lembranças."},
        {"art-sino", "Sino barométrico",
         "Instrumento da ilha cuja liga reage a variações invisíveis da "
         "corrente."},
        {"art-matriz", "Matriz da tipografia",
         "Tipos móveis para imprimir mapas táteis em papel resistente à "
         "água."}};
    const std::vector<EntitySpec> motifs{
        {"motif-margem", "Margens em branco",
         "Espaço visual associado a escolha, censura e futuro aberto."},
        {"motif-sete", "Sete badaladas",
         "Som que passa de alerta técnico a chamado coletivo."},
        {"motif-sal", "Sal na língua",
         "Sensação que acompanha memórias emprestadas ou incompletas."},
        {"motif-maos", "Mãos manchadas de azul",
         "Marca de quem altera um mapa e aceita responsabilidade pela "
         "mudança."},
        {"motif-peixes", "Peixes de cobre",
         "Imagem do impossível que primeiro parece superstição e depois "
         "evidência."},
        {"motif-portas", "Portas numeradas",
         "Contagem material das escolhas sobre quem terá passagem e abrigo."}};

    for (const auto &spec : characters)
      add_entity("character", spec);
    for (const auto &spec : locations)
      add_entity("location", spec);
    for (const auto &spec : informations)
      add_entity("information", spec);
    for (const auto &spec : objectives)
      add_entity("objective", spec);
    for (const auto &spec : factions)
      add_entity("faction", spec);
    for (const auto &spec : artifacts)
      add_entity("artifact", spec);
    for (const auto &spec : motifs)
      add_entity("motif", spec);

    create_event_entities();
  }

  void create_event_entities() {
    events_ = {
        {"ev-mapa",
         "O mapa muda durante a noite",
         "A primeira projeção impossível aparece no laboratório de Lia.",
         "main",
         0,
         "arquivo",
         {"lia", "iara", "tome"}},
        {"ev-elevadores",
         "Falha dos elevadores de maré",
         "Cabos param e revelam a vulnerabilidade do Bairro Baixo.",
         "main",
         1,
         "ponte-nove",
         {"lia", "bento", "nael"}},
        {"ev-chave",
         "Entrega da chave de nácar",
         "Nael cumpre uma instrução recebida anos antes.",
         "main",
         2,
         "mercado",
         {"nael", "lia", "safira"}},
        {"ev-decreto",
         "Apreensão dos mapas",
         "O Conselho recolhe documentos sob um decreto sem autoria.",
         "main",
         3,
         "bairro-alto",
         {"maira", "gaspar", "iara"}},
        {"ev-fuga",
         "Fuga pelo mercado flutuante",
         "Lia e Iara salvam cópias e atravessam plataformas em movimento.",
         "main",
         4,
         "mercado",
         {"lia", "iara", "celina"}},
        {"ev-abertura",
         "Abertura do Arquivo das Águas",
         "A chave desperta tanques selados desde o ciclo anterior.",
         "main",
         5,
         "arquivo",
         {"lia", "caio", "milo"}},
        {"ev-protocolo",
         "Revelação do Protocolo Nácar",
         "Tomé explica que certeza é comprada com memória coletiva.",
         "main",
         6,
         "arquivo",
         {"tome", "lia", "iara"}},
        {"ev-ausencias",
         "Descoberta das lacunas familiares",
         "Os registros de Ada foram removidos por várias mãos.",
         "main",
         7,
         "arquivo",
         {"lia", "tome", "cora"}},
        {"ev-sino",
         "Reativação do sino barométrico",
         "Caio confirma que a tempestade é artificialmente amplificada.",
         "main",
         8,
         "ilha-sino",
         {"caio", "davi", "nael"}},
        {"ev-teste",
         "Ensaio da inundação",
         "Um modelo físico encontra uma rota de sobrevivência não prevista.",
         "main",
         9,
         "observatorio",
         {"yara", "caio", "lia"}},
        {"ev-mapas",
         "Publicação dos dois mapas",
         "Previsões incompatíveis dividem a praça e encerram o segredo.",
         "main",
         10,
         "cais",
         {"maira", "bento", "leon"}},
        {"ev-motim",
         "Motim da comporta três",
         "Trabalhadores impedem o fechamento unilateral do canal.",
         "main",
         11,
         "reservatorio",
         {"bento", "gaspar", "nara"}},
        {"ev-gravacao",
         "A voz de Ada retorna",
         "Uma gravação recompõe a intenção da sabotagem original.",
         "main",
         12,
         "farol",
         {"lia", "cora", "tome"}},
        {"ev-negocio",
         "O acordo da terceira condição",
         "Safira troca uma cópia por barcos e autonomia de rota.",
         "main",
         13,
         "rota-azul",
         {"safira", "rui", "celina"}},
        {"ev-queda",
         "Queda do observatório",
         "Davi quebra a lente e elimina a previsão centralizada.",
         "main",
         14,
         "observatorio",
         {"davi", "yara", "gaspar"}},
        {"ev-pausa",
         "O dia sem maré",
         "A água fica suspensa e toda interpretação parece possível.",
         "main",
         15,
         "vespera",
         {"sol", "orfeu", "leon"}},
        {"ev-evacuacao",
         "Evacuação do Bairro Baixo",
         "A rede comunitária executa rotas adaptativas sem lista central.",
         "main",
         16,
         "bairro-baixo",
         {"lia", "bento", "nara"}},
        {"ev-rota",
         "Travessia da Rota Azul",
         "Barcos completam a passagem nos onze minutos disponíveis.",
         "main",
         17,
         "rota-azul",
         {"rui", "safira", "nael"}},
        {"ev-camara",
         "Entrada na Câmara das Correntes",
         "Rivais confrontam o reservatório de memórias sacrificadas.",
         "main",
         18,
         "camara",
         {"lia", "maira", "caio"}},
        {"ev-oferta",
         "A última previsão",
         "A máquina oferece uma cidade intacta em troca do apagamento da "
         "revolta.",
         "main",
         19,
         "camara",
         {"lia", "maira", "tome"}},
        {"ev-recusa",
         "Recusa de Lia",
         "Lia destrói a chave depois de abrir o controle da máquina à "
         "assembleia.",
         "main",
         20,
         "camara",
         {"lia", "iara", "maira"}},
        {"ev-atlas-publico",
         "Assembleia do atlas aberto",
         "Milhares de correções convertem o mapa em processo público.",
         "main",
         21,
         "cais",
         {"iara", "joana", "leon"}},
        {"ev-mare-maxima",
         "Chegada da maré máxima",
         "Véspera sofre danos, mas nenhum distrito é deliberadamente "
         "abandonado.",
         "main",
         22,
         "vespera",
         {"bento", "cora", "nara"}},
        {"ev-paginas",
         "Entrega das páginas vazias",
         "Lia devolve ao arquivo um atlas que se recusa a prever sozinho.",
         "main",
         23,
         "arquivo",
         {"lia", "sol", "iara"}},
        {"mem-barco",
         "Lia aprende a remar",
         "Uma lembrança afetiva contém a primeira menção cifrada à máquina.",
         "memory",
         0,
         "rota-azul",
         {"lia", "ada", "orfeu"}},
        {"mem-incendio",
         "Incêndio no livro de vigília",
         "Lia recorda versões diferentes da noite em que Ada desapareceu.",
         "memory",
         2,
         "farol",
         {"lia", "ada", "tome"}},
        {"mem-promessa",
         "Promessa no Jardim de Sal",
         "Iara e Lia prometem nunca corrigir silenciosamente o registro uma da "
         "outra.",
         "memory",
         4,
         "jardins",
         {"lia", "iara", "nara"}},
        {"mem-retorno",
         "A memória devolvida",
         "Depois da crise, Lia reconhece que algumas lacunas também a "
         "protegeram.",
         "memory",
         7,
         "arquivo",
         {"lia", "cora", "iara"}},
        {"farol-luz",
         "Primeira luz sob a água",
         "Ada observa a projeção impossível no subsolo do farol.",
         "lighthouse",
         0,
         "farol",
         {"ada", "tome", "orfeu"}},
        {"farol-visita",
         "Visita do emissário",
         "O Conselho oferece recursos em troca de exclusividade sobre os "
         "registros.",
         "lighthouse",
         2,
         "farol",
         {"ada", "maira", "gaspar"}},
        {"farol-sabotagem",
         "Sabotagem do primeiro ciclo",
         "Ada introduz incerteza no mecanismo e salva um distrito não "
         "previsto.",
         "lighthouse",
         5,
         "camara",
         {"ada", "caio", "tome"}},
        {"farol-garrafa",
         "Lançamento da garrafa azul",
         "Ada encerra os registros e confia ao mar a única cópia integral.",
         "lighthouse",
         7,
         "farol",
         {"ada", "cora", "nael"}},
    };
    for (const auto &event : events_)
      add_entity("event", {event.key, event.name, event.summary});
  }

  void create_relations() {
    const auto make_type =
        [this](const char *key, const char *name, const char *inverse,
               RelationDirectionality directionality, const char *description) {
          relation_types_[key] =
              service_.narrative()
                  .create_relation_type(key, name, inverse, directionality,
                                        description)
                  .id;
        };
    make_type("allied-with", "é aliado de", "",
              RelationDirectionality::Symmetric,
              "Aliança ativa, ainda que tensa.");
    make_type("opposes", "se opõe a", "", RelationDirectionality::Symmetric,
              "Conflito estrutural ou imediato.");
    make_type("member-of", "integra", "tem como integrante",
              RelationDirectionality::Directed, "Participação institucional.");
    make_type("pursues", "busca", "é buscado por",
              RelationDirectionality::Directed,
              "Objetivo que orienta decisões atuais.");
    make_type("protects", "protege", "é protegido por",
              RelationDirectionality::Directed,
              "Proteção material ou política.");
    make_type("located-at", "está ligado a", "abriga",
              RelationDirectionality::Directed,
              "Vínculo espacial persistente.");
    make_type("reveals", "revela", "é revelado por",
              RelationDirectionality::Directed,
              "Entrega ou torna legível uma informação.");
    make_type("conceals", "oculta", "é ocultado por",
              RelationDirectionality::Directed,
              "Retém deliberadamente uma informação.");
    make_type("requires", "depende de", "é necessário para",
              RelationDirectionality::Directed,
              "Dependência causal ou operacional.");
    make_type("echoes", "ecoa", "", RelationDirectionality::Symmetric,
              "Relação temática sem equivalência literal.");
    make_type("family", "é família de", "", RelationDirectionality::Symmetric,
              "Parentesco reconhecido pelos envolvidos.");

    const std::array<const char *, 22> people{
        "lia",  "iara",   "caio", "maira", "tome",   "bento", "safira", "davi",
        "nael", "ada",    "cora", "joana", "rui",    "yara",  "milo",   "ines",
        "sol",  "gaspar", "nara", "orfeu", "celina", "leon"};
    const std::array<const char *, 22> factions{
        "fac-arquivistas", "fac-arquivistas", "fac-conselho",
        "fac-conselho",    "fac-arquivistas", "fac-cooperativa",
        "fac-navegantes",  "fac-conselho",    "fac-radio",
        "fac-arquivistas", "fac-cooperativa", "fac-tipografos",
        "fac-navegantes",  "fac-conselho",    "fac-cooperativa",
        "fac-cooperativa", "fac-radio",       "fac-guardas",
        "fac-jardineiros", "fac-jardineiros", "fac-navegantes",
        "fac-radio"};
    const std::array<const char *, 22> goals{
        "goal-atlas",   "goal-expor",     "goal-maquina", "goal-controlar",
        "goal-memoria", "goal-evacuar",   "goal-rota",    "goal-maquina",
        "goal-ada",     "goal-ada",       "goal-memoria", "goal-expor",
        "goal-rota",    "goal-controlar", "goal-arquivo", "goal-evacuar",
        "goal-memoria", "goal-controlar", "goal-evacuar", "goal-memoria",
        "goal-rota",    "goal-expor"};
    const std::array<const char *, 22> homes{
        "arquivo",   "arquivo",      "observatorio", "bairro-alto",
        "arquivo",   "bairro-baixo", "mercado",      "observatorio",
        "cais",      "farol",        "bairro-baixo", "tipografia",
        "rota-azul", "observatorio", "reservatorio", "bairro-alto",
        "cais",      "bairro-alto",  "jardins",      "salinas",
        "mercado",   "cais"};
    for (std::size_t i = 0; i < people.size(); ++i) {
      add_relation("member-of", people[i], factions[i],
                   "Vínculo institucional relevante no início do romance.");
      add_relation(
          "pursues", people[i], goals[i],
          "O objetivo organiza suas escolhas, mas pode mudar de sentido.");
      add_relation(
          "located-at", people[i], homes[i],
          "Local de trabalho, residência ou pertencimento recorrente.");
    }

    add_relation("family", "lia", "ada",
                 "Mãe e filha separadas por uma história censurada.");
    add_relation("family", "maira", "gaspar",
                 "Irmãos com interpretações diferentes de proteção.");
    add_relation(
        "allied-with", "lia", "iara",
        "A confiança nasce de desacordos registrados com honestidade.");
    add_relation("allied-with", "bento", "nara",
                 "Organizam abrigo, alimento e evacuação no Bairro Baixo.");
    add_relation("allied-with", "safira", "celina",
                 "Parceria antiga com regras explícitas de risco.");
    add_relation("opposes", "bento", "gaspar",
                 "Disputam quem decide o fechamento das comportas.");
    add_relation(
        "opposes", "maira", "lia",
        "Conflito entre estabilidade administrada e incerteza pública.");
    add_relation("protects", "cora", "sol",
                 "Cora mantém Sol fora das listas oficiais de deslocamento.");
    add_relation(
        "protects", "tome", "lia",
        "Tomé esconde parte do passado para adiar o custo da verdade.");

    const std::array<const char *, 8> artifacts{
        "art-chave", "art-atlas",   "art-lente", "art-garrafa",
        "art-radio", "art-caderno", "art-sino",  "art-matriz"};
    const std::array<const char *, 8> artifact_locations{
        "farol", "arquivo", "observatorio", "rota-azul",
        "cais",  "arquivo", "ilha-sino",    "tipografia"};
    for (std::size_t i = 0; i < artifacts.size(); ++i)
      add_relation("located-at", artifacts[i], artifact_locations[i],
                   "Localização de referência antes da crise principal.");

    const std::array<const char *, 12> infos{
        "info-protocolo", "info-ciclo",    "info-carta", "info-custo",
        "info-rota",      "info-comporta", "info-lente", "info-ada",
        "info-sete",      "info-mapa",     "info-sal",   "info-decreto"};
    const std::array<const char *, 12> keepers{
        "fac-conselho", "fac-conselho",    "ada",
        "tome",         "fac-navegantes",  "gaspar",
        "davi",         "fac-conselho",    "fac-cooperativa",
        "maira",        "fac-arquivistas", "gaspar"};
    for (std::size_t i = 0; i < infos.size(); ++i)
      add_relation(
          "conceals", keepers[i], infos[i],
          "Retenção com motivação própria, não necessariamente maliciosa.");

    add_relation("reveals", "art-garrafa", "info-carta",
                 "O recipiente preserva e autentica a carta.");
    add_relation("reveals", "art-caderno", "info-custo",
                 "As palavras apagadas demonstram o preço cognitivo.");
    add_relation("reveals", "art-sino", "info-sete",
                 "O padrão acústico torna o código verificável.");
    add_relation("requires", "goal-evacuar", "info-rota",
                 "A janela navegável viabiliza a retirada.");
    add_relation("requires", "goal-maquina", "info-custo",
                 "Conhecer o custo impede uma solução simplista.");
    add_relation("requires", "goal-expor", "art-matriz",
                 "A impressão distribuída evita uma única cópia vulnerável.");
    add_relation("echoes", "motif-margem", "goal-pacto",
                 "O espaço vazio representa decisões ainda compartilháveis.");
    add_relation("echoes", "motif-sete", "info-sete",
                 "O som atravessa técnica, memória e mobilização.");
    add_relation("echoes", "motif-sal", "info-sal",
                 "Sensação corporal e evidência material se espelham.");
  }

  void create_timeline() {
    auto axes = service_.planning().time_axes();
    if (axes.empty())
      throw std::runtime_error("eixo temporal padrão ausente");
    auto main = axes.front();
    main.name = "Cronologia principal — A Maré de Vidro";
    main.description =
        "Ordem causal do romance, da primeira anomalia ao atlas aberto.";
    axes_["main"] = service_.planning().update_time_axis(main).id;
    axes_["memory"] =
        service_.planning()
            .create_time_axis("Memórias de Lia",
                              "Ordem subjetiva em que Lia recupera e "
                              "reinterpreta lembranças.")
            .id;
    axes_["lighthouse"] =
        service_.planning()
            .create_time_axis(
                "Registros do Farol",
                "Sequência documental do ciclo ocorrido dezoito anos antes.")
            .id;

    for (int i = 0; i < 25; ++i) {
      const auto point = service_.planning().create_time_point(
          axes_.at("main"), i * 10,
          "Dia " + std::to_string(i + 1) + " — " +
              std::string(i < 5    ? "Maré crescente"
                          : i < 15 ? "Pressão pública"
                                   : "Maré máxima"),
          "Marco operacional da cronologia principal; a distância ordinal "
          "preserva espaço para inserções futuras.");
      points_["main"].push_back(point.id);
    }
    const std::array<const char *, 8> memory_labels{
        "Infância — O barco",    "Infância — O farol",
        "Ausência — O incêndio", "Formação — Primeiro mapa",
        "Amizade — O jardim",    "Crise — A voz",
        "Escolha — A recusa",    "Depois — Memória devolvida"};
    const std::array<const char *, 8> lighthouse_labels{
        "Registro 1 — Luz invertida", "Registro 2 — Peixes de cobre",
        "Registro 3 — O emissário",   "Registro 4 — Primeira extração",
        "Registro 5 — A previsão",    "Registro 6 — Sabotagem",
        "Registro 7 — Livro apagado", "Registro 8 — Garrafa azul"};
    for (int i = 0; i < 8; ++i) {
      points_["memory"].push_back(
          service_.planning()
              .create_time_point(axes_.at("memory"), (i - 4) * 10,
                                 memory_labels[i],
                                 "Posição na lembrança, não data objetiva.")
              .id);
      points_["lighthouse"].push_back(
          service_.planning()
              .create_time_point(axes_.at("lighthouse"), (i + 1) * 100,
                                 lighthouse_labels[i],
                                 "Entrada reconstruída a partir de carta, "
                                 "instrumento e nota marginal.")
              .id);
    }

    for (const auto &event : events_) {
      const auto occurrence = service_.planning().place_event(
          entities_.at(event.key), points_.at(event.axis).at(event.point),
          std::string(event.summary) +
              " Local principal: " + display_name(event.location) + ".");
      occurrences_[event.key] = occurrence.id;
      for (std::size_t i = 0; i < event.participants.size(); ++i) {
        static_cast<void>(service_.planning().add_participant(
            occurrence.id, entities_.at(event.participants[i]),
            i == 0   ? "agente da mudança"
            : i == 1 ? "contraponto"
                     : "testemunha ou apoio",
            "Participação prevista; intensidade e ponto de vista podem mudar "
            "na revisão."));
      }
    }

    const std::array<const char *, 22> people{
        "lia",  "iara",   "caio", "maira", "tome",   "bento", "safira", "davi",
        "nael", "ada",    "cora", "joana", "rui",    "yara",  "milo",   "ines",
        "sol",  "gaspar", "nara", "orfeu", "celina", "leon"};
    const std::array<const char *, 16> locations{
        "arquivo",    "bairro-baixo", "observatorio", "bairro-alto",
        "farol",      "cais",         "mercado",      "ilha-sino",
        "ponte-nove", "salinas",      "jardins",      "tipografia",
        "rota-azul",  "reservatorio", "camara",       "vespera"};
    for (std::size_t i = 0; i < people.size(); ++i) {
      const int start = static_cast<int>(i % 8);
      const int end = 12 + static_cast<int>(i % 10);
      static_cast<void>(service_.planning().create_presence(
          entities_.at(people[i]),
          entities_.at(locations[i % locations.size()]),
          points_.at("main")[start], points_.at("main")[end],
          "Presença de base antes que a crise reorganize deslocamentos."));
      static_cast<void>(service_.planning().create_presence(
          entities_.at(people[i]),
          entities_.at(locations[(i + 7) % locations.size()]),
          points_.at("main")[end],
          i % 4 == 0 ? std::nullopt
                     : std::optional<std::string>{points_.at("main")[23]},
          "Deslocamento decorrente da revelação pública e da maré máxima."));
    }
    for (std::size_t i = 0; i < 8; ++i) {
      static_cast<void>(service_.planning().create_presence(
          entities_.at(people[i]),
          entities_.at(locations[(i + 3) % locations.size()]),
          points_.at("memory")[i], std::nullopt,
          "Presença lembrada; pode divergir da cronologia objetiva."));
      static_cast<void>(service_.planning().create_presence(
          entities_.at(people[i + 8]),
          entities_.at(i % 2 == 0 ? "farol" : "camara"),
          points_.at("lighthouse")[i], std::nullopt,
          "Presença atestada por um registro do farol."));
    }
    const std::array<const char *, 8> artifacts{
        "art-chave", "art-atlas",   "art-lente", "art-garrafa",
        "art-radio", "art-caderno", "art-sino",  "art-matriz"};
    for (std::size_t i = 0; i < artifacts.size(); ++i)
      static_cast<void>(service_.planning().create_presence(
          entities_.at(artifacts[i]),
          entities_.at(locations[(i + 4) % locations.size()]),
          points_.at("main")[static_cast<int>(i)],
          points_.at("main")[15 + static_cast<int>(i)],
          "Trajetória material do artefato ao longo da crise."));
  }

  std::string display_name(const std::string &key) const {
    const auto id = entities_.at(key);
    const auto entity = service_.narrative().entity(id);
    return entity ? entity->name : key;
  }

  void create_scopes_and_references() {
    for (const auto &key : entity_keys_)
      static_cast<void>(service_.narrative().add_entity_to_work(
          entities_.at(key), works_.at("romance"),
          "Disponível ao planejamento do romance principal; o vínculo não "
          "impõe aparição."));

    const std::array<const char *, 28> novella_scope{"lia",
                                                     "ada",
                                                     "tome",
                                                     "cora",
                                                     "orfeu",
                                                     "maira",
                                                     "gaspar",
                                                     "nael",
                                                     "farol",
                                                     "arquivo",
                                                     "camara",
                                                     "rota-azul",
                                                     "info-protocolo",
                                                     "info-ciclo",
                                                     "info-carta",
                                                     "info-custo",
                                                     "info-ada",
                                                     "art-chave",
                                                     "art-garrafa",
                                                     "art-caderno",
                                                     "motif-margem",
                                                     "motif-sal",
                                                     "farol-luz",
                                                     "farol-visita",
                                                     "farol-sabotagem",
                                                     "farol-garrafa",
                                                     "fac-conselho",
                                                     "fac-arquivistas"};
    for (const auto *key : novella_scope)
      static_cast<void>(service_.narrative().add_entity_to_work(
          entities_.at(key), works_.at("novela"),
          "Elemento ativo ou documentado na novela de arquivo."));

    const std::array<const char *, 30> story_scope{"nael",
                                                   "nara",
                                                   "safira",
                                                   "celina",
                                                   "sol",
                                                   "joana",
                                                   "bento",
                                                   "cora",
                                                   "rui",
                                                   "leon",
                                                   "bairro-baixo",
                                                   "mercado",
                                                   "jardins",
                                                   "tipografia",
                                                   "cais",
                                                   "rota-azul",
                                                   "fac-cooperativa",
                                                   "fac-navegantes",
                                                   "fac-radio",
                                                   "fac-jardineiros",
                                                   "fac-tipografos",
                                                   "art-radio",
                                                   "art-matriz",
                                                   "motif-sete",
                                                   "motif-maos",
                                                   "motif-peixes",
                                                   "goal-rota",
                                                   "goal-evacuar",
                                                   "info-rota",
                                                   "info-sete"};
    for (const auto *key : story_scope)
      static_cast<void>(service_.narrative().add_entity_to_work(
          entities_.at(key), works_.at("contos"),
          "Material compartilhado, reinterpretável pela perspectiva do "
          "conto."));

    for (const auto &key : entity_keys_) {
      if (key.rfind("info-", 0) == 0 || key.rfind("fac-", 0) == 0 ||
          key.rfind("art-", 0) == 0 || key.rfind("motif-", 0) == 0 ||
          key == "vespera" || key == "arquivo" || key == "farol" ||
          key == "camara")
        static_cast<void>(service_.narrative().add_entity_to_work(
            entities_.at(key), works_.at("dossie"),
            "Referência de continuidade; inclui grau de certeza e fonte "
            "interna."));
    }

    for (std::size_t i = 0; i < 24; ++i) {
      const auto &event = events_[i];
      const auto node =
          nodes_.at("romance-chapter-" + std::to_string(i % 20 + 1) +
                    (i % 2 == 0 ? "-a" : "-b"));
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(event.key), node,
          "acontecimento — " + std::string(event.name),
          "A cena dramatiza este acontecimento; a cronologia mantém a ordem "
          "causal."));
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(event.location), node,
          "cenário — " + std::string(event.name),
          "O espaço deve influenciar ação, acesso e risco, não servir apenas "
          "de fundo."));
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(event.participants[0]), node,
          "agente — " + std::string(event.name),
          "A revisão deve conferir objetivo local, custo e decisão "
          "observável."));
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(event.participants[1]), node,
          "contraponto — " + std::string(event.name),
          "Oferece interpretação ou interesse diferente na mesma situação."));
    }

    const std::array<const char *, 12> infos{
        "info-protocolo", "info-ciclo",    "info-carta", "info-custo",
        "info-rota",      "info-comporta", "info-lente", "info-ada",
        "info-sete",      "info-mapa",     "info-sal",   "info-decreto"};
    for (std::size_t i = 0; i < infos.size(); ++i)
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(infos[i]),
          nodes_.at("romance-chapter-" + std::to_string(i + 1)),
          "informação plantada",
          "Introduzir de forma verificável sem encerrar cedo demais a "
          "interpretação."));

    for (std::size_t i = 0; i < 4; ++i) {
      const auto &event = events_[28 + i];
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(event.key),
          nodes_.at("novela-chapter-" + std::to_string(i * 2 + 1)),
          "registro dramatizado",
          "A carta apresenta uma versão parcial deste acontecimento."));
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(event.participants[0]),
          nodes_.at("novela-chapter-" + std::to_string(i * 2 + 1)),
          "voz ou presença documental",
          "A confiabilidade depende da fonte do capítulo."));
    }

    const std::array<const char *, 18> dossier_entities{
        "vespera",         "farol",          "camara",         "fac-conselho",
        "fac-cooperativa", "fac-navegantes", "art-chave",      "art-atlas",
        "art-lente",       "art-garrafa",    "info-protocolo", "info-custo",
        "info-ada",        "motif-margem",   "motif-sete",     "motif-sal",
        "motif-maos",      "motif-peixes"};
    for (std::size_t i = 0; i < dossier_entities.size(); ++i)
      static_cast<void>(service_.narrative().add_editorial_reference(
          entities_.at(dossier_entities[i]),
          nodes_.at("dossie-entry-" + std::to_string(i + 1)),
          "verbete de continuidade",
          "Registrar fonte, grau de certeza, contradições e margem para "
          "derivações."));
  }

  void create_documents() {
    create_start_document();
    create_prologue_document();
    create_salt_on_paper_document();
    create_archive_document();
    create_two_maps_document();
    create_evacuation_document();
    create_open_atlas_document();
    create_lighthouse_letter_document();
    create_salt_garden_document();
    create_protocol_dossier_document();
    create_continuity_notebook_document();
    create_discarded_scene_document();
  }

  void organize_documents() {
    const auto manuscript = service_.writing().create_document_group(
        "Manuscritos por obra",
        "Textos principais e narrativas complementares em desenvolvimento.");
    const auto support = service_.writing().create_document_group(
        "Pesquisa e continuidade",
        "Roteiros de avaliação, anotações e referências de consistência.");
    const auto incubator = service_.writing().create_document_group(
        "Incubadora e versões",
        "Cenas retiradas e revisões documentais preservadas para comparação.");

    const auto organize =
        [&](const std::string &key, const inde::project::DocumentGroup &group,
            inde::project::DocumentPurpose purpose,
            const std::string &perspective = {},
            const std::optional<std::string> &revision_of = std::nullopt,
            const std::string &revision_label = {}) {
          auto document = *service_.writing().document(documents_.at(key));
          document.group_id = group.id;
          document.purpose = purpose;
          document.perspective = perspective;
          document.revision_of_id = revision_of;
          document.revision_label = revision_label;
          static_cast<void>(service_.writing().update_document(document));
        };
    organize("start", support, inde::project::DocumentPurpose::Annotation,
             "Sessão de avaliação");
    organize("protocol-entry", support,
             inde::project::DocumentPurpose::Reference, "Dossiê técnico");
    organize("continuity", support, inde::project::DocumentPurpose::Annotation,
             "Continuidade editorial");
    organize("prologue", manuscript, inde::project::DocumentPurpose::MainText,
             "Ada Avelar");
    organize("chapter-1", manuscript, inde::project::DocumentPurpose::MainText,
             "Lia Avelar");
    organize("chapter-6", manuscript, inde::project::DocumentPurpose::MainText,
             "Lia Avelar");
    organize("chapter-11", manuscript, inde::project::DocumentPurpose::MainText,
             "Lia e Nael");
    organize("chapter-17", manuscript, inde::project::DocumentPurpose::MainText,
             "Safira");
    organize("chapter-20", manuscript, inde::project::DocumentPurpose::MainText,
             "Lia Avelar");
    organize("letter-1", manuscript, inde::project::DocumentPurpose::MainText,
             "Ada Avelar");
    organize("story-2", manuscript, inde::project::DocumentPurpose::MainText,
             "Celina Prado");
    organize("discarded", incubator, inde::project::DocumentPurpose::Revision,
             "Safira e Celina", documents_.at("chapter-1"),
             "versão retirada — negociação duplicada");
  }

  void create_start_document() {
    const std::string content = R"inde(00 — COMECE AQUI
Sessão integrada de avaliação

Objetivo desta cópia

Este é um projeto de trabalho, não uma vitrine vazia. Os dados foram organizados
para que uma pessoa consiga partir de uma pergunta narrativa, localizar as
entidades envolvidas, observar a cronologia, chegar à unidade editorial e então
ler ou alterar o texto correspondente. Nada precisa ser memorizado por UUID.

Rota Planejamento → Escrita

No Planejamento, procure Lia Avelar. Abra a personagem, consulte as relações e a
seção de Documentos. A partir dali, abra “Capítulo 1 — Sal no papel”. Volte à
Biblioteca e filtre por Lia para comparar todos os textos em que ela foi
vinculada explicitamente. A presença do nome no corpo não substitui o vínculo.

Rota Editorial → Escrita

No Catálogo, abra “A Cidade de Vidro e Sal”. Na estrutura, entre na primeira
cena do prólogo e abra o Documento associado. Use “Abrir unidade” para retornar
ao ponto exato da árvore. Em seguida selecione a Parte I e observe as entidades
herdadas dos elementos filhos, sempre acompanhadas da unidade de origem.

Rota Tempo → Fonte

Em Gráficos, selecione a cronologia principal e investigue o começo da maré.
Abra a fonte de um acontecimento e confirme que o Planejamento apresenta o fato,
seus participantes, o local e as presenças sem transformar a timeline numa
segunda fonte de verdade.

Escrita longa e ferramentas

Abra o prólogo, clique no corpo, escreva uma frase temporária e use Desfazer.
Selecione um trecho, aplique negrito ou itálico, crie uma âncora e relacione uma
entidade à âncora. O painel Ferramentas deve desaparecer quando não estiver em
uso e o editor deve continuar recebendo cliques em toda a área restante.

Busca explicável

Pesquise “atlas” na Biblioteca. O card deve explicar se encontrou o termo no
nome, no conteúdo ou em ambos e priorizar correspondências de nome. Depois
pesquise “certeza” e compare o manuscrito com o verbete técnico. Nos filtros do
Planejamento, combine Personagem com uso em Documento e confirme que o motivo
principal permanece legível.

Este roteiro é deliberadamente um Documento livre. Ele prova que texto pode
existir antes de ganhar colocação editorial, sem ser tratado como sobra ou erro.)inde";
    auto document = begin_document("00 — Comece aqui: sessão integrada",
                                   content, std::nullopt, 650);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "00 — COMECE AQUI");
    add_format(document, inde::project::DocumentTextStyle::Subheading,
               "Sessão integrada de avaliação");
    add_format(document, inde::project::DocumentTextStyle::Bold,
               "Objetivo desta cópia");
    const auto editorial = add_anchor(document, "Rota Editorial–Escrita",
                                      "Rota Editorial → Escrita");
    const auto writing = add_anchor(document, "Teste de escrita longa",
                                    "Escrita longa e ferramentas");
    add_document_reference(
        document, "lia", editorial,
        "Personagem usada como entrada da navegação integrada.");
    add_document_reference(document, "art-atlas", writing,
                           "Termo de controle para a busca explicável.");
    finish_document("start", std::move(document));
  }

  void create_prologue_document() {
    const std::string content = R"inde(PRÓLOGO — A LUZ SOB A ÁGUA
Dezoito anos antes

O farol respirou antes da primeira badalada.

Ada Avelar estava acordada havia tempo suficiente para distinguir o ruído das
engrenagens do som que a torre fazia quando não havia ninguém olhando. O
primeiro era regular, um roçar de bronze e sal no interior das paredes. O outro
vinha das fundações. Entrava pelo piso de observação, subia pelos ossos das
pernas e fazia o vidro das lentes pulsar como uma pálpebra indecisa.

Ela pousou a caneca sobre o livro de vigília. A água lá fora alcançava a terceira
marca da janela, dois palmos abaixo do máximo previsto. Mesmo assim, uma luz
azulada corria sob a superfície. Não era reflexo da lua. Desenhava quarteirões.
Ruas inteiras se acendiam no mar onde naquela época só existiam pedras negras e
um canal fundo demais para ancoragem.

Ada abriu o registro na página daquela noite e escreveu: “A cidade apareceu
antes de ser construída.” Ficou olhando a frase. O papel absorveu a tinta, mas
as palavras voltaram mais claras, como se outra mão tivesse escrito por baixo.

No pavimento inferior, Tomé Varga chamou seu nome.

— Não desça — ela respondeu.

O aviso chegou tarde. Tomé surgiu na escada com o caderno apertado contra o
peito. Era jovem o bastante para acreditar que todo fenômeno aceitava uma
legenda e velho o bastante para desconfiar de quem oferecia a legenda depressa
demais. Aproximou-se da janela, viu o mapa submerso e perdeu a cor.

— Essas ruas não existem.

— Ainda — disse Ada.

A respiração do farol

A torre puxou o ar. As sete lâminas da lente meridiana giraram sem comando, uma
após a outra. Em cada lâmina apareceu uma parte da cidade futura: pontes de
vidro, jardins alimentados por água salgada, um bairro inteiro coberto pela
maré. Havia pessoas nos telhados. Algumas agitavam panos. Outras observavam o
farol como se soubessem que Ada estava ali.

Tomé abriu o caderno. Uma linha de sua própria anotação desapareceu enquanto
ele a lia.

— A máquina está usando memória outra vez.

Ada fechou o registro de vigília.

— Então não é uma máquina de prever. É uma máquina de escolher o que devemos
esquecer para acreditar numa previsão.

Na mesa de instrumentos, a chave de nácar começou a aquecer. Ada a envolveu num
pano e desceu. A cada lance de escada a água parecia mais próxima, embora as
janelas permanecessem secas. No laboratório inferior, encontrou uma garrafa
azul presa entre os tubos de pressão. O vidro trazia duas datas de fabricação:
aquela semana e dezoito anos depois.

Ela lavou a garrafa, retirou o selo ainda intacto e colocou dentro dela uma folha
do atlas que Tomé não tinha visto. Na folha, três Vésperas ocupavam o mesmo
arquipélago. Uma seria salva pelo Conselho. Uma seria abandonada para que a
cidade alta permanecesse seca. A terceira estava incompleta e coberta de
correções feitas por centenas de mãos.

— Qual delas aconteceu? — perguntou Tomé.

— Essa é a pergunta errada.

Ada escreveu uma carta para a filha que dormia do outro lado da baía. Não
explicou tudo. Explicações completas eram outra forma de ordenar a vida de quem
ainda não podia responder. Deixou coordenadas, três nomes e uma instrução:
quando o mapa parecer certo demais, procure o que ele precisou apagar.

O nome raspado

Antes do amanhecer, homens do Conselho cruzaram a ponte de serviço. Ada ouviu as
botas na plataforma e entregou a garrafa ao canal de drenagem. A corrente a
tomou sem ruído. Depois voltou ao livro de vigília e raspou o próprio nome da
página, não para desaparecer, mas para obrigar quem encontrasse o registro a
perguntar por que aquela ausência tinha o formato exato de uma assinatura.

Quando a porta superior se abriu, a luz sob a água já havia apagado. Restava a
respiração do farol e, no horizonte, uma cidade que ainda ignorava ter sido
lembrada pelo futuro.)inde";
    auto document = begin_document("Prólogo — A luz sob a água", content,
                                   "romance-prologo-a", 1800);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "PRÓLOGO — A LUZ SOB A ÁGUA");
    add_format(document, inde::project::DocumentTextStyle::Subheading,
               "Dezoito anos antes");
    add_format(document, inde::project::DocumentTextStyle::Quote,
               "A cidade apareceu");
    const auto breath =
        add_anchor(document, "Primeira ativação", "A respiração do farol");
    const auto bottle = add_anchor(document, "A garrafa enviada",
                                   "Ada escreveu uma carta para a filha");
    const auto erased =
        add_anchor(document, "Ausência deliberada", "O nome raspado");
    add_document_reference(document, "ada", breath,
                           "Ponto de vista e agente da primeira ativação.");
    add_document_reference(document, "farol", breath,
                           "Cenário ativo, tratado como mecanismo e lugar.");
    add_document_reference(document, "farol-luz", breath,
                           "Acontecimento dramatizado neste trecho.");
    add_document_reference(document, "art-garrafa", bottle,
                           "Objeto que transporta a carta entre épocas.");
    add_document_reference(document, "info-carta", bottle,
                           "Conteúdo parcial destinado a Lia.");
    add_document_reference(
        document, "info-ada", erased,
        "A ausência funda versões incompatíveis do paradeiro.");
    finish_document("prologue", std::move(document));
  }

  void create_salt_on_paper_document() {
    const std::string content = R"inde(CAPÍTULO 1 — SAL NO PAPEL
Lia

O mapa amanheceu molhado apenas nas ruas que ainda estavam secas.

Lia Avelar percebeu isso porque conhecia cada rasgo da folha. Passara três dias
repondo fibras na margem leste, onde o papel havia sido dobrado durante a última
evacuação. Às seis horas, as linhas restauradas estavam firmes. Às seis e sete,
uma mancha azul surgiu sobre o Bairro Baixo e avançou contra a inclinação da
mesa.

Ela encostou o mata-borrão. O papel bebeu água salgada.

Iara Nóbrega chegou ao laboratório carregando duas caixas de registros e parou
na porta.

— Você derramou alguma coisa?

— Se eu disser que não, você começa pela proveniência ou pela acusação?

— Pela janela. Está fechada?

Estava. Também estavam fechadas as válvulas, os frascos e a pequena comporta que
isolava a sala do reservatório. Iara aproximou uma lâmpada do mapa. Sob a luz,
a mancha deixou de parecer água e mostrou ruas desenhadas numa camada que não
existia no inventário.

A data de amanhã

No canto inferior havia um carimbo: Conselho das Cartas, inspeção concluída,
amanhã às dezesseis horas. A tinta era antiga. A data não.

Lia levou o mapa ao atlas de vidro. As lâminas responderam com um estalo e
projetaram três linhas de costa sobre a parede. Na primeira, o Bairro Baixo
desaparecia. Na segunda, a água alcançava o arquivo. Na terceira, dezenas de
rotas azuis cortavam a cidade sem obedecer às pontes oficiais.

Iara não perguntou qual futuro era verdadeiro. Perguntou quem havia catalogado
a folha por último. Era uma pergunta melhor e por isso assustava mais.

No registro, o nome do restaurador anterior fora raspado. Restava apenas a
pressão da escrita e um fragmento de letra: A.

— Ada? — disse Iara.

Lia guardou o mata-borrão. Havia dezoito anos ninguém pronunciava o nome de sua
mãe dentro do Arquivo das Águas. O lugar tinha regras para umidade, fogo,
contaminação e guerra; para aquele nome, possuía somente silêncio.

O primeiro desvio

O sino administrativo anunciou abertura. Em poucos minutos chegariam os
supervisores. Lia podia registrar a anomalia, entregar o mapa e confiar no mesmo
sistema que apagara a assinatura. Ou podia esconder uma cópia antes que a
história voltasse a ser corrigida sem testemunhas.

Iara abriu uma das caixas vazias.

— Eu não vi nada — disse.

— Você é péssima mentirosa.

— Sou excelente arquivista. Significa que sei distinguir ausência de prova e
prova de ausência.

Elas trabalharam sem combinar o resto. Lia fotografou as três camadas. Iara
copiou o registro de acesso. Quando passos ecoaram no corredor, o original já
estava outra vez sob o vidro, perfeitamente seco, fingindo que nunca havia
tentado avisá-las.)inde";
    auto document = begin_document("Capítulo 1 — Sal no papel", content,
                                   "romance-chapter-1-a", 2600);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CAPÍTULO 1 — SAL NO PAPEL");
    add_format(document, inde::project::DocumentTextStyle::Subheading, "Lia");
    add_format(
        document, inde::project::DocumentTextStyle::Italic,
        "O mapa amanheceu molhado apenas nas ruas que ainda estavam secas.");
    const auto date =
        add_anchor(document, "Carimbo impossível", "A data de amanhã");
    const auto choice =
        add_anchor(document, "Primeira escolha de Lia", "O primeiro desvio");
    add_document_reference(document, "lia", choice,
                           "Ponto de vista; decide preservar a cópia.");
    add_document_reference(document, "iara", choice,
                           "Aliada e contraponto metodológico.");
    add_document_reference(document, "arquivo", date,
                           "Local do aparecimento do mapa.");
    add_document_reference(document, "ev-mapa", date,
                           "Acontecimento correspondente na cronologia.");
    add_document_reference(document, "art-atlas", date,
                           "Instrumento usado para revelar as três camadas.");
    finish_document("chapter-1", std::move(document));
  }

  void create_archive_document() {
    const std::string content = R"inde(CAPÍTULO 6 — O ARQUIVO DAS ÁGUAS
Terceiro rascunho

A porta pneumática abriu para dentro, embora toda pressão indicasse que deveria
ter esmagado quem tentasse movê-la. Lia entrou primeiro. Caio manteve a chave de
nácar encostada no leitor e Milo segurou o cabo que os ligava ao corredor seco.

Do outro lado havia fileiras de tanques transparentes. Em cada tanque, folhas,
fotografias e pequenos objetos flutuavam numa solução densa. Etiquetas de cobre
traziam datas que não apareciam no catálogo público. Algumas antecediam a
fundação de Véspera. Outras ainda não tinham acontecido.

O tanque sem número

No centro da sala, um reservatório permanecia sem identificação. Quando Lia se
aproximou, vozes atravessaram a água. Não eram gravações completas, mas começos
de frases, nomes esquecidos no instante anterior à pronúncia, lembranças sem
dono tentando aderir a qualquer pessoa que escutasse.

Caio desligou o amplificador.

— Foi isso que alimentou os primeiros modelos — disse. — O Conselho chamava de
ruído residual.

Milo mergulhou uma pinça e retirou um cristal de sal. Dentro dele, uma criança
corria numa ponte que Lia reconheceu sem jamais ter visto. Ao tocar o cristal,
ela lembrou a temperatura da mão da criança e esqueceu por alguns segundos a
cor dos olhos de Ada.

O custo da certeza

Tomé havia dito que toda previsão cobrava memória. A sala mostrava algo pior:
as memórias não desapareciam. Eram removidas de suas comunidades, separadas de
contexto e usadas como matéria-prima. A cidade não pagava apenas com esquecimento;
pagava com a redistribuição secreta de quem tinha direito ao próprio passado.

Lia abriu o caderno de campo. Em vez de anotar uma conclusão, registrou três
perguntas: de quem era cada fragmento, quem autorizara a extração e como devolver
uma lembrança sem impor a ninguém uma versão estranha de si.

Caio queria copiar os dados. Milo queria quebrar os tanques. Lia decidiu que
nenhuma das duas ações podia acontecer sem testemunhas do Bairro Baixo. Fechou a
porta e marcou o corredor com tinta azul, não como segredo, mas como promessa de
retorno coletivo.)inde";
    auto document = begin_document("Capítulo 6 — O Arquivo das Águas", content,
                                   "romance-chapter-6-a", 3000);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CAPÍTULO 6 — O ARQUIVO DAS ÁGUAS");
    add_format(document, inde::project::DocumentTextStyle::Subheading,
               "Terceiro rascunho");
    const auto tank = add_anchor(document, "Descoberta do reservatório",
                                 "O tanque sem número");
    const auto cost =
        add_anchor(document, "Formulação do custo", "O custo da certeza");
    add_document_reference(document, "lia", cost,
                           "Reformula o problema ético da investigação.");
    add_document_reference(
        document, "caio", tank,
        "Conhece a infraestrutura e sua participação anterior.");
    add_document_reference(document, "milo", tank,
                           "Extrai a evidência material do tanque.");
    add_document_reference(document, "arquivo", tank,
                           "Local e sistema de custódia das memórias.");
    add_document_reference(document, "info-custo", cost,
                           "Informação explicitada sem encerrar a solução.");
    add_document_reference(document, "ev-abertura", tank,
                           "Acontecimento situado no sexto ponto principal.");
    finish_document("chapter-6", std::move(document));
  }

  void create_two_maps_document() {
    const std::string content = R"inde(CAPÍTULO 11 — DOIS FUTUROS
Versão para revisão de estrutura

A praça recebeu os mapas ao meio-dia.

O Conselho estendeu uma projeção branca sobre a fachada da Câmara: ruas secas,
abrigos numerados, setas precisas. No mapa oficial, ninguém corria. A água
obedecia às comportas e cada família ocupava o lugar calculado para ela.

No Cais das Vozes, Bento Sal abriu um rolo de papel azul produzido pela Oficina
das Margens. A projeção da Cooperativa admitia falhas. Algumas rotas terminavam
em perguntas. Outras mudavam conforme a disponibilidade de barcos, cordas e
pessoas dispostas a abrir as próprias casas.

A disputa pública

Maíra Seles falou primeiro pela rede oficial. Disse que a precisão era a única
defesa contra o pânico. Bento respondeu pela Rádio Ponte que um plano exato,
feito sem os ameaçados, era apenas uma ordem com boa cartografia.

Leon Marulho recebeu os dois documentos na cabine. Poderia escolher um e dar ao
público a tranquilidade de uma única voz. Em vez disso, descreveu as diferenças:
o mapa branco salvava o Bairro Alto em todos os cenários; o azul preservava mais
vidas, mas dependia de decisões ainda não tomadas.

— Não temos dois futuros — ele disse ao microfone. — Temos duas propostas sobre
quem pode decidir.

A frase atravessou as boias. Na praça, pessoas começaram a copiar à mão as
rotas que conheciam. Em minutos havia dezenas de mapas menores, contraditórios e
úteis. Maíra observou a fachada coberta de correções e percebeu que perdera o
monopólio antes de perder o argumento.

Lia não comemorou. Informação pública também podia ferir quando chegava sem
contexto. Ela marcou no próprio atlas quais trechos eram previsão, quais eram
ordem e quais eram apenas esperança. Pela primeira vez, o mapa ficou menos
elegante e mais honesto.)inde";
    auto document = begin_document("Capítulo 11 — Dois futuros", content,
                                   "romance-chapter-11-a", 2800);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CAPÍTULO 11 — DOIS FUTUROS");
    add_format(document, inde::project::DocumentTextStyle::Quote,
               "Não temos dois futuros");
    const auto dispute =
        add_anchor(document, "Confronto dos mapas", "A disputa pública");
    add_document_reference(document, "maira", dispute,
                           "Defende a precisão como instrumento de governo.");
    add_document_reference(document, "bento", dispute,
                           "Expõe o custo distributivo do plano oficial.");
    add_document_reference(
        document, "leon", dispute,
        "Recusa transformar incerteza em falsa equivalência.");
    add_document_reference(document, "cais", dispute,
                           "Espaço onde o mapa se torna debate público.");
    add_document_reference(
        document, "ev-mapas", dispute,
        "Acontecimento correspondente na cronologia principal.");
    add_document_reference(document, "info-mapa", dispute,
                           "As três camadas são reinterpretadas publicamente.");
    finish_document("chapter-11", std::move(document));
  }

  void create_evacuation_document() {
    const std::string content = R"inde(CAPÍTULO 17 — EVACUAÇÃO DO BAIRRO BAIXO
Rascunho de trabalho

A lista oficial tinha quatrocentos e doze nomes. O Bairro Baixo tinha mais de
duas mil pessoas e nenhuma delas podia esperar que o erro fosse corrigido antes
da maré.

Nara transformou a estufa em centro de rotas. Cada mesa representava uma rua;
sementes vermelhas marcavam quem precisava de ajuda para caminhar, conchas
brancas indicavam crianças e ferramentas de cobre mostravam onde ainda havia
barcos. Bento recebia mensagens das comportas. Lia desenhava caminhos que
mudavam mais depressa do que a tinta podia secar.

Onze minutos

A Rota Azul abriria às dezessete e vinte e três. Permaneceria navegável por onze
minutos se a comporta três não falhasse, se o vento mantivesse leste e se a
Guarda não bloqueasse o canal. Nenhuma dessas condições era certeza. Por isso
o plano tinha alternativas e pessoas autorizadas a abandoná-lo.

Lia entregou uma cópia a cada coordenador. No alto da página escreveu: “Se a
realidade discordar deste mapa, a realidade vence.”

Quando a água entrou pelas oficinas, ninguém esperou uma ordem central. Nara
abriu os jardins como abrigo. Bento manteve a comporta com uma corrente presa ao
próprio barco. Cora atravessou a rua principal recolhendo quem não aparecia em
lista alguma.

A porta sem número

Na última casa, Lia encontrou uma porta sem marca. O atlas não mostrava
moradores ali. Bateu mesmo assim. Uma mulher respondeu carregando o pai e uma
caixa de fotografias. Não havia espaço para os três objetos no barco.

Lia tomou a caixa. Não porque papel valesse mais que gente, mas porque a mulher
não precisava escolher sozinha qual parte da vida abandonar. Correram até o
canal quando a Rota Azul começava a fechar.

O último barco passou no décimo segundo minuto.

Mais tarde os relatórios chamariam aquilo de margem de erro. Lia registrou os
nomes de quem segurou a passagem aberta por sessenta segundos adicionais. Uma
cidade era também a diferença entre a duração calculada e o tempo que pessoas
conseguiam criar umas para as outras.)inde";
    auto document = begin_document("Capítulo 17 — Evacuação do Bairro Baixo",
                                   content, "romance-chapter-17-a", 3200);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CAPÍTULO 17 — EVACUAÇÃO DO BAIRRO BAIXO");
    add_format(document, inde::project::DocumentTextStyle::Quote,
               "realidade discordar deste mapa, a realidade vence.");
    const auto route =
        add_anchor(document, "Janela da Rota Azul", "Onze minutos");
    const auto door =
        add_anchor(document, "Pessoa fora da lista", "A porta sem número");
    add_document_reference(document, "lia", door,
                           "Transforma o princípio em escolha concreta.");
    add_document_reference(document, "bento", route,
                           "Mantém a comporta durante a travessia.");
    add_document_reference(document, "nara", route,
                           "Coordena rotas e abrigo comunitário.");
    add_document_reference(document, "bairro-baixo", door,
                           "Espaço ameaçado e rede social da evacuação.");
    add_document_reference(document, "goal-evacuar", route,
                           "Objetivo operacional deste capítulo.");
    add_document_reference(
        document, "info-rota", route,
        "Informação necessária, mas não suficiente, para o plano.");
    add_document_reference(document, "ev-evacuacao", route,
                           "Acontecimento dramatizado no texto.");
    finish_document("chapter-17", std::move(document));
  }

  void create_open_atlas_document() {
    const std::string content = R"inde(CAPÍTULO 20 — ATLAS ABERTO
Primeiro rascunho completo

Joana colocou a primeira matriz sobre a mesa do cais. Não havia espaço para uma
folha do tamanho da cidade, então imprimiu quadrados que podiam ser reunidos em
qualquer ordem. Cada pessoa recebeu um pedaço e a responsabilidade de corrigi-lo.

Mil mãos no papel

Iara lia em voz alta a origem de cada informação. Leon transmitia as correções
pela rádio. Quando duas versões entravam em conflito, nenhuma era apagada: as
fontes ficavam lado a lado até que alguém pudesse voltar ao lugar, medir a água
e conversar com quem morava ali.

Maíra entregou o mapa do Conselho sem discurso. Bento o abriu, comparou as
comportas e marcou três decisões que ainda precisavam de resposta pública. Não
houve perdão instantâneo nem consenso perfeito. Houve procedimento.

Lia levou o atlas de vidro até a beira do cais. As lâminas mostraram uma última
previsão: Véspera intacta, limpa de toda discordância. Ela virou o instrumento
para baixo. A imagem caiu na água e se partiu entre as ondas.

Margens em branco

No fim da tarde, crianças pediram páginas vazias. Desenharam passagens que só
existiam na maré baixa, escadas improvisadas, casas onde qualquer pessoa podia
pedir abrigo e um peixe de cobre maior que o observatório. Sol escreveu no canto:
“Este mapa ainda não sabe tudo.”

Lia assinou abaixo, não como autora, mas como uma das testemunhas.)inde";
    auto document = begin_document("Capítulo 20 — Atlas aberto", content,
                                   "romance-chapter-20-b", 2600);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CAPÍTULO 20 — ATLAS ABERTO");
    add_format(document, inde::project::DocumentTextStyle::Bold,
               "Mil mãos no papel");
    const auto public_map =
        add_anchor(document, "Cartografia pública", "Mil mãos no papel");
    const auto blank =
        add_anchor(document, "Imagem final", "Margens em branco");
    add_document_reference(document, "iara", public_map,
                           "Expõe proveniência e divergências.");
    add_document_reference(document, "joana", public_map,
                           "Produz a matriz distribuída.");
    add_document_reference(document, "leon", public_map,
                           "Transmite as correções pela Rádio Ponte.");
    add_document_reference(document, "cais", public_map,
                           "Local da assembleia do atlas.");
    add_document_reference(
        document, "goal-pacto", public_map,
        "Objetivo alcançado como processo, não estado perfeito.");
    add_document_reference(document, "ev-atlas-publico", public_map,
                           "Acontecimento correspondente na cronologia.");
    add_document_reference(document, "motif-margem", blank,
                           "Motivo visual da abertura para decisões futuras.");
    finish_document("chapter-20", std::move(document));
  }

  void create_lighthouse_letter_document() {
    const std::string content = R"inde(CARTA 1 — A LUZ INVERTIDA
Ada para Lia, destinatária futura

Lia,

hoje a luz do farol apareceu debaixo da água. Você tem quatro anos e acha que
todo farol serve para chamar barcos para casa. Espero que continue certa por
mais tempo do que eu.

Os homens do Conselho dizem que encontramos uma forma de ver a tempestade antes
que ela se forme. Não dizem que a lente precisa de lembranças para completar o
que não sabe. Primeiro desaparecem detalhes: uma canção, o cheiro de uma cozinha,
o rosto de alguém visto de passagem. Depois somem relações inteiras entre as
coisas, e a previsão parece mais limpa justamente porque já não lembramos das
alternativas.

Instrução para um mapa certo demais

Se esta carta chegar a você, procure três sinais. Uma página cuja data ainda não
existe. Sete badaladas sem vento. Uma garrafa fabricada duas vezes. Não confie
em quem disser que os sinais provam apenas uma história — inclusive se essa
pessoa for eu.

Tomé acredita que podemos corrigir a máquina. Talvez possamos. Eu acredito que
primeiro precisamos impedir que uma única pessoa decida quais memórias a cidade
pode gastar.

Vou enviar a chave pelo canal azul. Não use a chave para abrir uma porta sem
perguntar quem ficou do lado de fora.

Com amor e com a parte da verdade que consigo oferecer hoje,

Ada.)inde";
    auto document = begin_document("Carta 1 — A luz invertida", content,
                                   "novela-chapter-1-a", 1600);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CARTA 1 — A LUZ INVERTIDA");
    add_format(document, inde::project::DocumentTextStyle::Italic,
               "Ada para Lia, destinatária futura");
    const auto instruction = add_anchor(document, "Instrução de Ada",
                                        "Instrução para um mapa certo demais");
    add_document_reference(document, "ada", instruction,
                           "Voz documental e autora da carta.");
    add_document_reference(document, "tome", instruction,
                           "Aliado citado com posição diferente.");
    add_document_reference(document, "farol", std::nullopt,
                           "Lugar de emissão da carta.");
    add_document_reference(document, "art-garrafa", instruction,
                           "Meio material de preservação e transporte.");
    add_document_reference(document, "farol-luz", std::nullopt,
                           "Acontecimento que motiva a escrita.");
    finish_document("letter-1", std::move(document));
  }

  void create_salt_garden_document() {
    const std::string content = R"inde(JARDIM PARA UMA ÁGUA SALGADA
Conto — abertura

Nara dizia que nenhuma planta era teimosa. Teimoso era o jardineiro que pedia à
raiz para esquecer a água em que crescera.

Depois da tempestade, os canteiros receberam o dobro de sal. Técnicos do Bairro
Alto recomendaram abandonar as estufas e importar alimento por seis meses. Nara
guardou o relatório sob uma pedra, reuniu os Jardineiros de Sal e distribuiu
sementes entre recipientes com concentrações diferentes.

O primeiro broto

Sol apareceu todos os dias para medir a água. Não sabia ainda transformar as
medições em tabela, então desenhava peixes: um peixe para água doce, dois para
água difícil, três para a água que fazia arder um corte na mão.

O primeiro broto nasceu no recipiente de três peixes.

Nara levou a planta à assembleia. Não afirmou que o problema estava resolvido.
Mostrou a folha, o sal acumulado no vidro e os sete recipientes em que nada
crescera. A cidade precisava tanto dos fracassos quanto do broto para decidir o
que plantar.

Meses depois, visitantes chamariam aquilo de milagre agrícola. Sol corrigiria
as placas toda vez: “Não foi milagre. Foi muita gente prestando atenção.”)inde";
    auto document = begin_document("Conto 2 — Jardim para uma água salgada",
                                   content, "conto-2-opening", 2200);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "JARDIM PARA UMA ÁGUA SALGADA");
    const auto sprout =
        add_anchor(document, "Experimento dos recipientes", "O primeiro broto");
    add_document_reference(document, "nara", sprout,
                           "Protagonista e autora do experimento.");
    add_document_reference(document, "sol", sprout,
                           "Cria a notação concreta das medições.");
    add_document_reference(document, "jardins", sprout,
                           "Lugar de reconstrução após a tempestade.");
    add_document_reference(document, "fac-jardineiros", std::nullopt,
                           "Coletivo responsável pelo trabalho distribuído.");
    add_document_reference(
        document, "motif-sal", sprout,
        "Motivo reaparece como condição material, não metáfora vazia.");
    finish_document("story-2", std::move(document));
  }

  void create_protocol_dossier_document() {
    const std::string content = R"inde(VERBETE 7 — PROTOCOLO NÁCAR
Estado: estabelecido quanto ao mecanismo; disputado quanto à origem

Definição operacional

Procedimento de previsão que combina pressão oceânica, observação celeste e
fragmentos de memória humana. A precisão cresce quando o sistema elimina
alternativas consideradas improváveis. A eliminação não é apenas matemática:
ela reduz ou desloca lembranças associadas às alternativas descartadas.

O que o projeto trata como fato

1. O Protocolo foi executado ao menos duas vezes.
2. A Câmara das Correntes armazena resíduos de memória cristalizada.
3. O Conselho conhecia parte do custo cognitivo antes da crise principal.
4. Nenhuma previsão demonstrada inclui decisões tomadas depois da calibração.

Fontes internas

Livro de vigília do Farol Nacarado; caderno de Tomé Varga; tanques sem catálogo
do Arquivo das Águas; depoimentos incompatíveis de Ada Avelar e Orfeu Manso;
medições da lente meridiana conduzidas por Yara Pontal.

Perguntas deliberadamente abertas

Não está decidido quem iniciou o primeiro ciclo, se Ada permanece fisicamente
ligada à Câmara nem se memórias podem ser devolvidas sem criar uma nova versão
coercitiva. Produções futuras devem citar a fonte quando escolherem uma versão,
em vez de promover automaticamente uma hipótese a cânone.)inde";
    auto document = begin_document("Verbete — Protocolo Nácar", content,
                                   "dossie-entry-7", 900);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "VERBETE 7 — PROTOCOLO NÁCAR");
    add_format(
        document, inde::project::DocumentTextStyle::Subheading,
        "Estado: estabelecido quanto ao mecanismo; disputado quanto à origem");
    const auto facts = add_anchor(document, "Fatos estabelecidos",
                                  "O que o projeto trata como fato");
    const auto open = add_anchor(document, "Questões de continuidade",
                                 "Perguntas deliberadamente abertas");
    add_document_reference(document, "info-protocolo", facts,
                           "Objeto principal deste verbete.");
    add_document_reference(document, "info-custo", facts,
                           "Consequência confirmada por várias fontes.");
    add_document_reference(document, "fac-conselho", facts,
                           "Instituição que controlou a execução recente.");
    add_document_reference(document, "info-ada", open,
                           "Questão preservada como hipótese concorrente.");
    add_document_reference(document, "arquivo", std::nullopt,
                           "Fonte material citada pelo verbete.");
    finish_document("protocol-entry", std::move(document));
  }

  void create_continuity_notebook_document() {
    const std::string content =
        R"inde(CADERNO DE CONTINUIDADE — PERGUNTAS ABERTAS
Documento livre de produção

Paradeiro de Ada

Manter três versões ativas até a revisão do Capítulo 18: morte durante a primeira
sabotagem; exílio entre os Navegantes Livres; integração parcial à Câmara das
Correntes. O prólogo comprova ação e intenção, não o destino posterior.

Regra de evidência: quando um Documento escolher uma versão para a voz de um
personagem, vincular a entidade “Paradeiro de Ada” e registrar a fonte no trecho.
Não alterar a cronologia principal para representar lembrança subjetiva.

Margens em branco

O motivo deve aparecer quando uma escolha permanece compartilhável. Evitar
usá-lo como simples decoração em cenas onde a decisão já foi encerrada. No
Capítulo 20, as páginas vazias significam abertura de processo; na novela, a
margem raspada significa censura. A repetição precisa conservar a diferença.

Pendências para sessão humana

- conferir se Lia reconhece cedo demais a letra de Ada;
- comparar os Documentos dos capítulos 1 e 6 pelo filtro de Lia;
- verificar se o verbete técnico revela mais do que o romance deve saber;
- decidir se a Carta 1 permanece antes ou depois do prólogo na edição;
- testar retorno Escrita → Editorial sem perder a posição na Biblioteca.)inde";
    auto document =
        begin_document("Caderno de continuidade — perguntas abertas", content,
                       std::nullopt, 800);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CADERNO DE CONTINUIDADE — PERGUNTAS ABERTAS");
    const auto ada =
        add_anchor(document, "Hipóteses sobre Ada", "Paradeiro de Ada");
    const auto margins =
        add_anchor(document, "Uso do motivo", "Margens em branco");
    add_document_reference(document, "info-ada", ada,
                           "Hipótese mantida explicitamente aberta.");
    add_document_reference(
        document, "ada", ada,
        "Pessoa cuja identidade não se reduz à dúvida de continuidade.");
    add_document_reference(document, "lia", ada,
                           "Perspectiva afetada pelas versões concorrentes.");
    add_document_reference(document, "motif-margem", margins,
                           "Motivo acompanhado entre romance e novela.");
    finish_document("continuity", std::move(document));
  }

  void create_discarded_scene_document() {
    const std::string content = R"inde(CENA DESCARTADA — A PONTE QUE NÃO EXISTIA
Sem colocação editorial

Safira encontrou Celina no meio da Ponte Nove, num trecho que nenhum mapa
registrava naquela manhã. A cidade havia deslocado as plataformas durante a
noite, e por vinte minutos a ponte ligava o mercado diretamente ao cais.

— Podemos usar isso na fuga — disse Celina.

Safira mediu a corrente e respondeu que uma rota disponível não era ainda uma
rota segura. Tinham barcos para seis pessoas, informação para cinquenta e uma
dívida que faria os Navegantes cobrarem passagem no pior momento.

As duas atravessaram mesmo assim. No lado oposto, marcaram a madeira com tinta
azul para que Nael reconhecesse o caminho.

Nota de corte

A cena foi retirada do Capítulo 5 porque repetia a negociação apresentada no
Capítulo 14. O Documento permanece livre para possível uso no conto
“Contrabando de nomes”. Sua existência não deve criar uma cena editorial falsa
nem alterar a cronologia do acontecimento da fuga.)inde";
    auto document = begin_document("Cena descartada — A ponte que não existia",
                                   content, std::nullopt, std::nullopt);
    add_format(document, inde::project::DocumentTextStyle::Heading,
               "CENA DESCARTADA — A PONTE QUE NÃO EXISTIA");
    add_format(document, inde::project::DocumentTextStyle::Strikethrough,
               "Nota de corte");
    const auto bridge =
        add_anchor(document, "Rota provisória", "Safira encontrou Celina");
    add_document_reference(document, "safira", bridge,
                           "Avalia o risco da rota temporária.");
    add_document_reference(document, "celina", bridge,
                           "Propõe reaproveitar o deslocamento da ponte.");
    add_document_reference(document, "ponte-nove", bridge,
                           "Local transitório da cena retirada.");
    add_document_reference(
        document, "ev-fuga", std::nullopt,
        "A cena tangencia o acontecimento sem substituí-lo.");
    finish_document("discarded", std::move(document));
  }

  void create_narrative_structures() {
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

  void audit() const {
    const auto database_path = output_ / "data" / "project.sqlite3";
    inde::persistence::SqliteDatabase database(database_path);
    const std::array<const char *, 29> tables{"intellectual_properties",
                                              "works",
                                              "editorial_nodes",
                                              "entity_types",
                                              "entities",
                                              "relation_types",
                                              "relations",
                                              "entity_work_scopes",
                                              "editorial_entity_references",
                                              "fictional_time_axes",
                                              "fictional_time_points",
                                              "event_occurrences",
                                              "document_groups",
                                              "documents",
                                              "document_format_spans",
                                              "document_anchors",
                                              "document_entity_references",
                                              "structure_models",
                                              "structure_model_items",
                                              "structure_model_item_links",
                                              "editorial_structures",
                                              "narrative_structures",
                                              "narrative_lines",
                                              "narrative_units",
                                              "narrative_unit_lines",
                                              "narrative_unit_entities",
                                              "narrative_links",
                                              "narrative_roles",
                                              "narrative_role_assignments"};
    std::cout << "Projeto criado: " << output_ << '\n';
    std::cout << "UUID: " << service_.current()->manifest().project_id << '\n';
    for (const auto *table : tables)
      std::cout << table << '='
                << database.query_integer(std::string("SELECT COUNT(*) FROM ") +
                                          table)
                << '\n';
    std::cout << "event_participations="
              << database.query_integer(
                     "SELECT COUNT(*) FROM event_participations")
              << '\n';
    std::cout << "entity_presences="
              << database.query_integer("SELECT COUNT(*) FROM entity_presences")
              << '\n';
    std::cout << "change_log="
              << database.query_integer("SELECT COUNT(*) FROM change_log")
              << '\n';
    std::cout << "foreign_key_violations="
              << database.query_integer(
                     "SELECT COUNT(*) FROM pragma_foreign_key_check")
              << '\n';
    const auto require_count = [&database](const std::string &label,
                                           const std::string &query,
                                           std::int64_t expected) {
      const auto found = database.query_integer(query);
      if (found != expected)
        throw std::runtime_error(label + ": esperado " +
                                 std::to_string(expected) + ", obtido " +
                                 std::to_string(found));
    };
    require_count("Documentos", "SELECT COUNT(*) FROM documents", 12);
    require_count("Grupos documentais", "SELECT COUNT(*) FROM document_groups",
                  3);
    require_count("Documentos agrupados",
                  "SELECT COUNT(*) FROM documents WHERE group_id IS NOT NULL",
                  12);
    require_count(
        "Revisões documentais",
        "SELECT COUNT(*) FROM documents WHERE revision_of_id IS NOT NULL", 1);
    require_count(
        "Documentos colocados",
        "SELECT COUNT(*) FROM documents WHERE editorial_node_id IS NOT NULL",
        9);
    require_count(
        "Documentos livres",
        "SELECT COUNT(*) FROM documents WHERE editorial_node_id IS NULL", 3);
    require_count("Obras com texto",
                  "SELECT COUNT(DISTINCT n.work_id) FROM documents d "
                  "JOIN editorial_nodes n ON n.id = d.editorial_node_id",
                  4);
    require_count("Intervalos de formatação",
                  "SELECT COUNT(*) FROM document_format_spans", 25);
    require_count("Âncoras", "SELECT COUNT(*) FROM document_anchors", 21);
    require_count("Referências documentais",
                  "SELECT COUNT(*) FROM document_entity_references", 62);
    require_count("Estruturas editoriais alternativas",
                  "SELECT COUNT(*) FROM editorial_structures", 5);
    require_count("Estruturas narrativas",
                  "SELECT COUNT(*) FROM narrative_structures", 5);
    require_count("Linhas narrativas", "SELECT COUNT(*) FROM narrative_lines",
                  6);
    require_count("Unidades narrativas", "SELECT COUNT(*) FROM narrative_units",
                  12);
    require_count("Pertencimentos narrativos",
                  "SELECT COUNT(*) FROM narrative_unit_lines", 18);
    require_count("Vínculos narrativos", "SELECT COUNT(*) FROM narrative_links",
                  12);
    require_count("Papéis narrativos contextuais",
                  "SELECT COUNT(*) FROM narrative_role_assignments", 3);
    require_count("Violações de chave estrangeira",
                  "SELECT COUNT(*) FROM pragma_foreign_key_check", 0);
    if (database.query_text("PRAGMA integrity_check") != "ok")
      throw std::runtime_error("integrity_check do projeto integral falhou");
  }

  std::filesystem::path output_;
  inde::application::ProjectService service_;
  std::map<std::string, std::string> ips_;
  std::map<std::string, std::string> works_;
  std::map<std::string, std::string> nodes_;
  std::map<std::string, std::string> types_;
  std::map<std::string, std::string> entities_;
  std::vector<std::string> entity_keys_;
  std::map<std::string, std::string> relation_types_;
  std::map<std::string, std::string> axes_;
  std::map<std::string, std::vector<std::string>> points_;
  std::map<std::string, std::string> occurrences_;
  std::map<std::string, std::string> documents_;
  std::vector<EventSpec> events_;
};

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Uso: inde_demo_project_generator CAMINHO_DO_PROJETO.inde\n";
    return EXIT_FAILURE;
  }
  try {
    const auto output = std::filesystem::absolute(argv[1]).lexically_normal();
    DemoBuilder(output).build();
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "Falha ao criar projeto demonstrativo: " << error.what()
              << '\n';
    return EXIT_FAILURE;
  }
}

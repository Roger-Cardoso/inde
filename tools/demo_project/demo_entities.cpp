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

void DemoBuilder::create_entities() {
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

void DemoBuilder::create_event_entities() {
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

} // namespace inde::demo

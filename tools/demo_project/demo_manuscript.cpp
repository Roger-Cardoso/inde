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

void DemoBuilder::create_prologue_document() {
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

void DemoBuilder::create_salt_on_paper_document() {
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

void DemoBuilder::create_archive_document() {
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

void DemoBuilder::create_two_maps_document() {
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

void DemoBuilder::create_evacuation_document() {
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

void DemoBuilder::create_open_atlas_document() {
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

} // namespace inde::demo

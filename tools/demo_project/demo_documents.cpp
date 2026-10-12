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

std::pair<std::size_t, std::size_t>
DemoBuilder::unicode_range(const std::string &content, const std::string &needle) {
    const auto byte_start = content.find(needle);
    if (byte_start == std::string::npos)
      throw std::runtime_error("trecho documental não encontrado: " + needle);
    const auto start =
        inde::project::utf8_character_count(content.substr(0, byte_start));
    return {start, start + inde::project::utf8_character_count(needle)};
  }

void DemoBuilder::add_format(inde::project::Document &document,
                       inde::project::DocumentTextStyle style,
                       const std::string &needle) {
    const auto [start, end] = unicode_range(document.content, needle);
    document.formatting.push_back({style, start, end});
  }

std::string DemoBuilder::add_anchor(inde::project::Document &document,
                              const std::string &label,
                              const std::string &needle) {
    const auto [start, end] = unicode_range(document.content, needle);
    const auto id = inde::project::new_uuid();
    document.anchors.push_back({id, label, start, end});
    return id;
  }

void DemoBuilder::add_document_reference(inde::project::Document &document,
                            const std::string &entity,
                            const std::optional<std::string> &anchor,
                            const std::string &notes) {
    document.entity_references.push_back(
        {inde::project::new_uuid(), entities_.at(entity), anchor, notes});
  }

inde::project::Document DemoBuilder::begin_document(const std::string &title,
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

void DemoBuilder::finish_document(const std::string &key,
                     inde::project::Document document) {
    document = service_.writing().update_document(document);
    documents_[key] = document.id;
  }

void DemoBuilder::create_documents() {
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

void DemoBuilder::organize_documents() {
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

void DemoBuilder::create_start_document() {
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

void DemoBuilder::create_protocol_dossier_document() {
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

void DemoBuilder::create_continuity_notebook_document() {
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

void DemoBuilder::create_discarded_scene_document() {
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

} // namespace inde::demo

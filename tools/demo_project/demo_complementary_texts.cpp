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

void DemoBuilder::create_lighthouse_letter_document() {
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

void DemoBuilder::create_salt_garden_document() {
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

} // namespace inde::demo

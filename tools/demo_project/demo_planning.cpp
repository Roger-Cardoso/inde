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

void DemoBuilder::create_relations() {
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

void DemoBuilder::create_timeline() {
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

void DemoBuilder::create_scopes_and_references() {
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

} // namespace inde::demo

#pragma once

#include "inde/project/narrative.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace inde::application {

// Estado local e descartavel do explorador. Nao concede permissao, nao altera
// fatos narrativos e nao e persistido no projeto.
struct PlanningContext {
  std::string search;
  std::optional<std::string> work_id;
  std::vector<std::string> entity_type_ids;
  std::vector<std::string> relation_type_ids;
  std::optional<std::string> related_entity_id;
  // O eixo restringe o explorador a entidades ligadas a fatos naquele eixo.
  // Quando há ponto, Acontecimentos/participações precisam ocorrer nele e
  // presenças precisam estar ativas nele. A entidade presente e o Local da
  // presença entram no resultado: ambos explicam a situação ficcional.
  std::optional<std::string> fictional_axis_id;
  std::optional<std::string> fictional_time_point_id;
  // Um período fechado é sempre uma dupla pertencente ao mesmo eixo. Ele é
  // mantido no contexto para que Planejamento, Agenda e Gráficos possam
  // projetar a mesma fatia ficcional.
  std::optional<std::string> fictional_window_start_time_point_id;
  std::optional<std::string> fictional_window_end_time_point_id;
  // Refere uma unidade editorial e seus descendentes, sem confundir a
  // apresentação editorial com o escopo factual da Obra.
  std::optional<std::string> editorial_node_id;
  // Escrita apenas reduz o universo consultado: o vínculo continua sendo uma
  // referência explícita e não injeta dados de Planejamento no texto.
  bool require_document_reference{};
  std::optional<std::string> document_id;
  std::size_t limit{100};
  std::size_t offset{};
};

struct PlanningResultItem {
  project::NarrativeEntity entity;
  // A razão principal responde à pergunta que trouxe o autor ao explorador.
  // Os demais critérios apenas delimitam a lista e podem ser consultados sob
  // demanda, sem transformar cada cartão em um parágrafo de filtros.
  std::string primary_reason;
  std::vector<std::string> auxiliary_reasons;
  // Explicação completa, preservada para tooltips, acessibilidade e outras
  // projeções que precisem auditar a inclusão do resultado.
  std::string inclusion_reason;
};

struct PlanningFacet {
  std::string id;
  std::string label;
  std::size_t count{};
  bool selected{};
};

struct PlanningExplorerSnapshot {
  PlanningContext context;
  std::vector<PlanningResultItem> items;
  std::size_t matching_count{};
  std::vector<PlanningFacet> entity_type_facets;
  std::vector<PlanningFacet> relation_type_facets;
  bool has_previous{};
  bool has_next{};
};

} // namespace inde::application

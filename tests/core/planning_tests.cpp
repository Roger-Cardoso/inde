#include "inde/application/catalog_service.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/project_session.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/spelling_provider.hpp"
#include "inde/application/structural_service.hpp"
#include "inde/application/temporal_view.hpp"
#include "inde/application/timeline_projection.hpp"
#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_importer.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_catalog_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_structural_repository.hpp"
#include "inde/persistence/structural_repository.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

#include "core_test_cases.hpp"
#include "test_support.hpp"

namespace inde::tests {

void planning_places_events_in_explicit_fictional_time() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;

  bool guarded = false;
  try {
    static_cast<void>(service.planning().time_axes());
  } catch (const std::exception &) {
    guarded = true;
  }
  assert(guarded);

  service.create(temporary.path / "Planejamento", "Cenário temporal");
  auto &narrative = service.narrative();
  auto &planning = service.planning();
  const auto types = narrative.entity_types();
  const auto character_type = find_entity_type(types, "character").id;
  const auto location_type = find_entity_type(types, "location").id;
  const auto event_type = find_entity_type(types, "event").id;

  const auto axes = planning.time_axes();
  assert(axes.size() == 1);
  assert(axes.front().is_default);
  assert(axes.front().name == "Cronologia principal");

  auto secondary = planning.create_time_axis("Memórias", "Ordem recordada");
  secondary.name = "Cronologia das memórias";
  secondary.is_default = true; // O serviço deve preservar a marca persistida.
  secondary = planning.update_time_axis(secondary);
  assert(!secondary.is_default);

  const auto arrival = planning.create_time_point(
      axes.front().id, 1000, "Chegada à cidade", "Primeiro marco conhecido");
  const auto aftermath = planning.create_time_point(
      axes.front().id, 2000, "Depois da tempestade", "Segundo marco");
  const auto memory_point = planning.create_time_point(
      secondary.id, 1000, "Lembrança da chegada", "Outro eixo");
  const auto loaded_arrival = planning.time_point(arrival.id);
  assert(loaded_arrival.has_value());
  assert(loaded_arrival->label == "Chegada à cidade");
  assert(!planning.time_point(inde::project::new_uuid()).has_value());
  const auto ordered = planning.time_points(axes.front().id);
  assert(ordered.size() == 2);
  assert(ordered.front().id == arrival.id);
  assert(ordered.back().id == aftermath.id);
  inde::persistence::PlanningQuery second_point_page;
  second_point_page.limit = 1;
  second_point_page.offset = 1;
  assert(planning.time_points(axes.front().id, second_point_page).front().id ==
         aftermath.id);
  inde::persistence::PlanningQuery searched_points;
  searched_points.search = "tempestade";
  const auto matching_points =
      planning.time_points(axes.front().id, searched_points);
  assert(matching_points.size() == 1);
  assert(matching_points.front().id == aftermath.id);
  searched_points.search = "%";
  assert(planning.time_points(axes.front().id, searched_points).empty());
  bool excessive_planning_page_rejected = false;
  try {
    second_point_page.limit = 501;
    static_cast<void>(planning.time_points(axes.front().id, second_point_page));
  } catch (const std::exception &) {
    excessive_planning_page_rejected = true;
  }
  assert(excessive_planning_page_rejected);
  bool duplicate_ordinal_rejected = false;
  try {
    static_cast<void>(planning.create_time_point(
        axes.front().id, 1000, "Mesmo lugar na ordem", "Inválido"));
  } catch (const std::exception &) {
    duplicate_ordinal_rejected = true;
  }
  assert(duplicate_ordinal_rejected);

  {
    inde::persistence::SqliteDatabase database(service.current()->path() /
                                               "data" / "project.sqlite3");
    const auto axes_before = planning.time_axes().size();
    database.execute(
        "CREATE TRIGGER test_reject_planning_log BEFORE INSERT ON change_log "
        "BEGIN SELECT RAISE(ABORT, 'test planning log failure'); END");
    bool atomic_planning_log_failure = false;
    try {
      static_cast<void>(planning.create_time_axis("Não deve persistir"));
    } catch (const std::exception &) {
      atomic_planning_log_failure = true;
    }
    assert(atomic_planning_log_failure);
    assert(planning.time_axes().size() == axes_before);
    database.execute("DROP TRIGGER test_reject_planning_log");
  }

  const auto alice = narrative.create_entity(character_type, "Alice", "");
  const auto harbor = narrative.create_entity(location_type, "Porto", "");
  const auto storm =
      narrative.create_entity(event_type, "A tempestade", "Muda a cidade");
  const auto loaded_alice = narrative.entity(alice.id);
  assert(loaded_alice.has_value());
  assert(loaded_alice->name == "Alice");
  assert(!narrative.entity(inde::project::new_uuid()).has_value());

  bool non_event_rejected = false;
  try {
    static_cast<void>(planning.place_event(alice.id, arrival.id));
  } catch (const std::exception &) {
    non_event_rejected = true;
  }
  assert(non_event_rejected);

  auto occurrence = planning.place_event(
      storm.id, arrival.id, "A tempestade começa quando Alice chega");
  const auto loaded_occurrence = planning.event_occurrence_for_entity(storm.id);
  assert(loaded_occurrence.has_value());
  assert(loaded_occurrence->id == occurrence.id);
  assert(!planning.event_occurrence_for_entity(alice.id).has_value());
  auto participant = planning.add_participant(occurrence.id, alice.id,
                                              "testemunha", "Observa do cais");
  const auto place = planning.add_participant(
      occurrence.id, harbor.id, "local afetado", "O porto é inundado");
  assert(planning.event_occurrences().size() == 1);
  assert(planning.event_participations(occurrence.id).size() == 2);

  auto presence =
      planning.create_presence(alice.id, harbor.id, arrival.id, std::nullopt,
                               "Alice permanece no porto");
  inde::persistence::PresenceQuery alice_presences;
  alice_presences.entity_id = alice.id;
  assert(planning.presences(alice_presences).size() == 1);
  inde::persistence::PresenceQuery harbor_presences;
  harbor_presences.location_entity_id = harbor.id;
  assert(planning.presences(harbor_presences).front().id == presence.id);
  presence.end_time_point_id = aftermath.id;
  presence = planning.update_presence(presence);
  assert(presence.end_time_point_id == aftermath.id);

  bool non_location_rejected = false;
  try {
    static_cast<void>(planning.create_presence(storm.id, alice.id, arrival.id,
                                               std::nullopt, "Local inválido"));
  } catch (const std::exception &) {
    non_location_rejected = true;
  }
  assert(non_location_rejected);
  bool reversed_presence_rejected = false;
  try {
    static_cast<void>(planning.create_presence(
        alice.id, harbor.id, aftermath.id, arrival.id, "Intervalo invertido"));
  } catch (const std::exception &) {
    reversed_presence_rejected = true;
  }
  assert(reversed_presence_rejected);
  const auto planning_log_before_guard = narrative.change_log().size();
  const auto now = inde::project::utc_now();
  inde::project::EntityPresence reversed_direct{inde::project::new_uuid(),
                                                alice.id,
                                                harbor.id,
                                                aftermath.id,
                                                arrival.id,
                                                "Intervalo invertido direto",
                                                now,
                                                now};
  bool database_presence_guarded = false;
  try {
    inde::persistence::SqlitePlanningRepository{}.save(
        service.current()->path(), reversed_direct);
  } catch (const std::exception &) {
    database_presence_guarded = true;
  }
  assert(database_presence_guarded);
  assert(narrative.change_log().size() == planning_log_before_guard);
  bool cross_axis_presence_rejected = false;
  try {
    static_cast<void>(planning.create_presence(
        alice.id, harbor.id, arrival.id, memory_point.id, "Eixos misturados"));
  } catch (const std::exception &) {
    cross_axis_presence_rejected = true;
  }
  assert(cross_axis_presence_rejected);
  bool duplicate_presence_rejected = false;
  try {
    static_cast<void>(planning.create_presence(alice.id, harbor.id, arrival.id,
                                               aftermath.id, "Duplicada"));
  } catch (const std::exception &) {
    duplicate_presence_rejected = true;
  }
  assert(duplicate_presence_rejected);

  auto invalid_harbor = harbor;
  invalid_harbor.entity_type_id = character_type;
  bool location_reclassification_rejected = false;
  try {
    static_cast<void>(narrative.update_entity(invalid_harbor));
  } catch (const std::exception &) {
    location_reclassification_rejected = true;
  }
  assert(location_reclassification_rejected);
  auto moved_arrival = arrival;
  moved_arrival.ordinal = 500;
  bool referenced_point_move_rejected = false;
  try {
    static_cast<void>(planning.update_time_point(moved_arrival));
  } catch (const std::exception &) {
    referenced_point_move_rejected = true;
  }
  assert(referenced_point_move_rejected);

  auto invalid_storm = storm;
  invalid_storm.entity_type_id = character_type;
  bool placed_event_reclassification_rejected = false;
  try {
    static_cast<void>(narrative.update_entity(invalid_storm));
  } catch (const std::exception &) {
    placed_event_reclassification_rejected = true;
  }
  assert(placed_event_reclassification_rejected);
  auto invalid_alice = alice;
  invalid_alice.entity_type_id = event_type;
  bool participant_reclassification_rejected = false;
  try {
    static_cast<void>(narrative.update_entity(invalid_alice));
  } catch (const std::exception &) {
    participant_reclassification_rejected = true;
  }
  assert(participant_reclassification_rejected);

  participant.role = "protagonista";
  participant = planning.update_participation(participant);
  assert(participant.role == "protagonista");
  occurrence.time_point_id = aftermath.id;
  occurrence = planning.update_event_occurrence(occurrence);
  assert(occurrence.time_point_id == aftermath.id);

  bool duplicate_occurrence_rejected = false;
  try {
    static_cast<void>(planning.place_event(storm.id, arrival.id));
  } catch (const std::exception &) {
    duplicate_occurrence_rejected = true;
  }
  assert(duplicate_occurrence_rejected);

  bool event_as_participant_rejected = false;
  try {
    static_cast<void>(planning.add_participant(occurrence.id, storm.id,
                                               "evento participante"));
  } catch (const std::exception &) {
    event_as_participant_rejected = true;
  }
  assert(event_as_participant_rejected);

  bool referenced_point_rejected = false;
  try {
    planning.delete_time_point(aftermath.id);
  } catch (const std::exception &) {
    referenced_point_rejected = true;
  }
  assert(referenced_point_rejected);

  bool default_axis_rejected = false;
  try {
    planning.delete_time_axis(axes.front().id);
  } catch (const std::exception &) {
    default_axis_rejected = true;
  }
  assert(default_axis_rejected);

  const auto source_id = service.current()->manifest().project_id;
  const auto copy_path = temporary.path / "Planejamento copiado";
  service.save_as(copy_path);
  const auto copy_id = service.current()->manifest().project_id;
  assert(copy_id != source_id);
  assert(service.planning().time_axes().size() == 2);
  assert(service.planning().event_occurrences().front().id == occurrence.id);
  assert(service.planning().event_participations(occurrence.id).size() == 2);
  assert(service.planning().presences(alice_presences).front().id ==
         presence.id);
  {
    inde::persistence::SqliteDatabase copied_database(
        service.current()->path() / "data" / "project.sqlite3");
    for (const char *table :
         {"fictional_time_axes", "fictional_time_points", "event_occurrences",
          "event_participations", "entity_presences"}) {
      assert(copied_database.query_text("SELECT DISTINCT project_id FROM " +
                                        std::string(table)) == copy_id);
    }
  }

  planning.remove_participation(place.id);
  planning.remove_event_occurrence(occurrence.id);
  assert(planning.event_participations(occurrence.id).empty());
  planning.remove_presence(presence.id);
  planning.delete_time_point(aftermath.id);
  planning.delete_time_point(arrival.id);
  planning.delete_time_point(memory_point.id);
  planning.delete_time_axis(secondary.id);
  assert(planning.time_axes().size() == 1);
  assert(narrative.change_log().front().command_name ==
         "delete_fictional_time_axis");
}

void temporal_queries_are_limited_inside_the_selected_axis() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  service.create(temporary.path / "Dois eixos", "Dois eixos");
  auto &narrative = service.narrative();
  auto &planning = service.planning();
  const auto types = narrative.entity_types();
  const auto event_type = find_entity_type(types, "event").id;
  const auto character_type = find_entity_type(types, "character").id;
  const auto location_type = find_entity_type(types, "location").id;
  const auto primary = planning.time_axes().front();
  const auto secondary = planning.create_time_axis("Linha alternativa");
  const auto primary_point =
      planning.create_time_point(primary.id, 10, "Presente");
  const auto secondary_point =
      planning.create_time_point(secondary.id, 10, "Possibilidade");
  const auto primary_event =
      narrative.create_entity(event_type, "Fato principal");
  const auto secondary_event =
      narrative.create_entity(event_type, "Fato alternativo");
  const auto character = narrative.create_entity(character_type, "Lia");
  const auto location = narrative.create_entity(location_type, "Porto");
  const auto primary_occurrence =
      planning.place_event(primary_event.id, primary_point.id);
  const auto secondary_occurrence =
      planning.place_event(secondary_event.id, secondary_point.id);
  const auto primary_presence = planning.create_presence(
      character.id, location.id, primary_point.id, std::nullopt);
  const auto secondary_presence = planning.create_presence(
      character.id, location.id, secondary_point.id, std::nullopt);

  inde::persistence::EventOccurrenceQuery primary_events;
  primary_events.time_axis_id = primary.id;
  const auto primary_event_page = planning.event_occurrences(primary_events);
  assert(primary_event_page.size() == 1);
  assert(primary_event_page.front().id == primary_occurrence.id);
  primary_events.time_axis_id = secondary.id;
  const auto secondary_event_page = planning.event_occurrences(primary_events);
  assert(secondary_event_page.size() == 1);
  assert(secondary_event_page.front().id == secondary_occurrence.id);
  assert(planning.event_occurrences().size() == 2);

  inde::persistence::PresenceQuery axis_presences;
  axis_presences.time_axis_id = primary.id;
  const auto primary_presence_page = planning.presences(axis_presences);
  assert(primary_presence_page.size() == 1);
  assert(primary_presence_page.front().id == primary_presence.id);
  axis_presences.time_axis_id = secondary.id;
  const auto secondary_presence_page = planning.presences(axis_presences);
  assert(secondary_presence_page.size() == 1);
  assert(secondary_presence_page.front().id == secondary_presence.id);
  assert(planning.presences().size() == 2);
}

void planning_context_combines_filters_and_explains_results() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  service.create(temporary.path / "Contexto", "Contexto do Planejamento");

  const auto ip = service.create_intellectual_property("Universo", "",
                                                       "Fixture do explorador");
  const auto work_a =
      service.create_work(ip.id, "Obra Alfa", "", "", "pt-BR", "Planejamento");
  const auto work_b =
      service.create_work(ip.id, "Obra Beta", "", "", "pt-BR", "Planejamento");
  const auto chapter =
      service
          .create_structural_node(work_a.id, std::nullopt,
                                  inde::project::StructuralNodeType::Chapter,
                                  "Capítulo de investigação")
          .id;
  const auto scene =
      service
          .create_structural_node(work_a.id, chapter,
                                  inde::project::StructuralNodeType::Scene,
                                  "Cena da descoberta")
          .id;
  auto &narrative = service.narrative();
  const auto types = narrative.entity_types();
  const auto character_id = find_entity_type(types, "character").id;
  const auto location_id = find_entity_type(types, "location").id;
  const auto event_id = find_entity_type(types, "event").id;

  const auto ada = narrative.create_entity(character_id, "Ada", "Detetive");
  const auto bruno =
      narrative.create_entity(character_id, "Bruno", "Informante");
  const auto library =
      narrative.create_entity(location_id, "Biblioteca", "Local da pista");
  const auto discovery = narrative.create_entity(
      event_id, "Descoberta", "Acontecimento da segunda Obra");
  const auto distant = narrative.create_entity(
      character_id, "Cecília", "Não participa do recorte temporal");
  static_cast<void>(narrative.add_entity_to_work(ada.id, work_a.id));
  static_cast<void>(narrative.add_entity_to_work(bruno.id, work_a.id));
  static_cast<void>(narrative.add_entity_to_work(library.id, work_a.id));
  static_cast<void>(narrative.add_entity_to_work(discovery.id, work_b.id));

  const auto knows = narrative.create_relation_type(
      "knows", "Conhece", "É conhecido por",
      inde::project::RelationDirectionality::Directed);
  const auto visits = narrative.create_relation_type(
      "visits", "Visita", "É visitado por",
      inde::project::RelationDirectionality::Directed);
  static_cast<void>(narrative.create_relation(knows.id, ada.id, bruno.id));
  static_cast<void>(narrative.create_relation(visits.id, ada.id, library.id));

  const auto axis = service.planning().time_axes().front();
  const auto discovery_point = service.planning().create_time_point(
      axis.id, 100, "Descoberta", "Ponto de prova do explorador");
  const auto occurrence = service.planning().place_event(
      discovery.id, discovery_point.id, "A descoberta ocorre neste ponto");
  static_cast<void>(
      service.planning().add_participant(occurrence.id, ada.id, "descobre"));
  static_cast<void>(service.planning().create_presence(
      bruno.id, library.id, discovery_point.id, std::nullopt,
      "Encontro no arquivo"));
  static_cast<void>(narrative.add_editorial_reference(
      ada.id, scene, "ponto de vista", "A personagem conduz a cena"));

  inde::application::PlanningContext union_context;
  union_context.entity_type_ids = {location_id, character_id, character_id};
  union_context.limit = 2;
  const auto first = service.planning().explore_entities(union_context);
  assert(first.context.entity_type_ids.size() == 2);
  assert(first.matching_count == 4);
  assert(first.items.size() == 2);
  assert(!first.has_previous);
  assert(first.has_next);
  assert(first.items.front().inclusion_reason.find("tipo ") !=
         std::string::npos);
  const auto character_facet = std::find_if(
      first.entity_type_facets.begin(), first.entity_type_facets.end(),
      [&](const auto &facet) { return facet.id == character_id; });
  const auto location_facet = std::find_if(
      first.entity_type_facets.begin(), first.entity_type_facets.end(),
      [&](const auto &facet) { return facet.id == location_id; });
  assert(character_facet != first.entity_type_facets.end());
  assert(location_facet != first.entity_type_facets.end());
  assert(character_facet->count == 3 && character_facet->selected);
  assert(location_facet->count == 1 && location_facet->selected);

  union_context.offset = 2;
  const auto second = service.planning().explore_entities(union_context);
  assert(second.matching_count == 4);
  assert(second.items.size() == 2);
  assert(second.has_previous);
  assert(!second.has_next);

  inde::application::PlanningContext relation_union;
  relation_union.relation_type_ids = {knows.id, visits.id};
  const auto related = service.planning().explore_entities(relation_union);
  assert(related.matching_count == 3);
  const auto knows_facet = std::find_if(
      related.relation_type_facets.begin(), related.relation_type_facets.end(),
      [&](const auto &facet) { return facet.id == knows.id; });
  assert(knows_facet != related.relation_type_facets.end());
  assert(knows_facet->count == 2 && knows_facet->selected);

  inde::application::PlanningContext summary_search;
  summary_search.search = "pista";
  const auto searched = service.planning().explore_entities(summary_search);
  assert(searched.matching_count == 1);
  assert(searched.items.front().entity.id == library.id);
  assert(searched.items.front().inclusion_reason.find("resumo") !=
         std::string::npos);
  assert(searched.items.front().primary_reason.find("texto “pista”") !=
         std::string::npos);
  assert(searched.items.front().auxiliary_reasons.empty());

  const auto named_ada = narrative.create_entity(character_id, "Ada Leste",
                                                 "Uma personagem de apoio");
  const auto summary_ada = narrative.create_entity(
      character_id, "Registro", "Menciona Ada somente no resumo");
  inde::application::PlanningContext ranked_search;
  ranked_search.search = "Ada";
  const auto ranked = service.planning().explore_entities(ranked_search);
  assert(ranked.items.size() >= 3);
  assert(ranked.items.front().entity.id == ada.id);
  const auto summary_position = std::find_if(
      ranked.items.begin(), ranked.items.end(),
      [&](const auto &item) { return item.entity.id == summary_ada.id; });
  const auto named_position = std::find_if(
      ranked.items.begin(), ranked.items.end(),
      [&](const auto &item) { return item.entity.id == named_ada.id; });
  assert(named_position < summary_position);

  inde::application::PlanningContext temporal;
  temporal.fictional_axis_id = axis.id;
  temporal.fictional_time_point_id = discovery_point.id;
  const auto at_discovery = service.planning().explore_entities(temporal);
  assert(at_discovery.context.fictional_axis_id == axis.id);
  assert(at_discovery.matching_count == 4);
  assert(std::ranges::none_of(at_discovery.items, [&](const auto &item) {
    return item.entity.id == distant.id;
  }));
  const auto reason_for = [&](const std::string &entity_id) {
    const auto found = std::find_if(
        at_discovery.items.begin(), at_discovery.items.end(),
        [&](const auto &item) { return item.entity.id == entity_id; });
    assert(found != at_discovery.items.end());
    return found->inclusion_reason;
  };
  assert(reason_for(discovery.id).find("Acontecimento") != std::string::npos);
  assert(reason_for(ada.id).find("participante") != std::string::npos);
  assert(reason_for(bruno.id).find("sujeito de presença") != std::string::npos);
  assert(reason_for(library.id).find("Local de presença") != std::string::npos);

  inde::application::PlanningContext period;
  period.fictional_axis_id = axis.id;
  period.fictional_window_start_time_point_id = discovery_point.id;
  period.fictional_window_end_time_point_id = discovery_point.id;
  const auto period_result = service.planning().explore_entities(period);
  assert(period_result.matching_count == 4);
  const auto derived_period = service.planning().temporal_view_context(period);
  assert(derived_period.fictional_axis_id == axis.id);
  assert(derived_period.window.kind ==
         inde::application::TemporalWindowKind::ClosedRange);
  assert(derived_period.window.start_point_id == discovery_point.id);
  assert(derived_period.window.end_point_id == discovery_point.id);
  const auto period_view = service.planning().temporal_view(period);
  assert(period_view.timeline.points.size() == 1);
  assert(period_view.timeline.events.size() == 1);
  assert(period_view.timeline.presences.size() == 1);

  inde::application::PlanningContext editorial;
  editorial.editorial_node_id = chapter;
  const auto editorial_result = service.planning().explore_entities(editorial);
  assert(editorial_result.context.work_id == work_a.id);
  assert(editorial_result.matching_count == 1);
  assert(editorial_result.items.front().entity.id == ada.id);
  assert(editorial_result.items.front().inclusion_reason.find(
             "apresentada em") != std::string::npos);

  bool incomplete_period_rejected = false;
  try {
    inde::application::PlanningContext invalid_period;
    invalid_period.fictional_window_start_time_point_id = discovery_point.id;
    static_cast<void>(service.planning().explore_entities(invalid_period));
  } catch (const std::exception &) {
    incomplete_period_rejected = true;
  }
  assert(incomplete_period_rejected);

  const auto alternate_axis =
      service.planning().create_time_axis("Alternativa");
  bool incompatible_time_filter_rejected = false;
  try {
    temporal.fictional_axis_id = alternate_axis.id;
    static_cast<void>(service.planning().explore_entities(temporal));
  } catch (const std::exception &) {
    incompatible_time_filter_rejected = true;
  }
  assert(incompatible_time_filter_rejected);

  inde::application::PlanningContext intersection;
  intersection.work_id = work_a.id;
  intersection.entity_type_ids = {character_id};
  intersection.relation_type_ids = {knows.id, visits.id};
  intersection.related_entity_id = ada.id;
  const auto contextual = service.planning().explore_entities(intersection);
  assert(contextual.matching_count == 1);
  assert(contextual.items.front().entity.id == bruno.id);
  assert(contextual.items.front().inclusion_reason.find("Obra Alfa") !=
         std::string::npos);
  assert(contextual.items.front().inclusion_reason.find("Conhece") !=
         std::string::npos);
  assert(contextual.items.front().inclusion_reason.find("Ada") !=
         std::string::npos);
  assert(contextual.items.front().primary_reason.find("Conhece") !=
         std::string::npos);
  assert(contextual.items.front().primary_reason.find("Ada") !=
         std::string::npos);
  assert(contextual.items.front().auxiliary_reasons.size() == 2);

  auto contextual_text = intersection;
  contextual_text.search = "Bruno";
  const auto contextual_text_result =
      service.planning().explore_entities(contextual_text);
  assert(contextual_text_result.matching_count == 1);
  assert(contextual_text_result.items.front().primary_reason.find(
             "texto “Bruno”") != std::string::npos);
  assert(std::any_of(
      contextual_text_result.items.front().auxiliary_reasons.begin(),
      contextual_text_result.items.front().auxiliary_reasons.end(),
      [](const auto &reason) {
        return reason.find("Conhece") != std::string::npos;
      }));

  intersection.work_id = work_b.id;
  assert(service.planning().explore_entities(intersection).matching_count == 0);

  bool excessive_filters_rejected = false;
  try {
    inde::application::PlanningContext invalid;
    invalid.entity_type_ids.assign(33, character_id);
    static_cast<void>(service.planning().explore_entities(invalid));
  } catch (const std::exception &) {
    excessive_filters_rejected = true;
  }
  assert(excessive_filters_rejected);
}

} // namespace inde::tests

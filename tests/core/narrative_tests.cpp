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

void narrative_foundation_enforces_identity_and_relations() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;

  bool guarded = false;
  try {
    static_cast<void>(service.narrative().entity_types());
  } catch (const std::exception &) {
    guarded = true;
  }
  assert(guarded);

  service.create(temporary.path / "Narrativa", "Fundação narrativa");
  auto &narrative = service.narrative();
  const auto builtins = narrative.entity_types();
  assert(builtins.size() == 5);
  assert(std::all_of(builtins.begin(), builtins.end(),
                     [](const auto &value) { return value.is_builtin; }));
  const auto character_id = find_entity_type(builtins, "character").id;
  const auto location_id = find_entity_type(builtins, "location").id;
  assert(find_entity_type(builtins, "event").name == "Acontecimento");
  assert(find_entity_type(builtins, "information").name == "Informação");
  assert(find_entity_type(builtins, "objective").name == "Objetivo");
  assert(narrative.change_log().empty());

  bool builtin_delete_rejected = false;
  try {
    narrative.delete_entity_type(character_id);
  } catch (const std::exception &) {
    builtin_delete_rejected = true;
  }
  assert(builtin_delete_rejected);

  auto custom = narrative.create_entity_type("organization", "Organização",
                                             "Grupo organizado da ficção");
  auto custom_edit = custom;
  custom_edit.key = "key-cannot-change";
  custom_edit.name = "Organização ficcional";
  custom = narrative.update_entity_type(custom_edit);
  assert(custom.key == "organization");
  assert(custom.name == "Organização ficcional");

  auto alice = narrative.create_entity(character_id, "Alice", "Protagonista");
  const auto bob = narrative.create_entity(character_id, "Bob", "Aliado");
  const auto city =
      narrative.create_entity(location_id, "Cidade 100% Real", "Local");
  auto alice_edit = alice;
  alice_edit.summary = "Protagonista atualizada";
  alice = narrative.update_entity(alice_edit);
  assert(alice.summary == "Protagonista atualizada");

  inde::persistence::EntityQuery first_page;
  first_page.limit = 2;
  assert(narrative.entities(first_page).size() == 2);
  first_page.offset = 2;
  assert(narrative.entities(first_page).size() == 1);
  inde::persistence::EntityQuery literal_search;
  literal_search.search = "%";
  assert(narrative.entities(literal_search).size() == 1);
  assert(narrative.entities(literal_search).front().id == city.id);

  auto directed = narrative.create_relation_type(
      "lives-in", "Vive em", "Abriga",
      inde::project::RelationDirectionality::Directed);
  auto directed_edit = directed;
  directed_edit.directionality =
      inde::project::RelationDirectionality::Symmetric;
  directed_edit.name = "Reside em";
  directed = narrative.update_relation_type(directed_edit);
  assert(directed.directionality ==
         inde::project::RelationDirectionality::Directed);
  assert(directed.name == "Reside em");
  auto residence = narrative.create_relation(
      directed.id, alice.id, city.id, "Residência no início da história");
  auto residence_edit = residence;
  residence_edit.description = "Residência confirmada";
  residence = narrative.update_relation(residence_edit);
  assert(residence.description == "Residência confirmada");

  const auto relation_axis =
      service.planning().create_time_axis("Contexto de relação", "");
  const auto relation_point = service.planning().create_time_point(
      relation_axis.id, 42, "Chegada à cidade", "");
  const auto qualified = narrative.create_relation(
      directed.id, bob.id, city.id, "Moradia explicada", relation_point.id,
      city.id, alice.id);
  assert(qualified.fictional_time_point_id == relation_point.id);
  assert(qualified.location_entity_id == city.id);
  assert(qualified.cause_entity_id == alice.id);
  inde::persistence::RelationQuery qualified_query;
  qualified_query.fictional_time_point_id = relation_point.id;
  assert(narrative.relations(qualified_query).size() == 1);
  qualified_query.location_entity_id = city.id;
  qualified_query.cause_entity_id = alice.id;
  assert(narrative.relations(qualified_query).front().id == qualified.id);

  bool non_location_context_rejected = false;
  try {
    static_cast<void>(narrative.create_relation(directed.id, bob.id, alice.id,
                                                "Local inválido", std::nullopt,
                                                alice.id, std::nullopt));
  } catch (const std::exception &) {
    non_location_context_rejected = true;
  }
  assert(non_location_context_rejected);

  bool linked_entity_delete_rejected = false;
  try {
    narrative.delete_entity(alice.id);
  } catch (const std::exception &) {
    linked_entity_delete_rejected = true;
  }
  assert(linked_entity_delete_rejected);

  auto symmetric = narrative.create_relation_type(
      "siblings", "Irmãos", "",
      inde::project::RelationDirectionality::Symmetric);
  const auto lower = std::min(alice.id, bob.id);
  const auto upper = std::max(alice.id, bob.id);
  const auto sibling =
      narrative.create_relation(symmetric.id, upper, lower, "Laço familiar");
  assert(sibling.source_entity_id == lower);
  assert(sibling.target_entity_id == upper);

  // O repositório não pode contornar a ordem canônica exigida pelo banco.
  const auto now = inde::project::utc_now();
  inde::project::NarrativeRelation noncanonical{
      inde::project::new_uuid(), symmetric.id, upper, lower,
      "Ordem invertida direta",  now,          now};
  const auto log_before_guard = narrative.change_log().size();
  bool database_guarded_symmetry = false;
  try {
    inde::persistence::SqliteNarrativeRepository{}.save(
        service.current()->path(), noncanonical);
  } catch (const std::exception &) {
    database_guarded_symmetry = true;
  }
  assert(database_guarded_symmetry);
  assert(narrative.change_log().size() == log_before_guard);

  const auto log_before_duplicate = narrative.change_log().size();
  bool reverse_duplicate_rejected = false;
  try {
    static_cast<void>(
        narrative.create_relation(symmetric.id, lower, upper, "Duplicada"));
  } catch (const std::exception &) {
    reverse_duplicate_rejected = true;
  }
  assert(reverse_duplicate_rejected);
  assert(narrative.change_log().size() == log_before_duplicate);

  bool custom_type_delete_rejected = false;
  const auto organization = narrative.create_entity(custom.id, "Conselho", "");
  try {
    narrative.delete_entity_type(custom.id);
  } catch (const std::exception &) {
    custom_type_delete_rejected = true;
  }
  assert(custom_type_delete_rejected);
  narrative.delete_entity(organization.id);
  narrative.delete_entity_type(custom.id);

  bool linked_relation_type_delete_rejected = false;
  try {
    narrative.delete_relation_type(symmetric.id);
  } catch (const std::exception &) {
    linked_relation_type_delete_rejected = true;
  }
  assert(linked_relation_type_delete_rejected);
  narrative.delete_relation(residence.id);
  narrative.delete_relation(qualified.id);
  narrative.delete_relation(sibling.id);
  narrative.delete_relation_type(directed.id);
  narrative.delete_relation_type(symmetric.id);
  narrative.delete_entity(alice.id);
  assert(!narrative.change_log().empty());
  assert(narrative.change_log().front().command_name == "delete_entity");

  // O isolamento entre projetos é validado antes de chegar à FK composta.
  inde::application::ProjectService other;
  other.create(temporary.path / "Outro", "Outro projeto");
  const auto other_character =
      find_entity_type(other.narrative().entity_types(), "character").id;
  const auto local =
      other.narrative().create_entity(other_character, "Local ao projeto", "");
  const auto other_relation_type = other.narrative().create_relation_type(
      "knows", "Conhece", "É conhecido por",
      inde::project::RelationDirectionality::Directed);
  bool cross_project_rejected = false;
  try {
    static_cast<void>(other.narrative().create_relation(
        other_relation_type.id, bob.id, local.id, "Inválida"));
  } catch (const std::exception &) {
    cross_project_rejected = true;
  }
  assert(cross_project_rejected);
  assert(other.narrative().relations().empty());

  // Se o histórico não puder ser anexado, a alteração principal também volta.
  const auto other_path = other.current()->path();
  inde::persistence::SqliteDatabase database(other_path / "data" /
                                             "project.sqlite3");
  database.execute(
      "CREATE TRIGGER test_reject_change_log BEFORE INSERT ON change_log "
      "BEGIN SELECT RAISE(ABORT, 'test change log failure'); END");
  const auto entities_before = other.narrative().entities().size();
  bool atomic_log_failure = false;
  try {
    static_cast<void>(other.narrative().create_entity(
        other_character, "Não deve persistir", ""));
  } catch (const std::exception &) {
    atomic_log_failure = true;
  }
  assert(atomic_log_failure);
  assert(other.narrative().entities().size() == entities_before);
  database.execute("DROP TRIGGER test_reject_change_log");
}

void narrative_foundation_survives_save_as_independently() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto source_path =
      service.create(temporary.path / "Fonte narrativa", "Fonte narrativa")
          .path();
  const auto source_project_id = service.current()->manifest().project_id;
  const auto character_id =
      find_entity_type(service.narrative().entity_types(), "character").id;
  const auto entity =
      service.narrative().create_entity(character_id, "Personagem copiada", "");

  const auto copy = service.save_as(temporary.path / "Cópia narrativa");
  const auto copy_id = copy.manifest().project_id;
  assert(copy_id != source_project_id);
  const auto copied_entities = service.narrative().entities();
  assert(copied_entities.size() == 1);
  assert(copied_entities.front().id == entity.id);
  assert(service.narrative().change_log().front().command_name ==
         "save_as_project");

  inde::persistence::SqliteDatabase copied_database(copy.path() / "data" /
                                                    "project.sqlite3");
  assert(copied_database.query_integer(
             "SELECT count(*) FROM entity_types WHERE project_id = "
             "(SELECT id FROM projects)") == 5);
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM entities") == copy_id);
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM change_log") == copy_id);

  inde::application::ProjectService original;
  original.open(source_path);
  assert(original.current()->manifest().project_id == source_project_id);
  assert(original.narrative().entities().size() == 1);
  assert(original.narrative().entities().front().id == entity.id);
}

void narrative_editorial_links_preserve_layer_boundaries() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto source_path =
      service.create(temporary.path / "Vínculos", "Vínculos narrativos").path();
  const auto ip_id =
      service.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Romance", "", "", "pt-BR", "Planejamento").id;
  const auto scene_id =
      service
          .create_structural_node(work_id, std::nullopt,
                                  inde::project::StructuralNodeType::Scene,
                                  "Cena do cais")
          .id;
  const auto chapter_id =
      service
          .create_structural_node(work_id, std::nullopt,
                                  inde::project::StructuralNodeType::Chapter,
                                  "Capítulo inicial")
          .id;
  auto &narrative = service.narrative();
  const auto types = narrative.entity_types();
  const auto character_type = find_entity_type(types, "character").id;
  const auto location_type = find_entity_type(types, "location").id;
  const auto event_type = find_entity_type(types, "event").id;
  const auto information_type = find_entity_type(types, "information").id;
  const auto alice =
      narrative.create_entity(character_type, "Alice", "Protagonista");
  const auto secret =
      narrative.create_entity(information_type, "O segredo", "Informação");
  const auto objective_type = find_entity_type(types, "objective").id;
  const auto objective = narrative.create_entity(
      objective_type, "Proteger o segredo", "Objetivo de Alice");
  const auto harbor =
      narrative.create_entity(location_type, "Cais antigo", "Local");
  const auto discovery = narrative.create_entity(
      event_type, "Descoberta do segredo", "Acontecimento");
  const auto axis = service.planning().time_axes().front();
  const auto discovery_point = service.planning().create_time_point(
      axis.id, 1000, "Descoberta", "Alice encontra o segredo");
  const auto occurrence = service.planning().place_event(
      discovery.id, discovery_point.id, "Descoberta no cais");
  static_cast<void>(service.planning().add_participant(
      occurrence.id, alice.id, "descobre", "Protagonista"));
  static_cast<void>(service.planning().create_presence(
      alice.id, harbor.id, discovery_point.id, std::nullopt,
      "Alice está no cais durante a descoberta"));
  const auto pursues = narrative.create_relation_type(
      "pursues", "Persegue", "É perseguido por",
      inde::project::RelationDirectionality::Directed,
      "Uma entidade busca realizar um objetivo");
  const auto motivation = narrative.create_relation(
      pursues.id, alice.id, objective.id, "Motivação da protagonista");
  inde::persistence::RelationQuery alice_relations;
  alice_relations.entity_id = alice.id;
  assert(narrative.relations(alice_relations).front().id == motivation.id);

  bool reference_without_scope_rejected = false;
  try {
    static_cast<void>(narrative.add_editorial_reference(
        secret.id, scene_id, "revelada", "Ainda fora da Obra"));
  } catch (const std::exception &) {
    reference_without_scope_rejected = true;
  }
  assert(reference_without_scope_rejected);

  auto scope = narrative.add_entity_to_work(alice.id, work_id,
                                            "Personagem desta narrativa");
  inde::persistence::EntityWorkScopeQuery by_entity;
  by_entity.entity_id = alice.id;
  assert(narrative.work_scopes(by_entity).size() == 1);
  inde::persistence::EntityWorkScopeQuery by_work;
  by_work.work_id = work_id;
  assert(narrative.work_scopes(by_work).front().id == scope.id);

  bool duplicate_scope_rejected = false;
  try {
    static_cast<void>(narrative.add_entity_to_work(alice.id, work_id));
  } catch (const std::exception &) {
    duplicate_scope_rejected = true;
  }
  assert(duplicate_scope_rejected);

  auto reference = narrative.add_editorial_reference(
      alice.id, scene_id, "aparece", "Alice entra no cais");
  inde::persistence::EditorialReferenceQuery by_node;
  by_node.editorial_node_id = scene_id;
  assert(narrative.editorial_references(by_node).front().id == reference.id);
  inde::persistence::EditorialReferenceQuery by_reference_entity;
  by_reference_entity.entity_id = alice.id;
  assert(narrative.editorial_references(by_reference_entity).size() == 1);

  bool duplicate_reference_rejected = false;
  try {
    static_cast<void>(narrative.add_editorial_reference(
        alice.id, scene_id, "aparece", "Duplicada"));
  } catch (const std::exception &) {
    duplicate_reference_rejected = true;
  }
  assert(duplicate_reference_rejected);

  const auto now = inde::project::utc_now();
  inde::project::EditorialEntityReference unscoped_reference{
      inde::project::new_uuid(),
      secret.id,
      work_id,
      scene_id,
      "revelada",
      "",
      now,
      now};
  const auto log_before_database_guard = narrative.change_log().size();
  bool database_scope_guarded = false;
  try {
    inde::persistence::SqliteNarrativeRepository{}.save(
        service.current()->path(), unscoped_reference);
  } catch (const std::exception &) {
    database_scope_guarded = true;
  }
  assert(database_scope_guarded);
  assert(narrative.change_log().size() == log_before_database_guard);

  auto edited_scope = scope;
  edited_scope.entity_id = secret.id;
  edited_scope.work_id = inde::project::new_uuid();
  edited_scope.notes = "Escopo confirmado";
  scope = narrative.update_work_scope(edited_scope);
  assert(scope.entity_id == alice.id);
  assert(scope.work_id == work_id);
  assert(scope.notes == "Escopo confirmado");

  auto edited_reference = reference;
  edited_reference.entity_id = secret.id;
  edited_reference.work_id = inde::project::new_uuid();
  edited_reference.editorial_node_id = chapter_id;
  edited_reference.purpose = "foco";
  edited_reference.notes = "Ponto de vista da cena";
  reference = narrative.update_editorial_reference(edited_reference);
  assert(reference.entity_id == alice.id);
  assert(reference.work_id == work_id);
  assert(reference.editorial_node_id == scene_id);
  assert(reference.purpose == "foco");

  const auto secret_scope =
      narrative.add_entity_to_work(secret.id, work_id, "Informação da Obra");
  const auto objective_scope = narrative.add_entity_to_work(
      objective.id, work_id, "Objetivo dramático da Obra");
  const auto revelation = narrative.add_editorial_reference(
      secret.id, chapter_id, "revelada", "O leitor descobre o segredo");
  const auto objective_setup = narrative.add_editorial_reference(
      objective.id, chapter_id, "estabelecido", "Motivação apresentada");
  inde::persistence::EditorialReferenceQuery chapter_references;
  chapter_references.editorial_node_id = chapter_id;
  const auto chapter_values =
      narrative.editorial_references(chapter_references);
  assert(chapter_values.size() == 2);
  assert(std::any_of(
      chapter_values.begin(), chapter_values.end(),
      [&](const auto &value) { return value.id == revelation.id; }));
  assert(std::any_of(
      chapter_values.begin(), chapter_values.end(),
      [&](const auto &value) { return value.id == objective_setup.id; }));
  assert(secret_scope.work_id == work_id);
  assert(objective_scope.work_id == work_id);
  const auto harbor_scope =
      narrative.add_entity_to_work(harbor.id, work_id, "Cenário da Obra");
  const auto discovery_scope = narrative.add_entity_to_work(
      discovery.id, work_id, "Acontecimento da Obra");
  const auto setting_reference = narrative.add_editorial_reference(
      harbor.id, chapter_id, "cenário", "Local apresentado");
  const auto event_reference = narrative.add_editorial_reference(
      discovery.id, chapter_id, "dramatizado", "Acontecimento apresentado");
  assert(harbor_scope.work_id == work_id);
  assert(discovery_scope.work_id == work_id);
  assert(setting_reference.editorial_node_id == chapter_id);
  assert(event_reference.editorial_node_id == chapter_id);
  inde::persistence::EditorialReferenceQuery event_presentations;
  event_presentations.work_id = work_id;
  event_presentations.entity_type_ids = {event_type};
  const auto event_values = narrative.editorial_references(event_presentations);
  assert(event_values.size() == 1);
  assert(event_values.front().id == event_reference.id);
  inde::persistence::EditorialReferenceQuery presentation_facets;
  presentation_facets.work_id = work_id;
  const auto facets =
      narrative.editorial_reference_entity_type_facets(presentation_facets);
  assert(std::any_of(facets.begin(), facets.end(), [&](const auto &facet) {
    return facet.id == character_type && facet.count == 1;
  }));
  assert(std::any_of(facets.begin(), facets.end(), [&](const auto &facet) {
    return facet.id == event_type && facet.count == 1;
  }));

  bool empty_purpose_rejected = false;
  try {
    static_cast<void>(narrative.add_editorial_reference(
        alice.id, chapter_id, "", "Finalidade ausente"));
  } catch (const std::exception &) {
    empty_purpose_rejected = true;
  }
  assert(empty_purpose_rejected);

  bool referenced_scope_delete_rejected = false;
  try {
    narrative.remove_entity_from_work(scope.id);
  } catch (const std::exception &) {
    referenced_scope_delete_rejected = true;
  }
  assert(referenced_scope_delete_rejected);

  bool linked_entity_delete_rejected = false;
  try {
    narrative.delete_entity(alice.id);
  } catch (const std::exception &) {
    linked_entity_delete_rejected = true;
  }
  assert(linked_entity_delete_rejected);

  bool linked_work_delete_rejected = false;
  try {
    service.delete_work(work_id);
  } catch (const std::exception &) {
    linked_work_delete_rejected = true;
  }
  assert(linked_work_delete_rejected);

  bool linked_node_delete_rejected = false;
  try {
    service.delete_structural_branch(scene_id);
  } catch (const std::exception &) {
    linked_node_delete_rejected = true;
  }
  assert(linked_node_delete_rejected);
  assert(service.structural_nodes_for_work(work_id).size() == 2);

  inde::persistence::EntityWorkScopeQuery excessive;
  excessive.limit = 501;
  bool excessive_scope_page_rejected = false;
  try {
    static_cast<void>(narrative.work_scopes(excessive));
  } catch (const std::exception &) {
    excessive_scope_page_rejected = true;
  }
  assert(excessive_scope_page_rejected);

  const auto source_project_id = service.current()->manifest().project_id;
  const auto copy = service.save_as(temporary.path / "Vínculos copiados");
  const auto copy_project_id = copy.manifest().project_id;
  assert(copy_project_id != source_project_id);
  assert(service.narrative().work_scopes(by_entity).front().id == scope.id);
  const auto copied_scene_references =
      service.narrative().editorial_references(by_node);
  assert(std::any_of(
      copied_scene_references.begin(), copied_scene_references.end(),
      [&](const auto &value) { return value.id == reference.id; }));
  inde::persistence::SqliteDatabase copied_database(copy.path() / "data" /
                                                    "project.sqlite3");
  for (const char *table :
       {"entity_work_scopes", "editorial_entity_references"})
    assert(copied_database.query_text("SELECT DISTINCT project_id FROM " +
                                      std::string(table)) == copy_project_id);

  service.narrative().remove_editorial_reference(reference.id);
  service.narrative().remove_entity_from_work(scope.id);
  service.delete_structural_branch(scene_id);
  assert(service.structural_nodes_for_work(work_id).size() == 1);
  assert(service.narrative().change_log().front().command_name ==
         "delete_entity_work_scope");

  inde::application::ProjectService original;
  original.open(source_path);
  assert(original.current()->manifest().project_id == source_project_id);
  assert(original.narrative().work_scopes(by_entity).size() == 1);
  assert(original.narrative().editorial_references(by_node).size() == 1);
}

void narrative_queries_remain_bounded_at_scale() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto project_path =
      service.create(temporary.path / "Escala narrativa", "Escala narrativa")
          .path();
  const auto character_id =
      find_entity_type(service.narrative().entity_types(), "character").id;
  inde::persistence::SqliteDatabase database(project_path / "data" /
                                             "project.sqlite3");
  const auto owner = database.query_text("SELECT id FROM projects");
  const auto now = inde::project::utc_now();
  {
    inde::persistence::SqliteTransaction transaction(database);
    auto insert = database.prepare("INSERT INTO entities "
                                   "(id, project_id, entity_type_id, name, "
                                   "summary, created_at, updated_at) "
                                   "VALUES (?, ?, ?, ?, '', ?, ?)");
    for (int index = 0; index < 5000; ++index) {
      char id[37]{};
      std::snprintf(id, sizeof(id), "70000000-0000-4000-8000-%012x", index);
      insert.bind(1, id);
      insert.bind(2, owner);
      insert.bind(3, character_id);
      insert.bind(4, "Personagem " + std::to_string(index));
      insert.bind(5, now);
      insert.bind(6, now);
      insert.run();
      insert.reset();
    }
    transaction.commit();
  }

  assert(service.narrative().entities().size() == 100);
  inde::persistence::EntityQuery page;
  page.entity_type_ids = {character_id};
  page.limit = 125;
  page.offset = 4875;
  assert(service.narrative().entities(page).size() == 125);

  bool excessive_page_rejected = false;
  try {
    page.limit = 501;
    static_cast<void>(service.narrative().entities(page));
  } catch (const std::exception &) {
    excessive_page_rejected = true;
  }
  assert(excessive_page_rejected);

  auto plan = database.prepare(
      "EXPLAIN QUERY PLAN SELECT id FROM entities "
      "WHERE project_id = ? ORDER BY name COLLATE NOCASE, id LIMIT 100");
  plan.bind(1, owner);
  assert(plan.step());
  assert(plan.column_text(3).find("entities_project_name_idx") !=
         std::string::npos);
}

} // namespace inde::tests

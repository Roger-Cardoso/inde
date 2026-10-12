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

void structural_element_types_and_designators_are_managed() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto source_path =
      service.create(temporary.path / "Tipos estruturais", "Tipos estruturais")
          .path();
  const auto ip_id =
      service.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Obra", "", "", "pt-BR", "Planejamento").id;

  const auto builtins = service.structural_element_types();
  assert(builtins.size() == 9);
  assert(std::all_of(builtins.begin(), builtins.end(),
                     [](const auto &value) { return value.is_builtin; }));
  assert(std::ranges::any_of(
      builtins, [](const auto &value) { return value.name == "Série"; }));
  assert(std::ranges::any_of(
      builtins, [](const auto &value) { return value.name == "Cena"; }));

  bool builtin_protected = false;
  try {
    service.delete_structural_element_type(builtins.front().id);
  } catch (const std::exception &) {
    builtin_protected = true;
  }
  assert(builtin_protected);

  auto custom = service.create_structural_element_type("  Interlúdio  ");
  assert(custom.name == "Interlúdio");
  const auto &part = service.create_structural_node(
      work_id, std::nullopt,
      inde::project::builtin_structural_type_id(
          inde::project::StructuralNodeType::Part),
      "I", "Abertura");
  const auto part_id = part.id;
  const auto &interlude = service.create_structural_node(
      work_id, part_id, custom.id, "  IV  ", "Intervalo");
  const auto interlude_id = interlude.id;
  assert(interlude.designator == "IV");
  assert(interlude.structural_type_name == "Interlúdio");
  assert(inde::project::structural_node_path_label(
             service.structural_nodes_for_work(work_id), interlude_id) ==
         "Parte: I > Interlúdio: IV — Intervalo");

  custom.name = "Pausa narrativa";
  custom = service.update_structural_element_type(custom);
  const auto renamed = service.structural_nodes_for_work(work_id);
  const auto renamed_node = std::ranges::find(
      renamed, interlude_id, &inde::project::StructuralNode::id);
  assert(renamed_node != renamed.end());
  assert(renamed_node->structural_type_name == "Pausa narrativa");
  assert(renamed_node->custom_type_name == "Pausa narrativa");

  bool in_use_protected = false;
  try {
    service.delete_structural_element_type(custom.id);
  } catch (const std::exception &) {
    in_use_protected = true;
  }
  assert(in_use_protected);

  const auto unused = service.create_structural_element_type("Entrada");
  service.delete_structural_element_type(unused.id);
  assert(std::ranges::none_of(
      service.structural_element_types(),
      [&](const auto &value) { return value.id == unused.id; }));

  const auto copy = service.save_as(temporary.path / "Tipos estruturais copia");
  inde::application::ProjectService reopened;
  reopened.open(copy.path());
  const auto copied_types = reopened.structural_element_types();
  assert(std::ranges::any_of(copied_types, [&](const auto &value) {
    return value.id == custom.id && value.name == "Pausa narrativa" &&
           !value.is_builtin;
  }));
  const auto copied_nodes = reopened.structural_nodes_for_work(work_id);
  const auto copied_interlude = std::ranges::find(
      copied_nodes, interlude_id, &inde::project::StructuralNode::id);
  assert(copied_interlude != copied_nodes.end());
  assert(copied_interlude->designator == "IV");
  assert(copied_interlude->structural_type_name == "Pausa narrativa");

  // Salvar como cria uma cópia independente e não altera a origem.
  inde::application::ProjectService source;
  source.open(source_path);
  assert(std::ranges::any_of(
      source.structural_element_types(), [&](const auto &value) {
        return value.id == custom.id && value.name == "Pausa narrativa";
      }));
}

void structure_models_instances_and_narrative_flow_are_independent() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  static_cast<void>(
      service.create(temporary.path / "Estruturas", "Projeto estrutural"));
  const auto ip_id =
      service.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Obra", "", "", "pt-BR", "Planejamento").id;

  const auto editorial_models =
      service.structures().models(inde::project::StructureLayer::Editorial);
  const auto narrative_models =
      service.structures().models(inde::project::StructureLayer::Narrative);
  assert(editorial_models.size() == 3);
  assert(narrative_models.size() == 2);
  assert(std::ranges::all_of(
      editorial_models, [](const auto &value) { return value.is_builtin; }));
  assert(service.structures().active_editorial(work_id).has_value());
  assert(service.structures().active_narrative(work_id).has_value());
  bool builtin_model_delete_rejected = false;
  try {
    service.structures().delete_model(editorial_models.front().id);
  } catch (const std::exception &) {
    builtin_model_delete_rejected = true;
  }
  assert(builtin_model_delete_rejected);

  const auto three_acts =
      std::ranges::find(editorial_models, std::string{"three-acts"},
                        &inde::project::StructureModel::key);
  assert(three_acts != editorial_models.end());
  const auto editorial = service.structures().instantiate_editorial(
      work_id, three_acts->id, "Alternativa em três atos");
  assert(!editorial.is_active);
  assert(service.structures().editorial_node_count(editorial.id) == 3);
  const auto editorial_impact =
      service.structures().editorial_activation_impact(editorial.id);
  assert(editorial_impact.target_units == 3);
  service.structures().activate_editorial(editorial.id);
  assert(service.structural_nodes_for_work(work_id).size() == 3);

  const auto chapter = service.create_structural_node(
      work_id, std::nullopt,
      inde::project::builtin_structural_type_id(
          inde::project::StructuralNodeType::Chapter),
      "1", "Abertura");
  assert(chapter.structure_id == editorial.id);
  const auto duplicated = service.structures().duplicate_editorial(
      editorial.id, "Cópia editorial", false);
  assert(service.structures().editorial_node_count(duplicated.id) == 4);
  const auto captured = service.structures().capture_editorial_model(
      editorial.id, "Meu modelo editorial", "Modelo capturado da Obra");
  assert(!captured.is_builtin);
  const auto reapplied = service.structures().instantiate_editorial(
      work_id, captured.id, "Instância do meu modelo");
  assert(service.structures().editorial_node_count(reapplied.id) == 4);
  const auto document =
      service.writing().create_document("Texto preservado", chapter.id);
  const auto activation_impact =
      service.structures().editorial_activation_impact(reapplied.id);
  assert(activation_impact.affected_documents == 1);
  service.structures().activate_editorial(reapplied.id);
  assert(service.writing().document(document.id)->editorial_node_id ==
         chapter.id);
  bool active_editorial_delete_rejected = false;
  try {
    service.structures().delete_editorial(reapplied.id);
  } catch (const std::exception &) {
    active_editorial_delete_rejected = true;
  }
  assert(active_editorial_delete_rejected);

  const auto movements =
      std::ranges::find(narrative_models, std::string{"three-movements"},
                        &inde::project::StructureModel::key);
  assert(movements != narrative_models.end());
  const auto narrative = service.structures().instantiate_narrative(
      work_id, movements->id, "Fluxo de revelação");
  auto lines = service.structures().lines(narrative.id);
  auto units = service.structures().units(narrative.id);
  assert(lines.size() == 1);
  assert(units.size() == 3);
  assert(service.structures().unit_lines(narrative.id).size() == 3);

  const auto alternate_line = service.structures().create_line(
      narrative.id, "Linha da testemunha", "Revelação parcial");
  service.structures().add_unit_to_line(units.front().id, alternate_line.id);
  assert(service.structures().unit_lines(narrative.id).size() == 4);
  const auto link = service.structures().create_link(
      narrative.id, units[0].id, units[1].id,
      inde::project::NarrativeLinkKind::Precedes, "ordem de revelação");
  assert(service.structures().links(narrative.id).front().id == link.id);

  const auto entity_types = service.narrative().entity_types();
  const auto character_type = std::ranges::find(
      entity_types, std::string{"character"}, &inde::project::EntityType::key);
  assert(character_type != entity_types.end());
  const auto character = service.narrative().create_entity(
      character_type->id, "Lia", "Testemunha da abertura");
  service.structures().add_entity_to_unit(units.front().id, character.id,
                                          "Presente");
  assert(service.structures().unit_entities(narrative.id).size() == 1);

  const auto roles = service.structures().roles();
  assert(roles.size() == 5);
  const auto pov = std::ranges::find(roles, std::string{"Ponto de vista (PoV)"},
                                     &inde::project::NarrativeRole::name);
  assert(pov != roles.end());
  const auto assignment = service.structures().assign_role(
      pov->id, character.id, work_id, narrative.id, units.front().id,
      "PoV desta unidade");
  assert(service.structures().role_assignments(work_id).front().id ==
         assignment.id);

  const auto narrative_model = service.structures().capture_narrative_model(
      narrative.id, "Modelo de revelação", "Linhas e vínculos preservados");
  const auto narrative_reapplied = service.structures().instantiate_narrative(
      work_id, narrative_model.id, "Reaplicação narrativa");
  assert(service.structures().units(narrative_reapplied.id).size() == 3);
  assert(service.structures().unit_lines(narrative_reapplied.id).size() == 4);
  assert(service.structures().links(narrative_reapplied.id).size() == 1);

  const auto narrative_copy = service.structures().duplicate_narrative(
      narrative.id, "Fluxo alternativo", true);
  assert(service.builder().snapshot(narrative_copy.id).roles.size()==1);
  assert(service.builder().snapshot(narrative.id).roles.front().id==assignment.id);
  assert(service.structures().units(narrative_copy.id).size() == 3);
  assert(service.structures().unit_lines(narrative_copy.id).size() == 4);
  assert(service.structures().links(narrative_copy.id).size() == 1);
  assert(service.structures().unit_entities(narrative_copy.id).size() == 1);
  service.structures().activate_narrative(narrative_copy.id);
  assert(service.structures().active_narrative(work_id)->id ==
         narrative_copy.id);
  bool active_narrative_delete_rejected = false;
  try {
    service.structures().delete_narrative(narrative_copy.id);
  } catch (const std::exception &) {
    active_narrative_delete_rejected = true;
  }
  assert(active_narrative_delete_rejected);

  const auto removable_model = service.structures().capture_narrative_model(
      narrative_copy.id, "Modelo descartável", "Teste de ciclo de vida");
  service.structures().delete_model(removable_model.id);
  const auto models_after_removal =
      service.structures().models(inde::project::StructureLayer::Narrative);
  assert(std::ranges::none_of(models_after_removal, [&](const auto &value) {
    return value.id == removable_model.id;
  }));

  const auto old_project_id = service.current()->manifest().project_id;
  static_cast<void>(
      service.save_as(temporary.path / "Estruturas e modelos copiados"));
  const auto new_project_id = service.current()->manifest().project_id;
  assert(new_project_id != old_project_id);
  assert(service.structures().active_narrative(work_id)->id ==
         narrative_copy.id);
  assert(service.structures().links(narrative_copy.id).size() == 1);

  inde::persistence::SqliteDatabase database(service.current()->path() /
                                             "data" / "project.sqlite3");
  for (const char *table :
       {"structure_models", "structure_model_items",
        "structure_model_item_links", "editorial_structures",
        "narrative_structures", "narrative_lines", "narrative_units",
        "narrative_unit_lines", "narrative_unit_entities", "narrative_links",
        "narrative_roles", "narrative_role_assignments"}) {
    assert(database.query_text("SELECT DISTINCT project_id FROM " +
                               std::string(table)) == new_project_id);
  }
  assert(database.query_text("PRAGMA integrity_check") == "ok");
  assert(!database.prepare("PRAGMA foreign_key_check").step());
}

void sqlite_structural_batches_are_atomic() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto project_path =
      service.create(temporary.path / "Lote", "Projeto em lote").path();
  const auto ip_id = service.create_intellectual_property("IP", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Obra", "", "", "pt-BR", "Planejamento").id;
  const auto now = inde::project::utc_now();
  inde::project::StructuralNode valid{
      inde::project::new_uuid(),
      work_id,
      std::nullopt,
      inde::project::StructuralNodeType::Chapter,
      "Válido",
      "",
      "",
      "",
      "Planejamento",
      1000,
      now,
      now};
  inde::project::StructuralNode orphan{inde::project::new_uuid(),
                                       work_id,
                                       inde::project::new_uuid(),
                                       inde::project::StructuralNodeType::Scene,
                                       "Órfão",
                                       "",
                                       "",
                                       "",
                                       "Planejamento",
                                       1000,
                                       now,
                                       now};
  bool rejected = false;
  try {
    inde::persistence::SqliteStructuralRepository{}.save_many(project_path,
                                                              {valid, orphan});
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(inde::persistence::SqliteStructuralRepository{}
             .load(project_path)
             .empty());
}

} // namespace inde::tests

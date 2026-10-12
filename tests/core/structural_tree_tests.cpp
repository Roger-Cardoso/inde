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

void catalog_round_trip() {
  TemporaryDirectory temporary;
  const auto project_path = temporary.path / "Catalogo.inde";
  std::filesystem::create_directories(project_path / "data");
  inde::persistence::CatalogRepository repository;
  const auto now = inde::project::utc_now();
  inde::project::IntellectualProperty ip{inde::project::new_uuid(),
                                         "Universo Áureo",
                                         "Crônicas",
                                         "Uma propriedade de teste",
                                         "",
                                         now,
                                         now};
  repository.save(project_path, ip);
  inde::project::Work work{inde::project::new_uuid(),
                           ip.id,
                           "Primeira Obra",
                           "",
                           "Sinopse com acentuação e uma linha\nnova.",
                           "pt-BR",
                           "Planejamento",
                           now,
                           now};
  repository.save(project_path, work);

  auto loaded = repository.load(project_path);
  assert(loaded.intellectual_properties.size() == 1);
  assert(loaded.works.size() == 1);
  assert(loaded.intellectual_properties[0].title == ip.title);
  assert(loaded.works[0].intellectual_property_id == ip.id);
  assert(loaded.works[0].synopsis == work.synopsis);

  repository.remove_work(project_path, work.id);
  repository.remove_intellectual_property(project_path, ip.id);
  loaded = repository.load(project_path);
  assert(loaded.intellectual_properties.empty());
  assert(loaded.works.empty());
}

void structural_tree_round_trip_and_integrity() {
  TemporaryDirectory temporary;
  const auto project_path = temporary.path / "Estrutura.inde";
  std::filesystem::create_directories(project_path / "data");
  inde::persistence::StructuralRepository repository;
  const auto work_id = inde::project::new_uuid();
  const auto other_work_id = inde::project::new_uuid();
  const auto now = inde::project::utc_now();
  inde::project::StructuralNode volume{
      inde::project::new_uuid(),
      work_id,
      std::nullopt,
      inde::project::StructuralNodeType::Volume,
      "Volume I",
      "",
      "",
      "",
      "Planejamento",
      1000,
      now,
      now};
  inde::project::StructuralNode chapter{
      inde::project::new_uuid(),
      work_id,
      volume.id,
      inde::project::StructuralNodeType::Chapter,
      "Capítulo 1",
      "",
      "",
      "",
      "Planejamento",
      1000,
      now,
      now};
  inde::project::StructuralNode scene{inde::project::new_uuid(),
                                      work_id,
                                      chapter.id,
                                      inde::project::StructuralNodeType::Scene,
                                      "Cena 1",
                                      "",
                                      "",
                                      "",
                                      "Planejamento",
                                      1000,
                                      now,
                                      now};
  repository.save(project_path, volume);
  repository.save(project_path, chapter);
  repository.save(project_path, scene);
  auto nodes = repository.load(project_path);
  assert(nodes.size() == 3);
  inde::project::StructuralTree tree(nodes);
  tree.validate_all({work_id, other_work_id});
  assert(tree.descendants_of(volume.id).size() == 2);
  assert(!tree.can_move(volume.id, scene.id));
  assert(tree.can_move(scene.id, volume.id));

  auto cyclic = nodes;
  auto volume_copy =
      std::find_if(cyclic.begin(), cyclic.end(),
                   [&](const auto &n) { return n.id == volume.id; });
  volume_copy->parent_id = scene.id;
  bool rejected_cycle = false;
  try {
    inde::project::StructuralTree(cyclic).validate_all({work_id});
  } catch (const std::exception &) {
    rejected_cycle = true;
  }
  assert(rejected_cycle);

  auto cross_work = nodes;
  auto scene_copy =
      std::find_if(cross_work.begin(), cross_work.end(),
                   [&](const auto &n) { return n.id == scene.id; });
  scene_copy->work_id = other_work_id;
  bool rejected_cross_work = false;
  try {
    inde::project::StructuralTree(cross_work)
        .validate_all({work_id, other_work_id});
  } catch (const std::exception &) {
    rejected_cross_work = true;
  }
  assert(rejected_cross_work);

  repository.remove(project_path, scene.id);
  repository.remove(project_path, chapter.id);
  repository.remove(project_path, volume.id);
  assert(repository.load(project_path).empty());
}

void structural_path_labels_disambiguate_repeated_titles() {
  using inde::project::StructuralNode;
  using inde::project::StructuralNodeType;
  const auto stamp = std::string{"2026-09-08T00:00:00Z"};
  const auto work = std::string{"work"};
  const auto part_type =
      inde::project::builtin_structural_type_id(StructuralNodeType::Part);
  const auto chapter_type =
      inde::project::builtin_structural_type_id(StructuralNodeType::Chapter);
  const std::vector<StructuralNode> nodes{
      {"part-a", work, std::nullopt, StructuralNodeType::Part, "Abertura", "",
       "", "", "Planejamento", 1000, stamp, stamp, part_type, "Parte", "1"},
      {"part-b", work, std::nullopt, StructuralNodeType::Part, "Abertura", "",
       "", "", "Planejamento", 2000, stamp, stamp, part_type, "Parte", "2"},
      {"chapter-a", work, std::string{"part-b"}, StructuralNodeType::Chapter,
       "Chegada", "", "", "", "Planejamento", 2000, stamp, stamp, chapter_type,
       "Capítulo", "2"},
      {"chapter-b", work, std::string{"part-b"}, StructuralNodeType::Chapter,
       "Chegada", "", "", "", "Planejamento", 1000, stamp, stamp, chapter_type,
       "Capítulo", "1"},
      {"custom", work, std::string{"chapter-a"}, StructuralNodeType::Custom,
       "Intervalo", "", "", "Interlúdio", "Planejamento", 1000, stamp, stamp,
       "20000000-0000-4000-8000-000000000001", "Interlúdio", "I"}};

  assert(inde::project::structural_node_path_label(nodes, "chapter-a") ==
         "Parte: 2 > Capítulo: 2 — Chegada");
  assert(inde::project::structural_node_path_label(nodes, "chapter-b") ==
         "Parte: 2 > Capítulo: 1 — Chegada");
  assert(inde::project::structural_node_path_label(nodes, "custom") ==
         "Parte: 2 > Capítulo: 2 > Interlúdio: I — Intervalo");
  auto without_designator = nodes;
  without_designator.back().designator.clear();
  assert(
      inde::project::structural_node_path_label(without_designator, "custom") ==
      "Parte: 2 > Capítulo: 2 > Interlúdio — Intervalo");
  assert(inde::project::structural_node_path_label(nodes, "missing") ==
         "Elemento editorial indisponível");
}

void legacy_structural_positions_are_compatible() {
  TemporaryDirectory temporary;
  const auto project_path = temporary.path / "Legado.inde";
  const auto nodes_path = project_path / "data" / "structural-nodes";
  std::filesystem::create_directories(nodes_path);
  const std::string work_id = "10000000-0000-4000-8000-000000000001";

  const auto write_node = [&](std::string_view id, std::string_view title,
                              std::string_view position) {
    std::ofstream output(nodes_path / (std::string(id) + ".json"));
    output << "{\n"
           << "  \"schema\": \"inde.structural-node\",\n"
           << "  \"version\": 1,\n"
           << "  \"id\": \"" << id << "\",\n"
           << "  \"work_id\": \"" << work_id << "\",\n"
           << "  \"parent_id\": null,\n"
           << "  \"type\": \"chapter\",\n"
           << "  \"title\": \"" << title << "\",\n"
           << "  \"subtitle\": \"\",\n"
           << "  \"synopsis\": \"\",\n"
           << "  \"custom_type_name\": \"\",\n"
           << "  \"status\": \"Planejamento\",\n"
           << "  \"position\": " << position << ",\n"
           << "  \"created_at\": \"2026-01-01T00:00:00Z\",\n"
           << "  \"updated_at\": \"2026-01-01T00:00:00Z\"\n"
           << "}\n";
  };

  write_node("20000000-0000-4000-8000-000000000001", "Decimal inteiro",
             "1.000");
  write_node("20000000-0000-4000-8000-000000000002", "Decimal intermediário",
             "1.001");
  write_node("20000000-0000-4000-8000-000000000003", "Inteiro legado", "5");
  write_node("20000000-0000-4000-8000-000000000004", "Inteiro atual", "6000");

  const auto nodes =
      inde::persistence::StructuralRepository{}.load(project_path);
  const auto position_of = [&](std::string_view title) {
    const auto found =
        std::find_if(nodes.begin(), nodes.end(),
                     [&](const auto &node) { return node.title == title; });
    assert(found != nodes.end());
    return found->position;
  };
  assert(nodes.size() == 4);
  assert(position_of("Decimal inteiro") == 1000);
  assert(position_of("Decimal intermediário") == 1001);
  assert(position_of("Inteiro legado") == 5000);
  assert(position_of("Inteiro atual") == 6000);
}

void structural_templates_and_duplication() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  service.create(temporary.path / "Projeto", "Projeto estrutural");
  const auto ip_id =
      service.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Obra", "", "", "pt-BR", "Planejamento").id;
  service.create_structural_template(work_id, "three-acts");
  auto nodes = service.structural_nodes_for_work(work_id);
  assert(nodes.size() == 3);
  const auto copy_id = service.duplicate_structural_branch(nodes.front().id);
  nodes = service.structural_nodes_for_work(work_id);
  assert(nodes.size() == 4);
  const auto copy =
      std::find_if(nodes.begin(), nodes.end(),
                   [&](const auto &node) { return node.id == copy_id; });
  assert(copy != nodes.end());
  assert(copy->title.find("Cópia") != std::string::npos);
  service.delete_structural_branch(copy_id);
  assert(service.structural_nodes_for_work(work_id).size() == 3);
  assert(service.restore_last_structural_deletion());
  assert(service.structural_nodes_for_work(work_id).size() == 4);
  auto reopened = inde::application::ProjectService{};
  reopened.open(service.current()->path());
  assert(reopened.structural_nodes_for_work(work_id).size() == 4);
}

void large_structural_tree() {
  const std::string work_id = "10000000-0000-4000-8000-000000000001";
  const std::string root_id = "20000000-0000-4000-8000-000000000001";
  const auto now = inde::project::utc_now();
  std::vector<inde::project::StructuralNode> nodes;
  nodes.reserve(10000);
  nodes.push_back({root_id, work_id, std::nullopt,
                   inde::project::StructuralNodeType::Volume, "Volume", "", "",
                   "", "Planejamento", 1000, now, now});
  for (int i = 1; i < 10000; ++i) {
    char id[37]{};
    std::snprintf(id, sizeof(id), "30000000-0000-4000-8000-%012x", i);
    nodes.push_back({id, work_id, root_id,
                     inde::project::StructuralNodeType::Scene,
                     "Cena " + std::to_string(i), "", "", "", "Planejamento",
                     i * 1000LL, now, now});
  }
  inde::project::StructuralTree tree(nodes);
  tree.validate_all({work_id});
  assert(tree.descendants_of(root_id).size() == 9999);
}

} // namespace inde::tests

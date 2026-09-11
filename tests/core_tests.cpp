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
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

namespace {

struct TemporaryDirectory {
  std::filesystem::path path = std::filesystem::temp_directory_path() /
                               ("inde-tests-" + inde::project::new_uuid());
  TemporaryDirectory() { std::filesystem::create_directories(path); }
  ~TemporaryDirectory() { std::filesystem::remove_all(path); }
};

class DeterministicSpellingProvider final
    : public inde::application::SpellingProvider {
public:
  [[nodiscard]] bool available() const noexcept override { return true; }
  [[nodiscard]] std::string description() const override {
    return "Provedor ortográfico de teste";
  }
  [[nodiscard]] bool check(const std::string &word) const override {
    return word == "isto" || word == "casa" || word == "ação" ||
           word == "Ação";
  }
  [[nodiscard]] std::vector<std::string>
  suggest(const std::string &word, std::size_t limit) const override {
    if (word == "caza" && limit > 0)
      return {"casa"};
    return {};
  }
};

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void project_round_trip() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository repository;
  auto created =
      repository.create(temporary.path / "Romance", "Romance de teste");

  assert(created.path().extension() == ".inde");
  assert(std::filesystem::is_directory(created.path() / "assets"));
  assert(std::filesystem::is_directory(created.path() / "data"));
  assert(std::filesystem::is_directory(created.path() / "documents"));
  assert(std::filesystem::is_regular_file(created.path() / "manifest.json"));

  const auto opened = repository.open(created.path());
  assert(opened.manifest().format == "INDE");
  assert(opened.manifest().format_version ==
         inde::project::current_format_version);
  assert(opened.manifest().name == "Romance de teste");
  assert(opened.manifest().project_id == created.manifest().project_id);

  std::ofstream(created.path() / "documents" / "nota.txt")
      << "conteúdo preservado";
  const auto copy =
      repository.save_as(created, temporary.path / "Romance - cópia");
  assert(copy.path().extension() == ".inde");
  assert(copy.manifest().project_id != created.manifest().project_id);
  assert(copy.manifest().name == created.manifest().name);
  assert(
      std::filesystem::is_regular_file(copy.path() / "documents" / "nota.txt"));
  assert(repository.open(copy.path()).manifest().project_id ==
         copy.manifest().project_id);
}

void rejects_future_format() {
  TemporaryDirectory temporary;
  const auto project_path = temporary.path / "Futuro.inde";
  std::filesystem::create_directories(project_path);
  std::ofstream manifest(project_path / "manifest.json");
  manifest
      << R"({"format":"INDE","format_version":999,"project_id":"550e8400-e29b-41d4-a716-446655440000","name":"Futuro","created_at":"2026-01-01T00:00:00Z","updated_at":"2026-01-01T00:00:00Z"})";
  manifest.close();

  inde::persistence::ProjectRepository repository;
  bool rejected = false;
  try {
    static_cast<void>(repository.open(project_path));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
}

void strict_json_codec() {
  using inde::persistence::json::parse;
  using inde::persistence::json::serialize;

  const auto decoded = parse(
      R"({"texto":"Ação\n\u00e7\ud83d\ude80","ativo":true,"vazio":null,"itens":[1,2]})");
  assert(decoded.at("texto").as_string() == "Ação\nç🚀");
  assert(decoded.at("ativo").as_boolean());
  assert(decoded.at("vazio").is_null());
  assert(decoded.at("itens").as_array().size() == 2);
  const auto round_trip = parse(serialize(decoded));
  assert(round_trip.at("texto").as_string() == decoded.at("texto").as_string());

  bool duplicate_rejected = false;
  try {
    static_cast<void>(parse("{\n  \"id\": 1,\n  \"id\": 2\n}"));
  } catch (const inde::persistence::json::Error &error) {
    duplicate_rejected = error.line() == 3 && error.column() > 1;
  }
  assert(duplicate_rejected);

  bool trailing_rejected = false;
  try {
    static_cast<void>(parse("{} conteúdo indevido"));
  } catch (const inde::persistence::json::Error &) {
    trailing_rejected = true;
  }
  assert(trailing_rejected);

  bool wrong_type_rejected = false;
  try {
    static_cast<void>(parse(R"({"version":"1"})").at("version").as_integer());
  } catch (const std::exception &) {
    wrong_type_rejected = true;
  }
  assert(wrong_type_rejected);
}

void dictionaries_and_proofreading_are_global_and_persistent() {
  TemporaryDirectory temporary;
  const auto path = temporary.path / "preferences" / "dictionaries.json";
  inde::persistence::JsonDictionaryStore store(path);
  DeterministicSpellingProvider spelling;
  inde::application::ProofreadingService service(store, spelling);

  assert(service.dictionaries().size() == 1);
  assert(service.dictionaries().front().category ==
         inde::project::DictionaryCategory::System);
  assert(service.spelling_available());

  const auto issues = service.analyze("isto  isto , caza.");
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Spelling &&
           issue.excerpt == "caza" && !issue.suggestions.empty() &&
           issue.suggestions.front() == "casa";
  }));

  const auto unicode_issues = service.analyze("ação.");
  assert(std::ranges::any_of(unicode_issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.start == 0 && issue.length == 1 && issue.excerpt == "a" &&
           issue.replacement == std::optional<std::string>{"A"};
  }));
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.message.find("espaços consecutivos") != std::string::npos;
  }));
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.message.find("Palavra repetida") != std::string::npos;
  }));
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.message.find("antes deste sinal") != std::string::npos;
  }));

  const auto dictionary_id =
      service.create_dictionary("Nomes de Arvoredo").id;
  service.add_word(dictionary_id, "Aurélin");
  service.add_word(dictionary_id, "d'Água");
  assert(std::ranges::none_of(service.analyze("Aurélin."), [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Spelling;
  }));
  assert(std::filesystem::is_regular_file(path));

  inde::persistence::JsonDictionaryStore reopened_store(path);
  inde::application::ProofreadingService reopened(reopened_store, spelling);
  assert(reopened.dictionaries().size() == 2);
  assert(reopened.dictionaries().back().words.size() == 2);
  assert(std::ranges::find(reopened.dictionaries().back().words, "Aurélin") !=
         reopened.dictionaries().back().words.end());
  reopened.set_dictionary_enabled(dictionary_id, false);
  assert(std::ranges::any_of(reopened.analyze("Aurélin."), [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Spelling;
  }));

  bool system_removal_rejected = false;
  try {
    reopened.remove_dictionary(reopened.dictionaries().front().id);
  } catch (const std::exception &) {
    system_removal_rejected = true;
  }
  assert(system_removal_rejected);

  bool connector_only_rejected = false;
  try {
    reopened.add_word(dictionary_id, "-");
  } catch (const std::exception &) {
    connector_only_rejected = true;
  }
  assert(connector_only_rejected);

  const auto damaged_path = temporary.path / "damaged.json";
  {
    std::ofstream damaged(damaged_path);
    damaged << "{ arquivo interrompido";
  }
  inde::persistence::JsonDictionaryStore damaged_store(damaged_path);
  inde::application::ProofreadingService guarded(damaged_store, spelling);
  assert(!guarded.storage_error().empty());
  bool overwrite_rejected = false;
  try {
    static_cast<void>(guarded.create_dictionary("Não sobrescrever"));
  } catch (const std::exception &) {
    overwrite_rejected = true;
  }
  assert(overwrite_rejected);
  assert(read_file(damaged_path) == "{ arquivo interrompido");

  inde::persistence::EnchantSpellingProvider system_spelling;
  if (system_spelling.available()) {
    assert(system_spelling.check("casa"));
    assert(!system_spelling.check("xptoqz"));
    assert(!system_spelling.suggest("caza").empty());
  }
}

void recents_are_deduplicated() {
  TemporaryDirectory temporary;
  const auto first = temporary.path / "A.inde";
  const auto second = temporary.path / "B.inde";
  std::filesystem::create_directories(first);
  std::filesystem::create_directories(second);
  inde::persistence::RecentProjectsStore store(temporary.path / "config" /
                                               "recents");
  store.touch(first);
  store.touch(second);
  store.touch(first);
  const auto recents = store.load();
  assert(recents.size() == 2);
  assert(recents[0] == first);
  assert(recents[1] == second);
}

void recents_failure_does_not_block_project_lifecycle() {
  TemporaryDirectory temporary;
  const auto blocked_config = temporary.path / "config-is-a-file";
  std::ofstream(blocked_config) << "not a directory";
  setenv("XDG_CONFIG_HOME", blocked_config.c_str(), 1);

  inde::application::ProjectService service;
  const auto project_path = temporary.path / "Projeto.inde";
  const auto project_id =
      service.create(project_path, "Projeto").manifest().project_id;
  assert(service.current());
  assert(service.current()->manifest().project_id == project_id);
  service.save();
  service.close();
  assert(!service.current());
  service.open(project_path);
  assert(service.current());
  assert(service.current()->manifest().project_id == project_id);
}

void failed_open_preserves_active_session() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto active_path =
      service.create(temporary.path / "Ativo", "Projeto ativo").path();
  const auto active_id = service.current()->manifest().project_id;

  const auto invalid_path = temporary.path / "Invalido.inde";
  std::filesystem::create_directories(invalid_path / "data" / "works");
  std::ofstream(invalid_path / "manifest.json")
      << R"({"format":"INDE","format_version":1,"project_id":"550e8400-e29b-41d4-a716-446655440000","name":"Inválido","created_at":"2026-01-01T00:00:00Z","updated_at":"2026-01-01T00:00:00Z"})";
  std::ofstream(invalid_path / "data" / "works" / "corrompido.json")
      << "{ arquivo incompleto";

  bool rejected = false;
  try {
    static_cast<void>(service.open(invalid_path));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(service.current());
  assert(service.current()->path() == active_path);
  assert(service.current()->manifest().project_id == active_id);
}

void specialized_services_share_a_guarded_session() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository catalog_repository;
  inde::persistence::StructuralRepository structural_repository;
  inde::application::ProjectSession session;
  inde::application::CatalogService catalog(session, catalog_repository);
  inde::application::StructuralService structure(session,
                                                 structural_repository);

  bool catalog_guarded = false;
  try {
    static_cast<void>(
        catalog.create_intellectual_property("Sem sessão", "", ""));
  } catch (const std::exception &) {
    catalog_guarded = true;
  }
  assert(catalog_guarded);

  bool structure_guarded = false;
  try {
    static_cast<void>(structure.nodes_for_work(inde::project::new_uuid()));
  } catch (const std::exception &) {
    structure_guarded = true;
  }
  assert(structure_guarded);

  auto project =
      projects.create(temporary.path / "Servicos", "Serviços especializados");
  const auto project_path = project.path();
  session.begin(std::move(project), {}, {});
  const auto ip_id =
      catalog.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      catalog.create_work(ip_id, "Obra", "", "", "pt-BR", "Planejamento").id;
  const auto chapter_id =
      structure
          .create_node(work_id, std::nullopt,
                       inde::project::StructuralNodeType::Chapter, "Capítulo 1")
          .id;
  structure.create_node(work_id, chapter_id,
                        inde::project::StructuralNodeType::Scene, "Cena 1");

  assert(session.catalog().intellectual_properties.size() == 1);
  assert(session.catalog().works.size() == 1);
  assert(structure.nodes_for_work(work_id).size() == 2);
  assert(catalog_repository.load(project_path).works.size() == 1);
  assert(structural_repository.load(project_path).size() == 2);

  session.clear();
  assert(!session.current());
  assert(session.catalog().works.empty());
  assert(session.structural_nodes().empty());
}

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

void insert_database_project(inde::persistence::SqliteDatabase &database,
                             std::string_view id, std::string_view name) {
  auto insert = database.prepare(
      "INSERT INTO projects(id, name, format_version, created_at, updated_at) "
      "VALUES (?, ?, 1, '2026-01-01T00:00:00Z', "
      "'2026-01-01T00:00:00Z')");
  insert.bind(1, id);
  insert.bind(2, name);
  insert.run();
}

void downgrade_v12_to_v11(inde::persistence::SqliteDatabase &database) {
  database.execute("DROP TRIGGER editorial_nodes_structure_insert_guard;"
                   "DROP TRIGGER editorial_nodes_structure_update_guard;"
                   "DROP TRIGGER editorial_nodes_structure_default;"
                   "DROP TRIGGER works_default_structures;"
                   "DROP TRIGGER structure_models_builtin_update_guard;"
                   "DROP TRIGGER structure_models_builtin_delete_guard;"
                   "DROP TRIGGER narrative_roles_builtin_update_guard;"
                   "DROP TRIGGER narrative_roles_builtin_delete_guard;"
                   "DROP TRIGGER narrative_unit_lines_structure_insert_guard;"
                   "DROP TRIGGER narrative_unit_lines_structure_update_guard;"
                   "DROP TRIGGER narrative_links_structure_insert_guard;"
                   "DROP TRIGGER narrative_links_structure_update_guard;"
                   "DROP TRIGGER narrative_role_assignments_scope_insert_guard;"
                   "DROP TRIGGER narrative_role_assignments_scope_update_guard;"
                   "DROP INDEX editorial_nodes_structure_parent_position_idx;"
                   "DROP TABLE narrative_role_assignments;"
                   "DROP TABLE narrative_roles;"
                   "DROP TABLE narrative_links;"
                   "DROP TABLE narrative_unit_entities;"
                   "DROP TABLE narrative_unit_lines;"
                   "DROP TABLE narrative_units;"
                   "DROP TABLE narrative_lines;"
                   "DROP TABLE narrative_structures;"
                   "ALTER TABLE editorial_nodes DROP COLUMN structure_id;"
                   "ALTER TABLE editorial_node_trash DROP COLUMN structure_id;"
                   "DROP TABLE editorial_structures;"
                   "DROP TABLE structure_model_item_links;"
                   "DROP TABLE structure_model_items;"
                   "DROP TABLE structure_models;"
                   "DELETE FROM schema_migrations WHERE version = 12;");
}

void sqlite_schema_and_transactions() {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path / "project.sqlite3";
  const std::string project_id = "10000000-0000-4000-8000-000000000001";

  inde::persistence::SqliteDatabase database(database_path);
  inde::persistence::SchemaMigrator migrator;
  assert(database.query_integer("PRAGMA foreign_keys") == 1);
  assert(migrator.current_version(database) == 0);

  migrator.migrate(database);
  assert(migrator.current_version(database) ==
         inde::persistence::current_database_schema_version);
  assert(database.query_text("PRAGMA journal_mode") != "wal");
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('projects', 'intellectual_properties', 'works', "
             "'editorial_nodes', 'schema_migrations', "
             "'editorial_trash_operations', 'editorial_node_trash', "
             "'entity_types', 'entities', 'relation_types', 'relations', "
             "'relation_contexts', "
             "'change_log', 'fictional_time_axes', 'fictional_time_points', "
             "'event_occurrences', 'event_participations', "
             "'entity_presences', 'entity_work_scopes', "
             "'editorial_entity_references', 'documents', "
             "'document_groups', "
             "'document_format_spans', 'document_anchors', "
             "'document_entity_references', "
             "'structural_element_types', 'structure_models', "
             "'structure_model_items', 'structure_model_item_links', "
             "'editorial_structures', "
             "'narrative_structures', 'narrative_lines', "
             "'narrative_units', 'narrative_unit_lines', "
             "'narrative_unit_entities', 'narrative_links', "
             "'narrative_roles', 'narrative_role_assignments')") == 38);

  // Reaplicar o migrador é uma operação idempotente.
  migrator.migrate(database);
  assert(database.query_integer("SELECT count(*) FROM schema_migrations") ==
         inde::persistence::current_database_schema_version);

  // Simula um banco v1 e prova as migrações incrementais v2-v11 sem recriar
  // as tabelas editoriais.
  downgrade_v12_to_v11(database);
  database.execute("DROP TRIGGER editorial_nodes_structural_type_insert_guard");
  database.execute("DROP TRIGGER editorial_nodes_structural_type_update_guard");
  database.execute(
      "DROP TRIGGER structural_element_types_builtin_update_guard");
  database.execute("DROP TRIGGER structural_element_types_delete_guard");
  database.execute("ALTER TABLE editorial_nodes DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_nodes DROP COLUMN structural_type_id");
  database.execute("ALTER TABLE editorial_node_trash DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_node_trash DROP COLUMN structural_type_id");
  database.execute("DROP TABLE structural_element_types");
  database.execute("DELETE FROM schema_migrations WHERE version = 11");
  database.execute("DROP TABLE relation_contexts");
  database.execute("DELETE FROM schema_migrations WHERE version = 10");
  database.execute("DELETE FROM schema_migrations WHERE version = 9");
  database.execute("DROP TABLE document_entity_references");
  database.execute("DROP TABLE document_anchors");
  database.execute("DROP TABLE document_format_spans");
  database.execute("DELETE FROM schema_migrations WHERE version = 8");
  database.execute("DROP TABLE documents");
  database.execute("DROP TABLE document_groups");
  database.execute("DELETE FROM schema_migrations WHERE version = 7");
  database.execute("DROP TABLE editorial_entity_references");
  database.execute("DROP TABLE entity_work_scopes");
  database.execute("DELETE FROM schema_migrations WHERE version = 6");
  database.execute("DROP TABLE entity_presences");
  database.execute("DELETE FROM schema_migrations WHERE version = 5");
  database.execute("DROP TABLE event_participations");
  database.execute("DROP TABLE event_occurrences");
  database.execute("DROP TABLE fictional_time_points");
  database.execute("DROP TABLE fictional_time_axes");
  database.execute("DELETE FROM schema_migrations WHERE version = 4");
  database.execute("DROP TABLE change_log");
  database.execute("DROP TABLE relations");
  database.execute("DROP TABLE relation_types");
  database.execute("DROP TABLE entities");
  database.execute("DROP TABLE entity_types");
  database.execute("DELETE FROM schema_migrations WHERE version = 3");
  database.execute("DROP TABLE editorial_node_trash");
  database.execute("DROP TABLE editorial_trash_operations");
  database.execute("DELETE FROM schema_migrations WHERE version = 2");
  assert(migrator.current_version(database) == 1);
  migrator.migrate(database);
  assert(migrator.current_version(database) ==
         inde::persistence::current_database_schema_version);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('editorial_trash_operations', "
             "'editorial_node_trash')") == 2);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('entity_types', 'entities', 'relation_types', "
             "'relations', 'relation_contexts', 'change_log')") == 6);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('fictional_time_axes', 'fictional_time_points', "
             "'event_occurrences', 'event_participations')") == 4);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'entity_presences'") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('entity_work_scopes', "
             "'editorial_entity_references')") == 2);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'documents'") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('document_format_spans', 'document_anchors', "
             "'document_entity_references')") == 3);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'structural_element_types'") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name IN ('structure_models', 'structure_model_items', "
             "'structure_model_item_links', "
             "'editorial_structures', 'narrative_structures', "
             "'narrative_lines', 'narrative_units', "
             "'narrative_unit_lines', 'narrative_unit_entities', "
             "'narrative_links', 'narrative_roles', "
             "'narrative_role_assignments')") == 12);

  {
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, project_id, "Universo Áureo 🚀");
    transaction.commit();
  }
  assert(database.query_text("SELECT name FROM projects") ==
         "Universo Áureo 🚀");

  {
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, "10000000-0000-4000-8000-000000000002",
                            "Será revertido");
    // Sem commit: o destruidor precisa executar rollback.
  }
  assert(database.query_integer("SELECT count(*) FROM projects") == 1);

  {
    auto reusable = database.prepare("SELECT name FROM projects WHERE id = ?");
    reusable.bind(1, project_id);
    assert(reusable.step());
    assert(reusable.column_text(0) == "Universo Áureo 🚀");
    assert(!reusable.step());
    reusable.reset();
    reusable.bind(1, "10000000-0000-4000-8000-000000000099");
    assert(!reusable.step());
  }

  assert(std::filesystem::is_regular_file(database_path));
}

void sqlite_database_closes_before_copy() {
  TemporaryDirectory temporary;
  const auto source = temporary.path / "source.sqlite3";
  const auto copy = temporary.path / "copy.sqlite3";
  {
    inde::persistence::SqliteDatabase database(source);
    inde::persistence::SchemaMigrator{}.migrate(database);
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, "10000000-0000-4000-8000-000000000001",
                            "Projeto copiável");
    transaction.commit();
  }

  assert(std::filesystem::copy_file(source, copy));
  inde::persistence::SqliteDatabase reopened(copy);
  assert(reopened.query_text("SELECT name FROM projects") ==
         "Projeto copiável");
}

void sqlite_v11_migrates_structural_identity_and_per_type_designators() {
  TemporaryDirectory temporary;
  const auto database_path = temporary.path / "v10.sqlite3";
  inde::persistence::SqliteDatabase database(database_path);
  inde::persistence::SchemaMigrator migrator;
  migrator.migrate(database);

  // Reconstrói o estado imediatamente anterior ao corte para provar a
  // migração com dados, inclusive nomes personalizados que diferem apenas em
  // maiúsculas e irmãos de tipos diferentes.
  downgrade_v12_to_v11(database);
  database.execute("DROP TRIGGER editorial_nodes_structural_type_insert_guard");
  database.execute("DROP TRIGGER editorial_nodes_structural_type_update_guard");
  database.execute(
      "DROP TRIGGER structural_element_types_builtin_update_guard");
  database.execute("DROP TRIGGER structural_element_types_delete_guard");
  database.execute("ALTER TABLE editorial_nodes DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_nodes DROP COLUMN structural_type_id");
  database.execute("ALTER TABLE editorial_node_trash DROP COLUMN designator");
  database.execute(
      "ALTER TABLE editorial_node_trash DROP COLUMN structural_type_id");
  database.execute("DROP TABLE structural_element_types");
  database.execute("DELETE FROM schema_migrations WHERE version = 11");
  assert(migrator.current_version(database) == 10);

  database.execute(
      "INSERT INTO projects VALUES "
      "('10000000-0000-4000-8000-000000000001','Migração',2,'agora','agora');"
      "INSERT INTO intellectual_properties "
      "(id,project_id,title,created_at,updated_at) VALUES "
      "('20000000-0000-4000-8000-000000000001',"
      "'10000000-0000-4000-8000-000000000001','IP','agora','agora');"
      "INSERT INTO works "
      "(id,intellectual_property_id,title,language,status,created_at,updated_"
      "at) "
      "VALUES ('30000000-0000-4000-8000-000000000001',"
      "'20000000-0000-4000-8000-000000000001','Obra','pt-BR',"
      "'Planejamento','agora','agora');"
      "INSERT INTO editorial_nodes "
      "(id,work_id,parent_id,type,title,subtitle,synopsis,custom_type_name,"
      "status,position,created_at,updated_at) VALUES "
      "('40000000-0000-4000-8000-000000000001',"
      "'30000000-0000-4000-8000-000000000001',NULL,'custom','Intervalo A',"
      "'','','Interlúdio','Planejamento',1000,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000002',"
      "'30000000-0000-4000-8000-000000000001',NULL,'custom','Intervalo B',"
      "'','','interlúdio','Planejamento',2000,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000003',"
      "'30000000-0000-4000-8000-000000000001',"
      "'40000000-0000-4000-8000-000000000001','chapter','Primeiro',"
      "'','','','Planejamento',1000,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000004',"
      "'30000000-0000-4000-8000-000000000001',"
      "'40000000-0000-4000-8000-000000000001','scene','Cena',"
      "'','','','Planejamento',1500,'agora','agora'),"
      "('40000000-0000-4000-8000-000000000005',"
      "'30000000-0000-4000-8000-000000000001',"
      "'40000000-0000-4000-8000-000000000001','chapter','Segundo',"
      "'','','','Planejamento',2000,'agora','agora');");

  migrator.migrate(database);
  assert(migrator.current_version(database) ==
         inde::persistence::current_database_schema_version);
  assert(database.query_integer(
             "SELECT count(*) FROM structural_element_types") == 10);
  assert(database.query_integer(
             "SELECT count(DISTINCT structural_type_id) FROM editorial_nodes "
             "WHERE type = 'custom'") == 1);
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000001'") == "1");
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000002'") == "2");
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000004'") == "1");
  assert(
      database.query_text("SELECT designator FROM editorial_nodes WHERE id = "
                          "'40000000-0000-4000-8000-000000000005'") == "2");
  assert(database.query_text("PRAGMA integrity_check") == "ok");
  assert(!database.prepare("PRAGMA foreign_key_check").step());

  const auto project_path = temporary.path / "Migrado.inde";
  std::filesystem::create_directories(project_path / "data");
  const auto migrated_path = project_path / "data" / "project.sqlite3";
  std::filesystem::copy_file(database_path, migrated_path);
  assert(std::filesystem::is_regular_file(migrated_path));
  const auto nodes =
      inde::persistence::SqliteStructuralRepository{}.load(project_path);
  assert(nodes.size() == 5);
  assert(inde::project::structural_node_path_label(
             nodes, "40000000-0000-4000-8000-000000000005") ==
         "Interlúdio: 1 > Capítulo: 2 — Segundo");
}

void sqlite_foreign_keys_and_editorial_parent_integrity() {
  TemporaryDirectory temporary;
  inde::persistence::SqliteDatabase database(temporary.path /
                                             "integrity.sqlite3");
  inde::persistence::SchemaMigrator{}.migrate(database);

  bool missing_project_rejected = false;
  try {
    auto insert =
        database.prepare("INSERT INTO intellectual_properties "
                         "(id, project_id, title, created_at, updated_at) "
                         "VALUES ('20000000-0000-4000-8000-000000000001', "
                         "'ausente', 'IP órfã', 'agora', 'agora')");
    insert.run();
  } catch (const inde::persistence::SqliteError &error) {
    missing_project_rejected =
        error.code() == SQLITE_CONSTRAINT &&
        error.extended_code() == SQLITE_CONSTRAINT_FOREIGNKEY;
  }
  assert(missing_project_rejected);

  const std::string project_id = "10000000-0000-4000-8000-000000000001";
  const std::string ip_id = "20000000-0000-4000-8000-000000000001";
  const std::string work_a = "30000000-0000-4000-8000-000000000001";
  const std::string work_b = "30000000-0000-4000-8000-000000000002";
  const std::string parent_id = "40000000-0000-4000-8000-000000000001";
  const std::string child_id = "40000000-0000-4000-8000-000000000002";

  {
    inde::persistence::SqliteTransaction transaction(database);
    insert_database_project(database, project_id, "Projeto");

    auto ip =
        database.prepare("INSERT INTO intellectual_properties "
                         "(id, project_id, title, created_at, updated_at) "
                         "VALUES (?, ?, 'Universo', 'agora', 'agora')");
    ip.bind(1, ip_id);
    ip.bind(2, project_id);
    ip.run();

    auto work = database.prepare(
        "INSERT INTO works "
        "(id, intellectual_property_id, title, language, status, created_at, "
        "updated_at) VALUES (?, ?, ?, 'pt-BR', 'Planejamento', 'agora', "
        "'agora')");
    work.bind(1, work_a);
    work.bind(2, ip_id);
    work.bind(3, "Obra A");
    work.run();
    work.reset();
    work.bind(1, work_b);
    work.bind(2, ip_id);
    work.bind(3, "Obra B");
    work.run();
    transaction.commit();
  }

  bool duplicate_uuid_rejected = false;
  try {
    insert_database_project(database, project_id, "UUID repetido");
  } catch (const inde::persistence::SqliteError &) {
    duplicate_uuid_rejected = true;
  }
  assert(duplicate_uuid_rejected);

  bool cross_work_parent_rejected = false;
  try {
    inde::persistence::SqliteTransaction transaction(database);
    auto node = database.prepare(
        "INSERT INTO editorial_nodes "
        "(id, work_id, parent_id, type, title, status, position, created_at, "
        "updated_at) VALUES (?, ?, ?, 'chapter', ?, 'Planejamento', 1000, "
        "'agora', 'agora')");
    node.bind(1, parent_id);
    node.bind(2, work_a);
    node.bind_null(3);
    node.bind(4, "Pai");
    node.run();
    node.reset();
    node.bind(1, child_id);
    node.bind(2, work_b);
    node.bind(3, parent_id);
    node.bind(4, "Filho em outra obra");
    node.run();
    transaction.commit();
  } catch (const inde::persistence::SqliteError &) {
    cross_work_parent_rejected = true;
  }
  assert(cross_work_parent_rejected);
  assert(database.query_integer("SELECT count(*) FROM editorial_nodes") == 0);
}

void sqlite_rejects_future_schema() {
  TemporaryDirectory temporary;
  inde::persistence::SqliteDatabase database(temporary.path / "future.sqlite3");
  inde::persistence::SchemaMigrator migrator;
  migrator.migrate(database);
  database.execute("INSERT INTO schema_migrations(version, applied_at) "
                   "VALUES (999, 'futuro')");

  bool rejected = false;
  try {
    migrator.migrate(database);
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(migrator.current_version(database) == 999);
}

void sqlite_rolls_back_interrupted_migration() {
  TemporaryDirectory temporary;
  inde::persistence::SqliteDatabase database(temporary.path /
                                             "interrupted.sqlite3");
  // Simula um conflito durante o esquema v1, depois da criação transacional da
  // tabela de histórico de migrações.
  database.execute("CREATE TABLE projects(id INTEGER PRIMARY KEY)");

  bool failed = false;
  try {
    inde::persistence::SchemaMigrator{}.migrate(database);
  } catch (const inde::persistence::SqliteError &) {
    failed = true;
  }
  assert(failed);
  assert(inde::persistence::SchemaMigrator{}.current_version(database) == 0);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'schema_migrations'") == 0);
  assert(database.query_integer(
             "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND "
             "name = 'intellectual_properties'") == 0);
}

void sqlite_imports_json_project_safely() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository catalogs;
  inde::persistence::StructuralRepository structures;
  const auto project =
      projects.create(temporary.path / "Importacao", "Projeto de importação");
  const auto project_path = project.path();
  const auto now = inde::project::utc_now();

  inde::project::IntellectualProperty ip{
      inde::project::new_uuid(),
      "Universo Áureo 🚀",
      "Crônicas",
      "Descrição com acentos e\nquebra de linha",
      "capas/capa.png",
      now,
      now};
  catalogs.save(project_path, ip);
  inde::project::Work work{inde::project::new_uuid(),
                           ip.id,
                           "Obra importada",
                           "Subtítulo",
                           "Sinopse preservada",
                           "pt-BR",
                           "Planejamento",
                           now,
                           now};
  catalogs.save(project_path, work);

  inde::project::StructuralNode parent{
      inde::project::new_uuid(),
      work.id,
      std::nullopt,
      inde::project::StructuralNodeType::Chapter,
      "Capítulo",
      "Subtítulo",
      "Sinopse do capítulo",
      "",
      "Planejamento",
      2000,
      now,
      now};
  inde::project::StructuralNode child{inde::project::new_uuid(),
                                      work.id,
                                      parent.id,
                                      inde::project::StructuralNodeType::Scene,
                                      "Cena",
                                      "",
                                      "Sinopse da cena",
                                      "",
                                      "Planejamento",
                                      1000,
                                      now,
                                      now};
  // O filho é salvo com posição menor para ser importado antes do pai. A FK
  // diferível precisa permitir essa ordem e validar tudo no commit.
  structures.save(project_path, parent);
  structures.save(project_path, child);

  const auto manifest_before = read_file(project_path / "manifest.json");
  const auto ip_path =
      project_path / "data" / "intellectual-properties" / (ip.id + ".json");
  const auto work_path = project_path / "data" / "works" / (work.id + ".json");
  const auto parent_path =
      project_path / "data" / "structural-nodes" / (parent.id + ".json");
  const auto child_path =
      project_path / "data" / "structural-nodes" / (child.id + ".json");
  const auto ip_before = read_file(ip_path);
  const auto work_before = read_file(work_path);
  const auto parent_before = read_file(parent_path);
  const auto child_before = read_file(child_path);

  inde::persistence::ProjectDatabaseImporter importer;
  const auto imported = importer.create_from_json(project_path);
  assert(imported.created);
  assert(imported.database_path == project_path / "data" / "project.sqlite3");
  assert(imported.intellectual_property_count == 1);
  assert(imported.work_count == 1);
  assert(imported.editorial_node_count == 2);
  assert(std::filesystem::is_regular_file(imported.database_path));
  assert(
      !std::filesystem::exists(project_path / "data" / "project.sqlite3.tmp"));

  // A fonte precisa permanecer byte a byte idêntica.
  assert(read_file(project_path / "manifest.json") == manifest_before);
  assert(read_file(ip_path) == ip_before);
  assert(read_file(work_path) == work_before);
  assert(read_file(parent_path) == parent_before);
  assert(read_file(child_path) == child_before);

  {
    inde::persistence::SqliteDatabase database(imported.database_path);
    assert(database.query_text("SELECT title FROM intellectual_properties") ==
           ip.title);
    assert(database.query_text("SELECT synopsis FROM works") == work.synopsis);
    assert(database.query_integer("SELECT count(*) FROM editorial_nodes") == 2);
    auto loaded_child = database.prepare(
        "SELECT parent_id, position FROM editorial_nodes WHERE id = ?");
    loaded_child.bind(1, child.id);
    assert(loaded_child.step());
    assert(loaded_child.column_text(0) == parent.id);
    assert(loaded_child.column_integer(1) == child.position);
  }

  const auto repeated = importer.create_from_json(project_path);
  assert(!repeated.created);
  assert(repeated.database_path == imported.database_path);

  // Simula uma interrupção depois da validação e antes da promoção.
  const auto pending = project_path / "data" / "project.sqlite3.tmp";
  std::filesystem::rename(imported.database_path, pending);
  const auto resumed = importer.create_from_json(project_path);
  assert(resumed.created);
  assert(std::filesystem::is_regular_file(resumed.database_path));
  assert(!std::filesystem::exists(pending));

  // Um banco já promovido mas divergente nunca deve ser sobrescrito.
  ip.title = "Título alterado apenas no JSON";
  catalogs.save(project_path, ip);
  bool stale_database_rejected = false;
  try {
    static_cast<void>(importer.create_from_json(project_path));
  } catch (const std::exception &) {
    stale_database_rejected = true;
  }
  assert(stale_database_rejected);
  inde::persistence::SqliteDatabase unchanged(resumed.database_path);
  assert(unchanged.query_text("SELECT title FROM intellectual_properties") ==
         "Universo Áureo 🚀");
}

void sqlite_import_rejects_invalid_json_without_artifacts() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  const auto project =
      projects.create(temporary.path / "Corrompido", "Projeto corrompido");
  const auto bad_work = project.path() / "data" / "works" / "bad.json";
  std::filesystem::create_directories(bad_work.parent_path());
  std::ofstream(bad_work) << "{ arquivo incompleto";

  bool rejected = false;
  try {
    static_cast<void>(
        inde::persistence::ProjectDatabaseImporter{}.create_from_json(
            project.path()));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(read_file(bad_work) == "{ arquivo incompleto");
  assert(!std::filesystem::exists(project.path() / "data" / "project.sqlite3"));
  assert(!std::filesystem::exists(project.path() / "data" /
                                  "project.sqlite3.tmp"));
}

void sqlite_imports_empty_json_project() {
  TemporaryDirectory temporary;
  const auto project = inde::persistence::ProjectRepository{}.create(
      temporary.path / "Vazio", "Projeto vazio");
  const auto result =
      inde::persistence::ProjectDatabaseImporter{}.create_from_json(
          project.path());
  assert(result.created);
  assert(result.intellectual_property_count == 0);
  assert(result.work_count == 0);
  assert(result.editorial_node_count == 0);
  inde::persistence::SqliteDatabase database(result.database_path);
  assert(database.query_integer("SELECT count(*) FROM projects") == 1);
  assert(database.query_integer(
             "SELECT count(*) FROM intellectual_properties") == 0);
  assert(database.query_integer("SELECT count(*) FROM works") == 0);
  assert(database.query_integer("SELECT count(*) FROM editorial_nodes") == 0);
}

void sqlite_import_rejects_orphan_work_without_artifacts() {
  TemporaryDirectory temporary;
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository catalogs;
  const auto project =
      projects.create(temporary.path / "Orfao", "Projeto com obra órfã");
  const auto now = inde::project::utc_now();
  inde::project::Work orphan{inde::project::new_uuid(),
                             inde::project::new_uuid(),
                             "Obra órfã",
                             "",
                             "",
                             "pt-BR",
                             "Planejamento",
                             now,
                             now};
  catalogs.save(project.path(), orphan);

  bool rejected = false;
  try {
    static_cast<void>(
        inde::persistence::ProjectDatabaseImporter{}.create_from_json(
            project.path()));
  } catch (const std::exception &) {
    rejected = true;
  }
  assert(rejected);
  assert(std::filesystem::is_regular_file(project.path() / "data" / "works" /
                                          (orphan.id + ".json")));
  assert(!std::filesystem::exists(project.path() / "data" / "project.sqlite3"));
  assert(!std::filesystem::exists(project.path() / "data" /
                                  "project.sqlite3.tmp"));
}

void project_service_uses_sqlite_as_the_only_operational_store() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;
  const auto source_path =
      service.create(temporary.path / "Operacional", "Projeto operacional")
          .path();
  const auto source_id = service.current()->manifest().project_id;
  assert(service.current()->manifest().format_version ==
         inde::project::current_format_version);
  assert(std::filesystem::is_regular_file(source_path / "data" /
                                          "project.sqlite3"));

  const auto ip_id =
      service.create_intellectual_property("Universo SQLite", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Obra SQLite", "", "", "pt-BR", "Planejamento")
          .id;
  service.create_structural_template(work_id, "parts-and-chapters");
  assert(service.structural_nodes_for_work(work_id).size() == 18);

  // Após o corte, o catálogo e a estrutura não mantêm uma segunda fonte viva.
  assert(!std::filesystem::exists(source_path / "data" /
                                  "intellectual-properties"));
  assert(!std::filesystem::exists(source_path / "data" / "works"));
  assert(!std::filesystem::exists(source_path / "data" / "structural-nodes"));

  inde::application::ProjectService reopened;
  reopened.open(source_path);
  assert(reopened.catalog().intellectual_properties.size() == 1);
  assert(reopened.catalog().works.size() == 1);
  assert(reopened.structural_nodes_for_work(work_id).size() == 18);

  const auto copy = reopened.save_as(temporary.path / "Copia operacional");
  assert(copy.manifest().project_id != source_id);
  inde::persistence::SqliteDatabase copied_database(copy.path() / "data" /
                                                    "project.sqlite3");
  assert(copied_database.query_text("SELECT id FROM projects") ==
         copy.manifest().project_id);
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM intellectual_properties") ==
         copy.manifest().project_id);
  inde::persistence::SqliteDatabase source_database(source_path / "data" /
                                                    "project.sqlite3");
  assert(source_database.query_text("SELECT id FROM projects") == source_id);
}

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

void legacy_project_is_upgraded_once_and_v2_requires_its_database() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::persistence::ProjectRepository projects;
  inde::persistence::CatalogRepository legacy_catalog;
  auto legacy = projects.create(temporary.path / "Legado", "Projeto legado");
  legacy.manifest().format_version = 1;
  projects.save(legacy);
  const auto now = inde::project::utc_now();
  inde::project::IntellectualProperty ip{
      inde::project::new_uuid(), "IP legada", "", "", "", now, now};
  legacy_catalog.save(legacy.path(), ip);

  inde::application::ProjectService service;
  service.open(legacy.path());
  assert(service.current()->manifest().format_version ==
         inde::project::current_format_version);
  assert(service.catalog().intellectual_properties.size() == 1);
  assert(inde::project::Manifest::from_json(
             read_file(legacy.path() / "manifest.json"))
             .format_version == inde::project::current_format_version);

  const auto database_path = legacy.path() / "data" / "project.sqlite3";
  assert(std::filesystem::remove(database_path));
  bool missing_database_rejected = false;
  try {
    inde::application::ProjectService{}.open(legacy.path());
  } catch (const std::exception &) {
    missing_database_rejected = true;
  }
  assert(missing_database_rejected);
  // O JSON legado permanece como material de compatibilidade, mas não é
  // reutilizado silenciosamente depois que o manifesto marca o corte.
  assert(std::filesystem::is_regular_file(
      legacy.path() / "data" / "intellectual-properties" / (ip.id + ".json")));
  assert(!std::filesystem::exists(database_path));
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

const inde::project::EntityType &
find_entity_type(const std::vector<inde::project::EntityType> &types,
                 std::string_view key) {
  const auto found =
      std::find_if(types.begin(), types.end(),
                   [&](const auto &value) { return value.key == key; });
  assert(found != types.end());
  return *found;
}

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

void writing_documents_persist_without_collapsing_editorial_identity() {
  TemporaryDirectory temporary;
  setenv("XDG_CONFIG_HOME", (temporary.path / "config").c_str(), 1);
  inde::application::ProjectService service;

  bool guarded_without_project = false;
  try {
    static_cast<void>(service.writing().documents());
  } catch (const std::exception &) {
    guarded_without_project = true;
  }
  assert(guarded_without_project);

  const auto source_path =
      service.create(temporary.path / "Fonte escrita", "Fonte escrita").path();
  const auto source_project_id = service.current()->manifest().project_id;
  const auto ip_id =
      service.create_intellectual_property("Universo", "", "").id;
  const auto work_id =
      service.create_work(ip_id, "Romance", "", "", "pt-BR", "Em escrita").id;
  const auto chapter_id =
      service
          .create_structural_node(work_id, std::nullopt,
                                  inde::project::StructuralNodeType::Chapter,
                                  "Capítulo do Sol")
          .id;
  const auto entity_types = service.narrative().entity_types();
  const auto character_type =
      std::find_if(entity_types.begin(), entity_types.end(),
                   [](const auto &type) { return type.key == "character"; });
  assert(character_type != entity_types.end());
  const auto character = service.narrative().create_entity(
      character_type->id, "Aurora", "Personagem vinculada à Escrita");

  const auto free_document =
      service.writing().create_document("Rascunho 100% livre");
  auto placed_document =
      service.writing().create_document("A chegada", chapter_id);
  placed_document.content =
      "Rascunho 100% livre é citado aqui.\n\n"
      "O sol nasce sobre a cidade.\n\nA personagem finalmente chega.\n" +
      std::string(256 * 1024, 'x') + "\nFim Unicode: 🚀";
  placed_document.word_goal = 90000;
  placed_document.formatting.push_back(
      {inde::project::DocumentTextStyle::Bold, 0, 8});
  const auto anchor_id = inde::project::new_uuid();
  placed_document.anchors.push_back({anchor_id, "Chegada de Aurora", 38, 66});
  placed_document.entity_references.push_back(
      {inde::project::new_uuid(), character.id, anchor_id,
       "Aurora participa deste trecho"});
  placed_document = service.writing().update_document(placed_document);
  const auto group = service.writing().create_document_group(
      "Capítulos em revisão", "Conjunto virtual de trabalho");
  placed_document.group_id = group.id;
  placed_document.purpose = inde::project::DocumentPurpose::Revision;
  placed_document.revision_of_id = free_document.id;
  placed_document.revision_label = "v2 — ritmo";
  placed_document.perspective = "Aurora";
  placed_document = service.writing().update_document(placed_document);

  assert(service.writing().document_count() == 2);
  assert(service.writing().document_groups().size() == 1);
  inde::persistence::DocumentQuery by_group;
  by_group.group_id = group.id;
  assert(service.writing().document_count(by_group) == 1);
  inde::persistence::DocumentQuery by_purpose;
  by_purpose.purpose = inde::project::DocumentPurpose::Revision;
  by_purpose.perspective = "Aur";
  by_purpose.revisions_only = true;
  assert(service.writing().document_summaries(by_purpose).front().id ==
         placed_document.id);
  assert(!free_document.editorial_node_id);
  assert(placed_document.editorial_node_id == chapter_id);

  inde::persistence::DocumentQuery by_content;
  by_content.search = "sol";
  const auto content_results = service.writing().documents(by_content);
  assert(content_results.size() == 1);
  assert(content_results.front().id == placed_document.id);
  const auto content_summaries =
      service.writing().document_summaries(by_content);
  assert(content_summaries.size() == 1);
  assert(content_summaries.front().id == placed_document.id);
  assert(content_summaries.front().character_count > 256 * 1024);
  assert(content_summaries.front().search_match ==
         inde::project::DocumentSearchMatch::Content);
  assert(content_summaries.front().entity_reference_count == 1);

  inde::persistence::DocumentQuery ranked;
  ranked.search = "Rascunho 100% livre";
  const auto ranked_results = service.writing().document_summaries(ranked);
  assert(ranked_results.size() == 2);
  assert(ranked_results.front().id == free_document.id);
  assert(ranked_results.front().search_match ==
         inde::project::DocumentSearchMatch::Name);
  assert(ranked_results.back().id == placed_document.id);
  assert(ranked_results.back().search_match ==
         inde::project::DocumentSearchMatch::Content);

  inde::persistence::DocumentQuery literal_wildcard;
  literal_wildcard.search = "_";
  const auto literal_results = service.writing().documents(literal_wildcard);
  assert(literal_results.empty());

  inde::persistence::DocumentQuery by_placement;
  by_placement.editorial_node_id = chapter_id;
  assert(service.writing().document_count(by_placement) == 1);
  assert(service.writing().documents(by_placement).front().id ==
         placed_document.id);

  inde::persistence::DocumentQuery by_entity;
  by_entity.entity_id = character.id;
  assert(service.writing().document_count(by_entity) == 1);
  assert(service.writing().document_summaries(by_entity).front().id ==
         placed_document.id);

  inde::application::PlanningContext entities_used_in_writing;
  entities_used_in_writing.require_document_reference = true;
  const auto used_snapshot =
      service.planning().explore_entities(entities_used_in_writing);
  assert(used_snapshot.matching_count == 1);
  assert(used_snapshot.items.front().entity.id == character.id);
  assert(used_snapshot.items.front().primary_reason.find("Documento") !=
         std::string::npos);
  entities_used_in_writing.document_id = placed_document.id;
  const auto document_snapshot =
      service.planning().explore_entities(entities_used_in_writing);
  assert(document_snapshot.matching_count == 1);
  assert(document_snapshot.items.front().entity.id == character.id);

  auto invalid = placed_document;
  invalid.title.clear();
  bool invalid_title_rejected = false;
  try {
    static_cast<void>(service.writing().update_document(invalid));
  } catch (const std::exception &) {
    invalid_title_rejected = true;
  }
  assert(invalid_title_rejected);
  assert(service.writing().document(placed_document.id)->title == "A chegada");

  auto invalid_reference = placed_document;
  invalid_reference.entity_references.push_back(
      {inde::project::new_uuid(), inde::project::new_uuid(), std::nullopt, ""});
  bool invalid_reference_rejected = false;
  try {
    static_cast<void>(service.writing().update_document(invalid_reference));
  } catch (const std::exception &) {
    invalid_reference_rejected = true;
  }
  assert(invalid_reference_rejected);
  assert(service.writing()
             .document(placed_document.id)
             ->entity_references.size() == 1);

  std::string long_paragraph;
  for (int index = 0; index < 125; ++index)
    long_paragraph += "palavra ";
  const auto statistics = inde::project::analyze_document_text(
      "eco eco eco.\n\n" + long_paragraph, 200, 120);
  assert(statistics.words == 128);
  assert(statistics.paragraphs == 2);
  assert(statistics.sentences == 1);
  assert(statistics.reading_minutes == 1);
  assert(!statistics.repeated_words.empty());
  assert(statistics.long_paragraphs.size() == 1);
  const auto line_paragraphs =
      inde::project::analyze_document_text("primeiro\nsegundo\n\nterceiro");
  assert(line_paragraphs.paragraphs == 3);
  assert(inde::project::analyze_document_text("Espere... o quê?!").sentences ==
         2);

  service.save();
  service.close();
  service.open(source_path);
  assert(service.writing().document_count() == 2);
  assert(service.writing().document(placed_document.id)->content ==
         placed_document.content);
  const auto reopened_document = service.writing().document(placed_document.id);
  assert(reopened_document->word_goal == 90000);
  assert(reopened_document->formatting.size() == 1);
  assert(reopened_document->anchors.size() == 1);
  assert(reopened_document->entity_references.size() == 1);
  assert(reopened_document->group_id == group.id);
  assert(reopened_document->purpose ==
         inde::project::DocumentPurpose::Revision);
  assert(reopened_document->revision_of_id == free_document.id);
  assert(reopened_document->revision_label == "v2 — ritmo");
  assert(reopened_document->perspective == "Aurora");

  const auto copy = service.save_as(temporary.path / "Cópia escrita");
  const auto copy_project_id = copy.manifest().project_id;
  assert(copy_project_id != source_project_id);
  assert(service.writing().document_count() == 2);
  assert(service.writing().document_groups().size() == 1);
  auto copied_document = *service.writing().document(placed_document.id);
  copied_document.content += "\nEsta frase existe apenas na cópia.";
  copied_document = service.writing().update_document(copied_document);

  // A colocação é uma referência opcional: remover a unidade editorial
  // preserva o Documento e apenas limpa sua localização na cópia.
  service.delete_structural_branch(chapter_id);
  assert(!service.writing()
              .document(placed_document.id)
              ->editorial_node_id.has_value());
  service.narrative().delete_entity(character.id);
  assert(service.writing()
             .document(placed_document.id)
             ->entity_references.empty());
  assert(service.writing().document(placed_document.id)->anchors.size() == 1);
  assert(service.writing().document(placed_document.id)->formatting.size() ==
         1);
  service.writing().delete_document(free_document.id);
  assert(service.writing().document_count() == 1);

  inde::persistence::SqliteDatabase copied_database(copy.path() / "data" /
                                                    "project.sqlite3");
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM documents") == copy_project_id);
  assert(copied_database.query_text(
             "SELECT DISTINCT project_id FROM document_groups") ==
         copy_project_id);

  inde::application::ProjectService original;
  original.open(source_path);
  assert(original.writing().document_count() == 2);
  const auto original_document =
      original.writing().document(placed_document.id);
  assert(original_document);
  assert(original_document->content == placed_document.content);
  assert(original_document->editorial_node_id == chapter_id);
  assert(original_document->word_goal == 90000);
  assert(original_document->formatting.size() == 1);
  assert(original_document->anchors.size() == 1);
  assert(original_document->entity_references.size() == 1);
  assert(original_document->group_id == group.id);
  assert(original_document->perspective == "Aurora");
}

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

void timeline_projection_is_deterministic_and_selectable() {
  using namespace inde;
  const project::FictionalTimeAxis axis{
      "axis", "Cronologia principal", "", true, "", ""};
  const std::vector<project::FictionalTimePoint> points{
      {"late", "axis", 20, "Depois", "", "", ""},
      {"early", "axis", -10, "Antes", "", "", ""},
      {"middle", "axis", 0, "Agora", "", "", ""}};
  const std::vector<project::NarrativeEntity> entities{
      {"event", "event-type", "A chegada", "", "", ""},
      {"character", "character-type", "Lia", "", "", ""},
      {"place", "location-type", "Estação", "", "", ""}};
  const std::vector<project::EventOccurrence> occurrences{
      {"occurrence-b", "event", "middle", "segunda marca", "", ""},
      {"occurrence-a", "event", "middle", "primeira marca", "", ""}};
  const std::vector<project::EntityPresence> presences{
      {"open-presence", "character", "place", "middle", std::nullopt,
       "permanece", "", ""},
      {"closed-presence", "character", "place", "early",
       std::optional<std::string>{"late"}, "travessia", "", ""}};

  const auto snapshot = application::build_timeline_snapshot(
      axis, points, occurrences, presences, entities, true);
  assert(snapshot.possibly_truncated);
  assert(snapshot.points.size() == 3);
  assert(snapshot.points.front().id == "early");
  assert(snapshot.points.back().id == "late");
  assert(snapshot.events.front().occurrence_id == "occurrence-a");
  assert(snapshot.events.front().entity_name == "A chegada");
  assert(snapshot.presences.front().presence_id == "closed-presence");
  assert(snapshot.presences.back().location_name == "Estação");

  const auto layout = application::layout_timeline(snapshot, 900.0);
  assert(layout.width == 900.0);
  assert(layout.minimum_ordinal == -10);
  assert(layout.maximum_ordinal == 20);
  assert(application::timeline_x_for_ordinal(-10, layout) <
         application::timeline_x_for_ordinal(0, layout));
  assert(application::timeline_x_for_ordinal(0, layout) <
         application::timeline_x_for_ordinal(20, layout));
  assert(application::recommended_timeline_content_width(snapshot, 900.0,
                                                         1.0) == 900.0);

  auto dense_snapshot = snapshot;
  for (int index = 0; index < 30; ++index)
    dense_snapshot.points.push_back(
        {"dense-" + std::to_string(index), 30 + index,
         "Ponto denso " + std::to_string(index), ""});
  const auto dense_width = application::recommended_timeline_content_width(
      dense_snapshot, 900.0, 1.0);
  assert(dense_width > 4000.0);
  assert(application::recommended_timeline_content_width(
             dense_snapshot, 900.0, 2.0) == dense_width * 2.0);

  const auto event_item = std::find_if(
      layout.items.begin(), layout.items.end(), [](const auto &item) {
        return item.kind == application::TimelineVisualKind::Event &&
               item.id == "occurrence-a";
      });
  assert(event_item != layout.items.end());
  const auto selected = application::timeline_hit_test(
      layout, event_item->x + event_item->width / 2.0,
      event_item->y + event_item->height / 2.0);
  assert(selected && selected->id == "occurrence-a");

  const auto open_item = std::find_if(
      layout.items.begin(), layout.items.end(), [](const auto &item) {
        return item.kind == application::TimelineVisualKind::Presence &&
               item.id == "open-presence";
      });
  assert(open_item != layout.items.end());
  assert(open_item->x + open_item->width == layout.right);
  assert(!application::timeline_hit_test(layout, 1.0, 1.0));

  application::TimelineFilter filter;
  filter.search = "ESTAÇÃO";
  const auto by_location =
      application::filter_timeline_snapshot(snapshot, filter);
  assert(by_location.points.size() == snapshot.points.size());
  assert(by_location.events.empty());
  assert(by_location.presences.size() == 2);
  assert(by_location.possibly_truncated);

  filter.search = "chegada";
  filter.show_presences = false;
  const auto events_only =
      application::filter_timeline_snapshot(snapshot, filter);
  assert(events_only.events.size() == 2);
  assert(events_only.presences.empty());

  filter.search.clear();
  filter.show_events = false;
  filter.show_presences = false;
  const auto axis_only =
      application::filter_timeline_snapshot(snapshot, filter);
  assert(axis_only.points.size() == 3);
  assert(axis_only.events.empty());
  assert(axis_only.presences.empty());

  auto invalid_occurrences = occurrences;
  invalid_occurrences.front().time_point_id = "missing";
  bool missing_point_rejected = false;
  try {
    static_cast<void>(application::build_timeline_snapshot(
        axis, points, invalid_occurrences, presences, entities));
  } catch (const std::exception &) {
    missing_point_rejected = true;
  }
  assert(missing_point_rejected);
}

void temporal_view_context_has_explicit_window_scope_and_limits() {
  using namespace inde;
  const project::FictionalTimeAxis axis{
      "axis", "Cronologia principal", "", true, "", ""};
  const std::vector<project::FictionalTimePoint> points{
      {"early", "axis", -10, "Antes", "", "", ""},
      {"middle", "axis", 0, "Agora", "", "", ""},
      {"late", "axis", 20, "Depois", "", "", ""}};
  const std::vector<project::NarrativeEntity> entities{
      {"event-a", "event", "A descoberta", "", "", ""},
      {"event-b", "event", "A fuga", "", "", ""},
      {"character", "character", "Lia", "", "", ""},
      {"place", "location", "Estação", "", "", ""}};
  const std::vector<project::EventOccurrence> occurrences{
      {"occurrence-early", "event-a", "early", "", "", ""},
      {"occurrence-middle", "event-a", "middle", "pista confirmada", "", ""},
      {"occurrence-late", "event-b", "late", "", "", ""}};
  const std::vector<project::EntityPresence> presences{
      {"closed", "character", "place", "early", "late", "travessia", "", ""},
      {"open", "character", "place", "middle", std::nullopt, "espera", "", ""}};
  const std::vector<project::EntityWorkScope> scopes{
      {"scope-event-a", "event-a", "work-a", "", "", ""},
      {"scope-event-b", "event-b", "work-b", "", "", ""},
      {"scope-character-a", "character", "work-a", "", "", ""},
      {"scope-character-b", "character", "work-b", "", "", ""}};

  application::TemporalViewContext point_context;
  point_context.fictional_axis_id = "axis";
  point_context.work_id = "work-a";
  point_context.window.kind = application::TemporalWindowKind::Point;
  point_context.window.start_point_id = "middle";
  const auto at_middle = application::build_temporal_view_snapshot(
      point_context, axis, points, occurrences, presences, entities, scopes);
  assert(at_middle.timeline.points.size() == 1);
  assert(at_middle.timeline.points.front().id == "middle");
  assert(at_middle.timeline.events.size() == 1);
  assert(at_middle.timeline.events.front().occurrence_id ==
         "occurrence-middle");
  assert(at_middle.timeline.presences.size() == 2);
  assert(at_middle.timeline.events.front().inclusion_reason.find("ponto") !=
         std::string::npos);
  assert(at_middle.timeline.presences.front().inclusion_reason.find("Obra") !=
         std::string::npos);
  const auto point_agenda =
      application::build_temporal_agenda(at_middle.timeline);
  assert(point_agenda.size() == 4);
  assert(std::ranges::count_if(point_agenda, [](const auto &entry) {
           return entry.kind ==
                  application::TemporalAgendaEntryKind::PresenceActive;
         }) == 2);

  application::TemporalViewContext range_context;
  range_context.fictional_axis_id = "axis";
  range_context.window.kind = application::TemporalWindowKind::ClosedRange;
  range_context.window.start_point_id = "middle";
  range_context.window.end_point_id = "late";
  const auto ranged = application::build_temporal_view_snapshot(
      range_context, axis, points, occurrences, presences, entities, scopes);
  assert(ranged.timeline.points.size() == 2);
  assert(ranged.timeline.events.size() == 2);
  assert(ranged.timeline.presences.size() == 2);
  assert(ranged.timeline.events.front().occurrence_id == "occurrence-middle");
  assert(ranged.timeline.events.back().occurrence_id == "occurrence-late");
  assert(ranged.timeline.presences.back().end_ordinal == std::nullopt);
  const auto range_agenda = application::build_temporal_agenda(ranged.timeline);
  assert(range_agenda.size() == 7);
  assert(std::ranges::count_if(range_agenda, [](const auto &entry) {
           return entry.source_id == "closed";
         }) == 2);
  assert(std::ranges::count_if(range_agenda, [](const auto &entry) {
           return entry.source_id == "open";
         }) == 1);
  assert(std::ranges::any_of(range_agenda, [](const auto &entry) {
    return entry.kind == application::TemporalAgendaEntryKind::PresenceActive &&
           entry.detail.find("antes do recorte") != std::string::npos;
  }));

  range_context.work_id = "work-a";
  const auto scoped = application::build_temporal_view_snapshot(
      range_context, axis, points, occurrences, presences, entities, scopes,
      true);
  assert(scoped.timeline.events.size() == 1);
  assert(scoped.timeline.events.front().entity_id == "event-a");
  assert(scoped.timeline.presences.size() == 2);
  assert(scoped.truncation.source);

  range_context.limit = 1;
  range_context.work_id.reset();
  const auto limited = application::build_temporal_view_snapshot(
      range_context, axis, points, occurrences, presences, entities, scopes);
  assert(limited.matching.events == 2);
  assert(limited.timeline.events.size() == 1);
  assert(limited.truncation.events);
  assert(limited.timeline.possibly_truncated);

  bool inverted_rejected = false;
  try {
    range_context.window.start_point_id = "late";
    range_context.window.end_point_id = "middle";
    static_cast<void>(application::build_temporal_view_snapshot(
        range_context, axis, points, occurrences, presences, entities, scopes));
  } catch (const std::exception &) {
    inverted_rejected = true;
  }
  assert(inverted_rejected);

  bool incompatible_point_rejected = false;
  try {
    point_context.window.start_point_id = "other-axis-point";
    static_cast<void>(application::build_temporal_view_snapshot(
        point_context, axis, points, occurrences, presences, entities, scopes));
  } catch (const std::exception &) {
    incompatible_point_rejected = true;
  }
  assert(incompatible_point_rejected);
}

} // namespace

int main() {
  project_round_trip();
  rejects_future_format();
  strict_json_codec();
  dictionaries_and_proofreading_are_global_and_persistent();
  recents_are_deduplicated();
  recents_failure_does_not_block_project_lifecycle();
  failed_open_preserves_active_session();
  specialized_services_share_a_guarded_session();
  catalog_round_trip();
  structural_tree_round_trip_and_integrity();
  structural_path_labels_disambiguate_repeated_titles();
  legacy_structural_positions_are_compatible();
  structural_templates_and_duplication();
  large_structural_tree();
  sqlite_schema_and_transactions();
  sqlite_database_closes_before_copy();
  sqlite_v11_migrates_structural_identity_and_per_type_designators();
  sqlite_foreign_keys_and_editorial_parent_integrity();
  sqlite_rejects_future_schema();
  sqlite_rolls_back_interrupted_migration();
  sqlite_imports_json_project_safely();
  sqlite_import_rejects_invalid_json_without_artifacts();
  sqlite_imports_empty_json_project();
  sqlite_import_rejects_orphan_work_without_artifacts();
  project_service_uses_sqlite_as_the_only_operational_store();
  structural_element_types_and_designators_are_managed();
  structure_models_instances_and_narrative_flow_are_independent();
  legacy_project_is_upgraded_once_and_v2_requires_its_database();
  sqlite_structural_batches_are_atomic();
  narrative_foundation_enforces_identity_and_relations();
  narrative_foundation_survives_save_as_independently();
  writing_documents_persist_without_collapsing_editorial_identity();
  planning_places_events_in_explicit_fictional_time();
  temporal_queries_are_limited_inside_the_selected_axis();
  narrative_editorial_links_preserve_layer_boundaries();
  narrative_queries_remain_bounded_at_scale();
  planning_context_combines_filters_and_explains_results();
  timeline_projection_is_deterministic_and_selectable();
  temporal_view_context_has_explicit_window_scope_and_limits();
  std::cout << "Todos os testes do núcleo passaram.\n";
}

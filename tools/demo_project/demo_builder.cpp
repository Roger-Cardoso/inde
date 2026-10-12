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

DemoBuilder::DemoBuilder(std::filesystem::path output)
    : output_(std::move(output)) {}

void DemoBuilder::build() {
    if (std::filesystem::exists(output_))
      throw std::runtime_error("o destino já existe: " + output_.string());

    service_.create(output_,
                    "Atlas de Véspera — Projeto integral de avaliação");
    create_catalog();
    create_structures();
    create_entities();
    create_relations();
    create_timeline();
    create_narrative_structures();
    create_scopes_and_references();
    create_documents();
    organize_documents();
    service_.save();
    audit();
  }

std::string DemoBuilder::add_node(const std::string &key, const std::string &work,
                     const std::optional<std::string> &parent,
                     StructuralNodeType type, const std::string &title,
                     const std::string &subtitle, const std::string &synopsis,
                     const std::string &status,
                     const std::string &custom_type) {
    auto node = service_.create_structural_node(works_.at(work), parent, type,
                                                title, custom_type);
    const auto siblings = service_.structural_nodes_for_work(works_.at(work));
    node.designator = std::to_string(
        std::ranges::count_if(siblings, [&](const auto &candidate) {
          return candidate.parent_id == parent &&
                 candidate.structural_type_id == node.structural_type_id;
        }));
    node.subtitle = subtitle;
    node.synopsis = synopsis;
    node.status = status;
    service_.update_structural_node(node);
    nodes_[key] = node.id;
    return node.id;
  }

void DemoBuilder::add_entity(const std::string &type, const EntitySpec &spec) {
    const auto value = service_.narrative().create_entity(
        types_.at(type), spec.name, spec.summary);
    entities_[spec.key] = value.id;
    entity_keys_.push_back(spec.key);
  }

void DemoBuilder::add_relation(const std::string &type, const std::string &source,
                  const std::string &target, const std::string &description) {
    static_cast<void>(service_.narrative().create_relation(
        relation_types_.at(type), entities_.at(source), entities_.at(target),
        description));
  }

std::string DemoBuilder::display_name(const std::string &key) const {
    const auto id = entities_.at(key);
    const auto entity = service_.narrative().entity(id);
    return entity ? entity->name : key;
  }

void DemoBuilder::audit() const {
    const auto database_path = output_ / "data" / "project.sqlite3";
    inde::persistence::SqliteDatabase database(database_path);
    const std::array<const char *, 29> tables{"intellectual_properties",
                                              "works",
                                              "editorial_nodes",
                                              "entity_types",
                                              "entities",
                                              "relation_types",
                                              "relations",
                                              "entity_work_scopes",
                                              "editorial_entity_references",
                                              "fictional_time_axes",
                                              "fictional_time_points",
                                              "event_occurrences",
                                              "document_groups",
                                              "documents",
                                              "document_format_spans",
                                              "document_anchors",
                                              "document_entity_references",
                                              "structure_models",
                                              "structure_model_items",
                                              "structure_model_item_links",
                                              "editorial_structures",
                                              "narrative_structures",
                                              "narrative_lines",
                                              "narrative_units",
                                              "narrative_unit_lines",
                                              "narrative_unit_entities",
                                              "narrative_links",
                                              "narrative_roles",
                                              "narrative_role_assignments"};
    std::cout << "Projeto criado: " << output_ << '\n';
    std::cout << "UUID: " << service_.current()->manifest().project_id << '\n';
    for (const auto *table : tables)
      std::cout << table << '='
                << database.query_integer(std::string("SELECT COUNT(*) FROM ") +
                                          table)
                << '\n';
    std::cout << "event_participations="
              << database.query_integer(
                     "SELECT COUNT(*) FROM event_participations")
              << '\n';
    std::cout << "entity_presences="
              << database.query_integer("SELECT COUNT(*) FROM entity_presences")
              << '\n';
    std::cout << "change_log="
              << database.query_integer("SELECT COUNT(*) FROM change_log")
              << '\n';
    std::cout << "foreign_key_violations="
              << database.query_integer(
                     "SELECT COUNT(*) FROM pragma_foreign_key_check")
              << '\n';
    const auto require_count = [&database](const std::string &label,
                                           const std::string &query,
                                           std::int64_t expected) {
      const auto found = database.query_integer(query);
      if (found != expected)
        throw std::runtime_error(label + ": esperado " +
                                 std::to_string(expected) + ", obtido " +
                                 std::to_string(found));
    };
    require_count("Documentos", "SELECT COUNT(*) FROM documents", 12);
    require_count("Grupos documentais", "SELECT COUNT(*) FROM document_groups",
                  3);
    require_count("Documentos agrupados",
                  "SELECT COUNT(*) FROM documents WHERE group_id IS NOT NULL",
                  12);
    require_count(
        "Revisões documentais",
        "SELECT COUNT(*) FROM documents WHERE revision_of_id IS NOT NULL", 1);
    require_count(
        "Documentos colocados",
        "SELECT COUNT(*) FROM documents WHERE editorial_node_id IS NOT NULL",
        9);
    require_count(
        "Documentos livres",
        "SELECT COUNT(*) FROM documents WHERE editorial_node_id IS NULL", 3);
    require_count("Obras com texto",
                  "SELECT COUNT(DISTINCT n.work_id) FROM documents d "
                  "JOIN editorial_nodes n ON n.id = d.editorial_node_id",
                  4);
    require_count("Intervalos de formatação",
                  "SELECT COUNT(*) FROM document_format_spans", 25);
    require_count("Âncoras", "SELECT COUNT(*) FROM document_anchors", 21);
    require_count("Referências documentais",
                  "SELECT COUNT(*) FROM document_entity_references", 62);
    require_count("Estruturas editoriais alternativas",
                  "SELECT COUNT(*) FROM editorial_structures", 5);
    require_count("Estruturas narrativas",
                  "SELECT COUNT(*) FROM narrative_structures", 5);
    require_count("Linhas narrativas", "SELECT COUNT(*) FROM narrative_lines",
                  6);
    require_count("Unidades narrativas", "SELECT COUNT(*) FROM narrative_units",
                  12);
    require_count("Pertencimentos narrativos",
                  "SELECT COUNT(*) FROM narrative_unit_lines", 18);
    require_count("Vínculos narrativos", "SELECT COUNT(*) FROM narrative_links",
                  12);
    require_count("Papéis narrativos contextuais",
                  "SELECT COUNT(*) FROM narrative_role_assignments", 3);
    require_count("Violações de chave estrangeira",
                  "SELECT COUNT(*) FROM pragma_foreign_key_check", 0);
    if (database.query_text("PRAGMA integrity_check") != "ok")
      throw std::runtime_error("integrity_check do projeto integral falhou");
  }

} // namespace inde::demo

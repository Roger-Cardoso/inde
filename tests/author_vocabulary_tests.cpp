#include "inde/ui/author_vocabulary.hpp"
#include "inde/ui/author_diagnostics.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <glib.h>
#include <iostream>
#include <set>

using namespace inde;
template <class F> void refuses(F action) {
  bool failed = false;
  try { action(); } catch (const std::exception &) { failed = true; }
  assert(failed);
}
int main() {
  assert(ui::author::diagnostic("Opção vazia ou duplicada").find("opções repetidas") != std::string::npos);
  assert(ui::author::diagnostic("Entidade Mara — arquivo /tmp/âncora.txt") == "Entidade Mara — arquivo /tmp/âncora.txt");
  std::set<std::string> keys;
  for (int i = 0; i < 100; ++i) {
    const auto key = ui::author::new_category_key();
    project::validate_key(key);
    assert(key.size() <= 64 && keys.insert(key).second);
  }
  const auto builtin = project::builtin_annotation_types();
  for (const auto &original : builtin) {
    const auto saved = persistence::encode_semantic_type(original);
    auto type = original;
    const auto displayed = ui::author::annotation_label(type);
    assert(!displayed.empty());
    for (const auto &field : type.fields)
      assert(!ui::author::field_label(type, field).empty());
    assert(type == original && persistence::encode_semantic_type(type) == saved);
    type.system = false;
    type.label = "Entidade, âncora e composição — nomes pessoais";
    assert(ui::author::annotation_label(type) == type.label);
    for (auto &field : type.fields) {
      field.label = "Pensador — escolhido pelo autor";
      assert(ui::author::field_label(type, field) == field.label);
    }
  }
  project::EntityType location;
  location.key = "location"; location.name = "Local"; location.is_builtin = true;
  assert(ui::author::category_label(location) == "Lugar" && location.name == "Local");
  location.is_builtin = false;
  assert(ui::author::category_label(location) == "Local");
  std::set<std::string_view> names;
  for (const auto &term : ui::author::terms)
    assert(!term.explanation.empty() && !term.aliases.empty() && names.insert(term.name).second);
  for (const auto name : {"Ordem do plano narrativo", "Cronologia da história", "Ordem de escrita", "Ordem do manuscrito"})
    assert(names.contains(name));

  const auto key = "project." + ui::author::new_category_key();
  using K = project::SemanticFieldKind;
  std::vector<ui::author::PersonalField> fields{
      {"Tensão", K::Enum, true, false, {"Baixa", "Média", "Alta"}},
      {"Nota com \"aspas\" e coração ♥", K::Text, false, false, {}},
      {"Personagens", K::Entity, false, true, {}},
      {"Número", K::Integer, false, false, {}},
      {"Rever", K::Boolean, false, false, {}}};
  const auto source = ui::author::personal_definition("Ritmo ♥", "Campo pessoal", fields, key);
  const auto definition = persistence::decode_semantic_type(source);
  assert(definition.key == key && !definition.system && definition.version == 1 &&
         definition.label == "Ritmo ♥" && definition.fields.size() == 5);
  assert(definition.fields[0].values == fields[0].choices && definition.fields[1].label == fields[1].name);
  assert(ui::author::personal_definition("Outro nome", "", fields, key).find(key) != std::string::npos);
  refuses([&] { ui::author::personal_definition("", "", fields, key); });
  refuses([&] { ui::author::personal_definition(" \t ", "", fields, key); });
  refuses([&] { ui::author::personal_definition("Nome", "", fields, "inde.semantic.personal"); });
  auto invalid = fields;
  invalid[0].choices = {"Alta", "Alta"};
  refuses([&] { ui::author::personal_definition("Nome", "", invalid, key); });
  invalid[0].choices.clear();
  refuses([&] { ui::author::personal_definition("Nome", "", invalid, key); });
  invalid = fields; invalid[1].name.clear();
  refuses([&] { ui::author::personal_definition("Nome", "", invalid, key); });
  invalid.assign(33, fields[1]);
  refuses([&] { ui::author::personal_definition("Nome", "", invalid, key); });

  const auto root = std::filesystem::temp_directory_path() / ("inde-author-vocabulary-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService service;
  service.create(root / "Vazio.inde", "Projeto sem nomenclatura técnica");
  const auto old_types = service.writing().semantic_types();
  service.writing().register_semantic_type_json(source);
  refuses([&] { service.writing().register_semantic_type_json(source); });
  const auto personal_category = service.narrative().create_entity_type(ui::author::new_category_key(), "Entidade escolhida pelo autor", "Descrição");
  auto relation = service.narrative().create_relation_type(ui::author::new_category_key(), "protege", "é protegido por", project::RelationDirectionality::Directed, "Relação pessoal");
  assert(relation.name == "protege" && relation.inverse_name == "é protegido por");
  const auto inspect = [&] {
    const auto types = service.writing().semantic_types();
    for (const auto &old : old_types) {
      const auto *found = project::find_annotation_type(types, old.key, old.version);
      assert(found && *found == old);
    }
    const auto *found = project::find_annotation_type(types, key, 1);
    assert(found && *found == definition);
    const auto categories = service.narrative().entity_types();
    const auto c = std::find_if(categories.begin(), categories.end(), [&](const auto &v) { return v.id == personal_category.id; });
    assert(c != categories.end() && c->name == personal_category.name && ui::author::category_label(*c) == personal_category.name);
    persistence::SqliteDatabase db(persistence::ProjectDatabaseRepository::database_path(service.current()->path()));
    assert(persistence::SchemaMigrator{}.current_version(db) == 28);
    assert(db.query_text("PRAGMA integrity_check") == "ok" && !db.prepare("PRAGMA foreign_key_check").step());
    assert(db.query_integer("SELECT count(*) FROM works") == 0 && db.query_integer("SELECT count(*) FROM documents") == 0);
  };
  inspect();
  service.save_as(root / "Cópia.inde"); service.close(); service.open(root / "Cópia.inde");
  inspect(); service.close();
  std::filesystem::remove_all(root);
  std::cout << "PASS vocabulary: independent orders, unchanged contracts/custom names, opaque keys, graphical definitions, duplicate/invalid refusal, schema28, integrity, SaveAs and restart\n";
}

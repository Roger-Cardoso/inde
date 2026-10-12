#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "schema20_downgrade.hpp"
#include <cassert>
#include <fstream>
#include <glib.h>
#include <iostream>
#include <source_location>
using namespace inde;
template <class F>
void rejects(F f, std::source_location at = std::source_location::current()) {
  bool caught{};
  try {
    f();
  } catch (const std::exception &) {
    caught = true;
  }
  if (!caught)
    throw std::runtime_error("Expected refusal at " +
                             std::to_string(at.line()));
}
std::string read(const std::filesystem::path &p) {
  std::ifstream input(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-composition-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  const auto path = root / "Composição.inde";
  application::ProjectService s;
  s.create(path, "Composição QA");
  const auto dbpath =
      persistence::ProjectDatabaseRepository::database_path(path);
  assert(s.compositions().list().empty());
  auto d = s.writing().create_document("Origem — Água 🙂");
  d.content = "Água 🙂\nA carta chegou.\n";
  d.perspective = "Narrador externo";
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}},
                  {project::DocumentTextStyle::Foreground, 0, 4, "#123456"}};
  d.anchors.push_back({project::new_uuid(), "Atravessa corte", 2, 12});
  d.inline_commands = {{"@", "eco", "Eco", "Água 🙂"}};
  const auto definitions = s.writing().semantic_types();
  const auto *comment =
      project::find_annotation_type(definitions, "inde.editor.comment", 1);
  assert(comment);
  d = application::TextSemanticService::create(
      d, *comment, 0, 4,
      {{"note", {project::SemanticFieldKind::Text, {"Conferir origem"}}},
       {"state", {project::SemanticFieldKind::Enum, {"open"}}}});
  d = s.writing().update_document(d);
  const auto original = d;
  assert(project::same_document_state(
      d,
      persistence::decode_document_snapshot(
          persistence::encode_document_snapshot(d)),
      true));
  auto c = s.compositions().create("Minha Composição");
  const auto id = c.value.blocks.composition_id();
  const auto first = s.compositions().import(
      c.value, d, 0, project::utf8_character_count(d.content));
  const auto first_block = c.value.blocks.occurrence(first).block_id;
  const auto r0 = c.value.blocks.block(first_block).current_revision;
  const auto fixed = c.value.blocks.place(first_block);
  c.value.blocks.pin(fixed, r0);
  const auto cut = s.compositions().import(c.value, d, 0, 5);
  assert(!c.value.blocks.revision(c.value.blocks.occurrence(cut))
              .layers.warnings.empty());
  c = s.compositions().save(c);
  auto round = s.compositions().load(id);
  assert(persistence::encode_composition(round.value) ==
         persistence::encode_composition(c.value));
  // Even a matching recomputed hash must not authorize an invalid structure.
  const auto envelope =
      persistence::json::parse(persistence::encode_composition(c.value))
          .as_object();
  auto invalid_envelope = [&](const persistence::json::Value::Object &v) {
    const auto payload = persistence::json::serialize(v, false);
    rejects([&] {
      persistence::decode_composition(
          payload, persistence::composition_checksum(payload));
    });
  };
  auto bad = envelope;
  bad["version"] = 999;
  invalid_envelope(bad);
  bad = envelope;
  bad["unknown"] = true;
  invalid_envelope(bad);
  bad = envelope;
  bad["id"] = "identidade inválida";
  invalid_envelope(bad);
  bad = envelope;
  auto placements = bad.at("placements").as_array();
  placements.push_back(placements.front());
  bad["placements"] = placements;
  invalid_envelope(bad);
  bad = envelope;
  placements = bad.at("placements").as_array();
  auto item = placements.front().as_object();
  item["mode"] = "automatic";
  placements[0] = item;
  bad["placements"] = placements;
  invalid_envelope(bad);
  bad = envelope;
  bad["origins"] = persistence::json::Value::Object{};
  invalid_envelope(bad);
  bad = envelope;
  auto profile = bad.at("profile").as_object();
  profile["visible"] = persistence::json::Value::Array{"camada inexistente"};
  bad["profile"] = profile;
  invalid_envelope(bad);
  const std::string nested(20, '[');
  rejects([&] {
    persistence::decode_composition(nested + "0" + std::string(20, ']'),
                                    std::string(64, '0'));
  });
  auto cyclic = c.value.blocks.snapshot();
  auto &cyclic_block = cyclic.blocks.at(first_block);
  cyclic_block.fork_origin = {first_block, r0};
  rejects([&] { (void)experimental::TextBlocksLab::restore(cyclic); });
  assert(project::same_document_state(*s.writing().document(d.id), original,
                                      true));
  const auto projection = c.value.blocks.project(c.value.profile).text;

  // Durable drafts are not applied, are not Project-operation noise, survive
  // restart.
  const auto operation_count = s.project_operations().size();
  c.value.drafts[first_block] = {r0, "Água 🙂\nA carta mudou no rascunho.\n"};
  c = s.compositions().save(c, true);
  assert(s.project_operations().size() == operation_count);
  assert(c.value.blocks.project(c.value.profile).text == projection);
  rejects([&] { s.compositions().prepare(c, "Saída"); });
  s.close();
  s.open(path);
  c = s.compositions().load(id);
  assert(c.value.drafts.at(first_block).text.find("rascunho") !=
         std::string::npos);
  auto forged_draft = c;
  forged_draft.value.name = "Alteração indevida";
  rejects([&] { s.compositions().save(forged_draft, true); });
  const auto draft = c.value.drafts.at(first_block);
  c.value.blocks.revise(first_block, draft.expected_revision, draft.text);
  c.value.drafts.erase(first_block);
  c = s.compositions().save(c);
  assert(c.value.blocks.revision(c.value.blocks.occurrence(fixed)).text ==
         d.content);
  assert(c.value.blocks.revision(c.value.blocks.occurrence(first)).text ==
         draft.text);
  assert(project::same_document_state(*s.writing().document(d.id), original,
                                      true));

  // Stable full-source identity distinguishes imports of one Document at
  // different times.
  d.content = "Outra versão autoral\n";
  d.anchors.clear();
  d.semantics = {};
  d.formatting.clear();
  d = s.writing().update_document(d);
  const auto later = s.compositions().import(
      c.value, d, 0, project::utf8_character_count(d.content));
  assert(c.value.blocks.block(c.value.blocks.occurrence(later).block_id)
             .origin_snapshot_sha256 !=
         c.value.blocks.block(first_block).origin_snapshot_sha256);
  c.value.blocks.set_hidden(later, true);
  c = s.compositions().save(c);
  s.writing().delete_document(d.id);
  c = s.compositions().load(id);
  assert(c.value.origins.size() == 2 && !s.writing().document(d.id));
  assert(c.value.origins
             .at(*c.value.blocks.block(first_block).origin_snapshot_sha256)
             .content == original.content);

  // Frozen provenance and applied revisions cannot be overwritten through a
  // valid new hash.
  auto tampered = c;
  auto snapshot = tampered.value.blocks.snapshot();
  auto &r = snapshot.blocks.at(first_block).revisions.front();
  r.text = "Forjado";
  r.exact_source.reset();
  auto layer = *r.layers.document;
  layer.content = r.text;
  layer.formatting.clear();
  layer.anchors.clear();
  layer.semantics = {};
  r.layers.document = std::make_shared<const project::Document>(layer);
  tampered.value.blocks =
      experimental::TextBlocksLab::restore(std::move(snapshot));
  rejects([&] { s.compositions().save(tampered); });
  auto deleted_origin = c;
  deleted_origin.value.origins.erase(deleted_origin.value.origins.begin());
  rejects([&] { s.compositions().save(deleted_origin); });
  auto oversized = c;
  oversized.value.drafts[first_block] = {
      c.value.blocks.block(first_block).current_revision,
      std::string(project::composition_payload_limit, 'x')};
  rejects([&] { s.compositions().save(oversized, true); });
  assert(s.compositions().load(id).sha256 == c.sha256);

  // Fork, alternatives, layer visibility and Unicode cuts survive roundtrip.
  const auto alt = c.value.blocks.alternative(first, "Outra possibilidade");
  const auto alt_p = c.value.blocks.occurrence(alt);
  c.value.profile.selected_variants[*alt_p.variant_group] = *alt_p.variant;
  const auto alt_block = alt_p.block_id;
  c.value.blocks.revise(alt_block,
                        c.value.blocks.block(alt_block).current_revision,
                        "Alternativa 🙂\n");
  const auto layer_id = c.value.blocks.add_layer("Apêndice");
  c.value.blocks.classify(cut, {}, layer_id);
  const auto split =
      c.value.blocks.split_selection(fixed, 5, 6, "Emoji separado");
  assert(c.value.blocks.revision(c.value.blocks.occurrence(split)).text ==
         "🙂");
  c = s.compositions().save(c);
  c = s.compositions().load(id);
  assert(c.value.blocks.project(c.value.profile).text.find("Alternativa 🙂") !=
         std::string::npos);
  auto stale = c;
  c.value.profile.visible_layers.insert(layer_id);
  c = s.compositions().save(c);
  stale.value.name = "Não deve sobrescrever";
  rejects([&] { s.compositions().save(stale); });

  // A scoped Project reversal can reuse a numeric revision; SHA prevents ABA
  // overwrite.
  const auto change = s.project_operations().front().id;
  const auto stale_after = c;
  auto impact = s.reversal_impact(change);
  assert(impact.allowed);
  s.confirm_project_operation(impact);
  c = s.compositions().load(id);
  c.value.name = "Estado diferente no mesmo contador";
  c = s.compositions().save(c);
  auto aba = stale_after;
  aba.value.name = "Sobrescrita ABA";
  rejects([&] { s.compositions().save(aba); });

  // Output uses only applied versions, requires acknowledgment and keeps
  // recipe.
  auto prepared = s.compositions().prepare(c, "Leitura independente");
  const auto expected_text = prepared.output.document.content;
  const auto count = s.writing().document_count();
  rejects([&] { s.compositions().materialize(prepared, false); });
  rejects([&] {
    s.compositions().export_reading(prepared, root / "Não exportar", false);
  });
  assert(!std::filesystem::exists(root / "Não exportar"));
  s.compositions().export_reading(prepared, root / "Exportação", true);
  assert(read(root / "Exportação/documento.txt") == expected_text);
  assert(read(root / "Exportação/relatorio.json").find(c.sha256) !=
         std::string::npos);
  rejects([&] {
    s.compositions().export_reading(prepared, root / "Exportação", true);
  });
  prepared.output.document.content = "Saída forjada pelo chamador";
  auto output = s.compositions().materialize(prepared, true);
  assert(output.content == expected_text && output.id != original.id &&
         s.writing().document_count() == count + 1);
  const auto output_op = s.project_operations().front().id;
  {
    persistence::SqliteDatabase db(dbpath);
    auto q = db.prepare("SELECT recipe,source_sha256 FROM composition_outputs "
                        "WHERE document_id=?");
    q.bind(1, output.id);
    assert(q.step());
    assert(persistence::decode_composition(q.column_text(0), q.column_text(1))
               .blocks.composition_id() == id);
  }
  // One atomic transaction: a recipe-write failure must not leave a new
  // Document.
  {
    persistence::SqliteDatabase db(dbpath);
    db.execute("CREATE TRIGGER fail_composition_output BEFORE INSERT ON "
               "composition_outputs BEGIN SELECT RAISE(ABORT,'QA output "
               "rollback');END");
  }
  rejects([&] { s.compositions().materialize(prepared, true); });
  assert(s.writing().document_count() == count + 1);
  {
    persistence::SqliteDatabase db(dbpath);
    db.execute("DROP TRIGGER fail_composition_output");
  }
  const auto reverse = s.reversal_impact(output_op);
  assert(reverse.allowed);
  s.confirm_project_operation(reverse);
  assert(!s.writing().document(output.id));
  {
    persistence::SqliteDatabase db(dbpath);
    assert(db.query_integer("SELECT count(*) FROM composition_outputs") == 0);
  }
  c.value.name = "Mudou depois da prévia";
  c = s.compositions().save(c);
  rejects([&] { s.compositions().materialize(prepared, true); });
  rejects([&] {
    s.compositions().export_reading(prepared, root / "Saída obsoleta", true);
  });
  assert(!std::filesystem::exists(root / "Saída obsoleta"));

  // Corrupt evidence is refused on read AND on attempted overwrite.
  {
    persistence::SqliteDatabase db(dbpath);
    auto q = db.prepare("UPDATE compositions SET sha256=? WHERE id=?");
    q.bind(1, std::string(64, '0'));
    q.bind(2, id);
    q.run();
  }
  rejects([&] { s.compositions().load(id); });
  rejects([&] { s.compositions().save(c); });
  {
    persistence::SqliteDatabase db(dbpath);
    auto q = db.prepare("UPDATE compositions SET sha256=? WHERE id=?");
    q.bind(1, c.sha256);
    q.bind(2, id);
    q.run();
  }
  c = s.compositions().load(id);
  for (int i = 0; i < 31; ++i)
    s.compositions().create("Página " + std::to_string(i));
  assert(s.compositions().list().size() == 30 &&
         s.compositions().list(30).size() == 2);
  const auto source_payload = persistence::encode_composition(c.value);
  const auto old_owner = c.project_id;
  s.save_as(root / "Cópia.inde");
  auto copy = s.compositions().load(id);
  assert(copy.project_id != old_owner &&
         persistence::encode_composition(copy.value) == source_payload);
  rejects([&] { s.compositions().save(c); });
  copy.value.name = "Operação da cópia";
  copy = s.compositions().save(copy);
  s.close();
  s.open(path);
  assert(s.compositions().load(id).sha256 == c.sha256);

  // Additive schema20 -> 21 migration is atomic, FKs remain enabled.
  const auto migration = root / "migration.sqlite3";
  {
    persistence::SqliteDatabase db(migration);
    persistence::SchemaMigrator{}.migrate(db);
    downgrade_schema21_for_fixture(db);
    db.execute(
        "CREATE TRIGGER fail_v21 BEFORE INSERT ON schema_migrations WHEN "
        "NEW.version=21 BEGIN SELECT RAISE(ABORT,'QA migration rollback');END");
    rejects([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 20);
    assert(
        db.query_integer(
            "SELECT count(*) FROM sqlite_master WHERE name='compositions'") ==
            0 &&
        db.query_integer("PRAGMA foreign_keys") == 1);
    db.execute("DROP TRIGGER fail_v21");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) == persistence::current_database_schema_version);
  }
  {
    persistence::SqliteDatabase db(dbpath);
    assert(db.query_text("PRAGMA integrity_check") == "ok" &&
           !db.prepare("PRAGMA foreign_key_check").step());
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS persistent Composition: complete frozen origins/layers, "
               "drafts/restart, immutable revisions, cuts, variants, copy, "
               "hash, conflict/ABA, atomic output and migration rollback\n";
}

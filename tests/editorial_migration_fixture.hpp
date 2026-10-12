#pragma once
#include "schema20_downgrade.hpp"
#include "../src/persistence/schema_sql_v19.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/editorial_review.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include <algorithm>
#include <cassert>
// Test-only inverse. Refuses new review records; constructs the actual v18
// CHECK/index contract while retaining every authorial row and payload.
inline void editorial_migration_test(const std::filesystem::path &root) {
  using namespace inde;
  using S = application::TextSemanticService;
  using K = project::SemanticFieldKind;
  const auto path = root / "migration.inde";
  application::ProjectService service;
  service.create(path, "QA migração editorial");
  auto types = service.narrative().entity_types();
  const auto c = std::find_if(types.begin(), types.end(), [](const auto &t) {
    return t.key == "character";
  });
  assert(c != types.end());
  const auto who = service.narrative().create_entity(c->id, "Focalizadora QA");
  const auto axis = service.planning().create_time_axis("Eixo QA");
  const auto p0 = service.planning().create_time_point(axis.id, 0, "Início"),
             p1 = service.planning().create_time_point(axis.id, 1, "Fim");
  auto d = service.writing().create_document("Documento schema18");
  d.content = "Água 🙂";
  d = S::create(d, project::builtin_annotation_types().front(), 0, 4, {});
  const auto local = d.semantics.annotations.back().id;
  d = S::create_document_perspective(d,
                                     {{"person", {K::Enum, {"third_person"}}},
                                      {"focalization", {K::Enum, {"internal"}}},
                                      {"viewpoint", {K::Entity, {who.id}}}});
  const auto global = d.semantics.annotations.back().id;
  d = S::bind_fictional_time(d, local, axis.id, p0.id, p1.id);
  d = S::bind_fictional_time(d, global, axis.id, p0.id);
  d = service.writing().update_document(d);
  auto source = service.writing().create_document("Relação entre Documentos");
  source.content = "Retoma";
  source =
      S::create(source, project::builtin_annotation_types().front(), 0, 4, {});
  source = S::relate(source, source.semantics.annotations.back().id, global,
                     "inde.semantic.callback");
  source = service.writing().update_document(source);
  service.close();
  {
    persistence::SqliteDatabase db(path / "data/project.sqlite3");
    std::vector<std::pair<std::string, std::int64_t>> rows;
    for (const auto *t :
         {"semantic_annotations", "semantic_entity_bindings",
          "semantic_annotation_relations", "semantic_fictional_time_bindings"})
      rows.emplace_back(
          t, db.query_integer(std::string("SELECT count(*) FROM ") + t));
    assert(rows[0].second == 3 && rows[1].second == 1 && rows[2].second == 1 &&
           rows[3].second == 2);
    const auto payload =
        db.query_text("SELECT group_concat(payload,'|') FROM (SELECT payload "
                      "FROM document_semantics ORDER BY document_id)");
    std::string inverse = persistence::schema_sql::schema_version_19;
    const auto replace = [&](const std::string &a, const std::string &b) {
      auto p = inverse.find(a);
      assert(p != std::string::npos);
      inverse.replace(p, a.size(), b);
    };
    replace(" AND type_key!='inde.editor.review-state'", "");
    replace(
        "type_key IN ('inde.semantic.perspective','inde.editor.review-state')",
        "type_key='inde.semantic.perspective'");
    replace(" AND type_key='inde.semantic.perspective' AND status!='archived'",
            " AND status!='archived'");
    {
      persistence::SqliteTransaction tx(db);
      downgrade_schema21_for_fixture(db);
      db.execute("DROP TABLE project_operations;DELETE FROM schema_migrations WHERE version=20");
      db.execute(inverse);
      db.execute("DELETE FROM schema_migrations WHERE version=19");
      tx.commit();
    }
    const auto check = [&] {
      for (const auto &[table, count] : rows)
        assert(db.query_integer("SELECT count(*) FROM " + table) == count);
      assert(db.query_text(
                 "SELECT group_concat(payload,'|') FROM (SELECT payload FROM "
                 "document_semantics ORDER BY document_id)") == payload);
      assert(db.query_text("PRAGMA integrity_check") == "ok" &&
             !db.prepare("PRAGMA foreign_key_check").step());
    };
    check();
    const auto schema = db.query_text(
        "SELECT sql FROM sqlite_master WHERE name='semantic_annotations'");
    // Fault AFTER the destructive rebuild: migration recording aborts. Its
    // outer transaction must restore all rows, original DDL and all triggers.
    db.execute("CREATE TRIGGER qa_refuse_schema19 BEFORE INSERT ON "
               "schema_migrations WHEN NEW.version=19 BEGIN SELECT "
               "RAISE(ABORT,'QA forced migration rollback');END");
    bool failed{};
    try {
      persistence::SchemaMigrator{}.migrate(db);
    } catch (const std::exception &) {
      failed = true;
    }
    assert(failed && persistence::SchemaMigrator{}.current_version(db) == 18);
    check();
    assert(db.query_text("SELECT sql FROM sqlite_master WHERE "
                         "name='semantic_annotations'") == schema);
    assert(db.query_integer("SELECT count(*) FROM sqlite_temp_master WHERE "
                            "name LIKE '%v19_backup'") == 0);
    db.execute("DROP TRIGGER qa_refuse_schema19");
    persistence::SchemaMigrator{}.migrate(db);
    check();
    assert(persistence::SchemaMigrator{}.current_version(db) == persistence::current_database_schema_version);
    assert(db.query_integer(
               "SELECT count(*) FROM sqlite_master WHERE type='trigger' AND "
               "name LIKE 'semantic_fictional_%'") == 3);
    persistence::SchemaMigrator{}.migrate(db);
    check(); // idempotence
  }
  service.open(path);
  assert(project::same_document_state(*service.writing().document(d.id), d));
  assert(project::same_document_state(*service.writing().document(source.id),
                                      source));
  auto reviewed =
      application::set_editorial_state(d, "in_review", "Editora", "");
  reviewed =
      application::set_editorial_state(reviewed, "approved", "Editora", "");
  reviewed = service.writing().update_document_if_unchanged(reviewed, d);
  assert(!application::editorial_state(reviewed).outdated);
  assert(S::perspective_at(reviewed, 0).effective_declaration ==
         global); // review history never replaces perspective
  auto moved = p0;
  moved.ordinal = 2;
  bool refused{};
  try {
    (void)service.planning().update_time_point(moved);
  } catch (const std::exception &) {
    refused = true;
  }
  assert(refused);
  service.close();
}

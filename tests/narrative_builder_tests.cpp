#include "inde/application/project_service.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <glib.h>
#include <iostream>
#include <limits>
#include <source_location>
using namespace inde;
template <class F>
void refuses(F f, std::source_location at = std::source_location::current()) {
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
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-builder-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  const auto path = s.create(root / "Plano.inde", "Construtor QA").path();
  const auto dbpath =
      persistence::ProjectDatabaseRepository::database_path(path);
  auto count = [&](const char *table) {
    persistence::SqliteDatabase db(dbpath);
    return db.query_integer("SELECT count(*) FROM " + std::string(table));
  };
  auto &b = s.builder();
  const auto ip = s.create_intellectual_property("Universo", "", "").id;
  const auto work =
      s.create_work(ip, "Obra", "", "", "pt-BR", "Planejamento").id;
  auto st = s.structures().create_narrative(work, "Revelação");
  auto line =
      s.structures().create_line(st.id, "Investigação", "Ordem própria");
  auto snapshot = [&] { return b.snapshot(st.id); };
  auto make = [&](const std::string &title,
                  const std::string &ln = std::string{}) {
    project::NarrativeUnit u;
    u.title = title;
    u.purpose = "Informação ao leitor";
    return b.create_unit(snapshot(), u, ln);
  };
  auto a = make("Resultado antes da causa", line.id),
       c = make("Causa revelada depois", line.id), other = make("Sem fila");
  const auto original = snapshot();
  assert(original.contexts.empty());
  project::NarrativeUnit invalid;
  invalid.title = "Não deve existir";
  refuses([&] { b.create_unit(snapshot(), invalid, project::new_uuid()); });
  assert(count("narrative_units") == 3);
  auto duplicate_structure =
      s.structures().create_narrative(work, "Outra estrutura");
  auto foreign = s.structures().create_unit(duplicate_structure.id, "", "Outro",
                                            "", "", "");
  refuses([&] { b.queue_unit(snapshot(), foreign.id, true); });
  auto forged = snapshot();
  forged.project_id = project::new_uuid();
  refuses([&] { b.queue_unit(forged, a.id, true); });
  b.queue_unit(snapshot(), c.id, true);
  b.queue_unit(snapshot(), a.id, true);
  const auto queued = snapshot();
  auto qpos = [&](const std::string &id) {
    for (auto &v : snapshot().contexts)
      if (v.unit_id == id)
        return v.writing_position;
    return std::optional<std::int64_t>{};
  };
  assert(qpos(c.id) == 1000 && qpos(a.id) == 2000 && !qpos(other.id));
  // A Local context without a queue must not prevent queued units moving.
  auto local = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000002", "Porto");
  auto person = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000001", "Lia");
  b.set_location(snapshot(), other.id, local.id);
  refuses([&] { b.set_location(snapshot(), a.id, person.id); });
  {
    persistence::SqliteDatabase db(dbpath);
    auto change = db.prepare("UPDATE entities SET entity_type_id=? WHERE id=?");
    change.bind(1, person.entity_type_id);
    change.bind(2, local.id);
    refuses([&] { change.run(); });
  }
  const auto memberships = s.structures().unit_lines(st.id);
  auto calendar = s.worldbuilding().create_calendar(
      project::calendar_template("gregorian"));
  auto axis = s.planning().create_time_axis("Tempo dos acontecimentos");
  project::TemporalReference time;
  time.calendar_id = calendar.id;
  time.axis_id = axis.id;
  time.year = 2050;
  time.month = 1;
  time.day = 1;
  time.label = "Depois";
  time = s.worldbuilding().save_temporal_reference(time);
  project::TemporalAttachment attachment;
  attachment.reference_id = time.id;
  attachment.kind = "unit";
  attachment.target_id = a.id;
  attachment.role = "Acontecimento";
  attachment = s.worldbuilding().attach_date(
      attachment, time, s.worldbuilding().temporal_target("unit", a.id));
  const auto node =
      s.create_structural_node(work, {},
                               project::builtin_structural_type_id(
                                   project::StructuralNodeType::Chapter),
                               "1", "Apresentação pública")
          .id;
  auto doc = s.writing().create_document("Texto independente", node);
  doc.content = "Texto autoral — Água 🙂";
  doc = s.writing().update_document(doc);
  b.attach_document(snapshot(), a.id, doc.id);
  b.attach_document(snapshot(), c.id, doc.id); // many-to-many is intentional
  refuses([&] { b.attach_document(snapshot(), a.id, doc.id); });
  const auto chronology = s.worldbuilding().temporal_reference(time.id);
  const auto before_queue_a = qpos(a.id), before_queue_c = qpos(c.id);
  b.move_unit(snapshot(), a.id, application::BuilderOrder::Narrative, 1);
  assert(snapshot().units[0].id == c.id && snapshot().units[1].id == a.id);
  assert(qpos(a.id) == before_queue_a && qpos(c.id) == before_queue_c);
  const auto mounted = snapshot().units;
  auto altered = snapshot();
  altered.units.clear();
  altered.contexts.clear(); // supplied rows are never write authority
  b.move_unit(altered, a.id, application::BuilderOrder::WritingQueue, -1);
  assert(qpos(a.id) == 1000 && qpos(c.id) == 2000);
  assert(snapshot().units[0].id == mounted[0].id &&
         snapshot().units[1].id == mounted[1].id);
  for (std::size_t i = 0; i < memberships.size(); ++i)
    assert(s.structures().unit_lines(st.id)[i].position ==
           memberships[i].position);
  assert(s.worldbuilding().temporal_reference(time.id) == chronology);
  assert(s.writing().document(doc.id)->editorial_node_id == node &&
         s.writing().document(doc.id)->content == doc.content);
  refuses([&] {
    b.move_unit(snapshot(), other.id, application::BuilderOrder::WritingQueue,
                1);
  });
  refuses([&] {
    b.move_unit(snapshot(), a.id, static_cast<application::BuilderOrder>(99),
                1);
  });
  refuses([&] {
    b.move_unit(queued, a.id, application::BuilderOrder::Narrative, -1);
  });
  b.queue_unit(snapshot(), a.id, false);
  assert(!qpos(a.id));
  auto edit = snapshot().units.front();
  edit.title = "Nova intenção";
  auto stale = snapshot();
  b.update_unit(stale, edit);
  refuses([&] { b.update_unit(stale, edit); });
  auto bad = snapshot().units.front();
  bad.position += 1;
  refuses([&] { b.update_unit(snapshot(), bad); });
  line.description = "Pergunta em aberto";
  b.update_line(snapshot(), line);
  assert(snapshot().lines.front().description == line.description);
  auto relation_type = s.narrative().create_relation_type(
      "builder-focus", "Conhece", "Conhecido por",
      project::RelationDirectionality::Directed);
  auto second = s.narrative().create_entity(person.entity_type_id, "Ivo");
  auto relation = s.narrative().create_relation(
      relation_type.id, person.id, second.id, "Hipótese declarada");
  b.attach_relation(snapshot(), a.id, relation.id);
  assert(s.narrative().relation(relation.id)->description ==
         relation.description);
  refuses([&] { b.attach_relation(snapshot(), a.id, relation.id); });
  // Usages must lead to the unit, not force a manual project search.
  for (auto [kind, id, table] :
       std::vector<std::tuple<project::RemovalKind, std::string, std::string>>{
           {project::RemovalKind::Document, doc.id, "narrative_realizations"},
           {project::RemovalKind::Entity, local.id, "narrative_unit_contexts"},
           {project::RemovalKind::Relation, relation.id,
            "narrative_unit_relations"}}) {
    auto p = s.removal_impact(kind, id);
    assert(!p.allowed);
    auto at = std::find_if(p.uses.begin(), p.uses.end(),
                           [&](auto &u) { return u.table == table; });
    assert(at != p.uses.end() && at->location_table == "narrative_units" &&
           !at->guidance.empty() && at->structure_id == st.id);
  }
  const auto docs_before = count("documents");
  refuses([&] { b.create_document(snapshot(), other.id, ""); });
  assert(count("documents") == docs_before);
  {
    persistence::SqliteDatabase db(dbpath);
    db.execute(
        "CREATE TRIGGER reject_realization BEFORE INSERT ON "
        "narrative_realizations BEGIN SELECT RAISE(ABORT,'QA failure');END");
    refuses(
        [&] { b.create_document(snapshot(), other.id, "Documento atômico"); });
    assert(count("documents") == docs_before);
    db.execute("DROP TRIGGER reject_realization");
  }
  const auto created =
      b.create_document(snapshot(), other.id, "Documento atômico");
  assert(!s.writing().document(created)->editorial_node_id &&
         s.writing().document(created)->content.empty());
  auto reverse = s.reversal_impact(s.project_operations().front().id);
  assert(reverse.allowed);
  s.confirm_project_operation(reverse);
  assert(!s.writing().document(created));
  for (auto &r : snapshot().realizations)
    if (r.unit_id == a.id)
      b.detach(snapshot(), "document", r.id);
  assert(s.writing().document(doc.id)->content == doc.content);
  auto focus = snapshot().relations.front();
  b.detach(snapshot(), "relation", focus.id);
  assert(s.narrative().relation(relation.id));
  // The compatibility API now routes through complete transactional derivation.
  const auto structure_count = count("narrative_structures");
  const auto copy =
      s.structures().duplicate_narrative(st.id, "Alternativa completa", true);
  assert(count("narrative_structures") == structure_count + 1);
  assert(b.snapshot(copy.id).dates.size() == snapshot().dates.size());
  assert(b.compare(st.id, copy.id).differences.empty());
  const auto large =
      s.structures().create_narrative(work, "Limites explícitos");
  const auto project_id = snapshot().project_id;
  {
    persistence::SqliteDatabase db(dbpath);
    persistence::SqliteTransaction tx(db);
    auto insert =
        db.prepare("INSERT INTO "
                   "narrative_units(id,project_id,structure_id,title,position,"
                   "created_at,updated_at) VALUES(?,?,?,?,?,'QA','QA')");
    for (int i = 0; i < 2000; ++i) {
      insert.bind(1, project::new_uuid());
      insert.bind(2, project_id);
      insert.bind(3, large.id);
      insert.bind(4, "Unidade " + std::to_string(i));
      insert.bind(5, static_cast<std::int64_t>(i));
      insert.run();
      insert.reset();
    }
    tx.commit();
  }
  const auto started = std::chrono::steady_clock::now();
  const auto large_view = b.snapshot(large.id);
  assert(large_view.units.size() == 2000);
  std::cout << "MEASURE snapshot 2000 units milliseconds="
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - started)
                   .count()
            << '\n';
  project::NarrativeUnit extra;
  extra.title = "Acima do limite";
  refuses([&] { b.create_unit(large_view, extra); });
  {
    persistence::SqliteDatabase db(dbpath);
    auto insert = db.prepare(
        "INSERT INTO "
        "narrative_units(id,project_id,structure_id,title,position,created_at,"
        "updated_at) VALUES(?,?,?,'Legacy oversized',3000,'QA','QA')");
    insert.bind(1, project::new_uuid());
    insert.bind(2, snapshot().project_id);
    insert.bind(3, large.id);
    insert.run();
    refuses([&] {
      b.snapshot(large.id);
    }); // refuse, never claim a truncated view is complete
  }
  const auto old_project = snapshot().project_id;
  s.save_as(root / "Cópia.inde");
  assert(snapshot().project_id != old_project);
  assert(snapshot().realizations.size() == 1 &&
         snapshot().contexts.size() == 3);
  refuses([&] { b.queue_unit(original, c.id, true); });
  s.close();
  s.open(root / "Cópia.inde");
  assert(snapshot().realizations.front().document_id == doc.id);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(root /
                                                              "Cópia.inde"));
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    auto fk = db.prepare("PRAGMA foreign_key_check");
    assert(!fk.step());
  }
  // Simulate the accepted schema 25 and inject a marker failure: DDL rolls
  // back.
  s.close();
  s.create(root / "Migration.inde", "Migração");
  s.close();
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(
            root / "Migration.inde"));
    db.execute("DROP TABLE narrative_text_targets;DROP TABLE narrative_text_decisions;DROP TABLE narrative_continuity_exceptions;DROP TABLE narrative_unit_presence_intents;DROP INDEX compositions_identity_owner;DELETE FROM schema_migrations WHERE version=28");
    db.execute(
        "DROP TABLE narrative_dependency_scopes;DROP TABLE "
        "narrative_branch_provenance;DROP INDEX narrative_link_owner;DROP "
        "TRIGGER narrative_context_local_type;DROP TABLE "
        "narrative_unit_relations;DROP TABLE narrative_realizations;DROP TABLE "
        "narrative_unit_contexts;DELETE FROM schema_migrations WHERE "
        "version>=26;CREATE TRIGGER fail_v26 BEFORE INSERT ON "
        "schema_migrations "
        "WHEN NEW.version=26 BEGIN SELECT RAISE(ABORT,'QA rollback');END");
    refuses([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 25 &&
           db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='narrative_realizations'") == 0);
    db.execute("DROP TRIGGER fail_v26");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) ==
           persistence::current_database_schema_version);
  }
  std::filesystem::remove_all(root);
  std::cout
      << "PASS builder: independent orders, explicit queue, CAS, ownership, "
         "atomic creation, detach preservation, usage guide, undo, Save As, "
         "restart, derivation gate and migration rollback\n";
}

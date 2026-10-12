#include "inde/application/project_service.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "schema21_downgrade.hpp"
#include <algorithm>
#include <cassert>
#include <glib.h>
#include <iostream>
#include <source_location>
using namespace inde;
template <class F>
void refuses(F f, std::source_location at = std::source_location::current()) {
  bool caught = false;
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
  auto root = std::filesystem::temp_directory_path() /
              ("inde-alternatives-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  s.create(root / "Alternativas.inde", "Alternativas QA");
  auto &b = s.builder();
  const auto work =
      s.create_work(s.create_intellectual_property("Mundo", "", "").id, "Obra",
                    "", "", "pt-BR", "Planejamento")
          .id;
  auto st = s.structures().create_narrative(work, "Montagem original",
                                            "Sem fórmula obrigatória");
  auto line = s.structures().create_line(st.id, "Investigação",
                                         "Pergunta: quem sabia?"),
       other = s.structures().create_line(st.id, "Viagem", "Outra ordem");
  auto a = s.structures().create_unit(st.id, "", "Consequência primeiro", "",
                                      "Revelar antes", "Lia"),
       c = s.structures().create_unit(st.id, "", "Causa depois", "", "", ""),
       last = s.structures().create_unit(st.id, "", "Sem fila", "", "", "");
  for (auto u : {a, c, last})
    s.structures().add_unit_to_line(u.id, line.id);
  for (auto u : {c, a})
    s.structures().add_unit_to_line(u.id, other.id);
  auto snapshot = [&] { return b.snapshot(st.id); };
  auto local = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000002", "Porto");
  auto person = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000001", "Lia");
  s.structures().add_entity_to_unit(a.id, person.id, "menção, não presença");
  b.set_location(snapshot(), a.id, local.id);
  b.queue_unit(snapshot(), c.id, true);
  b.queue_unit(snapshot(), a.id, true);
  auto roles = s.structures().roles();
  static_cast<void>(s.structures().assign_role(roles.front().id, person.id, work, st.id, a.id,
                             "Papel da unidade"));
  static_cast<void>(s.structures().assign_role(roles.back().id, person.id, work, st.id, {},
                             "Papel da estrutura"));
  static_cast<void>(s.structures().assign_role(roles.front().id, person.id, work, {}, {},
                             "Papel global: não duplicar"));
  auto link = s.structures().create_link(st.id, a.id, c.id,
                                         project::NarrativeLinkKind::Precedes,
                                         "Revelação, não data");
  b.set_dependency_scope(snapshot(), link.id, "narrative");
  const auto relation_type=s.narrative().create_relation_type("qa-visit","Visita QA","Visitado por",project::RelationDirectionality::Directed);
  const auto relation =
      s.narrative().create_relation(relation_type.id,
                                    person.id, local.id, "Conexão canônica");
  b.attach_relation(snapshot(), a.id, relation.id);
  auto doc = s.writing().create_document("Texto compartilhado");
  doc.content = "Prosa — Água 🙂";
  doc = s.writing().update_document(doc);
  b.attach_document(snapshot(), a.id, doc.id);
  b.attach_document(snapshot(), c.id, doc.id);
  const auto calendar = s.worldbuilding().create_calendar(
      project::calendar_template("gregorian"));
  auto axis = s.planning().create_time_axis("Fatos");
  project::TemporalReference ref;
  ref.calendar_id = calendar.id;
  ref.axis_id = axis.id;
  ref.year = 2050;
  ref.month = 1;
  ref.day = 1;
  ref.label = "Data independente";
  ref = s.worldbuilding().save_temporal_reference(ref);
  for (auto [kind, id] :
       {std::pair{"unit", a.id}, std::pair{"line", line.id}}) {
    project::TemporalAttachment date;
    date.reference_id = ref.id;
    date.kind = kind;
    date.target_id = id;
    date.role = "contexto";
    date.note = "Não converter ordem em data";
    s.worldbuilding().attach_date(date, ref,
                                  s.worldbuilding().temporal_target(kind, id));
  }
  s.structures().activate_narrative(st.id);
  auto dbpath = persistence::ProjectDatabaseRepository::database_path(
      s.current()->path());
  auto count = [&](const std::string &table) {
    persistence::SqliteDatabase db(dbpath);
    return db.query_integer("SELECT count(*) FROM " + table);
  };
  auto before = snapshot();
  const auto doc_count = count("documents"),
             role_count = count("narrative_role_assignments"),
             structure_count = count("narrative_structures");
  // Inject failure in the last step: no structure or partial associations
  // survive.
  {
    persistence::SqliteDatabase db(dbpath);
    db.execute("CREATE TRIGGER reject_branch BEFORE INSERT ON "
               "narrative_branch_provenance BEGIN SELECT RAISE(ABORT,'QA "
               "rollback');END");
    refuses([&] { b.derive(before, "Falha atômica"); });
    db.execute("DROP TRIGGER reject_branch");
  }
  assert(count("narrative_structures") == structure_count &&
         count("narrative_role_assignments") == role_count);
  auto stale = before;
  b.set_location(snapshot(), last.id, local.id);
  refuses([&] { b.derive(stale, "Obsoleta"); });
  auto forged = snapshot();
  forged.units.clear();
  forged.dates.clear();
  forged.roles.clear(); // rows cannot authorize lossy cloning
  auto copy = b.derive(forged, "Alternativa completa");
  auto target = b.snapshot(copy.id);
  assert(!copy.is_active && copy.derived_from_id == st.id &&
         s.structures().active_narrative(work)->id == st.id);
  assert(snapshot().units.front().id == a.id &&
         target.units.front().id != a.id && target.units.size() == 3);
  assert(target.lines.size() == 2 && target.memberships.size() == 5 &&
         target.entities.size() == 1 && target.roles.size() == 2 &&
         target.dates.size() == 2 && target.realizations.size() == 2 &&
         target.relations.size() == 1 && target.scopes.size() == 1);
  assert(count("documents") == doc_count &&
         count("narrative_role_assignments") == role_count + 2);
  assert(target.realizations.front().document_id == doc.id &&
         target.dates.front().reference_id == ref.id &&
         s.writing().document(doc.id)->content == doc.content);
  assert(b.compare(st.id, copy.id).differences.empty() &&
         b.compare_to_origin(copy.id).differences.empty());
  auto reverse = s.reversal_impact(s.project_operations().front().id);
  assert(reverse.allowed);
  s.confirm_project_operation(reverse);
  refuses([&] { b.snapshot(copy.id); });
  assert(count("narrative_structures") == structure_count &&
         s.writing().document(doc.id));
  copy = b.derive(snapshot(), "Alternativa completa");
  target = b.snapshot(copy.id);
  const auto copied_a = target.units.front().id, copied_c = target.units[1].id;
  auto membership_before = snapshot().memberships;
  const auto unit_positions = snapshot().units;
  const auto queue = snapshot().contexts;
  b.move_in_line(snapshot(), a.id, line.id, 1);
  assert(snapshot().units.front().id == unit_positions.front().id &&
         snapshot().units.front().position == unit_positions.front().position);
  for (auto &old : membership_before)
    if (old.line_id == other.id) {
      auto all = snapshot().memberships;
      auto it = std::find_if(all.begin(), all.end(), [&](auto &v) {
        return v.unit_id == old.unit_id && v.line_id == old.line_id;
      });
      assert(it != all.end() && it->position == old.position);
    }
  for (auto &q : queue) {
    auto all = snapshot().contexts;
    auto it = std::find_if(all.begin(), all.end(),
                           [&](auto &v) { return v.unit_id == q.unit_id; });
    assert(it != all.end() && it->writing_position == q.writing_position);
  }
  refuses([&] { b.move_in_line(snapshot(), a.id, project::new_uuid(), 1); });
  auto impact = b.preview_move(b.snapshot(copy.id), copied_a,
                               application::BuilderOrder::Narrative, 1);
  assert(
      impact.documents == 1 && impact.dates == 1 &&
      std::any_of(impact.warnings.begin(), impact.warnings.end(), [](auto &w) {
        return w.find("contraria") != std::string::npos;
      }));
  const auto observed = b.snapshot(copy.id);
  b.move_unit(observed, copied_a, application::BuilderOrder::Narrative, 1);
  refuses([&] {
    b.move_unit(observed, copied_a, application::BuilderOrder::Narrative, -1);
  });
  auto delta = b.compare_to_origin(copy.id);
  assert(std::count_if(
             delta.differences.begin(), delta.differences.end(),
             [](auto &d) { return d.dimension == "Montagem narrativa"; }) == 2);
  assert(s.worldbuilding().temporal_reference(ref.id) == std::optional{ref} &&
         s.writing().document(doc.id)->content == doc.content);
  auto unit = b.snapshot(copy.id).units.front();
  unit.title = "Outra intenção, mesma identidade";
  b.update_unit(b.snapshot(copy.id), unit);
  delta = b.compare_to_origin(copy.id);
  assert(std::any_of(delta.differences.begin(), delta.differences.end(),
                     [](auto &d) { return d.dimension == "Título"; }));
  auto grand = b.derive(b.snapshot(copy.id), "Terceira geração");
  assert(b.compare(copy.id, grand.id).differences.empty());
  auto unrelated = s.structures().create_narrative(work, "Sem parentesco");
  static_cast<void>(s.structures().create_unit(unrelated.id, "", unit.title, "", "", ""));
  auto no_match = b.compare(unrelated.id, grand.id);
  assert(std::any_of(no_match.differences.begin(), no_match.differences.end(),
                     [](auto &d) { return d.dimension == "Adicionado"; }));
  auto another_work =
      s.create_work(s.create_intellectual_property("Outro", "", "").id,
                    "Outra Obra", "", "", "pt-BR", "Planejamento")
          .id;
  refuses([&] {
    b.compare(st.id,
              s.structures().create_narrative(another_work, "Alheia").id);
  });
  refuses([&] { b.set_dependency_scope(snapshot(), link.id, "invented"); });
  refuses(
      [&] { b.set_dependency_scope(b.snapshot(copy.id), link.id, "writing"); });
  b.set_dependency_scope(snapshot(), link.id, "chronology");
  auto independent =
      b.preview_move(snapshot(), a.id, application::BuilderOrder::Narrative, 1);
  assert(std::none_of(
      independent.warnings.begin(), independent.warnings.end(),
      [](auto &w) { return w.find("contraria") != std::string::npos; }));
  const auto pid = snapshot().project_id;
  s.save_as(root / "Cópia.inde");
  dbpath = persistence::ProjectDatabaseRepository::database_path(
      s.current()->path());
  assert(b.snapshot(copy.id).project_id != pid);
  s.close();
  s.open(root / "Cópia.inde");
  assert(!b.compare_to_origin(copy.id).differences.empty() &&
         b.compare(copy.id, grand.id).differences.empty());
  {
    persistence::SqliteDatabase db(dbpath);
    assert(db.query_text("PRAGMA integrity_check") == "ok" &&
           !db.prepare("PRAGMA foreign_key_check").step());
    assert(db.query_text(
               "SELECT DISTINCT project_id FROM narrative_branch_provenance") ==
           s.current()->manifest().project_id);
    assert(db.query_text(
               "SELECT DISTINCT project_id FROM narrative_dependency_scopes") ==
           s.current()->manifest().project_id);
  }
  s.close();
  s.create(root / "Migração.inde", "Migração");
  s.close();
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(root /
                                                              "Migração.inde"));
    downgrade_schema27_for_fixture(db);
    db.execute(
        "CREATE TRIGGER fail27 BEFORE "
        "INSERT ON schema_migrations WHEN NEW.version=27 BEGIN SELECT "
        "RAISE(ABORT,'QA migration');END");
    refuses([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 26 &&
           db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='narrative_branch_provenance'") == 0);
    db.execute("DROP TRIGGER fail27");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) == persistence::current_database_schema_version);
  }
  std::filesystem::remove_all(root);
  std::cout
      << "PASS alternatives: complete atomic transfer, lineage, recorded/live "
         "comparison, independent line order, advisory typed impact, stale "
         "refusal, sharing, undo, Save As, restart and schema27 rollback\n";
}

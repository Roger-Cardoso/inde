#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/calendar_navigation_store.hpp"
#include "inde/persistence/document_recovery_store.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include <algorithm>
#include <cassert>
#include <glib.h>
#include <iostream>
using namespace inde;
template <class F> void refuses(F f) {
  bool fail = false;
  try {
    f();
  } catch (const std::exception &) {
    fail = true;
  }
  assert(fail);
}
int main() {
  auto root = std::filesystem::temp_directory_path() /
              ("inde-temporal-workflow-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  s.create(root / "Workflow.inde", "Fluxo temporal");
  auto &w = s.worldbuilding();
  auto c = w.create_calendar(project::calendar_template("gregorian"));
  auto axis = s.planning().create_time_axis("História");
  auto binding = *w.assign_calendar(axis.id, c.id, 0, {});
  auto d = s.writing().create_document("Texto com tempo");
  d.content = "Lia escreve. 🙂";
  using S = application::TextSemanticService;
  auto type = *project::find_annotation_type(
      project::builtin_annotation_types(), "inde.semantic.temporal-context", 1);
  d = S::create(d, type, 0, 3, {});
  d = s.writing().update_document(d);
  auto annotation = d.semantics.annotations.front().id;
  project::TemporalReference r;
  r.id = project::new_uuid();
  r.token = project::new_uuid();
  r.axis_id = axis.id;
  r.calendar_id = c.id;
  r.label = "Cena criada no texto";
  r.year = 2000;
  r.month = 1;
  r.day = 10;
  r.clock = {12, 30};
  r.qualifier = "between";
  r.upper = project::TemporalBoundary{2000, 1, 18, {}};
  project::TextDateCreation plan{r, c};
  auto next = S::bind_date(d, annotation, r.id, "scene", "Hora autoral");
  const auto original = *s.writing().document(d.id);
  auto database = persistence::ProjectDatabaseRepository::database_path(
      s.current()->path());
  auto rows = [&](const char *table) {
    persistence::SqliteDatabase db(database);
    auto q = db.prepare("SELECT count(*) FROM " + std::string(table));
    q.step();
    return q.column_integer(0);
  };
  auto refs = rows("temporal_references"),
       operations = rows("project_operations");
  // A draft/cancel performs no writes.
  assert(rows("temporal_references") == refs);
  auto stale =
      application::replace_selection(
          {d, {0, 0}}, project::utf8_character_count(d.content),
          project::utf8_character_count(d.content), " alteração externa")
          .document;
  stale = s.writing().update_document(stale);
  refuses([&] { (void)s.writing().create_date_from_text(next, d, plan); });
  assert(rows("temporal_references") == refs);
  d = s.writing().update_document(original);
  next = S::bind_date(d, annotation, r.id, "scene");
  auto bad = plan;
  bad.calendar.name += " snapshot antigo";
  refuses([&] { (void)s.writing().create_date_from_text(next, d, bad); });
  bad = plan;
  bad.reference.axis_id = project::new_uuid();
  refuses([&] { (void)s.writing().create_date_from_text(next, d, bad); });
  refuses([&] {
    static_cast<void>(s.writing().create_date_from_text(d, d, plan));
  }); // cannot create orphan
  assert(rows("temporal_references") == refs &&
         rows("project_operations") == operations);
  {
    persistence::SqliteDatabase db(database);
    db.execute("CREATE TRIGGER workflow_failure BEFORE INSERT ON "
               "semantic_date_bindings BEGIN SELECT RAISE(ABORT,'injected "
               "binding failure'); END");
    refuses([&] { (void)s.writing().create_date_from_text(next, d, plan); });
    assert(rows("temporal_references") == refs &&
           rows("project_operations") == operations);
    assert(project::same_document_state(*s.writing().document(d.id), d));
    db.execute("DROP TRIGGER workflow_failure");
  }
  application::DocumentEditSession edit;
  edit.open({d, {3, 0}});
  auto persisted = s.writing().create_date_from_text(next, d, plan);
  assert(w.temporal_reference(r.id) == r && w.text_date_uses(r.id).size() == 1);
  assert(rows("project_operations") == operations + 1);
  assert(persisted.content == d.content);
  edit.commit({persisted, {3, 0}}, "Criar data e vincular");
  edit.acknowledge_saved(persisted);
  auto undo = *edit.undo();
  assert(undo.selection.cursor == 3 && undo.selection.bound == 0);
  assert(undo.document.semantics.annotations.front().dates.empty());
  auto saved_undo =
      s.writing().update_document_if_unchanged(undo.document, persisted);
  assert(w.temporal_reference(r.id) == r && w.text_date_uses(r.id).empty());
  auto redo = *edit.redo();
  assert(redo.document.semantics.annotations.front().dates.size() == 1);
  persisted =
      s.writing().update_document_if_unchanged(redo.document, saved_undo);
  refuses([&] {
    static_cast<void>(s.writing().create_date_from_text(next, d, plan));
  }); // stale/double confirmation
  application::DocumentSaveCoordinator worker;
  auto typed =
      application::replace_selection({persisted, {3, 3}}, 3, 3, " voltou")
          .document;
  worker.submit(s.writing().save_snapshot(typed, persisted, 1, 2));
  worker.drain();
  auto result = worker.take_latest();
  assert(result && result->error.empty() && result->persisted);
  persisted = *result->persisted;
  assert(w.text_date_uses(r.id).size() == 1);
  worker.clear();
  // Same document/date survives the existing recovery codec.
  project::RecoveryRecord record;
  record.session_id = project::new_uuid();
  record.project_id = s.current()->manifest().project_id;
  record.revision = 3;
  record.durable_revision = 2;
  record.state = {
      application::replace_selection({persisted, {3, 0}}, 0, 0, "Ontem, ")
          .document,
      3, 0};
  record.baseline = persisted;
  persistence::DocumentRecoveryStore recovery;
  recovery.checkpoint(*s.current(), record);
  auto recovered = recovery.load(*s.current(), record.session_id);
  assert(recovered &&
         recovered->state.document.semantics.annotations.front().dates.size() ==
             1 &&
         recovered->state.cursor == 3 && recovered->state.bound == 0);
  auto person_type =
      s.narrative().create_entity_type("workflow-person", "Pessoa");
  auto person = s.narrative().create_entity(person_type.id, "Lia");
  auto dest = w.temporal_target("entity", person.id);
  project::TemporalAttachment a;
  a.kind = "entity";
  a.target_id = person.id;
  a.role = "início";
  a.meaning = "start";
  auto fresh = r;
  fresh.id.clear();
  fresh.token.clear();
  fresh.label = "Data contextual";
  auto stale_dest = dest;
  stale_dest.token = "stale";
  refuses([&] { w.create_attached_date(fresh, c, a, stale_dest); });
  assert(rows("temporal_references") == refs + 1);
  a.role.clear();
  refuses([&] { w.create_attached_date(fresh, c, a, dest); });
  assert(rows("temporal_references") == refs + 1);
  a.role = "início";
  auto link = w.create_attached_date(fresh, c, a, dest);
  assert(w.target_dates(dest).size() == 1 &&
         w.temporal_attachments(link.reference_id).size() == 1);
  auto earlier = r;
  earlier.id.clear();
  earlier.token.clear();
  earlier.qualifier = "exact";
  earlier.upper.reset();
  earlier.clock.clear();
  earlier.day = 2;
  earlier.label = "Z — data mais antiga";
  earlier = w.save_temporal_reference(earlier);
  auto unknown = earlier;
  unknown.id.clear();
  unknown.qualifier = "approximate";
  unknown.uncertainty = "Sem limites presumidos";
  unknown = w.save_temporal_reference(unknown);
  auto julian = w.create_calendar(project::calendar_template("julian"));
  auto cut = project::calendar_ordinal(c, {2000, 1, 15});
  auto jdate = project::terrestrial_date(
      julian, project::terrestrial_day(c, {2000, 1, 15}));
  w.adopt_calendar({{},
                    axis.id,
                    julian.id,
                    cut,
                    cut - project::calendar_ordinal(julian, jdate),
                    "coexist"},
                   binding, {});
  auto custom = project::calendar_template("gregorian");
  custom.engine = "custom";
  custom.name = "Novo próprio";
  custom = w.create_calendar(custom);
  auto retire = project::calendar_ordinal(c, {2000, 1, 20});
  w.adopt_calendar({{}, axis.id, custom.id, retire, 0, "retire"}, binding,
                   w.adoptions(axis.id));
  auto monthly = w.month_view(binding, 2000, 1);
  assert(monthly.cuts.size() == 2 && monthly.cuts.contains(15) &&
         monthly.cuts.contains(20));
  assert(monthly.regimes.at(14) == "active" &&
         monthly.regimes.at(15) == "parallel" &&
         monthly.regimes.at(20) == "outside");
  assert(monthly.unpositioned.size() == 1);
  auto hit = std::find_if(monthly.references.begin(), monthly.references.end(),
                          [&](auto &v) { return v.reference.id == r.id; });
  assert(hit != monthly.references.end() &&
         hit->days == std::vector<int>({10, 11, 12, 13, 14, 15, 16, 17, 18}));
  auto parallel = w.month_view(binding, 2000, 1, "parallel");
  assert(parallel.references.size() ==
         2); // both interval references, not day 2
  for (auto &v : parallel.references)
    for (auto day : v.days)
      assert(day >= 15 && day < 20);
  auto outside = w.month_view(binding, 2000, 1, "outside");
  assert(outside.references.empty());
  refuses([&] { w.month_view(binding, 2000, 1, "invalid"); });
  auto sorted = w.temporal_references(axis.id);
  assert(sorted.front().id == earlier.id && sorted.back().id == unknown.id);
  // Different epochs of one definition cannot choose a winning alignment.
  w.adopt_calendar({{},
                    axis.id,
                    c.id,
                    project::calendar_ordinal(c, {2000, 1, 25}),
                    1,
                    "coexist"},
                   binding, w.adoptions(axis.id));
  monthly = w.month_view(binding, 2000, 1);
  assert(monthly.unpositioned.size() == 4);
  persistence::CalendarNavigationStore cache(root / "navigation");
  assert(!cache.load(c));
  cache.remember(c, {2000, 2, 1});
  assert(cache.load(c) == project::CalendarDate({2000, 2, 1}));
  cache.remember(c, {2000, 99, 1});
  assert(cache.load(c)->month == 2);
  auto invalid_cal = c;
  invalid_cal.id = "../escape";
  cache.remember(invalid_cal, {2000, 1, 1});
  assert(!cache.load(invalid_cal));
  persistence::CalendarNavigationStore unavailable(root / "navigation" / c.id /
                                                   "not-a-directory");
  unavailable.remember(c, {2000, 3, 1});
  assert(!unavailable.load(c));
  s.close();
  s.open(root / "Workflow.inde");
  assert(s.writing().document(persisted.id)->content == persisted.content &&
         w.temporal_reference(r.id) == r);
  persistence::SqliteDatabase db(database);
  auto integrity = db.prepare("PRAGMA integrity_check");
  assert(integrity.step() && integrity.column_text(0) == "ok");
  auto fk = db.prepare("PRAGMA foreign_key_check");
  assert(!fk.step());
  std::cout
      << "PASS temporal workflow atomic creation, rollback, undo/redo, "
         "autosave, monthly intervals/regimes, ordering, navigation, restart\n";
}

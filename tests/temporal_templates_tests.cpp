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
void refuses(F f, std::source_location l = std::source_location::current()) {
  try {
    f();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("Expected refusal at " + std::to_string(l.line()));
}
int main() {
  auto g = project::calendar_template("gregorian");
  auto ch = project::calendar_template("chinese"),
       h = project::calendar_template("hebrew"),
       i = project::calendar_template("islamic-civil"),
       e = project::calendar_template("ethiopic"),
       a = project::calendar_template("egyptian-civil"),
       m = project::calendar_template("maya-haab");
  assert(project::calendar_months(ch, 2023).size() == 13);
  assert(project::calendar_months(ch, 2023)[2].name ==
         "Mês lunar 2 (intercalar)");
  assert(project::terrestrial_day(ch, {2023, 1, 1}) ==
         project::terrestrial_day(g, {2023, 1, 22}));
  assert(project::terrestrial_day(ch, {2023, 3, 1}) ==
         project::terrestrial_day(g, {2023, 3, 22}));
  assert(project::calendar_months(h, 5783).size() == 12);
  assert(project::calendar_months(h, 5784).size() == 13);
  assert(project::calendar_months(h, 5784)[5].name == "Adar I" &&
         project::calendar_months(h, 5784)[6].name == "Adar II");
  assert(project::terrestrial_day(h, {5784, 1, 1}) ==
         project::terrestrial_day(g, {2023, 9, 16}));
  assert(project::terrestrial_day(h, {5784, 8, 15}) ==
         project::terrestrial_day(g, {2024, 4, 23}));
  assert(project::terrestrial_day(i, {1445, 1, 1}) ==
         project::terrestrial_day(g, {2023, 7, 19}));
  assert(project::days_in_month(e, 2015, 13) == 6 &&
         project::days_in_month(e, 2016, 13) == 5);
  assert(project::terrestrial_day(e, {2016, 1, 1}) ==
         project::terrestrial_day(g, {2023, 9, 12}));
  assert(project::calendar_ordinal(a, {1, 1, 1}) -
             project::calendar_ordinal(a, {0, 1, 1}) ==
         365);
  assert(project::days_in_month(a, 4, 13) == 5);
  refuses([&] { project::terrestrial_day(a, {1, 1, 1}); });
  auto maya = project::terrestrial_date(m, 2451545);
  assert(maya.month == 14 && maya.day == 10);
  auto rendered = project::format_calendar_date(m, maya);
  assert(rendered.find("12.19.6.15.2") != std::string::npos &&
         rendered.find("11 Ik’") != std::string::npos);
  assert(project::terrestrial_day(m, {0, 18, 8}) == 584283);
  refuses([&] { project::calendar_ordinal(m, {0, 19, 5}); });
  refuses([&] { project::calendar_ordinal(ch, {1900, 1, 1}); });
  refuses([&] { project::calendar_ordinal(ch, {2101, 1, 1}); });
  refuses([&] { project::calendar_ordinal(h, {5783, 13, 1}); });
  refuses([&] { project::calendar_ordinal(h, {0, 1, 1}); });
  auto bad = ch;
  bad.rule_revision = "icu-other";
  refuses([&] { project::validate(bad); });
  bad = ch;
  bad.months[0].days = 29;
  refuses([&] { project::validate(bad); });
  std::size_t count = 0;
  for (auto c : {ch, h, i, e, a, m}) {
    auto years = c.engine == "chinese"
                     ? std::vector<std::int64_t>{1901, 2023, 2024, 2057, 2100}
                 : c.engine == "hebrew"
                     ? std::vector<std::int64_t>{1, 5783, 5784, 9999}
                     : std::vector<std::int64_t>{1, 3, 2015, 2016, 9999};
    for (auto y : years) {
      auto months = project::calendar_months(c, y);
      for (std::size_t mon = 0; mon < months.size(); ++mon) {
        for (int d = project::calendar_first_day(c);
             d < project::calendar_first_day(c) + months[mon].days; ++d) {
          project::CalendarDate date{y, static_cast<int>(mon + 1), d};
          assert(project::calendar_date(
                     c, project::calendar_ordinal(c, date)) == date);
          ++count;
        }
      }
    }
  }
  auto root = std::filesystem::temp_directory_path() /
              ("inde-template-links-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  s.create(root / "Tempo.inde", "Templates e vínculos");
  auto &w = s.worldbuilding();
  for (auto c : {ch, h, i, e, a, m}) {
    auto saved = w.create_calendar(c);
    assert(w.calendar(saved.id) == saved);
  }
  g = w.create_calendar(g);
  auto axis = s.planning().create_time_axis("História");
  project::TemporalReference r;
  r.axis_id = axis.id;
  r.calendar_id = g.id;
  r.label = "Data compartilhada";
  r.year = 2023;
  r.month = 9;
  r.day = 16;
  r = w.save_temporal_reference(r);
  auto maya_ref = r;
  maya_ref.id.clear();
  maya_ref.token.clear();
  maya_ref.label = "Maia — dia zero";
  maya_ref.year = 0;
  maya_ref.month = 1;
  maya_ref.day = 0;
  for (const auto &c : w.calendars())
    if (c.engine == "maya-haab")
      maya_ref.calendar_id = c.id;
  maya_ref = w.save_temporal_reference(maya_ref);
  const auto all_refs = w.temporal_references(axis.id);
  assert(std::find(all_refs.begin(), all_refs.end(), maya_ref) !=
         all_refs.end());
  assert(w.temporal_targets("entity", "%_").empty());
  auto et = s.narrative().create_entity_type("testperson", "Pessoa de teste");
  auto x = s.narrative().create_entity(et.id, "QA Pessoa"),
       y = s.narrative().create_entity(et.id, "QA Outra");
  auto rt = s.narrative().create_relation_type(
      "testlink", "Ligação", "Ligação inversa",
      project::RelationDirectionality::Directed);
  auto rel = s.narrative().create_relation(rt.id, x.id, y.id, "QA Relação");
  auto ip = s.create_intellectual_property("QA Universo", "", "");
  auto work = s.create_work(ip.id, "QA Obra", "", "", "pt-BR", "draft");
  auto ns = s.structures().create_narrative(work.id, "QA Estrutura");
  auto unit = s.structures().create_unit(ns.id, "1", "QA Unidade", "", "", "");
  auto line = s.structures().create_line(ns.id, "QA Linha", "");
  std::vector<project::TemporalAttachment> links;
  for (auto [kind, id] :
       std::vector<std::pair<std::string, std::string>>{{"entity", x.id},
                                                        {"relation", rel.id},
                                                        {"unit", unit.id},
                                                        {"line", line.id}}) {
    auto target = w.temporal_target(kind, id);
    auto results = w.temporal_targets(kind, "QA");
    assert(!results.empty());
    project::TemporalAttachment link;
    link.reference_id = r.id;
    link.kind = kind;
    link.target_id = id;
    link.role = "ocorrência";
    auto saved = w.attach_date(link, r, target);
    links.push_back(saved);
    assert(w.target_dates(target) ==
           std::vector<project::TemporalReference>{r});
    refuses([&] { w.attach_date(link, r, target); });
  }
  assert(w.temporal_attachments(r.id).size() == 4);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(root /
                                                              "Tempo.inde"));
    auto corrupt =
        db.prepare("UPDATE temporal_attachments SET entity_id=? WHERE id=?");
    corrupt.bind(1, project::new_uuid());
    corrupt.bind(2, links[0].id);
    refuses([&] { corrupt.run(); });
    auto corrupt2 =
        db.prepare("UPDATE temporal_attachments SET relation_id=? WHERE id=?");
    corrupt2.bind(1, rel.id);
    corrupt2.bind(2, links[0].id);
    refuses([&] { corrupt2.run(); });
  }
  refuses([&] { w.remove_temporal_reference(r); });
  refuses([&] { s.narrative().delete_relation(rel.id); });
  refuses([&] { s.structures().delete_unit(unit.id); });
  refuses([&] { s.structures().delete_line(line.id); });
  refuses([&] { s.structures().delete_narrative(ns.id); });
  auto stale_target = w.temporal_target("entity", y.id);
  auto y2 = y;
  y2.name = "Renomeada";
  y2 = s.narrative().update_entity(y2);
  project::TemporalAttachment link;
  link.reference_id = r.id;
  link.kind = "entity";
  link.target_id = y.id;
  link.role = "outra";
  refuses([&] { w.attach_date(link, r, stale_target); });
  auto changed = r;
  changed.year = 2024;
  refuses([&] { w.save_temporal_reference(changed, r); });
  auto snapshot = w.temporal_attachments(r.id);
  auto previous = r;
  r = w.save_temporal_reference(changed, r, snapshot);
  refuses([&] {
    w.attach_date(link, previous, w.temporal_target("entity", y.id));
  });
  w.remove_date_attachment(links[0]);
  refuses([&] { w.remove_date_attachment(links[0]); });
  auto impact = s.reversal_impact(s.project_operations().front().id);
  assert(impact.allowed);
  s.confirm_project_operation(impact);
  assert(w.temporal_attachments(r.id).size() == 4);
  s.save_as(root / "Cópia.inde");
  s.close();
  s.open(root / "Cópia.inde");
  assert(w.temporal_attachments(r.id).size() == 4);
  assert(w.target_dates(w.temporal_target("entity", x.id)).front() == r);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(root /
                                                              "Cópia.inde"));
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    auto check = db.prepare("PRAGMA foreign_key_check");
    assert(!check.step());
  }
  for (auto l : w.temporal_attachments(r.id))
    w.remove_date_attachment(l);
  w.remove_temporal_reference(r);
  s.close();
  s.open(root / "Tempo.inde");
  assert(w.temporal_attachments(r.id).size() == 4);
  s.close();
  s.create(root / "Migration.inde", "Migration");
  s.close();
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(
            root / "Migration.inde"));
    downgrade_schema28_for_fixture(db);
    db.execute(
        "DROP TABLE narrative_dependency_scopes;DROP TABLE "
        "narrative_branch_provenance;DROP INDEX narrative_link_owner;DROP "
        "TRIGGER narrative_context_local_type;DROP TABLE "
        "narrative_unit_relations;DROP TABLE narrative_realizations;DROP TABLE "
        "narrative_unit_contexts;DELETE FROM schema_migrations WHERE "
        "version>=26;"
        "DROP TABLE semantic_date_bindings;DROP TABLE "
        "temporal_reference_extents;DELETE FROM schema_migrations WHERE "
        "version=25;"
        "DROP TABLE temporal_attachments;DROP INDEX "
        "temporal_references_owner;DROP INDEX relations_temporal_owner;DELETE "
        "FROM schema_migrations WHERE version=24;CREATE TRIGGER fail_v24 "
        "BEFORE INSERT ON schema_migrations WHEN NEW.version=24 BEGIN SELECT "
        "RAISE(ABORT,'QA rollback');END");
    refuses([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 23);
    assert(db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='temporal_attachments'") == 0);
    db.execute("DROP TRIGGER fail_v24");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) ==
           persistence::current_database_schema_version);
  }
  std::filesystem::remove_all(root);
  std::cout
      << "PASS six templates, " << count
      << " daily round trips, independent vectors, four links, stale guards, "
         "restrictions, reversal, Save As, restart, migration rollback\n";
}

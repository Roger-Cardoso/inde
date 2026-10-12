#include "inde/application/project_service.hpp"
#include "inde/persistence/json.hpp"
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
  try {
    f();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("Expected refusal at " + std::to_string(at.line()));
}
int main() {
  auto root = std::filesystem::temp_directory_path() /
              ("inde-temporal-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  auto g = project::calendar_template("gregorian"),
       j = project::calendar_template("julian");
  assert(project::days_in_month(g, 1900, 2) == 28);
  assert(project::days_in_month(j, 1900, 2) == 29);
  assert(project::days_in_month(g, 2000, 2) == 29);
  assert(project::days_in_month(g, 2100, 2) == 28);
  assert(project::days_in_month(g, 0, 2) == 29);
  assert(project::terrestrial_day(g, {2000, 1, 1}) == 2451545);
  assert(project::terrestrial_day(j, {1582, 10, 4}) == 2299160);
  assert(project::terrestrial_day(g, {1582, 10, 15}) == 2299161);
  assert((project::terrestrial_date(
              g, project::terrestrial_day(j, {1582, 10, 5})) ==
          project::CalendarDate{1582, 10, 15}));
  assert(project::calendar_weekday(g, {2000, 1, 1}) == 5);
  assert(project::calendar_weekday(j, {1582, 10, 4}) == 3);
  for (auto y : {-1000000LL, -401LL, -100LL, -4LL, -1LL, 0LL, 1LL, 100LL, 400LL,
                 1000000LL})
    for (const auto &c : {g, j})
      for (int m = 1; m <= 12; ++m)
        for (int d = 1; d <= project::days_in_month(c, y, m); ++d) {
          project::CalendarDate date{y, m, d};
          assert(project::calendar_date(
                     c, project::calendar_ordinal(c, date)) == date);
        }
  auto invalid = g;
  invalid.months[0].days = 30;
  refuses([&] { project::validate(invalid); });
  invalid = g;
  invalid.engine = "unknown";
  refuses([&] { project::validate(invalid); });
  auto own = g;
  own.engine = "custom";
  own.name = "Vigílias";
  own.months = {{"Aurora", 30}, {"Noite", 20}};
  own.leap_month = 1;
  own.clock = {{"vigília", "vigílias", "v", 8}, {"pulso", "pulsos", "p", 12}};
  project::validate(own);
  assert(project::clock_ticks(own, {7, 11}) == 95);
  refuses([&] { project::clock_ticks(own, {8, 0}); });
  refuses([&] { project::clock_ticks(own, {7}); });
  invalid = own;
  invalid.clock[0].parts = 0;
  refuses([&] { project::validate(invalid); });
  invalid = own;
  invalid.clock.resize(8, {"x", "xs", "x", 10000});
  refuses([&] { project::validate(invalid); });
  application::ProjectService s;
  s.create(root / "Tempo.inde", "Tempo");
  auto &w = s.worldbuilding();
  g = w.create_calendar(g);
  j = w.create_calendar(j);
  own = w.create_calendar(own);
  assert(w.calendar(own.id) == own && w.calendar(g.id) == g);
  auto rev = g;
  rev.name = "Gregoriano — revisão de nomes";
  rev.months[0].name = "Primeiro";
  rev = w.create_calendar(rev, g.id);
  assert(w.calendar_parent(rev.id) == g.id);
  assert(w.calendar(g.id) == g);
  refuses([&] { w.create_calendar(rev, project::new_uuid()); });
  auto legacy = own;
  legacy.name = "Calendário legado v1";
  legacy.clock = project::FictionalCalendar{}.clock;
  legacy = w.create_calendar(legacy);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(root /
                                                              "Tempo.inde"));
    auto payload =
        db.prepare("SELECT payload FROM fictional_calendars WHERE id=?");
    payload.bind(1, legacy.id);
    assert(payload.step());
    auto object = persistence::json::parse(payload.column_text(0)).as_object();
    object.erase("engine");
    object.erase("clock");
    object["version"] = 1;
    const auto v1 = persistence::json::serialize(object, false);
    auto *sha =
        g_compute_checksum_for_string(G_CHECKSUM_SHA256, v1.data(), v1.size());
    auto update = db.prepare(
        "UPDATE fictional_calendars SET payload=?,sha256=? WHERE id=?");
    update.bind(1, v1);
    update.bind(2, std::string(sha));
    update.bind(3, legacy.id);
    update.run();
    g_free(sha);
    assert(w.calendar(legacy.id) == legacy);
    assert(db.query_text("SELECT payload FROM fictional_calendars WHERE id='" +
                         legacy.id + "'") == v1);
  }
  auto axis = s.planning().create_time_axis("Terra");
  auto base = w.assign_calendar(axis.id, j.id, 1721058, {});
  assert(base);
  auto before = s.planning().create_time_point(axis.id, 2299160, "Antes");
  auto after = s.planning().create_time_point(axis.id, 2299161, "Depois");
  auto history = w.adoptions(axis.id);
  assert(history.empty());
  project::CalendarAdoption adoption{{},      axis.id, g.id,
                                     2299161, 1721060, "coexist"};
  adoption = w.adopt_calendar(adoption, *base, history);
  auto adoption_removal =
      s.removal_impact(project::RemovalKind::CalendarSystem, g.id);
  assert(!adoption_removal.allowed &&
         adoption_removal.blocked_reason.find("adoções e sucessões") !=
             std::string::npos);
  assert(w.point_date(before) == "4 de Outubro, ano 1582");
  assert(w.point_date(after) == "15 de Outubro, ano 1582");
  assert(w.parallel_dates(after).size() == 2 &&
         w.parallel_dates(before).size() == 1);
  project::AxisCalendar successor{axis.id, g.id, 1721060, adoption.id};
  auto created_after =
      w.create_dated_point(successor, {1582, 10, 16}, "Novo depois da reforma");
  assert(created_after.ordinal == 2299162);
  const auto successor_month = w.month_view(successor, 1582, 10);
  assert(successor_month.matching_points == 3);
  auto invalid_lens = successor;
  invalid_lens.token = project::new_uuid();
  refuses([&] { w.month_view(invalid_lens, 1582, 10); });
  invalid_lens = successor;
  ++invalid_lens.epoch_ordinal;
  refuses([&] { w.month_view(invalid_lens, 1582, 10); });
  refuses(
      [&] { w.create_dated_point(*base, {1582, 10, 6}, "Sistema sucedido"); });
  refuses([&] {
    w.create_dated_point(successor, {1582, 10, 14},
                         "Sucessor antes da vigência");
  });
  refuses(
      [&] { w.adopt_calendar(adoption, *base, history); }); // stale snapshot
  refuses([&] {
    w.assign_calendar(axis.id, j.id, 0, base);
  }); // no global rewrite of history
  history = w.adoptions(axis.id);
  auto retired = w.adopt_calendar(
      {{}, axis.id, rev.id, 2299162, 1721060, "retire"}, *base, history);
  const auto adoption_operation = s.project_operations().front().id;
  assert(w.parallel_dates(created_after).size() == 1);
  assert(w.month_view(successor, 1582, 10).matching_points ==
         3); // Explicit read-only conversion after retirement.
  assert(w.parallel_dates(after).size() ==
         2); // historical parallel remains before next reform
  auto impact = s.reversal_impact(adoption_operation);
  assert(impact.allowed);
  s.confirm_project_operation(impact);
  assert(w.adoptions(axis.id) ==
         history); // reversible cutover, unchanged points
  assert(s.planning().time_point(created_after.id)->ordinal == 2299162);
  assert(s.planning().time_point(after.id)->ordinal == 2299161);
  assert(s.planning().time_point(before.id)->label == before.label);
  refuses([&] {
    s.planning().delete_time_axis(axis.id);
  }); // authorial temporal dependency protected
  project::TemporalReference r;
  r.axis_id = axis.id;
  r.calendar_id = own.id;
  r.label = "Colheita";
  r.year = 12;
  r = w.save_temporal_reference(r);
  assert(!r.month && !r.day && r.clock.empty());
  assert(project::format_temporal_reference(own, r) ==
         "ano 12 (hora não informada)");
  auto previous = r;
  r.month = 1;
  r = w.save_temporal_reference(r, previous);
  assert(!r.day && r.clock.empty());
  refuses([&] { w.save_temporal_reference(previous, previous); });
  previous = r;
  r.day = 3;
  r.clock = {7};
  r = w.save_temporal_reference(r, previous);
  assert(project::format_temporal_reference(own, r).find("horário parcial") !=
         std::string::npos);
  previous = r;
  r.clock = {7, 11};
  r = w.save_temporal_reference(r, previous);
  assert(w.temporal_references(axis.id).size() == 1);
  auto another = r;
  another.id.clear();
  another.label = "Mesmo dia, outro horário";
  another.clock = {6, 2};
  another = w.save_temporal_reference(another);
  assert(w.temporal_references(axis.id).size() == 2);
  auto impossible = r;
  impossible.day = 60;
  refuses([&] { w.save_temporal_reference(impossible, r); });
  impossible = r;
  impossible.month.reset();
  refuses([&] { w.save_temporal_reference(impossible, r); });
  impossible = r;
  impossible.clock = {8, 0};
  refuses([&] { w.save_temporal_reference(impossible, r); });
  impossible = r;
  impossible.calendar_id = project::new_uuid();
  refuses([&] { w.save_temporal_reference(impossible, r); });
  refuses([&] { w.remove_temporal_reference(previous); });
  auto r_snapshot = r;
  (void)retired;
  const auto origin_path = root / "Tempo.inde";
  s.save_as(root / "Cópia.inde");
  assert(w.adoptions(axis.id) == history && w.calendar_parent(rev.id) == g.id);
  assert(w.temporal_references(axis.id).size() == 2 &&
         w.calendar(own.id) == own);
  s.close();
  s.open(root / "Cópia.inde");
  assert(w.temporal_references(axis.id).size() == 2);
  w.remove_temporal_reference(r);
  assert(w.temporal_references(axis.id).size() == 1);
  auto removal = s.reversal_impact(s.project_operations().front().id);
  assert(removal.allowed);
  s.confirm_project_operation(removal);
  auto restored = w.temporal_references(axis.id);
  assert(std::find(restored.begin(), restored.end(), r_snapshot) !=
         restored.end());
  s.close();
  s.open(origin_path);
  assert(w.temporal_references(axis.id).size() == 2);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(origin_path));
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    auto fk = db.prepare("PRAGMA foreign_key_check");
    assert(!fk.step());
    db.execute("UPDATE temporal_references SET clock='[[[[0]]]]'");
    refuses([&] { w.temporal_references(axis.id); });
  }
  // Migration rollback is tested against a disposable schema22 project, no
  // authored rows.
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
        "relations_temporal_owner;DELETE FROM schema_migrations WHERE "
        "version=24;DROP TABLE temporal_references;DROP TABLE "
        "calendar_adoptions;DROP TABLE calendar_revisions;DELETE FROM "
        "schema_migrations WHERE version=23");
    db.execute(
        "CREATE TRIGGER fail_v23 BEFORE INSERT ON schema_migrations WHEN "
        "NEW.version=23 BEGIN SELECT RAISE(ABORT,'QA rollback');END");
    refuses([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 22);
    assert(db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='calendar_adoptions'") == 0);
    db.execute("DROP TRIGGER fail_v23");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) ==
           persistence::current_database_schema_version);
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS temporal systems: calendars, clocks, partial dates, "
               "revisions, cutovers, conflicts, restart, Save As, reversals, "
               "integrity, migration rollback\n";
}

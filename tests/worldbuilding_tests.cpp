#include "inde/application/project_service.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "schema21_downgrade.hpp"
#include <algorithm>
#include <cassert>
#include <glib.h>
#include <iostream>
#include <limits>
#include <source_location>
using namespace inde;
template <class F>
void refuses(F f, std::source_location at = std::source_location::current()) {
  try {
    f();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("Expected refusal: " + std::to_string(at.line()));
}
int main() {
  auto root = std::filesystem::temp_directory_path() /
              ("inde-world-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  const auto path = root / "Mundo.inde";
  s.create(path, "Mundo");
  auto &w = s.worldbuilding();
  auto a = s.narrative().create_entity("00000000-0000-4000-9000-000000000001",
                                       "Ágata", "Resumo"),
       b = s.narrative().create_entity(a.entity_type_id, "Bento");
  project::EntityAlias alias;
  alias.entity_id = a.id;
  alias.name = "  ÁGUA 🙂  ";
  alias.kind = "nickname";
  alias.note = "Nome de viagem";
  alias = w.save_alias(alias);
  assert(project::alias_key(alias.name) == project::alias_key("água 🙂"));
  auto duplicate = alias;
  duplicate.id.clear();
  duplicate.name = "Água 🙂";
  refuses([&] { w.save_alias(duplicate); });
  duplicate.name = "ÁGATA";
  refuses([&] { w.save_alias(duplicate); });
  duplicate.name = "%_";
  duplicate = w.save_alias(duplicate);
  auto homonym = alias;
  homonym.id.clear();
  homonym.entity_id = b.id;
  homonym = w.save_alias(homonym);
  persistence::EntityQuery q;
  q.search = "água 🙂";
  q.limit = 1;
  assert(s.narrative().entity_count(q) == 2);
  auto first = s.narrative().entities(q);
  assert(first.size() == 1);
  q.offset = 1;
  auto next = s.narrative().entities(q);
  assert(next.size() == 1 && next[0].id != first[0].id);
  q.search = "%_";
  q.offset = 0;
  assert(s.narrative().entity_count(q) == 1);
  q.entity_type_ids = {"00000000-0000-4000-9000-000000000002"};
  assert(s.narrative().entity_count(q) == 0);
  auto before = alias;
  alias.name = "Maré";
  alias = w.save_alias(alias, before);
  refuses([&] { w.save_alias(before, before); });
  refuses([&] { w.remove_alias(before); });
  auto later = alias;
  later.name = before.name;
  later = w.save_alias(later, alias);
  assert(later.token != before.token);
  refuses([&] { w.remove_alias(before); });
  assert(s.narrative().entity(a.id)->name == a.name);
  w.remove_alias(duplicate);
  assert(w.aliases(a.id).size() == 1);
  project::FictionalCalendar c{project::new_uuid(),
                               "Ano das marés",
                               {{"Aurora", 30}, {"Bruma", 20}, {"Noite", 10}},
                               {"Luz", "Sombra", "Eco"},
                               4,
                               0,
                               2};
  project::validate(c);
  for (auto y :
       {-1000000LL, -101LL, -5LL, -4LL, -1LL, 0LL, 1LL, 4LL, 1000000LL})
    for (int month = 1; month <= 3; ++month)
      for (int day = 1; day <= project::days_in_month(c, y, month); ++day) {
        project::CalendarDate d{y, month, day};
        assert(project::calendar_date(c, project::calendar_ordinal(c, d, 1000),
                                      1000) == d);
      }
  assert(project::days_in_month(c, -4, 2) == 21 &&
         project::days_in_month(c, -1, 2) == 20);
  assert(project::calendar_ordinal(c, {0, 1, 1}) == 0);
  assert(project::calendar_ordinal(c, {-1, 3, 10}) == -1);
  refuses([&] { project::calendar_ordinal(c, {1, 2, 21}); });
  refuses([&] { project::calendar_ordinal(c, {1000001, 1, 1}); });
  refuses([&] {
    project::calendar_ordinal(c, {0, 1, 2},
                              std::numeric_limits<std::int64_t>::max());
  });
  refuses([&] {
    project::calendar_date(c, std::numeric_limits<std::int64_t>::min(),
                           std::numeric_limits<std::int64_t>::max());
  });
  auto invalid = c;
  invalid.months[1].name = "AURORA";
  refuses([&] { project::validate(invalid); });
  invalid = c;
  invalid.leap_phase = 4;
  refuses([&] { project::validate(invalid); });
  invalid = c;
  invalid.months[1].days = 60;
  refuses([&] { project::validate(invalid); });
  c = w.create_calendar(c);
  assert(w.calendar(c.id) == c);
  auto auxiliary = s.planning().create_time_axis("Eixo removível");
  auto auxiliary_binding = w.assign_calendar(auxiliary.id, c.id, 0, {});
  auto auxiliary_point =
      w.create_dated_point(*auxiliary_binding, {-1, 1, 1}, "Origem auxiliar");
  refuses([&] { s.planning().delete_time_axis(auxiliary.id); });
  assert(w.binding(auxiliary.id) == auxiliary_binding);
  s.planning().delete_time_point(auxiliary_point.id);
  s.planning().delete_time_axis(auxiliary.id);
  const auto remaining_axes = s.planning().time_axes();
  assert(std::none_of(remaining_axes.begin(), remaining_axes.end(),
                      [&](const auto &a) { return a.id == auxiliary.id; }) &&
         !s.planning().time_point(auxiliary_point.id));
  auto axis_reversal = s.reversal_impact(s.project_operations().front().id);
  if (!axis_reversal.allowed)
    throw std::runtime_error("Axis reversal refused: " +
                             axis_reversal.blocked_reason);
  s.confirm_project_operation(axis_reversal);
  assert(w.binding(auxiliary.id) == auxiliary_binding &&
         !s.planning().time_point(auxiliary_point.id));
  auto axis = s.planning().create_time_axis("Era das marés");
  auto original =
      s.planning().create_time_point(axis.id, 777, "Marco relativo");
  auto binding = w.assign_calendar(axis.id, c.id, 1000, {});
  assert(binding);
  assert(s.planning().time_point(original.id)->ordinal == 777 &&
         s.planning().time_point(original.id)->label == original.label);
  auto point = w.create_dated_point(*binding, {4, 2, 21}, "Travessia");
  try {
    (void)w.create_dated_point(*binding, {4, 2, 21}, "Colisão não fundida");
    assert(false);
  } catch (const std::exception &e) {
    assert(std::string(e.what()).find("Já existe um ponto nessa data") !=
           std::string::npos);
  }
  assert(w.point_date(point) == "21 de Bruma, ano 4");
  auto event = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000003", "Chegada");
  (void)s.planning().place_event(event.id, point.id);
  auto view = w.month_view(*binding, 4, 2);
  assert(view.matching_points == 1 && view.matching_events == 1 &&
         view.events[0].name == event.name);
  for (int i = 0; i < 505; ++i)
    (void)s.planning().create_time_point(axis.id, -10000 + i,
                                         "Distante " + std::to_string(i));
  assert(w.month_view(*binding, 4, 2).matching_points == 1);
  auto stale = binding;
  binding = w.assign_calendar(axis.id, c.id, 1001, binding);
  refuses([&] { w.create_dated_point(*stale, {0, 1, 1}, "Stale"); });
  refuses([&] { w.assign_calendar(axis.id, c.id, 1000, stale); });
  binding = w.assign_calendar(axis.id, c.id, 1000, binding);
  assert(binding->token != stale->token);
  refuses([&] { w.month_view(*stale, 4, 2); });
  const auto alias_op =
      s.project_operations()
          .front(); // association operation, reversible with identical rows.
  auto preview = s.reversal_impact(alias_op.id);
  if (!preview.allowed)
    throw std::runtime_error("Association reversal refused: " +
                             preview.blocked_reason);
  s.confirm_project_operation(preview);
  assert(w.binding(axis.id)->epoch_ordinal == 1001);
  binding = w.binding(axis.id);
  w.assign_calendar(axis.id, {}, 0, binding);
  assert(!w.binding(axis.id));
  assert(s.planning().time_point(point.id)->ordinal == point.ordinal);
  binding = w.assign_calendar(axis.id, c.id, 1000, {});
  s.close();
  s.open(path);
  assert(w.aliases(a.id)[0].name == later.name &&
         w.binding(axis.id) == binding && w.calendar(c.id) == c);
  auto dbpath = persistence::ProjectDatabaseRepository::database_path(path);
  {
    persistence::SqliteDatabase db(dbpath);
    db.execute("UPDATE fictional_calendars SET sha256='" +
               std::string(64, '0') + "' WHERE id='" + c.id + "'");
    refuses([&] { w.calendar(c.id); });
    refuses([&] { w.create_dated_point(*binding, {0, 1, 1}, "Corrupt"); });
    db.execute("UPDATE fictional_calendars SET sha256=lower(hex(zeroblob(32))) "
               "WHERE 0");
    auto payload = db.query_text(
        "SELECT payload FROM fictional_calendars WHERE id='" + c.id + "'");
    auto *hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256,
                                               payload.data(), payload.size());
    db.execute("UPDATE fictional_calendars SET sha256='" + std::string(hash) +
               "' WHERE id='" + c.id + "'");
    g_free(hash);
  }
  const auto owner_id = s.current()->manifest().project_id;
  s.save_as(root / "Cópia.inde");
  assert(s.current()->manifest().project_id != owner_id &&
         w.calendar(c.id) == c && w.binding(axis.id) == binding &&
         w.aliases(a.id)[0] == later);
  s.close();
  s.open(path);
  // Removing an entity and reverting preserves its aliases through the actual
  // operation changeset.
  auto impact = s.removal_impact(project::RemovalKind::Entity, b.id);
  if (!impact.allowed)
    throw std::runtime_error("Entity removal refused: " +
                             impact.blocked_reason);
  s.confirm_project_operation(impact);
  assert(!s.narrative().entity(b.id));
  auto removal = s.project_operations().front();
  auto reversal = s.reversal_impact(removal.id);
  if (!reversal.allowed)
    throw std::runtime_error("Entity reversal refused: " +
                             reversal.blocked_reason);
  s.confirm_project_operation(reversal);
  assert(w.aliases(b.id)[0] == homonym);
  {
    persistence::SqliteDatabase db(dbpath);
    assert(db.query_text("PRAGMA integrity_check") == "ok" &&
           !db.prepare("PRAGMA foreign_key_check").step());
  }
  {
    persistence::SqliteDatabase db(root / "migration.sqlite3");
    persistence::SchemaMigrator{}.migrate(db);
    downgrade_schema22_for_fixture(db);
    db.execute(
        "CREATE TRIGGER fail_v22 BEFORE INSERT ON schema_migrations WHEN "
        "NEW.version=22 BEGIN SELECT RAISE(ABORT,'QA rollback');END");
    refuses([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(
        persistence::SchemaMigrator{}.current_version(db) == 21 &&
        db.query_integer(
            "SELECT count(*) FROM sqlite_master WHERE name='entity_aliases'") ==
            0);
    db.execute("DROP TRIGGER fail_v22");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) == persistence::current_database_schema_version &&
           db.query_integer("PRAGMA foreign_keys") == 1);
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS aliases/calendar: Unicode, collisions/search/pagination, "
               "dates/negative years/overflow, month range, conflict/ABA, "
               "restart/Save As, cascade/reversal, corruption and migration "
               "rollback\n";
}

#include "../src/application/temporal_extent_codec.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/experimental/text_blocks.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "schema21_downgrade.hpp"
#include <cassert>
#include <glib.h>
#include <iostream>
using namespace inde;
template <class F> void refuses(F f) {
  bool refused = false;
  try {
    f();
  } catch (const std::exception &) {
    refused = true;
  }
  assert(refused);
}
int main() {
  using project::compare_temporal_coordinates;
  using project::TemporalCoordinate;
  for (std::int64_t ad = 1; ad < 20; ++ad)
    for (std::int64_t bd = 1; bd < 20; ++bd)
      for (std::int64_t an = 0; an < ad; ++an)
        for (std::int64_t bn = 0; bn < bd; ++bn) {
          const auto lhs = an * bd, rhs = bn * ad;
          assert(compare_temporal_coordinates({-1, an, ad}, {-1, bn, bd}) ==
                 (lhs < rhs   ? -1
                  : lhs > rhs ? 1
                              : 0));
        }
  assert(compare_temporal_coordinates({0, 999999999999LL, 1000000000000LL},
                                      {0, 999999999998LL, 999999999999LL}) ==
         1);
  assert(compare_temporal_coordinates({-2, 1, 2}, {-1, 0, 1}) == -1);
  refuses([&] { compare_temporal_coordinates({0, 1, 1}, {0, 0, 1}); });
  refuses([&] { compare_temporal_coordinates({0, 0, 0}, {0, 0, 1}); });
  project::TemporalReference malformed;
  refuses([&] {
    application::temporal_extent_codec::decode(
        R"({"version":1,"qualifier":"between","upper":{"year":1,"month":1,"day":1,"clock":[null]},"uncertainty":""})",
        malformed);
  });
  auto root = std::filesystem::temp_directory_path() /
              ("inde-temporal-context-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  s.create(root / "Contexto.inde", "Tempo do texto");
  auto &w = s.worldbuilding();
  auto c = w.create_calendar(project::calendar_template("gregorian"));
  auto axis = s.planning().create_time_axis("História");
  project::TemporalReference base;
  base.calendar_id = c.id;
  base.axis_id = axis.id;
  base.label = "Nascimento";
  base.year = 2000;
  base.month = 1;
  base.day = 1;
  auto birth = w.save_temporal_reference(base);
  auto earlier = base;
  earlier.label = "Cena antes do nascimento";
  earlier.year = 1990;
  earlier = w.save_temporal_reference(earlier);
  for (auto mode : {"before", "after", "approximate", "season", "between"}) {
    auto ref = base;
    ref.label = mode;
    ref.qualifier = mode;
    ref.uncertainty = "Precisão autoral declarada";
    if (ref.qualifier == "between")
      ref.upper = project::TemporalBoundary{2001, 2, {}, {}};
    ref = w.save_temporal_reference(ref);
    assert(w.temporal_reference(ref.id) == ref);
    auto window = project::temporal_window(c, ref, 0);
    assert(window.comparable ==
           (ref.qualifier != "approximate" && ref.qualifier != "season"));
    assert(project::format_temporal_reference(c, ref).size() > 10);
  }
  auto interval = base;
  interval.qualifier = "between";
  interval.upper = project::TemporalBoundary{1999, 12, 31, {}};
  refuses([&] { w.save_temporal_reference(interval); });
  interval.upper = project::TemporalBoundary{2000, 1, 1, {}};
  interval.clock = {23, 0, 0};
  interval.upper->clock = {12, 0, 0};
  refuses([&] { w.save_temporal_reference(interval); });
  auto unknown = base;
  unknown.qualifier = "approximate";
  refuses([&] { w.save_temporal_reference(unknown); });
  auto partial = base;
  partial.day.reset();
  auto month = project::temporal_window(c, partial, 0);
  assert(*month.latest - *month.earliest == 31);
  partial.month.reset();
  auto year = project::temporal_window(c, partial, 0);
  assert(*year.latest - *year.earliest == 366);
  partial = base;
  partial.clock = {12};
  auto hour = project::temporal_window(c, partial, 0);
  assert(*hour.latest > *hour.earliest &&
         *hour.latest - *hour.earliest < 0.05L);
  auto type = s.narrative().create_entity_type("qa-person", "Pessoa QA");
  auto person = s.narrative().create_entity(type.id, "Lia");
  project::TemporalAttachment a;
  a.kind = "entity";
  a.target_id = person.id;
  a.reference_id = birth.id;
  a.role = "nascimento";
  auto free = w.attach_date(a, birth, w.temporal_target("entity", person.id));
  auto d = s.writing().create_document("Cena de viagem no tempo");
  d.content = "Lia chegou ao passado. 🙂";
  using S = application::TextSemanticService;
  using K = project::SemanticFieldKind;
  const auto context = *project::find_annotation_type(
      project::builtin_annotation_types(), "inde.semantic.temporal-context", 1);
  d = S::create(d, context, 0, 3, {{"participants", {K::Entity, {person.id}}}});
  auto id = d.semantics.annotations.back().id;
  const auto anchor = d.semantics.annotations.back().anchor_id;
  auto before = d;
  d = S::bind_date(d, id, earlier.id, "scene", "Data da ocorrência narrada");
  refuses([&] { S::bind_date(d, id, earlier.id, "scene"); });
  auto invalid = S::bind_date(before, id, project::new_uuid(), "scene");
  refuses([&] { static_cast<void>(s.writing().update_document(invalid)); });
  assert(w.continuity_notices(d)
             .empty()); // free word "nascimento" is not a formal birth
  w.remove_date_attachment(free);
  a.meaning = "birth";
  auto formal = w.attach_date(a, birth, w.temporal_target("entity", person.id));
  assert(w.continuity_notices(d).size() == 1);
  auto exceptional = S::update(d, id,
                               {{"participants", {K::Entity, {person.id}}},
                                {"exception", {K::Enum, {"time_travel"}}}},
                               project::builtin_annotation_types());
  assert(w.continuity_notices(exceptional).front().exceptional);
  d = s.writing().update_document(exceptional); // warning cannot block saving
  assert(
      s.writing().document(d.id)->semantics.annotations.front().dates.size() ==
      1);
  const auto encoded = persistence::encode_semantics(d.semantics);
  assert(project::same_semantics(persistence::decode_semantics(encoded),
                                 d.semantics));
  auto uses = w.text_date_uses(earlier.id);
  assert(uses.size() == 1 && uses.front().title == d.title);
  auto changed = earlier;
  changed.note = "Revisada";
  refuses([&] { w.save_temporal_reference(changed, earlier); });
  earlier = w.save_temporal_reference(changed, earlier, {}, uses);
  auto interval_change = earlier;
  interval_change.qualifier = "between";
  interval_change.upper = project::TemporalBoundary{1991, 1, 1, {}};
  interval_change = w.save_temporal_reference(interval_change, earlier, {},
                                              w.text_date_uses(earlier.id));
  auto reversal = s.reversal_impact(s.project_operations().front().id);
  assert(reversal.allowed);
  s.confirm_project_operation(reversal);
  earlier = w.temporal_reference(earlier.id);
  assert(earlier.qualifier == "exact" && !earlier.upper);
  assert(w.text_date_uses(earlier.id).size() == 1);
  refuses([&] { w.remove_temporal_reference(earlier); });
  auto olduses = w.text_date_uses(earlier.id);
  const auto end = project::utf8_character_count(d.content);
  auto revised =
      application::replace_selection({d, {end, end}}, end, end, " Outra frase.")
          .document;
  revised = s.writing().update_document(revised);
  refuses([&] { w.save_temporal_reference(earlier, earlier, {}, olduses); });
  d = revised;
  application::DocumentEditSession edit;
  edit.open({before, {0, 3}});
  edit.commit({d, {0, 3}}, "Marcar tempo");
  assert(edit.undo()->document.semantics.annotations.front().dates.empty());
  assert(edit.redo()->document.semantics.annotations.front().dates.size() == 1);
  auto moved =
      application::replace_selection({d, {0, 0}}, 0, 0, "Ontem, ").document;
  assert(moved.semantics.annotations.front().anchor_id == anchor &&
         moved.semantics.annotations.front().dates ==
             d.semantics.annotations.front().dates);
  assert(moved.anchors.front().end_offset == 10);
  auto missing = d;
  application::SemanticAnchorEngine::edit(missing, 0, 3, "");
  missing.content = d.content.substr(3);
  assert(w.continuity_notices(missing).empty());
  auto no_presence = S::update(d, id, {}, project::builtin_annotation_types());
  assert(w.continuity_notices(no_presence).empty());
  auto mention =
      S::unbind_date(d, id, d.semantics.annotations.front().dates.front().id);
  mention = S::bind_date(mention, id, earlier.id, "mention");
  assert(w.continuity_notices(mention).empty());
  auto ambiguous = S::bind_date(d, id, birth.id, "scene");
  assert(w.continuity_notices(ambiguous).empty());
  auto approximate = earlier;
  approximate.id.clear();
  approximate.token.clear();
  approximate.qualifier = "approximate";
  approximate.uncertainty = "Talvez uma década antes";
  approximate = w.save_temporal_reference(approximate);
  auto uncertain =
      S::unbind_date(d, id, d.semantics.annotations.front().dates.front().id);
  uncertain = S::bind_date(uncertain, id, approximate.id, "scene");
  assert(w.continuity_notices(uncertain).empty());
  auto another = s.planning().create_time_axis("Outro eixo");
  auto other = earlier;
  other.id.clear();
  other.token.clear();
  other.axis_id = another.id;
  other = w.save_temporal_reference(other);
  auto different =
      S::unbind_date(d, id, d.semantics.annotations.front().dates.front().id);
  different = S::bind_date(different, id, other.id, "scene");
  assert(w.continuity_notices(different).empty());
  auto julian = w.create_calendar(project::calendar_template("julian"));
  auto alternate = earlier;
  alternate.id.clear();
  alternate.token.clear();
  alternate.calendar_id = julian.id;
  alternate = w.save_temporal_reference(alternate);
  auto cross_calendar =
      S::unbind_date(d, id, d.semantics.annotations.front().dates.front().id);
  cross_calendar = S::bind_date(cross_calendar, id, alternate.id, "scene");
  assert(w.continuity_notices(cross_calendar).empty()); // no alignment guessed
  auto initial = w.assign_calendar(axis.id, c.id, 0, {});
  w.adopt_calendar({{}, axis.id, julian.id, 0, 0, "coexist"}, *initial, {});
  assert(w.continuity_notices(cross_calendar).size() == 1);
  w.adopt_calendar({{}, axis.id, c.id, 999999, 0, "coexist"}, *initial,
                   w.adoptions(axis.id));
  w.adopt_calendar({{}, axis.id, julian.id, 1000000, 100, "coexist"}, *initial,
                   w.adoptions(axis.id));
  assert(w.continuity_notices(cross_calendar)
             .empty()); // repeated version with distinct alignments
  auto perspective = S::create_document_perspective(
      d, {{"person", {K::Enum, {"third_person"}}},
          {"focalization", {K::Enum, {"external"}}}});
  perspective =
      S::bind_date(perspective, perspective.semantics.annotations.back().id,
                   earlier.id, "reference");
  d = s.writing().update_document(perspective);
  assert(w.text_date_uses(earlier.id).size() == 2);
#ifdef INDE_TEMPORAL_CONTEXT_LAB
  experimental::TextBlocksLab lab;
  auto block =
      lab.import_document(d, 0, project::utf8_character_count(d.content));
  lab.place(block);
  lab.place(block);
  auto output =
      experimental::materialize(lab, lab.project({}), "Montagem temporal");
  assert(output.document.semantics.annotations.size() == 2);
  assert(output.document.semantics.annotations[0].dates.front().id !=
         output.document.semantics.annotations[1].dates.front().id);
  s.writing().validate_semantic_document(output.document);
#endif
  s.save_as(root / "Cópia.inde");
  s.close();
  s.open(root / "Cópia.inde");
  assert(project::same_semantics(s.writing().document(d.id)->semantics,
                                 d.semantics) &&
         w.text_date_uses(earlier.id).size() == 2);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(root /
                                                              "Cópia.inde"));
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    auto fk = db.prepare("PRAGMA foreign_key_check");
    assert(!fk.step());
  }
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
        "TRIGGER narrative_context_local_type;"
        "DROP TABLE narrative_unit_relations;DROP TABLE narrative_realizations;"
        "DROP TABLE narrative_unit_contexts;"
        "DROP TABLE semantic_date_bindings;DROP TABLE "
        "temporal_reference_extents;ALTER TABLE temporal_attachments DROP "
        "COLUMN meaning;DELETE FROM schema_migrations WHERE version>=25;"
        "CREATE TRIGGER fail_v25 BEFORE INSERT ON schema_migrations WHEN "
        "NEW.version=25 BEGIN SELECT RAISE(ABORT,'QA rollback');END");
    refuses([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 24);
    assert(db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='semantic_date_bindings'") == 0);
    db.execute("DROP TRIGGER fail_v25");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) ==
           persistence::current_database_schema_version);
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS temporal context: precision/intervals, dates on "
               "passages/perspective, authorial warnings, no inference, "
               "exceptions, stale guards, undo, anchors, Save "
               "As, restart and rollback\n";
}

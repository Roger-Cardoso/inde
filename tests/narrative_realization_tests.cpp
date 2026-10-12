#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
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
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-realization-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  s.create(root / "Realização.inde", "Realização QA");
  const auto work =
      s.create_work(s.create_intellectual_property("Mundo", "", "").id, "Obra",
                    "", "", "pt-BR", "Planejamento")
          .id;
  const auto structure = s.structures().create_narrative(work, "Montagem").id;
  auto a = s.structures().create_unit(structure, "", "A", "", "", ""),
       b = s.structures().create_unit(structure, "", "B", "", "", ""),
       c = s.structures().create_unit(structure, "", "C", "", "", ""),
       d = s.structures().create_unit(structure, "", "D", "", "", "");
  auto &builder = s.builder();
  auto snap = [&] { return builder.snapshot(structure); };
  auto doc = s.writing().create_document("Autoridade textual");
  doc.content = "Prólogo — Água 🙂 e consequências.";
  doc.formatting = {{project::DocumentTextStyle::Italic, 0, 7, ""}};
  doc.inline_commands = {{"/", "cena", "Cena", "Texto literal"}};
  doc = s.writing().update_document(doc);
  builder.attach_document(snap(), a.id, doc.id);
  auto source_count = s.writing().document_count();
  auto before = snap();
  builder.attach_range(before, a.id, doc, 10, 16, "Água e emoji");
  doc = *s.writing().document(doc.id);
  assert(doc.anchors.size() == 1 &&
         s.writing().document_count() == source_count);
  refuses([&] { builder.attach_range(before, b.id, doc, 10, 16, "Stale"); });
  builder.attach_anchor(snap(), b.id, doc, doc.anchors.front().id);
  assert(snap().realizations.size() == 3);
  auto realization = [&](const std::string &u) {
    for (auto &r : snap().realizations)
      if (r.unit_id == u && !r.anchor_id.empty())
        return r;
    throw std::runtime_error("missing range");
  };
  const auto ar = realization(a.id);
  assert(ar.start == 10 && ar.end == 16 && ar.status == "current");
  assert(builder.text_uses(doc.id).size() == 3);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(
            s.current()->path()));
    const auto revision = snap().revision;
    db.execute("CREATE TRIGGER fail_target_decision BEFORE INSERT ON "
               "narrative_text_decisions BEGIN SELECT RAISE(ABORT,'QA atomic "
               "range');END");
    refuses([&] {
      builder.attach_range(snap(), c.id, doc, 0, 7, "Não gravar metade");
    });
    assert(snap().revision == revision &&
           project::same_document_state(*s.writing().document(doc.id), doc));
    db.execute("DROP TRIGGER fail_target_decision");
  }
  auto branch = builder.derive(snap(), "Alternativa compartilhada");
  auto alternative = builder.snapshot(branch.id);
  assert(alternative.realizations.size() == 3 &&
         s.writing().document_count() == source_count);
  assert(builder.compare(structure, branch.id).differences.empty());
  auto br = realization(b.id);
  builder.review_realization(snap(), br.id, "Texto conferido pelo autor",
                             false);
  assert(realization(b.id).review_state == "confirmed");
  auto edited =
      application::replace_selection({doc, {0, 0}}, 0, 0, "Antes 🙂 ");
  doc = s.writing().update_document(edited.document);
  assert(realization(a.id).start == 18 &&
         realization(a.id).current_sha == ar.current_sha);
  auto changed =
      application::replace_selection({doc, {0, 0}}, 18, 24, "Novo sentido");
  doc = s.writing().update_document(changed.document);
  assert(realization(a.id).status == "changed");
  auto notices = builder.continuity(snap());
  assert(std::any_of(notices.begin(), notices.end(),
                     [](auto &n) { return n.kind == "review"; }));
  builder.review_realization(snap(), ar.id, "Divergiu da intenção A", true);
  assert(realization(a.id).review_state == "diverged");
  builder.make_independent(snap(), br.id, "Texto B independente");
  auto independent = realization(b.id);
  auto copy = *s.writing().document(independent.document_id);
  assert(copy.id != doc.id && copy.content == doc.content &&
         copy.anchors.front().id != doc.anchors.front().id &&
         copy.formatting.size() == doc.formatting.size() &&
         copy.inline_commands == doc.inline_commands &&
         !copy.editorial_node_id);
  copy.content += " Só na alternativa.";
  copy = s.writing().update_document(copy);
  assert(s.writing().document(doc.id)->content == doc.content &&
         !independent.provenance.empty());
  auto missing = doc;
  missing.anchors.clear();
  doc = s.writing().update_document(missing);
  assert(realization(a.id).status == "missing");
  refuses(
      [&] { builder.make_independent(snap(), ar.id, "Não inventar alvo"); });
  refuses([&] {
    builder.review_realization(snap(), ar.id, "Falso aceite", false);
  });
  doc.anchors.push_back({project::new_uuid(), "Trecho substituto", 0, 7});
  doc = s.writing().update_document(doc);
  builder.attach_anchor(snap(), a.id, doc, doc.anchors.front().id, ar.id);
  assert(realization(a.id).id == ar.id &&
         realization(a.id).status == "current");
  auto reverse = s.reversal_impact(s.project_operations().front().id);
  assert(reverse.allowed);
  s.confirm_project_operation(reverse);
  assert(realization(a.id).status == "missing");
  builder.attach_anchor(snap(), a.id, doc, doc.anchors.front().id, ar.id);
  auto composition = s.compositions().create("Composição da obra");
  const auto occurrence = s.compositions().import(composition.value, doc, 0, 7);
  composition = s.compositions().save(composition);
  builder.attach_occurrence(snap(), c.id, composition, occurrence);
  builder.attach_occurrence(snap(), d.id, composition, occurrence);
  auto occurrence_for = [&](const std::string &u) {
    for (auto &r : snap().realizations)
      if (r.unit_id == u && !r.composition_id.empty())
        return r;
    throw std::runtime_error("missing occurrence");
  };
  auto cr = occurrence_for(c.id);
  assert(
      builder
          .text_uses("", composition.value.blocks.composition_id(), occurrence)
          .size() == 2);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(
            s.current()->path()));
    const auto revision = snap().revision, hash = composition.sha256;
    db.execute("CREATE TRIGGER fail_fork_save BEFORE UPDATE ON compositions "
               "BEGIN SELECT RAISE(ABORT,'QA atomic fork');END");
    refuses([&] {
      builder.make_independent(snap(), cr.id, "Bifurcação sem metade");
    });
    assert(snap().revision == revision &&
           s.compositions()
                   .load(composition.value.blocks.composition_id())
                   .sha256 == hash);
    db.execute("DROP TRIGGER fail_fork_save");
  }
  builder.make_independent(snap(), cr.id, "Bloco C independente");
  auto fork = occurrence_for(c.id);
  assert(fork.occurrence_id != occurrence &&
         occurrence_for(d.id).occurrence_id == occurrence);
  composition = s.compositions().load(fork.composition_id);
  auto fp = composition.value.blocks.occurrence(fork.occurrence_id);
  assert(fp.mode == experimental::PlacementMode::Forked && fp.hidden);
  auto old_text = composition.value.blocks
                      .revision(composition.value.blocks.occurrence(occurrence))
                      .text;
  composition.value.blocks.revise(fp.block_id,
                                  composition.value.blocks.revision(fp).id,
                                  "C independente — Água 🙂");
  composition = s.compositions().save(composition);
  assert(composition.value.blocks
             .revision(composition.value.blocks.occurrence(occurrence))
             .text == old_text);
  assert(occurrence_for(c.id).status == "hidden" &&
         occurrence_for(c.id).current_sha != occurrence_for(c.id).observed_sha);
  composition.value.blocks.remove_placement(fork.occurrence_id);
  composition = s.compositions().save(composition);
  assert(occurrence_for(c.id).status == "missing");
  builder.attach_occurrence(snap(), c.id, composition, occurrence, fork.id);
  assert(occurrence_for(c.id).status == "current");
  // Sources are protected and the deletion assistant points to their units.
  auto removal = s.removal_impact(project::RemovalKind::Document, doc.id);
  assert(!removal.allowed &&
         std::any_of(removal.uses.begin(), removal.uses.end(), [&](auto &u) {
           return u.table == "narrative_text_targets" && u.location_id == a.id;
         }));
  // Independent authorities; transitive root->B->C even when no root->C link.
  auto ab = s.structures().create_link(structure, a.id, b.id,
                                       project::NarrativeLinkKind::Precedes,
                                       "A antes de B"),
       bc = s.structures().create_link(structure, b.id, c.id,
                                       project::NarrativeLinkKind::Precedes,
                                       "B antes de C");
  builder.set_dependency_scope(snap(), ab.id, "narrative");
  builder.set_dependency_scope(snap(), bc.id, "narrative");
  builder.queue_unit(snap(), c.id, true);
  builder.queue_unit(snap(), a.id, true);
  builder.move_unit(snap(), c.id, application::BuilderOrder::Narrative, -1);
  builder.move_unit(snap(), c.id, application::BuilderOrder::Narrative, -1);
  notices = builder.continuity(snap());
  auto chain = std::find_if(notices.begin(), notices.end(), [&](auto &n) {
    return n.unit_id == a.id && n.other_unit_id == c.id && n.kind == "order";
  });
  assert(chain != notices.end() &&
         chain->evidence.find(ab.id) != std::string::npos &&
         chain->evidence.find(bc.id) != std::string::npos);
  const auto chain_notice = *chain;
  builder.set_exception(snap(), chain_notice,
                        "Leitura deliberadamente não linear");
  notices = builder.continuity(snap());
  assert(std::any_of(notices.begin(), notices.end(), [&](auto &n) {
    return n.key == chain_notice.key && !n.exception_note.empty();
  }));
  builder.move_unit(snap(), c.id, application::BuilderOrder::Narrative, 1);
  notices = builder.continuity(snap());
  assert(std::any_of(notices.begin(), notices.end(), [](auto &n) {
    return n.key.starts_with("stale-exception:");
  }));
  refuses([&] {
    builder.set_exception(snap(), chain_notice,
                          "Não justificar evidência antiga");
  });
  auto ca = s.structures().create_link(structure, c.id, a.id,
                                       project::NarrativeLinkKind::Precedes,
                                       "C antes de A");
  builder.set_dependency_scope(snap(), ca.id, "narrative");
  notices = builder.continuity(snap());
  assert(std::any_of(notices.begin(), notices.end(),
                     [](auto &n) { return n.kind == "cycle"; }));
  builder.set_dependency_scope(snap(), ca.id, "writing");
  notices = builder.continuity(snap());
  assert(std::none_of(notices.begin(), notices.end(),
                      [](auto &n) { return n.kind == "cycle"; }));
  auto local = s.narrative().create_entity(
           "00000000-0000-4000-9000-000000000002", "Porto"),
       other = s.narrative().create_entity(
           "00000000-0000-4000-9000-000000000002", "Montanha"),
       person = s.narrative().create_entity(
           "00000000-0000-4000-9000-000000000001", "Lia");
  s.structures().add_entity_to_unit(a.id, person.id, "Menção");
  assert(snap().presences.empty());
  builder.declare_presence(snap(), a.id, person.id, true);
  builder.declare_presence(snap(), b.id, person.id, true);
  builder.set_location(snap(), a.id, local.id);
  builder.set_location(snap(), b.id, other.id);
  auto calendar = s.worldbuilding().create_calendar(
      project::calendar_template("gregorian"));
  auto axis = s.planning().create_time_axis("Fatos");
  auto date = [&](const std::string &kind, const std::string &id, int day,
                  const std::string &meaning) {
    project::TemporalReference ref;
    ref.calendar_id = calendar.id;
    ref.axis_id = axis.id;
    ref.label = "Dia " + std::to_string(day);
    ref.year = 2050;
    ref.month = 1;
    ref.day = day;
    ref = s.worldbuilding().save_temporal_reference(ref);
    project::TemporalAttachment at;
    at.reference_id = ref.id;
    at.kind = kind;
    at.target_id = id;
    at.role = "Data explícita";
    at.meaning = meaning;
    s.worldbuilding().attach_date(at, ref,
                                  s.worldbuilding().temporal_target(kind, id));
    return ref;
  };
  auto adate = date("unit", a.id, 1, "occurrence"),
       bdate = date("unit", b.id, 1, "occurrence");
  date("entity", person.id, 2, "birth");
  builder.set_dependency_scope(snap(), ab.id, "chronology");
  notices = builder.continuity(snap());
  assert(std::any_of(notices.begin(), notices.end(), [](auto &n) {
    return n.kind == "presence" &&
           n.title.find("nascimento") != std::string::npos;
  }));
  assert(std::any_of(notices.begin(), notices.end(), [](auto &n) {
    return n.title.find("sobreposição") != std::string::npos;
  }));
  assert(std::any_of(notices.begin(), notices.end(), [](auto &n) {
    return n.kind == "insufficient" &&
           n.title.find("precedência") != std::string::npos;
  }));
  auto previous_date = bdate;
  bdate.day = 3;
  bdate = s.worldbuilding().save_temporal_reference(
      bdate, previous_date, s.worldbuilding().temporal_attachments(bdate.id));
  const auto day = project::calendar_ordinal(calendar, {2050, 1, 1}, 0);
  const auto p1 = s.planning().create_time_point(axis.id, day - 1, "Antes").id;
  const auto p2 = s.planning().create_time_point(axis.id, day + 1, "Depois").id;
  const auto canonical_presence = s.planning().create_presence(
      person.id, other.id, p1, p2, "Declaração canônica");
  assert(!canonical_presence.id.empty());
  notices = builder.continuity(snap());
  assert(std::any_of(notices.begin(), notices.end(), [](auto &n) {
    return n.kind == "insufficient" &&
           n.title.find("correlação explícita") != std::string::npos;
  }));
  s.worldbuilding().assign_calendar(axis.id, calendar.id, 0, std::nullopt);
  notices = builder.continuity(snap());
  assert(std::any_of(notices.begin(), notices.end(), [](auto &n) {
    return n.kind == "presence" &&
           n.title.find("presença canônica") != std::string::npos;
  }));
  const auto explicit_base = *s.worldbuilding().binding(axis.id);
  const auto julian =
      s.worldbuilding().create_calendar(project::calendar_template("julian"));
  s.worldbuilding().adopt_calendar(
      {{}, axis.id, julian.id, day + 10, 0, "coexist"}, explicit_base, {});
  s.worldbuilding().adopt_calendar(
      {{}, axis.id, calendar.id, day + 20, 100, "coexist"}, explicit_base,
      s.worldbuilding().adoptions(axis.id));
  notices = builder.continuity(snap());
  assert(std::none_of(notices.begin(), notices.end(), [](auto &n) {
    return n.kind == "presence" &&
           n.title.find("nascimento") != std::string::npos;
  }));
  assert(std::any_of(notices.begin(), notices.end(), [](auto &n) {
    return n.kind == "insufficient" &&
           n.evidence.find("correlação ambígua") != std::string::npos;
  }));
  const auto another_axis =
      s.planning().create_time_axis("Outro mundo sem equivalência").id;
  auto disconnected = adate;
  disconnected.id.clear();
  disconnected.token.clear();
  disconnected.axis_id = another_axis;
  disconnected = s.worldbuilding().save_temporal_reference(disconnected);
  const auto attachments = s.worldbuilding().temporal_attachments(adate.id);
  for (const auto &at : attachments)
    if (at.kind == "unit" && at.target_id == a.id)
      s.worldbuilding().remove_date_attachment(at);
  project::TemporalAttachment disconnected_link;
  disconnected_link.kind = "unit";
  disconnected_link.target_id = a.id;
  disconnected_link.reference_id = disconnected.id;
  disconnected_link.role = "Data em outro eixo";
  disconnected_link.meaning = "occurrence";
  s.worldbuilding().attach_date(
      disconnected_link, disconnected,
      s.worldbuilding().temporal_target("unit", a.id));
  adate = disconnected;
  notices = builder.continuity(snap());
  assert(std::none_of(notices.begin(), notices.end(), [&](auto &n) {
    return n.unit_id == a.id && n.kind == "presence" &&
           n.title.find("nascimento") != std::string::npos;
  }));
  // Rich semantic layers retain meaning and external canonical identities,
  // while every document-owned identity and internal relation is remapped.
  auto e = s.structures().create_unit(structure, "", "Camadas", "", "", "").id;
  auto rich = s.writing().create_document("Fonte em camadas");
  rich.content = "Água 🙂 em duas perspectivas.";
  rich.perspective = "Declaração livre";
  rich.entity_references.push_back(
      {project::new_uuid(), person.id, std::nullopt, "Referência declarada"});
  auto types = s.writing().semantic_types();
  const auto narration =
      *project::find_annotation_type(types, "inde.semantic.narration", 1);
  using S = application::TextSemanticService;
  rich = S::create(rich, narration, 0, 6, {});
  rich = S::create(rich, narration, 8, 12, {});
  rich.entity_references.front().anchor_id = rich.anchors.front().id;
  rich = S::relate(rich, rich.semantics.annotations.front().id,
                   rich.semantics.annotations.back().id,
                   "inde.semantic.reference");
  auto external = s.writing().create_document("Alvo externo preservado");
  external.content = "Outro texto";
  external = S::create(external, narration, 0, 5, {});
  external = s.writing().update_document(external);
  rich = S::relate(rich, rich.semantics.annotations.back().id,
                   external.semantics.annotations.front().id,
                   "inde.semantic.callback");
  rich = S::bind_date(rich, rich.semantics.annotations.front().id, adate.id,
                      "scene");
  rich = S::bind_fictional_time(rich, rich.semantics.annotations.front().id,
                                axis.id, p1, p2, "Contexto canônico declarado");
  rich = s.writing().update_document(rich);
  builder.attach_anchor(snap(), e, rich, rich.anchors.front().id);
  auto er = realization(e);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(
            s.current()->path()));
    const auto revision = snap().revision;
    const auto count = s.writing().document_count();
    db.execute("CREATE TRIGGER fail_independent_document BEFORE INSERT ON "
               "documents BEGIN SELECT RAISE(ABORT,'QA atomic document');END");
    refuses([&] {
      builder.make_independent(snap(), er.id, "Não gravar cópia parcial");
    });
    assert(snap().revision == revision &&
           s.writing().document_count() == count);
    db.execute("DROP TRIGGER fail_independent_document");
  }
  builder.make_independent(snap(), er.id, "Camadas independentes");
  const auto richcopy = *s.writing().document(realization(e).document_id);
  assert(richcopy.content == rich.content &&
         richcopy.perspective == rich.perspective);
  assert(richcopy.entity_references.front().entity_id == person.id &&
         richcopy.entity_references.front().id !=
             rich.entity_references.front().id);
  assert(richcopy.semantics.annotations.size() == 2 &&
         richcopy.semantics.relations.size() == 2);
  assert(richcopy.semantics.annotations.front().id !=
         rich.semantics.annotations.front().id);
  assert(richcopy.semantics.annotations.front().anchor_id ==
         richcopy.anchors.front().id);
  assert(richcopy.semantics.relations.front().source_annotation_id ==
         richcopy.semantics.annotations.front().id);
  assert(richcopy.semantics.relations.front().target_annotation_id ==
         richcopy.semantics.annotations.back().id);
  assert(richcopy.semantics.relations.back().target_annotation_id ==
         external.semantics.annotations.front().id);
  assert(richcopy.semantics.annotations.front().dates.front().reference_id ==
             adate.id &&
         richcopy.semantics.annotations.front().dates.front().id !=
             rich.semantics.annotations.front().dates.front().id);
  assert(richcopy.semantics.annotations.front()
                 .fictional_time.front()
                 .fictional_start_point_id == p1 &&
         richcopy.semantics.annotations.front().fictional_time.front().id !=
             rich.semantics.annotations.front().fictional_time.front().id);
  s.save_as(root / "Cópia.inde");
  s.close();
  s.open(root / "Cópia.inde");
  assert(!snap().realizations.empty() && snap().presences.size() == 2 &&
         !builder.continuity(snap()).empty());
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(
            s.current()->path()));
    assert(db.query_text("PRAGMA integrity_check") == "ok" &&
           !db.prepare("PRAGMA foreign_key_check").step());
    for (const auto *table :
         {"narrative_text_targets", "narrative_text_decisions",
          "narrative_continuity_exceptions", "narrative_unit_presence_intents"})
      assert(db.query_text("SELECT DISTINCT project_id FROM " +
                           std::string(table)) ==
             s.current()->manifest().project_id);
  }
  s.close();
  s.create(root / "Migração.inde", "Migração QA");
  s.close();
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(root /
                                                              "Migração.inde"));
    db.execute(
        "DROP TABLE narrative_text_targets;DROP TABLE "
        "narrative_text_decisions;DROP TABLE "
        "narrative_continuity_exceptions;DROP TABLE "
        "narrative_unit_presence_intents;DROP INDEX "
        "compositions_identity_owner;DELETE FROM schema_migrations WHERE "
        "version=28;CREATE TRIGGER fail28 BEFORE INSERT ON schema_migrations "
        "WHEN NEW.version=28 BEGIN SELECT RAISE(ABORT,'QA rollback');END");
    refuses([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 27);
    assert(db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='narrative_text_targets'") == 0);
    db.execute("DROP TRIGGER fail28");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) == 28);
  }
  std::filesystem::remove_all(root);
  std::cout
      << "PASS realization: Unicode anchors, sharing, explicit independent "
         "prose, forked occurrences, reconciliation, author review, transitive "
         "authority chains, cycles, presence warnings, exceptions, removal "
         "guidance, undo, Save As, restart and schema28 rollback\n";
}

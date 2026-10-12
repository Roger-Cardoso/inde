#include "inde/application/editor_interaction.hpp"
#include "inde/application/document_save_coordinator.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include <cassert>
#include <iostream>
using namespace inde;
template <class F> void rejects(F f) {
  bool failed{};
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
}
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-editor-interaction-" + project::new_uuid());
  std::filesystem::create_directories(root);
  setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
  application::ProjectService s;
  s.create(root / "project.inde", "Interação QA");
  const auto path = s.current()->path();
  const auto types = s.narrative().entity_types();
  auto type = [&](const std::string &key) {
    return *std::find_if(types.begin(), types.end(),
                         [&](const auto &t) { return t.key == key; });
  };
  const auto now = project::utc_now();
  auto d = s.writing().create_document("Trecho Unicode");
  d.content = "Água 🙂 Marcio veio a falecer. Informação.";
  d.formatting = {{project::DocumentTextStyle::Bold, 7, 13, {}}};
  d = s.writing().update_document(d);
  assert(application::selected_text(d, {7, 13}) == "Marcio");
  rejects([&] { (void)application::selected_text(d, {7, 7}); });
  rejects([&] { (void)application::selected_text(d, {0, 1000}); });
  project::TextEntityCreation p{{project::new_uuid(), type("character").id,
                                 "Marcio", "Médico de Véspera", now, now},
                                type("character"),
                                false};
  // Reproduce the real autosave -> external compound save -> autosave handoff.
  application::DocumentSaveCoordinator worker;
  worker.submit(s.writing().save_snapshot(d, d, 1, 1));
  worker.drain();
  auto acknowledgment = worker.take_latest();
  assert(acknowledgment && acknowledgment->error.empty() &&
         acknowledgment->persisted);
  d = *acknowledgment->persisted;
  auto linked =
      application::link_text_entity(d, {7, 13}, p.entity.id, p.entity.name);
  assert(linked.content == d.content && linked.entity_references.empty() &&
         linked.semantics.annotations.size() == 1);
  auto saved = s.writing().create_entity_from_text(linked, d, p);
  worker.clear(); // adopt the successful compound save at the UI boundary
  worker.submit(s.writing().save_snapshot(saved, saved, 1, 2));
  worker.drain();
  acknowledgment = worker.take_latest();
  assert(acknowledgment && acknowledgment->error.empty() &&
         acknowledgment->persisted);
  saved = *acknowledgment->persisted;
  assert(s.narrative().entity(p.entity.id));
  assert(saved.formatting.size() == 1 &&
         saved.formatting.front().style == d.formatting.front().style &&
         saved.formatting.front().start_offset == 7 &&
         saved.formatting.front().end_offset == 13);
  assert(project::same_document_state(*s.writing().document(d.id), saved));
  assert(application::entities_in_text(saved, {7, 13}) ==
         std::set<std::string>{p.entity.id});
  assert(application::entities_in_text(saved, {0, 6}).empty());
  assert(project::same_document_state(
      application::link_text_entity(saved, {7, 13}, p.entity.id, "Marcio"),
      saved));
  persistence::EntityQuery eq;
  eq.document_id = d.id;
  assert(s.narrative().entity_count(eq) == 1);
  eq.require_document_reference = true;
  assert(s.narrative().entities(eq).size() == 1);
  persistence::DocumentQuery dq;
  dq.entity_id = p.entity.id;
  assert(s.writing().document_count(dq) == 1 &&
         s.writing().document_summaries(dq).front().entity_reference_count ==
             1);
  // Atomic insert + failed semantic FK rolls back new entity, type and change
  // log.
  project::EntityType custom{project::new_uuid(),
                             "guilda-texto",
                             "Guilda",
                             "Tipo autoral",
                             false,
                             now,
                             now};
  project::TextEntityCreation newp{
      {project::new_uuid(), custom.id, "Ordem da Aurora", "", now, now},
      custom,
      true};
  auto bad = application::link_text_entity(saved, {0, 4}, newp.entity.id,
                                           newp.entity.name);
  bad = application::materialize_entity(bad, {0, 4}, project::new_uuid(),
                                        "materialization");
  const auto count = s.narrative().entity_count();
  const auto changes = s.narrative().change_log().size();
  rejects([&] { (void)s.writing().create_entity_from_text(bad, saved, newp); });
  assert(s.narrative().entity_count() == count &&
         !s.narrative().entity(newp.entity.id));
  assert(s.narrative().entity_types().size() == types.size() &&
         s.narrative().change_log().size() == changes);
  assert(project::same_document_state(*s.writing().document(d.id), saved));
  auto good = application::link_text_entity(saved, {0, 4}, newp.entity.id,
                                            newp.entity.name);
  auto unresolved_creation = good;
  unresolved_creation.semantics.anchors.back().state =
      project::AnchorResolution::Ambiguous;
  rejects([&] {
    (void)s.writing().create_entity_from_text(unresolved_creation, saved, newp);
  });
  assert(!s.narrative().entity(newp.entity.id));
  auto external = saved;
  external.title = "Mudança externa";
  external = s.writing().update_document(external);
  rejects(
      [&] { (void)s.writing().create_entity_from_text(good, saved, newp); });
  assert(!s.narrative().entity(newp.entity.id));
  saved = external;
  good = application::link_text_entity(saved, {0, 4}, newp.entity.id,
                                       newp.entity.name);
  saved = s.writing().create_entity_from_text(good, saved, newp);
  assert(s.narrative().entity_types().size() == types.size() + 1);
  rejects([&] {
    (void)s.writing().create_entity_from_text(saved, saved, newp);
  }); // insert only, never overwrites existing entity
  auto event =
      s.narrative().create_entity(type("event").id, "Falecimento de Marcio");
  auto info =
      s.narrative().create_entity(type("information").id, "Causa incerta",
                                  "A causa da morte não foi determinada.");
  saved = application::materialize_entity(
      saved, {14, 28}, event.id, "materialization", "Trecho do narrador");
  saved = application::materialize_entity(saved, {29, 39}, info.id, "evidence");
  saved = s.writing().update_document(saved);
  dq.entity_id = event.id;
  assert(s.writing().document_count(dq) == 1);
  eq.document_id = d.id;
  assert(s.narrative().entity_count(eq) == 4);
  auto semantic_types = s.writing().semantic_types();
  saved = application::TextSemanticService::create(
      saved,
      *project::find_annotation_type(semantic_types, "inde.semantic.narration",
                                     1),
      0, 28, {});
  saved = application::TextSemanticService::create(
      saved,
      *project::find_annotation_type(semantic_types,
                                     "inde.semantic.description", 1),
      0, 6, {});
  saved = application::TextSemanticService::create(
      saved,
      *project::find_annotation_type(semantic_types, "inde.semantic.narration",
                                     1),
      0, 6, {});
  saved = s.writing().update_document(saved);
  const auto distribution =
      application::semantic_distribution(saved, semantic_types, {0, 39});
  assert(distribution.covered == 38 && distribution.overlapping == 26 &&
         distribution.entity_ids.size() == 4);
  auto narration = std::find_if(
      distribution.rows.begin(), distribution.rows.end(),
      [](const auto &r) { return r.key == "inde.semantic.narration"; });
  assert(narration->characters == 28 &&
         narration->markings == 2); // union, not sum
  const auto selected =
      application::semantic_distribution(saved, semantic_types, {7, 13});
  assert(selected.covered == 6 && selected.entity_ids.size() == 1);
  rejects([&] {
    (void)application::semantic_distribution(saved, semantic_types, {1, 200});
  });
  auto edited =
      application::replace_selection({saved, {13, 7}}, 7, 13, "Outro nome");
  assert(application::entities_in_text(edited.document, {7, 17})
             .count(p.entity.id) == 0);
  const auto unresolved = application::semantic_distribution(
      edited.document, semantic_types,
      {0, project::utf8_character_count(edited.document.content)});
  auto mention = std::find_if(
      unresolved.rows.begin(), unresolved.rows.end(),
      [](const auto &r) { return r.key == "inde.semantic.entity-mention"; });
  assert(mention->unresolved == 1);
  // Undo textual binding doesn't remove the entity/type from canonical
  // planning.
  application::DocumentEditSession history;
  history.open({d, {13, 7}});
  history.commit({linked, {13, 7}}, "Vincular");
  assert(history.undo()->document.semantics.annotations.empty() &&
         s.narrative().entity(p.entity.id));
  assert(history.redo()->document.semantics.annotations.size() == 1);
  // New relation type and relation share a transaction; stale snapshots refuse.
  project::RelationType rt{project::new_uuid(),
                           "influencia-texto",
                           "influencia",
                           "é influenciado por",
                           {},
                           project::RelationDirectionality::Directed,
                           false,
                           now,
                           now};
  project::TextRelationCreation rel{p.entity,
                                    event,
                                    rt,
                                    {project::new_uuid(),
                                     rt.id,
                                     p.entity.id,
                                     event.id,
                                     "Relação declarada",
                                     now,
                                     now,
                                     {},
                                     {},
                                     {}},
                                    true};
  auto stale = rel;
  stale.target.summary = "Não é o estado salvo";
  rejects([&] { (void)s.narrative().create_relation_from_text(stale); });
  assert(s.narrative().relation_types().empty());
  auto canonical = s.narrative().create_relation_from_text(rel);
  assert(canonical.source_entity_id == p.entity.id &&
         s.narrative().relations().size() == 1);
  auto invalid = rel;
  invalid.type.id = project::new_uuid();
  invalid.type.key = "outra-relacao";
  invalid.relation.id = canonical.id;
  invalid.relation.relation_type_id = invalid.type.id;
  rejects([&] { (void)s.narrative().create_relation_from_text(invalid); });
  assert(s.narrative().relation_types().size() == 1);
  auto symmetric = rel;
  symmetric.type = {project::new_uuid(),
                    "alianca-texto",
                    "é aliado de",
                    {},
                    {},
                    project::RelationDirectionality::Symmetric,
                    false,
                    now,
                    now};
  symmetric.relation.id = project::new_uuid();
  symmetric.relation.relation_type_id = symmetric.type.id;
  auto sym = s.narrative().create_relation_from_text(symmetric);
  assert(sym.source_entity_id < sym.target_entity_id);
  assert(application::project_inline_query("@Marcio")->search == "Marcio");
  assert(application::project_inline_query("@informação:causa")->type_key ==
         "information");
  assert(!application::project_inline_query("@inde") &&
         !application::project_inline_query("@desconhecido:a") &&
         !application::project_inline_query("texto"));
  auto blank=d;blank.content="";blank.formatting.clear();blank.anchors.clear();blank.entity_references.clear();blank.semantics={};
  const auto empty=application::semantic_distribution(blank,semantic_types,{0,0});
  assert(empty.characters==0 && empty.covered==0 && empty.overlapping==0 && empty.entity_ids.empty());
  blank=application::TextSemanticService::create_document_perspective(blank,
      {{"person",{project::SemanticFieldKind::Enum,{"third_person"}}},
       {"focalization",{project::SemanticFieldKind::Enum,{"external"}}}});
  const auto global=application::semantic_distribution(blank,semantic_types,{0,0});
  auto global_row=std::find_if(global.rows.begin(),global.rows.end(),[](const auto &r){return r.key=="inde.semantic.perspective";});
  assert(global.covered==0 && global_row->global==1 && global_row->characters==0);
  auto incomplete=application::TextSemanticService::create(d,*project::find_annotation_type(semantic_types,"inde.semantic.dialogue",1),0,6,{});
  const auto incomplete_report=application::semantic_distribution(incomplete,semantic_types,{0,6});
  auto dialogue_row=std::find_if(incomplete_report.rows.begin(),incomplete_report.rows.end(),[](const auto &r){return r.key=="inde.semantic.dialogue";});
  assert(dialogue_row->incomplete==1 && dialogue_row->characters==6);
  auto plain=application::semantic_distribution(d,semantic_types,{0,6});
  assert(plain.covered==0 && plain.entity_ids.empty());
  auto corrupt=d;corrupt.content=std::string("\xff",1);rejects([&]{(void)application::selected_text(corrupt,{0,1});});
  s.close();
  s.open(path);
  assert(project::same_document_state(*s.writing().document(d.id), saved));
  persistence::SqliteDatabase db(path / "data/project.sqlite3");
  assert(!db.prepare("PRAGMA foreign_key_check").step());
  assert(db.query_text("PRAGMA integrity_check") == "ok");
  assert(db.query_integer("SELECT count(*) FROM event_occurrences") == 0);
  std::cout << "PASS: criação textual atômica, Unicode, vínculos, "
               "materialização, projeções, conflitos, rollback, distribuição, "
               "relações e reabertura\n";
}

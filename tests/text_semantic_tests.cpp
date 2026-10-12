#include "schema17_downgrade.hpp"
#include "../src/persistence/recovery_codec.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/document_recovery_store.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace inde;
using S = application::TextSemanticService;
using K = project::SemanticFieldKind;
template <class F> void rejects(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
}
void test(application::ProjectService &service,
          const std::filesystem::path &root) {
  auto ts = service.writing().semantic_types();
  assert(ts.size() == project::builtin_annotation_types().size());
  for (auto &t : ts) {
    project::validate(t);
    assert(t.system);
    assert(persistence::decode_semantic_type(
               persistence::encode_semantic_type(t)) == t);
  }
  auto type = [&](const std::string &key) {
    return *project::find_annotation_type(ts, "inde.semantic." + key, 1);
  };
  auto character_types = service.narrative().entity_types();
  auto character = std::find_if(character_types.begin(), character_types.end(),
                                [](auto &t) { return t.key == "character"; });
  assert(character != character_types.end());
  auto entity = service.narrative().create_entity(character->id,
                                                  "QA personagem semântica");
  auto location_type =
      std::find_if(character_types.begin(), character_types.end(),
                   [](auto &t) { return t.key == "location"; });
  assert(location_type != character_types.end());
  auto place = service.narrative().create_entity(location_type->id,
                                                 "QA local semântico");
  auto d = service.writing().create_document("QA núcleo semântico");
  d.content = "Água 🙂 fala e pensamento. Água retorna.";
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 4}};
  d = service.writing().update_document(d);
  auto original = d;
  d = S::create(d, type("narration"), 0, 10, {{"kind", {K::Enum, {"action"}}}});
  auto narration = d.semantics.annotations.back().id;
  d = S::create(d, type("description"), 0, 6,
                {{"targets", {K::Entity, {entity.id, place.id}}},
                 {"aspects", {K::Enum, {"appearance", "sensory"}}}});
  d = S::create(d, type("dialogue"), 7, 11,
                {{"speaker", {K::Entity, {entity.id}}},
                 {"acts", {K::Enum, {"question", "request"}}}});
  auto dialogue = d.semantics.annotations.back().id;
  d = S::create(d, type("thought"), 7, 22, {});
  assert(d.semantics.annotations.back().status == "incomplete");
  auto thought = d.semantics.annotations.back().id;
  d = S::update(d, thought,
                {{"thinker", {K::Entity, {entity.id}}},
                 {"form", {K::Enum, {"internal_monologue"}}}},
                ts);
  assert(d.semantics.annotations.back().status == "active");
  d = S::create(d, type("marked-passage"), 25, 37,
                {{"category", {K::Enum, {"callback"}}},
                 {"note", {K::Text, {"Marca autoral 🙂"}}}});
  assert(d.content == original.content && d.formatting.size() == 1 &&
         d.formatting[0].start_offset == 0 && d.formatting[0].end_offset == 4 &&
         d.formatting[0].style == project::DocumentTextStyle::Bold);
  assert(d.semantics.annotations.size() == 5 &&
         d.semantics.anchors.size() == 5);
  rejects([&] {
    S::create(d, type("dialogue"), 0, 4,
              {{"speaker", {K::Entity, {entity.id, place.id}}}});
  });
  rejects([&] {
    S::create(d, type("narration"), 0, 4, {{"kind", {K::Enum, {"inventado"}}}});
  });
  rejects([&] { S::create(d, type("thought"), 1, 1, {}); });
  auto wrong = S::create(d, type("dialogue"), 0, 4,
                         {{"speaker", {K::Entity, {place.id}}}});
  rejects([&] { static_cast<void>(service.writing().update_document(wrong)); });
  auto missing = S::create(d, type("dialogue"), 0, 4,
                           {{"speaker", {K::Entity, {project::new_uuid()}}}});
  rejects([&] { static_cast<void>(service.writing().update_document(missing)); });
  d = S::relate(d, dialogue, narration, "inde.semantic.response-to");
  rejects([&] { S::remove(d, narration); });
  d = service.writing().update_document(d);
  assert(project::same_document_state(d, *service.writing().document(d.id)));
  auto persisted = d;
  rejects([&] { service.narrative().delete_entity(entity.id); });
  assert(service.narrative().entity(entity.id));
  persistence::SemanticUseQuery q;
  q.document_id = d.id;
  q.limit = 2;
  auto page = service.writing().semantic_uses(q);
  assert(page.total == 5 && page.rows.size() == 2);
  q.offset = 2;
  assert(service.writing().semantic_uses(q).rows.size() == 2);
  q.offset = 4;
  assert(service.writing().semantic_uses(q).rows.size() == 1);
  q.offset = 0;
  q.entity_id = entity.id;
  assert(service.writing().semantic_uses(q).total == 3);
  q.entity_id.reset();
  q.category = "callback";
  assert(service.writing().semantic_uses(q).total == 1);
  q.category.reset();
  q.type_key = type("dialogue").key;
  assert(service.writing().semantic_uses(q).total == 1);
  q.limit = 0;
  rejects([&] { static_cast<void>(service.writing().semantic_uses(q)); });
  auto metadata = S::update(d, dialogue,
                            {{"speaker", {K::Entity, {entity.id}}},
                             {"medium", {K::Enum, {"spoken"}}}},
                            ts);
  application::DocumentEditSession history;
  history.open({d, {7, 11}});
  assert(history.commit({metadata, {7, 11}}, "campos"));
  assert(history.dirty());
  auto undone = history.undo();
  assert(undone && project::same_document_state(d, undone->document));
  auto redone = history.redo();
  assert(redone && project::same_document_state(metadata, redone->document));
  assert(history.history_text_bytes() > 0);
  auto inserted = application::replace_selection({d, {0, 0}}, 0, 0, "🙂 ");
  assert(inserted.document.semantics.anchors[0].revision >
         d.semantics.anchors[0].revision);
  application::SemanticAnchorEngine::inspect(inserted.document);
  assert(inserted.document.semantics.anchors[0].state ==
         project::AnchorResolution::Resolved);
  assert(inserted.document.anchors[0].start_offset == 0 &&
         inserted.document.anchors[0].end_offset == 12);
  auto deleted = application::replace_selection({d, {7, 11}}, 7, 11, "");
  auto evidence =
      std::find_if(deleted.document.semantics.anchors.begin(),
                   deleted.document.semantics.anchors.end(), [&](auto &e) {
                     return e.anchor_id == d.semantics.annotations[2].anchor_id;
                   });
  assert(evidence->state == project::AnchorResolution::Orphaned);
  assert(deleted.document.semantics.annotations[2].id == dialogue);
  auto repaired = S::reanchor(deleted.document, dialogue, 0, 4);
  auto repaired_e = std::find_if(
      repaired.semantics.anchors.begin(), repaired.semantics.anchors.end(),
      [&](auto &e) { return e.anchor_id == evidence->anchor_id; });
  assert(repaired_e->state == project::AnchorResolution::Resolved &&
         repaired.semantics.annotations[2].id == dialogue);
  auto replaced =
      application::replace_selection({d, {7, 11}}, 7, 11, "outra fala");
  auto amb =
      std::find_if(replaced.document.semantics.anchors.begin(),
                   replaced.document.semantics.anchors.end(), [&](auto &e) {
                     return e.anchor_id == d.semantics.annotations[2].anchor_id;
                   });
  assert(amb->state == project::AnchorResolution::Ambiguous);
  auto plan = application::find_text(d.content, {"Água", true, false});
  auto all = application::replace_text({d, {0, 0}}, plan, "Rio");
  assert(all.document.semantics.annotations.size() == 5);
  project::validate(all.document);
  auto mismatch = d;
  mismatch.content.replace(0, 2, "X");
  application::SemanticAnchorEngine::inspect(mismatch);
  assert(mismatch.semantics.anchors[0].state ==
         project::AnchorResolution::Ambiguous);
  auto contextual = d;
  contextual.content.back() = '!';
  rejects([&] { service.writing().validate_semantic_document(contextual); });
  auto contextual_edit =
      application::replace_selection({contextual, {0, 0}}, 0, 0, "prefixo ");
  assert(contextual_edit.document.semantics.anchors[0].state ==
         project::AnchorResolution::Ambiguous);
  auto encoded = persistence::encode_semantics(d.semantics);
  assert(project::same_semantics(d.semantics,
                                 persistence::decode_semantics(encoded)));
  auto future = persistence::json::parse(encoded).as_object();
  future["version"] = 3;
  rejects([&] {
    persistence::decode_semantics(persistence::json::serialize(future));
  });
  rejects([&] {
    persistence::decode_semantics(std::string(100, '[') +
                                  std::string(100, ']'));
  });
  auto custom = project::AnnotationTypeDefinition{
      "project.qa",
      "Tipo autoral",
      "Definição explícita",
      1,
      false,
      true,
      {{"weight", "Peso", K::Integer, true, false, {}, {}},
       {"enabled", "Ativa", K::Boolean, false, false, {}, {}},
       {"notes", "Valores autorais", K::Text, false, true, {}, {}}}};
  service.writing().register_semantic_type(custom);
  rejects([&] { service.writing().register_semantic_type(custom); });
  auto protected_type = custom;
  protected_type.key = "inde.semantic.custom";
  rejects([&] { service.writing().register_semantic_type(protected_type); });
  d = S::create(d, custom, 1, 4,
                {{"weight", {K::Integer, {"42"}}},
                 {"enabled", {K::Boolean, {"true"}}},
                 {"notes", {K::Text, {"um", "dois\ntrês"}}}});
  auto customid = d.semantics.annotations.back().id;
  d.semantics.annotations.back().fields["future"] = {K::Text, {"preservar"}};
  d.semantics.annotations.back().opaque_payload = "{\"futuro\": \"🙂\"}";
  d = service.writing().update_document(d);
  d = S::update(d, customid, {{"weight", {K::Integer, {"7"}}}},
                service.writing().semantic_types());
  assert(d.semantics.annotations.back().fields.contains("future") &&
         !d.semantics.annotations.back().opaque_payload.empty());
  auto unknown = d;
  unknown.semantics.annotations.back().type_key = "extension.unavailable";
  unknown.semantics.annotations.back().type_version = 77;
  unknown = service.writing().update_document(unknown);
  assert(!project::semantic_diagnostics(unknown, ts).empty());
  rejects([&] { S::update(unknown, customid, {}, ts); });
  assert(service.writing()
             .document(d.id)
             ->semantics.annotations.back()
             .opaque_payload ==
         unknown.semantics.annotations.back().opaque_payload);
  auto baseline = *service.writing().document(d.id);
  auto next = S::archive(baseline, dialogue, true);
  next = service.writing().update_document_if_unchanged(next, baseline);
  rejects([&] {
    static_cast<void>(service.writing().update_document_if_unchanged(baseline, baseline));
  });
  auto other = service.writing().create_document("QA origem entre Documentos");
  other.content = "Resposta explícita.";
  other = S::create(other, type("narration"), 0, 8, {});
  other = S::relate(other, other.semantics.annotations.front().id, customid,
                    "inde.semantic.callback");
  other = service.writing().update_document(other);
  auto attempted = S::remove(next, customid);
  rejects([&] { static_cast<void>(service.writing().update_document(attempted)); });
  assert(
      project::same_document_state(next, *service.writing().document(next.id)));
  rejects([&] { service.writing().delete_document(next.id); });
  auto owner = *service.current();
  project::RecoveryRecord record{owner.manifest().project_id,
                                 project::new_uuid(),
                                 3,
                                 2,
                                 {next, 1, 4},
                                 baseline,
                                 "pending",
                                 "",
                                 ""};
  auto recovery = persistence::recovery::encode(record);
  auto restored = persistence::recovery::decode(
      recovery, persistence::recovery::checksum(recovery));
  assert(project::same_document_state(next, restored.state.document));
  auto legacy = persistence::json::parse(recovery).as_object();
  legacy["version"] = 1;
  for (auto key : {"state", "baseline"}) {
    auto value = legacy.at(key).as_object();
    value.erase("semantics");
    value.erase("inline_commands");
    auto formats=value.at("formatting").as_array();
    for(auto &format:formats){auto span=format.as_object();span.erase("value");format=span;}
    value["formatting"]=formats;
    value["anchors"] = persistence::json::Value::Array{};
    value["references"] = persistence::json::Value::Array{};
    legacy[key] = value;
  }
  auto legacy_string = persistence::json::serialize(legacy);
  assert(persistence::recovery::decode(
             legacy_string, persistence::recovery::checksum(legacy_string))
             .state.document.semantics.annotations.empty());
  int pipefd[2];
  assert(pipe(pipefd) == 0);
  auto pid = fork();
  assert(pid >= 0);
  if (pid == 0) {
    close(pipefd[0]);
    persistence::RecoveryLease lease(owner, record.session_id);
    persistence::DocumentRecoveryStore{}.checkpoint(owner, record);
    char ready = 'y';
    assert(write(pipefd[1], &ready, 1) == 1);
    for (;;)
      pause();
  }
  close(pipefd[1]);
  char ready;
  assert(read(pipefd[0], &ready, 1) == 1 && ready == 'y');
  close(pipefd[0]);
  assert(kill(pid, SIGKILL) == 0);
  int status;
  waitpid(pid, &status, 0);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
  auto durable =
      persistence::DocumentRecoveryStore{}.load(owner, record.session_id);
  assert(durable &&
         project::same_document_state(next, durable->state.document));
  std::cout << "CHECK: real SIGKILL preserved typed semantic layers and "
               "relationships\n";
  auto oldpath = owner.path();
  auto clonepath = root / "semantic-save-as.inde";
  service.save_as(clonepath);
  assert(service.current()->manifest().project_id !=
         owner.manifest().project_id);
  assert(
      project::same_document_state(next, *service.writing().document(next.id)));
  assert(service.writing().semantic_types().size() ==
         project::builtin_annotation_types().size() + 1);
  service.close();
  service.open(oldpath);
  assert(
      project::same_document_state(next, *service.writing().document(next.id)));
  service.writing().delete_document(other.id);
  next = *service.writing().document(next.id);
  next = S::unrelate(next, next.semantics.relations.front().id);
  next = service.writing().update_document(next);
  service.writing().delete_document(next.id);
  service.narrative().delete_entity(entity.id);
  service.narrative().delete_entity(place.id);
  std::cout << "PASS: native roles, overlapping Unicode anchors, typed fields, "
               "required diagnostics, undo/redo, fingerprints, manual repair, "
               "immutable registry, unknown payload preservation, indexed "
               "uses, protected canonical entities, CAS, cross-document "
               "relations, journal v1/v2 and save-as identity\n";
}
int main(int argc, char **argv) {
  auto root = std::filesystem::temp_directory_path() /
              ("inde-semantic-" + project::new_uuid());
  std::filesystem::create_directories(root);
  setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
  if (argc == 3 && std::string(argv[1]) == "--prepare-review") {
    application::ProjectService service;
    service.open(argv[2]);
    auto types = service.writing().semantic_types();
    auto ets = service.narrative().entity_types();
    auto et = std::find_if(ets.begin(), ets.end(),
                           [](auto &t) { return t.key == "character"; });
    assert(et != ets.end());
    auto entity = service.narrative().create_entity(
        et->id, "Mara — exemplo sintético semântico");
    auto d = service.writing().create_document(
        "Leia primeiro — exemplos das camadas semânticas");
    d.content = "Mara observou a ponte sob a chuva.\n— Volto amanhã — disse "
                "Mara.\nE se a passagem estivesse fechada?\nNa pedra, uma "
                "marca antiga estava gravada.";
    auto add =
        [&](const std::string &type, const std::string &selection,
            const std::map<std::string, project::SemanticFieldValue> &fields) {
          auto t =
              project::find_annotation_type(types, "inde.semantic." + type, 1);
          assert(t);
          auto byte = d.content.find(selection);
          assert(byte != std::string::npos);
          auto start = project::utf8_character_count(d.content.substr(0, byte));
          d = S::create(d, *t, start,
                        start + project::utf8_character_count(selection),
                        fields);
          d.semantics.annotations.back().origin = "imported";
          d.semantics.annotations.back().source =
              "synthetic-fixture/native-semantic-kernel-2026-10-03";
          d.semantics.annotations.back().notes =
              "Exemplo sintético para revisão, não análise automática de texto "
              "real";
        };
    add("narration", "Mara observou a ponte sob a chuva.",
        {{"kind", {K::Enum, {"action"}}}});
    add("description", "ponte sob a chuva",
        {{"aspects", {K::Enum, {"architecture", "weather"}}}});
    add("dialogue", "— Volto amanhã — disse Mara.",
        {{"speaker", {K::Entity, {entity.id}}},
         {"medium", {K::Enum, {"spoken"}}},
         {"acts", {K::Enum, {"promise"}}}});
    add("thought", "E se a passagem estivesse fechada?",
        {{"thinker", {K::Entity, {entity.id}}},
         {"form", {K::Enum, {"internal_monologue"}}}});
    add("marked-passage", "uma marca antiga",
        {{"category", {K::Enum, {"clue"}}},
         {"note", {K::Text, {"Pista sintética"}}}});
    d = service.writing().update_document(d);
    std::cout << "REVIEW_EXAMPLE document=" << d.id
              << " annotations=" << d.semantics.annotations.size() << "\n";
    service.close();
    std::filesystem::remove_all(root);
    return 0;
  }
  {
    application::ProjectService service;
    if (argc == 3 && std::string(argv[1]) == "--fixture")
      service.open(argv[2]);
    else
      service.create(root / "fixture.inde", "QA semântica");
    test(service, root);
    service.close();
  }
  {
    persistence::SqliteDatabase db(root / "migration.sqlite3");
    persistence::SchemaMigrator migrator;
    migrator.migrate(db);
    downgrade_schema17_for_fixture(db);
    db.execute("DROP TABLE semantic_annotation_relations;DROP TABLE "
               "semantic_entity_bindings;DROP TABLE semantic_annotations;DROP "
               "TABLE document_semantics;DROP TABLE "
               "semantic_type_definitions;DELETE FROM schema_migrations WHERE "
               "version=16;CREATE TABLE document_semantics(fault INTEGER)");
    rejects([&] { migrator.migrate(db); });
    assert(migrator.current_version(db) == 15);
    assert(db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='semantic_type_definitions'") == 0);
    db.execute("DROP TABLE document_semantics");
    migrator.migrate(db);
    assert(migrator.current_version(db) == persistence::current_database_schema_version);
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    assert(!db.prepare("PRAGMA foreign_key_check").step());
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS: additive v16 migration rollback and integrity\n";
}

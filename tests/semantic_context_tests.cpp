#include "../src/persistence/recovery_codec.hpp"
#include "inde/application/document_edit_session.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/project/document_state.hpp"
#include "schema17_downgrade.hpp"
#include <cassert>
#include <iostream>
#include <set>
using namespace inde;
using S = application::TextSemanticService;
using K = project::SemanticFieldKind;
template <class F> void rejects(F f) {
  bool failed{};
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  assert(failed);
}
const auto perspective_fields =
    std::map<std::string, project::SemanticFieldValue>{
        {"person", {K::Enum, {"third_person"}}},
        {"focalization", {K::Enum, {"external"}}}};
std::string v1_payload(const project::DocumentSemantics &s) {
  auto root =
      persistence::json::parse(persistence::encode_semantics(s)).as_object();
  root["version"] = 1;
  auto annotations = root.at("annotations").as_array();
  for (auto &v : annotations) {
    auto a = v.as_object();
    a.erase("scope");
    a.erase("fictional_time");
    v = std::move(a);
  }
  root["annotations"] = std::move(annotations);
  return persistence::json::serialize(root, false);
}
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-semantic-context-" + project::new_uuid());
  std::filesystem::create_directories(root);
  setenv("XDG_CONFIG_HOME", (root / "config").c_str(), 1);
  const auto path = root / "project.inde";
  application::ProjectService service;
  service.create(path, "Contextos semânticos QA");
  auto types = service.writing().semantic_types();
  auto perspective =
      *project::find_annotation_type(types, "inde.semantic.perspective", 1);
  auto narration =
      *project::find_annotation_type(types, "inde.semantic.narration", 1);
  auto ets = service.narrative().entity_types();
  auto character = std::find_if(ets.begin(), ets.end(),
                                [](auto &t) { return t.key == "character"; });
  assert(character != ets.end());
  auto who = service.narrative().create_entity(character->id,
                                               "Focalizadora de contexto");
  auto d =
      service.writing().create_document("Documento com perspectiva e tempo");
  d.content = "Água 🙂 início. Meio da história. Último trecho.";
  d.perspective = "Declaração livre, não convertida";
  d.inline_commands = {{"@", "nota", "Nota", "Nota literal"}};
  d.formatting = {{project::DocumentTextStyle::Foreground, 0, 6, "#88aaff"}};
  d = S::create(d, narration, 0, 6, {});
  const auto local_id = d.semantics.annotations.front().id;
  auto local_fields = perspective_fields;
  local_fields["viewpoint"] = {K::Entity, {who.id}};
  d = S::create(d, perspective, 0, 6, local_fields);
  const auto local_perspective = d.semantics.annotations.back().id;
  d = service.writing().update_document(d);
  const auto legacy = d;
  auto source = service.writing().create_document("Fonte de relação legada");
  source.content = "Uma retomada.";
  source = S::create(source, narration, 0, 4, {});
  source = S::relate(source, source.semantics.annotations.front().id,
                     local_perspective, "inde.semantic.callback");
  source = service.writing().update_document(source);
  service.close();
  {
    persistence::SqliteDatabase db(path / "data/project.sqlite3");
    downgrade_schema18_for_fixture(db);
    assert(persistence::SchemaMigrator{}.current_version(db) == 17);
    auto q = db.prepare(
        "UPDATE document_semantics SET payload=? WHERE document_id=?");
    q.bind(1, v1_payload(d.semantics));
    q.bind(2, d.id);
    q.run();
    q.reset();
    q.bind(1, v1_payload(source.semantics));
    q.bind(2, source.id);
    q.run();
    const auto annotations =
        db.query_integer("SELECT count(*) FROM semantic_annotations");
    const auto entities =
        db.query_integer("SELECT count(*) FROM semantic_entity_bindings");
    const auto relations =
        db.query_integer("SELECT count(*) FROM semantic_annotation_relations");
    db.execute("CREATE TABLE semantic_fictional_time_bindings(fault INTEGER)");
    rejects([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 17);
    assert(db.query_integer("SELECT count(*) FROM semantic_annotations") ==
           annotations);
    assert(db.query_integer("SELECT count(*) FROM semantic_entity_bindings") ==
           entities);
    assert(db.query_integer(
               "SELECT count(*) FROM semantic_annotation_relations") ==
           relations);
    assert(!db.prepare("PRAGMA foreign_key_check").step());
    db.execute("DROP TABLE semantic_fictional_time_bindings");
  }
  service.open(path);
  assert(
      project::same_document_state(*service.writing().document(d.id), legacy));
  assert(project::same_document_state(*service.writing().document(source.id),
                                      source));
  d = *service.writing().document(d.id);
  d = S::create_document_perspective(d, perspective_fields);
  const auto global_id = d.semantics.annotations.back().id;
  assert(d.semantics.annotations.back().scope ==
             project::AnnotationScope::Document &&
         d.semantics.annotations.back().anchor_id.empty());
  assert(d.anchors.size() == legacy.anchors.size());
  rejects([&] { S::create_document_perspective(d, {}); });
  rejects([&] { S::reanchor(d, global_id, 0, 3); });
  assert(S::perspective_at(d, 2).effective_declaration == local_perspective);
  assert(S::perspective_at(d, 10).effective_declaration == global_id);
  auto overlapping = S::create(d, perspective, 1, 5, perspective_fields);
  auto pc = S::perspective_at(overlapping, 2);
  assert(pc.ambiguous && !pc.effective_declaration &&
         pc.interval_declarations.size() == 2);
  auto archived = S::archive(d, global_id, true);
  archived = S::create_document_perspective(archived, {});
  assert(S::perspective_at(archived, 10).incomplete &&
         !S::perspective_at(archived, 10).effective_declaration);
  rejects([&] { S::archive(archived, global_id, false); });
  auto erased = application::replace_selection(
                    {d, {0, project::utf8_character_count(d.content)}}, 0,
                    project::utf8_character_count(d.content), "")
                    .document;
  assert(S::perspective_at(erased, 0).effective_declaration == global_id);
  assert(erased.semantics.annotations.back().id == global_id &&
         erased.semantics.annotations.back().scope ==
             project::AnnotationScope::Document);
  auto empty =
      service.writing().create_document("Perspectiva em Documento vazio");
  empty = S::create_document_perspective(empty, perspective_fields);
  empty = service.writing().update_document(empty);
  assert(empty.content.empty() && empty.anchors.empty() &&
         empty.semantics.anchors.empty());
  auto axis = service.planning().create_time_axis("Eixo manual QA");
  auto other = service.planning().create_time_axis("Outro eixo QA");
  auto p0 = service.planning().create_time_point(axis.id, 0, "Ponto zero");
  auto p10 = service.planning().create_time_point(axis.id, 10, "Começo");
  auto p15 = service.planning().create_time_point(axis.id, 15, "Meio");
  auto p20 = service.planning().create_time_point(axis.id, 20, "Fim");
  auto p30 = service.planning().create_time_point(axis.id, 30, "Fora");
  auto foreign_point = service.planning().create_time_point(
      other.id, 10, "Mesmo ordinal em outro eixo");
  d = S::bind_fictional_time(d, global_id, axis.id, p0.id, {},
                             "Contexto global pontual");
  d = S::bind_fictional_time(d, local_id, axis.id, p10.id, p20.id,
                             "Período textual, não acontecimento");
  const auto binding_id =
      d.semantics.annotations.front().fictional_time.front().id;
  rejects(
      [&] { S::bind_fictional_time(d, local_id, axis.id, p10.id, p20.id); });
  auto invalid = S::bind_fictional_time(d, local_id, axis.id, foreign_point.id);
  rejects([&] { service.writing().validate_semantic_document(invalid); });
  invalid = S::bind_fictional_time(d, local_id, axis.id, p20.id, p10.id);
  rejects([&] { static_cast<void>(service.writing().update_document(invalid)); });
  invalid = S::bind_fictional_time(d, local_id, axis.id, project::new_uuid());
  rejects([&] { service.writing().validate_semantic_document(invalid); });
  d = service.writing().update_document(d);
  auto authorial = d;
  authorial.semantics = legacy.semantics;
  assert(project::same_document_state(authorial, legacy, false));
  persistence::SemanticUseQuery q;
  q.annotation_scope = project::AnnotationScope::Document;
  auto page = service.writing().semantic_uses(q);
  assert(page.total == 2);
  for (auto &row : page.rows)
    assert(row.scope == project::AnnotationScope::Document &&
           row.anchor_id.empty());
  q = {};
  q.fictional_axis_id = axis.id;
  assert(service.writing().semantic_uses(q).total == 2);
  for (const auto &p : {p10, p15, p20}) {
    q.fictional_point_id = p.id;
    page = service.writing().semantic_uses(q);
    assert(page.total == 1 && page.rows.front().annotation_id == local_id);
  }
  q.fictional_point_id = p0.id;
  page = service.writing().semantic_uses(q);
  assert(page.total == 1 &&
         page.rows.front().scope == project::AnnotationScope::Document);
  q.fictional_point_id = p30.id;
  assert(service.writing().semantic_uses(q).total == 0);
  q.fictional_point_id = foreign_point.id;
  rejects([&] { static_cast<void>(service.writing().semantic_uses(q)); });
  q.fictional_axis_id.reset();
  rejects([&] { static_cast<void>(service.writing().semantic_uses(q)); });
  rejects([&] { service.planning().delete_time_point(p10.id); });
  rejects([&] { service.planning().delete_time_axis(axis.id); });
  auto moved = p10;
  moved.ordinal = 25;
  rejects([&] { static_cast<void>(service.planning().update_time_point(moved)); });
  moved = p0;
  moved.axis_id = other.id;
  rejects([&] { static_cast<void>(service.planning().update_time_point(moved)); });
  assert(service.planning().time_point(p10.id)->ordinal == 10);
  source = S::relate(source, source.semantics.annotations.front().id, global_id,
                     "inde.semantic.reference");
  source = service.writing().update_document(source);
  persistence::SemanticRelationQuery rq;
  rq.annotation_id = global_id;
  auto rp = service.writing().semantic_relations(rq);
  assert(rp.total == 1 &&
         rp.rows.front().target.scope == project::AnnotationScope::Document);
  rq.direction = persistence::SemanticRelationDirection::Outgoing;
  rq.annotation_id = source.semantics.annotations.front().id;
  rp = service.writing().semantic_relations(rq);
  assert(rp.total == 2 && rp.rows.size() == 2);
  auto before = d;
  auto unbound = S::unbind_fictional_time(d, local_id, binding_id);
  application::DocumentEditSession history;
  history.open({d, {0, 6}});
  assert(history.commit({unbound, {0, 6}}, "Remover contexto temporal"));
  assert(project::same_document_state(history.undo()->document, d, false));
  assert(
      project::same_document_state(history.redo()->document, unbound, false));
  unbound = service.writing().update_document(unbound);
  d = service.writing().update_document_if_unchanged(before, unbound);
  rejects([&] {
    static_cast<void>(service.writing().update_document_if_unchanged(unbound, unbound));
  });
  project::RecoveryRecord record{service.current()->manifest().project_id,
                                 project::new_uuid(),
                                 3,
                                 2,
                                 {d, 0, 6},
                                 legacy,
                                 "pending",
                                 "",
                                 ""};
  auto encoded = persistence::recovery::encode(record);
  assert(persistence::json::parse(encoded).at("version").as_integer() == 4);
  assert(project::same_document_state(
      persistence::recovery::decode(encoded,
                                    persistence::recovery::checksum(encoded))
          .state.document,
      d));
  assert(project::same_semantics(
      persistence::decode_semantics(persistence::encode_semantics(d.semantics)),
      d.semantics));
  assert(project::same_semantics(
      persistence::decode_semantics(v1_payload(legacy.semantics)),
      legacy.semantics));
  auto future =
      persistence::json::parse(persistence::encode_semantics(d.semantics))
          .as_object();
  future["version"] = 4;
  rejects([&] {
    persistence::decode_semantics(persistence::json::serialize(future, false));
  });
  const auto old_project = service.current()->manifest().project_id;
  service.save_as(root / "copy.inde");
  assert(service.current()->manifest().project_id != old_project);
  assert(project::same_document_state(*service.writing().document(d.id), d));
  q = {};
  q.fictional_axis_id = axis.id;
  assert(service.writing().semantic_uses(q).total == 2);
  service.close();
  service.open(root / "copy.inde");
  assert(project::same_document_state(*service.writing().document(d.id), d));
  service.close();
  {
    persistence::SqliteDatabase db(root / "copy.inde/data/project.sqlite3");
    assert(db.query_text("PRAGMA integrity_check") == "ok" &&
           !db.prepare("PRAGMA foreign_key_check").step());
    assert(persistence::SchemaMigrator{}.current_version(db) == persistence::current_database_schema_version);
    db.execute("INSERT INTO schema_migrations VALUES("+std::to_string(persistence::current_database_schema_version+1)+",'future')");
    rejects([&] { persistence::SchemaMigrator{}.migrate(db); });
  }
  std::filesystem::remove_all(root);
  std::cout
      << "PASS: v17 populated migration/rollback, global/interval perspective, "
         "ambiguity/empty document, explicit time/axis/order/FK guards, "
         "inclusive filters, global relations, undo/CAS, recovery v4/semantic "
         "v1-v2, save-as/reopen and future refusal\n";
}

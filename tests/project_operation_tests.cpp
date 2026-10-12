#include "inde/application/editor_interaction.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_cartography_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/document_state.hpp"
#include "inde/project/manifest.hpp"
#include "schema20_downgrade.hpp"
#include <algorithm>
#include <cassert>
#include <glib.h>
#include <iostream>
#include <source_location>
using namespace inde;
template <class F>
void rejects(F f, std::source_location at = std::source_location::current()) {
  bool refused{};
  try {
    f();
  } catch (const std::exception &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Expected refusal at " +
                             std::to_string(at.line()));
}
std::string latest(application::ProjectService &s) {
  const auto rows = s.project_operations();
  assert(!rows.empty());
  return rows.front().id;
}
void reverse(application::ProjectService &s, const std::string &id) {
  auto p = s.reversal_impact(id);
  if (!p.allowed)
    throw std::runtime_error(p.blocked_reason);
  s.confirm_project_operation(p);
}
void calendar_and_map_removal() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-removal-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  const auto path = s.create(root / "Excluir.inde", "Excluir — QA").path();
  auto &w = s.worldbuilding();
  auto calendar = w.create_calendar(project::calendar_template("gregorian"));
  auto derived = w.create_calendar(calendar, calendar.id);
  assert(!s.removal_impact(project::RemovalKind::CalendarSystem, calendar.id)
              .allowed);
  auto deletion =
      s.removal_impact(project::RemovalKind::CalendarSystem, derived.id);
  assert(deletion.allowed);
  const auto operations_before = s.project_operations().size();
  assert(w.calendar(derived.id) == derived); // Preview does not mutate.
  assert(s.project_operations().size() == operations_before);
  s.confirm_project_operation(deletion);
  const auto deleted_revision = latest(s);
  rejects([&] { w.calendar(derived.id); });
  assert(w.calendar(calendar.id) == calendar);
  reverse(s, deleted_revision);
  assert(w.calendar(derived.id) == derived &&
         w.calendar_parent(derived.id) == calendar.id);
  s.confirm_project_operation(
      s.removal_impact(project::RemovalKind::CalendarSystem, derived.id));
  deletion =
      s.removal_impact(project::RemovalKind::CalendarSystem, calendar.id);
  assert(deletion.allowed);
  auto axis = s.planning().create_time_axis("Eixo QA");
  auto binding = w.assign_calendar(axis.id, calendar.id, 0, {});
  rejects([&] {
    s.confirm_project_operation(deletion);
  }); // New use blocks stale approval.
  assert(w.calendar(calendar.id) == calendar);
  assert(!s.removal_impact(project::RemovalKind::CalendarSystem, calendar.id)
              .allowed);
  w.assign_calendar(axis.id, {}, 0, binding);
  project::TemporalReference date;
  date.axis_id = axis.id;
  date.calendar_id = calendar.id;
  date.label = "Ano autoral";
  date.year = 2000;
  date = w.save_temporal_reference(date);
  assert(!s.removal_impact(project::RemovalKind::CalendarSystem, calendar.id)
              .allowed);
  w.remove_temporal_reference(date);
  deletion =
      s.removal_impact(project::RemovalKind::CalendarSystem, calendar.id);
  s.confirm_project_operation(deletion);
  reverse(s, latest(s));
  assert(w.calendar(calendar.id) == calendar);

  auto local = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000002", "Local preservado", "");
  project::geo::Planet planet;
  planet.id = project::new_uuid();
  planet.name = "Mapa a excluir";
  planet.created_at = project::utc_now();
  planet.detail = 3; // Full production pyramid, including WITHOUT ROWID chunks.
  auto generated = project::geo::generate(planet);
  s.cartography().accept(generated);
  project::geo::Position position{planet.id, local.id, {10, 20}};
  s.cartography().change_position(planet.id, local.id, {}, position);
  project::geo::TerrainLock lock{
      project::new_uuid(), planet.id, "Região protegida", {0, 0}, 2,
      project::utc_now()};
  s.cartography().add_terrain_lock(lock);
  deletion = s.removal_impact(project::RemovalKind::CartographicMap, planet.id);
  assert(deletion.allowed);
  for (const auto &[table, amount] :
       {std::pair{"cartographic_planets", std::size_t{1}},
        std::pair{"cartographic_chunks", generated.chunks.size()},
        std::pair{"cartographic_positions", std::size_t{1}},
        std::pair{"cartographic_terrain_locks", std::size_t{1}}}) {
    auto i = std::find_if(deletion.impacts.begin(), deletion.impacts.end(),
                          [&](const auto &v) { return v.table == table; });
    assert(i != deletion.impacts.end() && i->removed == amount);
  }
  auto changed = position;
  changed.coordinate.longitude = 11;
  s.cartography().change_position(planet.id, local.id, position, changed);
  rejects([&] {
    s.confirm_project_operation(deletion);
  }); // Same count, changed content.
  assert(s.cartography().planets().size() == 1);
  auto fresh =
      s.removal_impact(project::RemovalKind::CartographicMap, planet.id);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(path));
    db.execute("CREATE TRIGGER refuse_map_removal BEFORE DELETE ON "
               "cartographic_planets "
               "BEGIN SELECT RAISE(ABORT,'QA failure'); END");
  }
  rejects([&] { s.confirm_project_operation(fresh); });
  assert(s.cartography().position(planet.id, local.id) == changed);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(path));
    db.execute("DROP TRIGGER refuse_map_removal");
  }
  auto other_planet = planet;
  other_planet.id = project::new_uuid();
  other_planet.name = "Outro mapa preservado";
  other_planet.detail = 0;
  s.cartography().accept(project::geo::generate(other_planet));
  s.confirm_project_operation(fresh); // An unrelated map is outside the impact.
  const auto deleted_map = latest(s);
  assert(s.cartography().planets().size() == 1 &&
         s.cartography().planets().front().id == other_planet.id);
  assert(s.narrative().entity(local.id));
  assert(!s.cartography().position(planet.id, local.id));
  s.close();
  s.open(path);
  reverse(s, deleted_map); // Persistence and complete restoration.
  assert(s.cartography().planets().size() == 2);
  assert(s.cartography().position(planet.id, local.id) == changed);
  assert(s.cartography().terrain_locks(planet.id) == std::vector{lock});
  persistence::SqliteCartographyRepository store;
  std::vector<project::geo::ChunkKey> keys;
  for (auto &chunk : generated.chunks)
    keys.push_back(chunk.key);
  std::vector<project::geo::TerrainChunk> restored_chunks;
  for (std::size_t i = 0; i < keys.size(); i += 32) {
    const auto end = std::min(keys.size(), i + 32);
    const auto batch =
        store.chunks(path, planet.id, {keys.begin() + i, keys.begin() + end});
    restored_chunks.insert(restored_chunks.end(), batch.begin(), batch.end());
  }
  assert(restored_chunks == generated.chunks);
  assert(!s.removal_impact(project::RemovalKind::CartographicMap,
                           project::new_uuid())
              .allowed);
  {
    persistence::SqliteDatabase db(
        persistence::ProjectDatabaseRepository::database_path(path));
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    assert(db.query_text("SELECT rtreecheck('cartographic_position_index')") ==
           "ok");
    auto fk = db.prepare("PRAGMA foreign_key_check");
    assert(!fk.step());
  }
  s.close();
  std::filesystem::remove_all(root);
  std::cout << "PASS calendar-map removal: dependencies, preview/cancel "
               "safety, stale impact, "
               "fault rollback, full 170-chunk restoration, positions, locks, "
               "Local preservation, restart\n";
}
int main() {
  calendar_and_map_removal();
  const auto root = std::filesystem::temp_directory_path() /
                    ("inde-project-operations-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  const auto path = root / "Operações.inde";
  s.create(path, "Operações — QA");
  assert(s.project_operations().empty());
  const auto dbpath =
      persistence::ProjectDatabaseRepository::database_path(path);
  {
    persistence::SqliteDatabase db(dbpath);
    assert(persistence::SchemaMigrator{}.current_version(db) ==
           persistence::current_database_schema_version);
    assert(db.query_integer("PRAGMA foreign_keys") == 1);
  }

  auto type = s.narrative().create_entity_type("custom", "Personagem QA", "");
  const auto type_op = latest(s);
  auto entity =
      s.narrative().create_entity(type.id, "Mara 🙂", "Guarda o mapa");
  const auto entity_op = latest(s);
  assert(!s.reversal_impact(type_op).allowed); // New dependent is never lost.
  auto p = s.reversal_impact(entity_op);
  assert(p.allowed && !p.impacts.empty());
  auto edited = entity;
  edited.summary = "Alteração posterior";
  edited = s.narrative().update_entity(edited);
  rejects([&] { s.confirm_project_operation(p); });
  assert(s.narrative().entity(entity.id)->summary == edited.summary);
  reverse(s, latest(s));
  assert(s.narrative().entity(entity.id)->summary == entity.summary);
  reverse(s, entity_op);
  assert(!s.narrative().entity(entity.id));
  assert(!s.reversal_impact(entity_op).allowed);
  assert(s.project_operations().front().compensation &&
         !s.reversal_impact(latest(s)).allowed);
  reverse(s, type_op);
  const auto remaining_types = s.narrative().entity_types();
  assert(std::none_of(remaining_types.begin(), remaining_types.end(),
                      [&](auto &t) { return t.id == type.id; }));

  auto d = s.writing().create_document("Água 🙂 — fonte");
  const auto create_doc_op = latest(s);
  const auto size = s.project_operations().size();
  d.content = "Água 🙂 encontra memória.\nSegundo parágrafo.";
  d.formatting = {{project::DocumentTextStyle::Bold, 0, 4, {}}};
  d = s.writing().update_document(d);
  assert(s.project_operations().size() ==
         size); // Pure text autosave is not Project undo.
  assert(!s.reversal_impact(create_doc_op)
              .allowed); // A written Document cannot be deleted by reversing
                         // its empty creation.
  // Keep the definition alive across the use below.
  const auto definitions = s.writing().semantic_types();
  const auto *comment =
      project::find_annotation_type(definitions, "inde.editor.comment", 1);
  assert(comment);
  d = application::TextSemanticService::create(
      d, *comment, 0, 4,
      {{"note", {project::SemanticFieldKind::Text, {"Conferir"}}},
       {"state", {project::SemanticFieldKind::Enum, {"open"}}}});
  d = s.writing().update_document(d);
  const auto observed = d;
  auto deletion = s.removal_impact(project::RemovalKind::Document, d.id);
  assert(deletion.allowed);
  assert(std::any_of(deletion.impacts.begin(), deletion.impacts.end(),
                     [](auto &i) {
                       return i.table == "document_semantics" && i.removed == 1;
                     }));
  assert(project::same_document_state(
      *s.writing().document(d.id),
      observed)); // Preparing/canceling changes nothing.
  s.confirm_project_operation(deletion);
  assert(!s.writing().document(d.id));
  const auto delete_op = latest(s);
  reverse(s, delete_op);
  assert(project::same_document_state(*s.writing().document(d.id), observed));
  d = *s.writing().document(d.id);
  deletion = s.removal_impact(project::RemovalKind::Document, d.id);
  d.content += " Texto posterior";
  d = s.writing().update_document(d);
  rejects([&] { s.confirm_project_operation(deletion); });
  assert(s.writing().document(d.id)->content == d.content);
  auto revision =
      s.writing().create_editorial_revision(d, "Avaliação independente");
  assert(!s.removal_impact(project::RemovalKind::Document, d.id).allowed);
  rejects([&] { s.writing().delete_document(d.id); });
  assert(s.writing().document(revision.id)->revision_of_id == d.id);

  auto group = s.writing().create_document_group("Grupo com textos", "");
  d.group_id = group.id;
  d = s.writing().update_document(d);
  auto group_delete =
      s.removal_impact(project::RemovalKind::DocumentGroup, group.id);
  assert(group_delete.allowed);
  assert(std::any_of(
      group_delete.impacts.begin(), group_delete.impacts.end(),
      [](auto &i) { return i.table == "documents" && i.updated == 1; }));
  s.confirm_project_operation(group_delete);
  const auto group_op = latest(s);
  assert(!s.writing().document(d.id)->group_id);
  d = *s.writing().document(d.id);
  d.content += " Outra edição preservada";
  d = s.writing().update_document(d);
  reverse(s, group_op);
  assert(s.writing().document(d.id)->group_id == group.id &&
         s.writing().document(d.id)->content == d.content);

  type = s.narrative().create_entity_type("used", "Tipo em uso", "");
  entity = s.narrative().create_entity(type.id, "Iara", "");
  d = *s.writing().document(d.id);
  d.entity_references.push_back(
      {project::new_uuid(), entity.id, std::nullopt, "Referência de QA"});
  d = s.writing().update_document(d);
  assert(!s.removal_impact(project::RemovalKind::Entity, entity.id).allowed);
  rejects([&] { s.narrative().delete_entity(entity.id); });
  assert(s.narrative().entity(entity.id));

  // A compound creation restores the binding and entity together, not text.
  auto from_text = s.writing().create_document("Criação composta");
  from_text.content = "Água 🙂 Marcio escreve.";
  from_text = s.writing().update_document(from_text);
  const auto now = project::utc_now();
  project::TextEntityCreation plan{
      {project::new_uuid(), type.id, "Marcio", "", now, now}, type, false};
  auto bound = application::link_text_entity(from_text, {7, 13}, plan.entity.id,
                                             plan.entity.name);
  bound = s.writing().create_entity_from_text(bound, from_text, plan);
  const auto bound_op = latest(s);
  assert(s.narrative().entity(plan.entity.id));
  reverse(s, bound_op);
  assert(!s.narrative().entity(plan.entity.id));
  auto unbound = *s.writing().document(from_text.id);
  assert(unbound.content == from_text.content &&
         unbound.semantics.annotations.empty());

  // New relation type and relation form one reversible canonical transaction.
  const auto other =
      s.narrative().create_entity(type.id, "Outro participante", "");
  project::RelationType relation_type{project::new_uuid(),
                                      "qa-influencia",
                                      "influencia",
                                      "recebe influência",
                                      {},
                                      project::RelationDirectionality::Directed,
                                      false,
                                      now,
                                      now};
  project::TextRelationCreation relation_plan{entity,
                                              other,
                                              relation_type,
                                              {project::new_uuid(),
                                               relation_type.id,
                                               entity.id,
                                               other.id,
                                               "Relação QA",
                                               now,
                                               now,
                                               {},
                                               {},
                                               {}},
                                              true};
  const auto relation = s.narrative().create_relation_from_text(relation_plan);
  const auto relation_operation = latest(s);
  reverse(s, relation_operation);
  const auto relation_types = s.narrative().relation_types();
  assert(std::none_of(relation_types.begin(), relation_types.end(),
                      [&](const auto &t) { return t.id == relation_type.id; }));
  const auto has_relation = [&](const std::string &id) {
    const auto rows = s.narrative().relations();
    return std::any_of(rows.begin(), rows.end(),
                       [&](const auto &r) { return r.id == id; });
  };
  assert(!has_relation(relation.id));
  const auto recreated = s.narrative().create_relation_from_text(relation_plan);
  auto relation_removal =
      s.removal_impact(project::RemovalKind::Relation, recreated.id);
  assert(relation_removal.allowed &&
         relation_removal.description.find("Iara") != std::string::npos);
  s.confirm_project_operation(relation_removal);
  reverse(s, latest(s));
  assert(has_relation(recreated.id));

  // Human-readable labels must never make a valid operation non-reversible.
  std::string long_name;
  for (int i = 0; i < 72; ++i)
    long_name += "🙂";
  const auto long_a =
      s.narrative().create_entity(type.id, long_name + " A", "");
  const auto long_b =
      s.narrative().create_entity(type.id, long_name + " B", "");
  const auto long_type = s.narrative().create_relation_type(
      "long-name", "conhece", "é conhecido por",
      project::RelationDirectionality::Directed);
  const auto long_relation =
      s.narrative().create_relation(long_type.id, long_a.id, long_b.id);
  const auto long_operation = s.project_operations().front();
  assert(g_utf8_strlen(long_operation.label.data(),
                       long_operation.label.size()) <=
         101); // 100 characters plus ellipsis.
  reverse(s, latest(s));
  assert(!has_relation(long_relation.id));

  // Branch restoration must recover the placement lost by ON DELETE SET NULL.
  const auto ip =
      s.create_intellectual_property("Universo de reversão", "", "").id;
  const auto work =
      s.create_work(ip, "Obra de reversão", "", "", "pt-BR", "Planejamento").id;
  const auto parent =
      s.create_structural_node(work, std::nullopt,
                               project::StructuralNodeType::Chapter, "Capítulo")
          .id;
  const auto child =
      s.create_structural_node(work, parent,
                               project::StructuralNodeType::Chapter, "Filho")
          .id;
  auto placed = s.writing().create_document("Texto colocado", child);
  placed.content = "Conteúdo permanece.";
  placed = s.writing().update_document(placed);
  const auto branch =
      s.removal_impact(project::RemovalKind::EditorialBranch, parent);
  assert(branch.allowed && s.structural_nodes_for_work(work).size() == 2);
  s.confirm_project_operation(branch);
  assert(s.structural_nodes_for_work(work).empty() &&
         !s.writing().document(placed.id)->editorial_node_id);
  assert(s.restore_last_structural_deletion());
  assert(s.structural_nodes_for_work(work).size() == 2);
  assert(
      project::same_document_state(*s.writing().document(placed.id), placed));
  s.delete_structural_branch(parent);
  reverse(s, latest(s));
  assert(s.structural_nodes_for_work(work).size() == 2 &&
         s.writing().document(placed.id)->editorial_node_id == child);

  // Active structures have a unique partial index: order must not cause a false
  // conflict.
  const auto active = s.structures().active_editorial(work);
  assert(active);
  const auto alternate = s.structures().create_editorial(work, "Alternativa");
  s.structures().activate_editorial(alternate.id);
  assert(s.structures().active_editorial(work)->id == alternate.id);
  reverse(s, latest(s));
  assert(s.structures().active_editorial(work)->id == active->id);

  // Failure to persist the journal must roll back the canonical operation
  // itself.
  {
    persistence::SqliteDatabase db(dbpath);
    db.execute(
        "CREATE TRIGGER qa_fail_journal BEFORE INSERT ON project_operations "
        "BEGIN SELECT RAISE(ABORT,'QA journal unavailable');END");
    const auto groups_before =
        db.query_integer("SELECT count(*) FROM document_groups");
    const auto changes_before =
        db.query_integer("SELECT count(*) FROM change_log");
    rejects([&] {
      static_cast<void>(
          s.writing().create_document_group("Não deve persistir", ""));
    });
    assert(db.query_integer("SELECT count(*) FROM document_groups") ==
           groups_before);
    assert(db.query_integer("SELECT count(*) FROM change_log") ==
           changes_before);
    db.execute("DROP TRIGGER qa_fail_journal");
  }

  for (int i = 0; i < 30; ++i)
    static_cast<void>(s.writing().create_document_group(
        "Paginação " + std::to_string(i), ""));
  assert(s.project_operations().size() == 25 &&
         !s.project_operations(25).empty());
  const auto hash_op = latest(s);
  {
    persistence::SqliteDatabase db(dbpath);
    auto q = db.prepare("UPDATE project_operations SET sha256=? WHERE id=?");
    q.bind(1, std::string(64, '0'));
    q.bind(2, hash_op);
    q.run();
  }
  assert(!s.reversal_impact(hash_op).allowed);

  const auto before = s.project_operations(0);
  s.save_as(root / "Cópia.inde");
  assert(s.project_operations().size() == before.size());
  assert(!s.reversal_impact(before.front().id).allowed);
  auto new_group = s.writing().create_document_group("Operação da cópia", "");
  reverse(s, latest(s));
  assert(!s.writing().document_group(new_group.id));
  s.close();
  s.open(path);
  assert(!s.reversal_impact(delete_op).allowed);
  {
    persistence::SqliteDatabase db(dbpath);
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    assert(!db.prepare("PRAGMA foreign_key_check").step());
  }

  // Populated additive migration and rollback, without disabling foreign keys.
  const auto migration = root / "migration.sqlite3";
  {
    persistence::SqliteDatabase db(migration);
    persistence::SchemaMigrator{}.migrate(db);
    downgrade_schema21_for_fixture(db);
    db.execute("DROP TABLE project_operations;DELETE FROM schema_migrations "
               "WHERE version=20");
    db.execute(
        "CREATE TRIGGER fail_v20 BEFORE INSERT ON schema_migrations WHEN "
        "NEW.version=20 BEGIN SELECT RAISE(ABORT,'QA rollback');END");
    rejects([&] { persistence::SchemaMigrator{}.migrate(db); });
    assert(persistence::SchemaMigrator{}.current_version(db) == 19);
    assert(db.query_integer("SELECT count(*) FROM sqlite_master WHERE "
                            "name='project_operations'") == 0 &&
           db.query_integer("PRAGMA foreign_keys") == 1);
    db.execute("DROP TRIGGER fail_v20");
    persistence::SchemaMigrator{}.migrate(db);
    assert(persistence::SchemaMigrator{}.current_version(db) ==
           persistence::current_database_schema_version);
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS: scoped atomic journal, conflict/new-use refusal, "
               "complete Document restoration, source protection, pure text "
               "exclusion, group delta, pagination, copied identity, restart, "
               "hash and migration rollback\n";
}

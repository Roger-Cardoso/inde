#include "inde/application/project_service.hpp"
#include "inde/application/text_semantic_service.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <algorithm>
#include <cassert>
#include <glib.h>
#include <iostream>
using namespace inde;
using K = project::RemovalKind;
template <class F> void refuses(F action) {
  bool refused{};
  try {
    action();
  } catch (const std::exception &) {
    refused = true;
  }
  assert(refused);
}
const project::RemovalUse &use(const project::OperationPreview &p,
                               const std::string &table,
                               const std::string &location = {}) {
  const auto it =
      std::find_if(p.uses.begin(), p.uses.end(), [&](const auto &u) {
        return u.table == table &&
               (location.empty() || u.location_id == location);
      });
  if (it == p.uses.end())
    throw std::runtime_error("Missing named use: " + table + " / " +
                             p.blocked_reason);
  assert(!it->name.empty() && !it->path.empty() && !it->guidance.empty());
  return *it;
}
int main() {
  auto root = std::filesystem::temp_directory_path() /
              ("inde-removal-assistant-" + project::new_uuid());
  std::filesystem::create_directory(root);
  g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), true);
  application::ProjectService s;
  const auto path = s.create(root / "Usos.inde", "Auxiliar — QA").path();
  const auto database =
      persistence::ProjectDatabaseRepository::database_path(path);
  auto rows = [&](const char *table) {
    persistence::SqliteDatabase db(database);
    return db.query_integer("SELECT count(*) FROM " + std::string(table));
  };
  auto &w = s.worldbuilding();
  auto calendar = w.create_calendar(project::calendar_template("gregorian"));
  auto axis = s.planning().create_time_axis("Meu único calendário em testes");
  auto base = *w.assign_calendar(axis.id, calendar.id, 0, {});
  auto point =
      s.planning().create_time_point(axis.id, 42, "Antes da experiência");
  project::TemporalReference date;
  date.axis_id = axis.id;
  date.calendar_id = calendar.id;
  date.label = "Nascimento de Lia";
  date.year = 2000;
  date.month = 1;
  date.day = 2;
  date = w.save_temporal_reference(date);
  const auto entity = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000001", "Lia", "");
  project::TemporalAttachment attachment;
  attachment.reference_id = date.id;
  attachment.kind = "entity";
  attachment.target_id = entity.id;
  attachment.role = "nascimento";
  attachment =
      w.attach_date(attachment, date, w.temporal_target("entity", entity.id));
  auto document = s.writing().create_document("Capítulo que usa a data");
  document.content = "Antes: Lia entra.";
  using T = application::TextSemanticService;
  const auto type = *project::find_annotation_type(
      project::builtin_annotation_types(), "inde.semantic.temporal-context", 1);
  document = T::create(document, type, 7, 10, {});
  const auto annotation = document.semantics.annotations.front().id;
  document = T::bind_date(document, annotation, date.id, "scene");
  document = s.writing().update_document(document);
  const auto operation_count = rows("project_operations");
  auto p = s.removal_impact(K::CalendarSystem, calendar.id);
  assert(!p.allowed && p.description.find(calendar.name) != std::string::npos);
  assert(use(p, "axis_calendars", axis.id).name.find(axis.name) !=
         std::string::npos);
  use(p, "temporal_references", date.id);
  assert(use(p, "temporal_attachments", date.id).name.find(entity.name) !=
         std::string::npos);
  assert(use(p, "semantic_date_bindings", document.id).text_offset == 7);
  assert(rows("project_operations") == operation_count &&
         w.temporal_reference(date.id) == date);
  p = s.removal_impact(K::TemporalReference, date.id);
  assert(!p.allowed);
  use(p, "temporal_attachments");
  use(p, "semantic_date_bindings");
  refuses([&] { s.confirm_project_operation(p); });
  // Detaching the sole calendar keeps all points, dates and textual precision.
  w.assign_calendar(axis.id, {}, 0, base);
  assert(!w.binding(axis.id) && w.point_date(point) == "Ordem 42");
  assert(w.temporal_reference(date.id) == date &&
         s.writing().document(document.id)->content == document.content);
  base = *w.assign_calendar(axis.id, calendar.id, 0, {});
  auto successor = w.create_calendar(project::calendar_template("julian"));
  auto a =
      w.adopt_calendar({{}, axis.id, successor.id, 10, 0, "coexist"}, base, {});
  auto history = w.adoptions(axis.id);
  refuses([&] { w.assign_calendar(axis.id, {}, 0, base); });
  auto b = w.adopt_calendar({{}, axis.id, calendar.id, 20, 0, "retire"}, base,
                            history);
  refuses([&] { w.remove_last_adoption(base, history); });
  auto all = w.adoptions(axis.id);
  {
    persistence::SqliteDatabase db(database);
    db.execute("CREATE TRIGGER refuse_adoption_removal BEFORE DELETE ON "
               "calendar_adoptions BEGIN SELECT RAISE(ABORT,'injected'); END");
    refuses([&] { w.remove_last_adoption(base, all); });
    assert(w.adoptions(axis.id) == all);
    db.execute("DROP TRIGGER refuse_adoption_removal");
  }
  w.remove_last_adoption(base, all);
  assert(w.adoptions(axis.id) == history && history.front().id == a.id);
  auto reverse = s.reversal_impact(s.project_operations().front().id);
  assert(reverse.allowed);
  s.confirm_project_operation(reverse);
  assert(w.adoptions(axis.id).back().id == b.id);
  w.remove_last_adoption(base, all);
  w.remove_last_adoption(base, history);
  w.assign_calendar(axis.id, {}, 0, base);
  assert(w.temporal_reference(date.id) == date &&
         w.text_date_uses(date.id).size() == 1);
  // More than a hundred distinct uses: no previous preview example-limit
  // truncation.
  for (int i = 0; i < 105; ++i) {
    auto r = date;
    r.id.clear();
    r.label = "Teste " + std::to_string(i);
    (void)w.save_temporal_reference(r);
  }
  p = s.removal_impact(K::CalendarSystem, calendar.id);
  assert(std::count_if(p.uses.begin(), p.uses.end(), [](const auto &u) {
           return u.table == "temporal_references";
         }) == 106);
  auto source = s.writing().create_document("Documento de origem do link");
  auto reference =
      s.writing().add_text_reference(source.id, document.id, {}, "Teste");
  p = s.removal_impact(K::Document, document.id);
  assert(!p.allowed);
  use(p, "document_text_references", source.id);
  const auto anchor_id = document.anchors.front().id;
  (void)s.writing().add_text_reference(source.id, document.id, anchor_id,
                                       "Âncora usada");
  p = s.removal_impact(K::DocumentAnchor, anchor_id);
  assert(!p.allowed);
  use(p, "document_text_references", source.id);
  const auto narration = *project::find_annotation_type(
      project::builtin_annotation_types(), "inde.semantic.narration", 1);
  document = T::create(document, narration, 11, 15, {});
  const auto target_annotation = document.semantics.annotations.back().id;
  document = s.writing().update_document(document);
  source.content = "Uma retomada.";
  source = T::create(source, narration, 4, 8, {});
  source = T::relate(source, source.semantics.annotations.front().id,
                     target_annotation, "inde.semantic.callback");
  source = s.writing().update_document(source);
  p = s.removal_impact(K::TextAnnotation, target_annotation);
  assert(!p.allowed &&
         use(p, "semantic_annotation_relations", source.id).text_offset == 4);
  const auto ip = s.create_intellectual_property("Universo QA", "", "");
  const auto work = s.create_work(ip.id, "Obra QA", "", "", "pt-BR", "draft");
  assert(!s.removal_impact(K::IntellectualProperty, ip.id).allowed);
  use(s.removal_impact(K::IntellectualProperty, ip.id), "works", work.id);
  auto scope = s.narrative().add_entity_to_work(entity.id, work.id);
  const auto node = s.create_structural_node(
      work.id, {}, project::StructuralNodeType::Chapter, "Capítulo QA com uso");
  (void)s.narrative().add_editorial_reference(entity.id, node.id,
                                              "apresentação");
  p = s.removal_impact(K::EntityWorkScope, scope.id);
  assert(!p.allowed);
  assert(
      use(p, "editorial_entity_references", entity.id).name.find(node.title) !=
      std::string::npos);
  const auto trash_type =
      s.create_structural_element_type("Tipo guardado na lixeira QA");
  const auto trash_node = s.create_structural_node(work.id, {}, trash_type.id,
                                                   "QA", "Unidade guardada QA");
  auto branch = s.removal_impact(K::EditorialBranch, trash_node.id);
  assert(branch.allowed);
  s.confirm_project_operation(branch);
  const auto trash_operation = s.project_operations().front().id;
  p = s.removal_impact(K::StructuralType, trash_type.id);
  assert(!p.allowed);
  const auto trash_use = use(p, "editorial_node_trash");
  assert(trash_use.location_table == "project_operations" &&
         trash_use.location_id == trash_operation);
  const auto event = s.narrative().create_entity(
      "00000000-0000-4000-9000-000000000003", "Encontro QA");
  const auto occurrence = s.planning().place_event(event.id, point.id, "");
  (void)s.planning().add_participant(occurrence.id, entity.id, "testemunha",
                                     "");
  p = s.removal_impact(K::Entity, entity.id);
  assert(use(p, "event_participations", event.id).name.find(entity.name) !=
         std::string::npos);
  p = s.removal_impact(K::TimePoint, point.id);
  assert(!p.allowed);
  use(p, "event_occurrences", event.id);
  auto structure =
      s.structures().create_narrative(work.id, "Estrutura com data");
  auto unit = s.structures().create_unit(structure.id, "1", "Cena com vínculo",
                                         "", "", "");
  auto line = s.structures().create_line(structure.id, "Linha com vínculo", "");
  attachment.id.clear();
  attachment.kind = "unit";
  attachment.target_id = unit.id;
  attachment =
      w.attach_date(attachment, date, w.temporal_target("unit", unit.id));
  p = s.removal_impact(K::NarrativeStructure, structure.id);
  assert(!p.allowed);
  use(p, "temporal_attachments", date.id); // Transitive block in owned unit.
  assert(!s.removal_impact(K::NarrativeUnit, unit.id).allowed);
  auto role = s.structures().create_role("Papel em teste", "");
  auto assignment = s.structures().assign_role(role.id, entity.id, work.id);
  p = s.removal_impact(K::NarrativeRole, role.id);
  assert(!p.allowed &&
         use(p, "narrative_role_assignments", role.id).work_id == work.id);
  auto entity_type =
      s.narrative().create_entity_type("qa-usos", "Tipo em teste", "");
  auto typed =
      s.narrative().create_entity(entity_type.id, "Entidade com tipo em teste");
  assert(!s.removal_impact(K::EntityType, entity_type.id).allowed);
  use(s.removal_impact(K::EntityType, entity_type.id), "entities", typed.id);
  assert(
      !s.removal_impact(K::EntityType, "00000000-0000-4000-9000-000000000001")
           .allowed);
  for (const auto &ax : s.planning().time_axes())
    if (ax.is_default)
      assert(!s.removal_impact(K::TimeAxis, ax.id).allowed);
  auto deletion = s.removal_impact(K::NarrativeLine, line.id);
  assert(deletion.allowed);
  s.confirm_project_operation(deletion);
  reverse = s.reversal_impact(s.project_operations().front().id);
  assert(reverse.allowed);
  s.confirm_project_operation(reverse);
  assert(s.structures().lines(structure.id).front().id == line.id);
  {
    persistence::SqliteDatabase db(database);
    assert(db.query_text("PRAGMA integrity_check") == "ok");
    auto fk = db.prepare("PRAGMA foreign_key_check");
    assert(!fk.step());
  }
  s.close();
  s.open(path);
  assert(w.temporal_reference(date.id) == date && !w.binding(axis.id));
  std::cout << "PASS removal assistant: named/transitive/text usages, "
               "sole-calendar detach, adoption CAS/rollback/reversal, 106 "
               "dates, protections, restart\n";
}

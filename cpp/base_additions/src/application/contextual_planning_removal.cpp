#include "inde/application/contextual_planning.hpp"
#include "contextual_planning_support.hpp"
#include "inde/application/project_service.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace inde::application {
using namespace contextual_detail;
namespace {
void require_detachable(persistence::SqliteDatabase &db, const std::string &id) {
  auto q = db.prepare(
      "SELECT (SELECT count(*) FROM contextual_option_members WHERE participation_id=?1)+"
      "(SELECT count(*) FROM contextual_declarations WHERE participation_id=?1)+"
      "(SELECT count(*) FROM contextual_dependencies WHERE source_id=?1 OR target_id=?1)+"
      "(SELECT count(*) FROM contextual_text_links WHERE participation_id=?1)+"
      "(SELECT count(*) FROM contextual_text_targets WHERE participation_id=?1)+"
      "(SELECT count(*) FROM contextual_references WHERE participation_id=?1)+"
      "(SELECT count(*) FROM contextual_bridges WHERE source_id=?1 OR target_id=?1)");
  q.bind(1, id);
  q.step();
  if (q.column_integer(0))
    throw std::runtime_error(
        "Há trechos selecionados com encaixes, declarações, ligações, referências "
        "ou vínculos de escrita. Retire esses vínculos nas ferramentas do trecho "
        "antes de excluir sua participação. Nenhum elemento foi retirado.");
}
void erase_by(persistence::SqliteDatabase &db, const std::string &table,
              const std::string &column, const std::string &id) {
  // Table and column names are internal constants, never supplied by the caller.
  auto q = db.prepare("DELETE FROM " + table + " WHERE " + column + "=?");
  q.bind(1, id);
  q.run();
}
} // namespace

void ContextualPlanningService::remove_visual_selection(
    const ContextualSnapshot &expected, const std::vector<std::string> &ids) {
  const auto &p = current(service_);
  auto db = open(p);
  persistence::SqliteTransaction tx(db, "Excluir elementos da composição visual");
  const auto actual = check(db, p, expected);
  if (ids.empty() || ids.size() > 2000)
    throw std::runtime_error("Selecione elementos da composição atual.");
  const auto layout = contextual_visual_layout(actual);
  std::set<std::string> unique, parts, annotations;
  for (const auto &id : ids) {
    if (!unique.insert(id).second)
      throw std::runtime_error("Seleção repetida; nada foi retirado.");
    const auto v = std::find_if(layout.begin(), layout.end(), [&](const auto &item) {
      return item.id == id || (item.kind == "block" && item.participation_id == id);
    });
    if (v == layout.end())
      throw std::runtime_error("Elemento ausente nesta composição; atualize a seleção.");
    if (v->kind == "block") {
      participation(actual, v->participation_id);
      if (!parts.insert(v->participation_id).second)
        throw std::runtime_error("Seleção repetida; nada foi retirado.");
      require_detachable(db, v->participation_id);
    } else {
      annotations.insert(v->id);
    }
  }
  // Validate every member before any write: mixed selections are all or nothing.
  for (const auto &id : annotations)
    erase_by(db, "contextual_visual_items", "id", id);
  for (const auto &id : parts) {
    erase_by(db, "contextual_roles", "participation_id", id);
    erase_by(db, "contextual_visual_items", "participation_id", id);
    erase_by(db, "contextual_participations", "id", id);
  }
  tx.commit();
}

void ContextualPlanningService::remove_plan(const ContextualSnapshot &expected) {
  const auto &p = current(service_);
  auto db = open(p);
  persistence::SqliteTransaction tx(db, "Excluir composição visual e seus vínculos locais");
  const auto actual = check(db, p, expected);
  auto external = db.prepare(
      "SELECT (SELECT count(*) FROM contextual_modules WHERE contents_plan_id=?1)+"
      "(SELECT count(*) FROM contextual_bridges WHERE plan_id=?1 OR source_plan_id=?1)");
  external.bind(1, actual.plan_id);
  external.step();
  if (external.column_integer(0))
    throw std::runtime_error(
        "Esta composição é o interior de um trecho ou tem ligações com outras "
        "composições. Retire esses usos e ligações antes de excluí-la. Nada foi alterado.");
  // The confirmation covers all local annotations and links, never their targets.
  for (const auto *table : {"contextual_visual_items", "contextual_roles",
                           "contextual_text_targets", "contextual_text_links",
                           "contextual_references", "contextual_dependencies",
                           "contextual_declarations", "contextual_option_members"})
    erase_by(db, table, "plan_id", actual.plan_id);
  // Slots and options refer to one another; clear the selected option first.
  auto clear = db.prepare("UPDATE contextual_slots SET selected_option_id=NULL WHERE plan_id=?");
  clear.bind(1, actual.plan_id);
  clear.run();
  erase_by(db, "contextual_options", "plan_id", actual.plan_id);
  erase_by(db, "contextual_slots", "plan_id", actual.plan_id);
  erase_by(db, "contextual_participations", "plan_id", actual.plan_id);
  erase_by(db, "contextual_plans", "id", actual.plan_id);
  tx.commit();
}
} // namespace inde::application

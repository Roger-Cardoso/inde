#include "inde/application/worldbuilding_service.hpp"
#include "inde/application/semantic_anchor_engine.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"
#include "temporal_extent_codec.hpp"
#include "../persistence/calendar_codec.hpp"
#include <algorithm>
#include <glib.h>
#include <stdexcept>
namespace inde::application {
using namespace persistence;
namespace {
SqliteDatabase open(const project::Project &p) {
  auto path = ProjectDatabaseRepository::database_path(p.path());
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Banco do Projeto ausente");
  return SqliteDatabase(path);
}
void guard(SqliteDatabase &db, const project::Project &p) {
  if (SchemaMigrator{}.current_version(db) != current_database_schema_version)
    throw std::runtime_error(
        "Schema incompatível; reabra o Projeto nesta versão");
  auto q = db.prepare("SELECT id FROM projects");
  if (!q.step() || q.column_text(0) != p.manifest().project_id || q.step())
    throw std::runtime_error("Identidade do Projeto incompatível");
}
void exists(SqliteDatabase &db, const project::Project &p, const char *table,
            const std::string &id) {
  auto q = db.prepare("SELECT id FROM " + std::string(table) +
                      " WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step())
    throw std::runtime_error("Origem canônica ausente no Projeto");
}
using calendar_codec::hash;
using calendar_codec::encode;
using calendar_codec::decode;
project::FictionalCalendar read_calendar(SqliteDatabase &db,
                                         const project::Project &p,
                                         const std::string &id) {
  auto q =
      db.prepare("SELECT name,payload,sha256,length(CAST(payload AS BLOB)) "
                 "FROM fictional_calendars WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step() || q.column_integer(3) > 16384)
    throw std::runtime_error("Calendário ausente ou excessivo");
  auto c = decode(q.column_text(1), q.column_text(2));
  if (c.id != id || c.name != q.column_text(0))
    throw std::runtime_error("Metadados de calendário incompatíveis");
  return c;
}
project::EntityAlias read_alias(SqliteStatement &q) {
  project::EntityAlias a{q.column_text(0), q.column_text(1), q.column_text(2),
                         q.column_text(3), q.column_text(4), q.column_text(5),
                         q.column_text(6)};
  project::validate(a);
  if (!g_uuid_string_is_valid(a.token.c_str()) ||
      project::alias_key(a.name) != q.column_text(7))
    throw std::runtime_error("Alias corrompido");
  return a;
}
std::optional<project::EntityAlias>
alias(SqliteDatabase &db, const project::Project &p, const std::string &id) {
  auto q = db.prepare(
      "SELECT id,entity_id,name,kind,note,updated_at,token,normalized FROM "
      "entity_aliases WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step())
    return {};
  return read_alias(q);
}
std::optional<project::AxisCalendar> read_binding(SqliteDatabase &db,
                                                  const project::Project &p,
                                                  const std::string &axis) {
  auto q = db.prepare("SELECT calendar_id,epoch_ordinal,token FROM "
                      "axis_calendars WHERE project_id=? AND axis_id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, axis);
  if (!q.step())
    return {};
  project::AxisCalendar b{axis, q.column_text(0), q.column_integer(1),
                          q.column_text(2)};
  if (!g_uuid_string_is_valid(b.token.c_str()))
    throw std::runtime_error("Associação de calendário corrompida");
  (void)read_calendar(db, p, b.calendar_id);
  return b;
}
std::vector<project::CalendarAdoption> read_adoptions(SqliteDatabase &db,
                                                      const project::Project &p,
                                                      const std::string &axis) {
  auto q = db.prepare(
      "SELECT id,axis_id,calendar_id,from_ordinal,epoch_ordinal,mode FROM "
      "calendar_adoptions WHERE project_id=? AND axis_id=? ORDER BY "
      "from_ordinal,id LIMIT 129");
  q.bind(1, p.manifest().project_id);
  q.bind(2, axis);
  std::vector<project::CalendarAdoption> out;
  while (q.step()) {
    project::CalendarAdoption a{q.column_text(0),    q.column_text(1),
                                q.column_text(2),    q.column_integer(3),
                                q.column_integer(4), q.column_text(5)};
    if (!g_uuid_string_is_valid(a.id.c_str()) ||
        (a.mode != "retire" && a.mode != "coexist"))
      throw std::runtime_error("Adoção temporal inválida");
    out.push_back(a);
  }
  if (out.size() > 128)
    throw std::runtime_error("Eixo excede 128 adoções");
  return out;
}
project::AxisCalendar
effective_binding(project::AxisCalendar base,
                  const std::vector<project::CalendarAdoption> &adoptions,
                  std::int64_t at) {
  for (const auto &a : adoptions) {
    if (a.from_ordinal > at)
      break;
    base.calendar_id = a.calendar_id;
    base.epoch_ordinal = a.epoch_ordinal;
    base.token = a.id;
  }
  return base;
}
project::TemporalReference read_reference(SqliteDatabase &db,
                                          const project::Project &p,
                                          SqliteStatement &q) {
  project::TemporalReference r;
  r.id = q.column_text(0);
  r.axis_id = q.column_text(1);
  r.calendar_id = q.column_text(2);
  r.label = q.column_text(3);
  r.year = q.column_integer(4);
  if (!q.column_is_null(5)) {
    auto n = q.column_integer(5);
    if (n < 1 || n > 32)
      throw std::runtime_error("Mês corrompido");
    r.month = static_cast<int>(n);
  }
  if (!q.column_is_null(6)) {
    auto n = q.column_integer(6);
    if (n < 0 || n > 60)
      throw std::runtime_error("Dia corrompido");
    r.day = static_cast<int>(n);
  }
  auto clock = q.column_text(7);
  if (clock.size() > 128 || clock.find_first_of("{}\"[") == std::string::npos ||
      clock.find_first_of("{}\"") != std::string::npos)
    throw std::runtime_error("Horário corrompido");
  auto parsed = json::parse(clock);
  const auto &units = parsed.as_array();
  if (units.size() > 8)
    throw std::runtime_error("Horário excessivo");
  for (const auto &v : units) {
    auto n = v.as_integer();
    if (n < 0 || n > 9999)
      throw std::runtime_error("Horário inválido");
    r.clock.push_back(static_cast<int>(n));
  }
  r.note = q.column_text(8);
  r.token = q.column_text(9);
  auto extent = db.prepare("SELECT payload FROM temporal_reference_extents "
                           "WHERE project_id=? AND reference_id=?");
  extent.bind(1, p.manifest().project_id);
  extent.bind(2, r.id);
  if (extent.step())
    temporal_extent_codec::decode(extent.column_text(0), r);
  if (!g_uuid_string_is_valid(r.id.c_str()) ||
      !g_uuid_string_is_valid(r.token.c_str()))
    throw std::runtime_error("Identidade temporal corrompida");
  project::validate(read_calendar(db, p, r.calendar_id), r);
  return r;
}
std::optional<project::TemporalReference> reference(SqliteDatabase &db,
                                                    const project::Project &p,
                                                    const std::string &id) {
  auto q = db.prepare(
      "SELECT id,axis_id,calendar_id,label,year,month,day,clock,note,token "
      "FROM temporal_references WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step())
    return {};
  return read_reference(db, p, q);
}
std::string target_column(const std::string &kind) {
  if (kind == "entity")
    return "entity_id";
  if (kind == "relation")
    return "relation_id";
  if (kind == "unit")
    return "unit_id";
  if (kind == "line")
    return "line_id";
  throw std::runtime_error("Tipo de destino inválido");
}
std::string target_query(const std::string &kind) {
  if (kind == "entity")
    return "SELECT e.id,e.name,t.name,e.summary,e.entity_type_id,e.updated_at "
           "FROM entities e JOIN entity_types t ON t.id=e.entity_type_id AND "
           "t.project_id=e.project_id WHERE e.project_id=?";
  if (kind == "relation")
    return "SELECT e.id,s.name||' — '||t.name||CASE WHEN "
           "t.directionality='symmetric' THEN ' ↔ ' ELSE ' → ' END"
           "||d.name,e.description,e.source_entity_id,e.target_entity_id,e."
           "relation_type_id||e.updated_at FROM relations e JOIN "
           "relation_types t ON t.id=e.relation_type_id AND "
           "t.project_id=e.project_id JOIN entities s ON "
           "s.id=e.source_entity_id AND s.project_id=e.project_id JOIN "
           "entities d ON d.id=e.target_entity_id AND "
           "d.project_id=e.project_id WHERE e.project_id=?";
  if (kind == "unit")
    return "SELECT e.id,e.designator||' "
           "'||e.title,w.title||' / "
           "'||s.name,e.summary,e.purpose||char(10)||e.perspective,e."
           "structure_id||e.updated_at FROM narrative_units e JOIN "
           "narrative_structures s ON s.id=e.structure_id AND "
           "s.project_id=e.project_id JOIN works w ON w.id=s.work_id JOIN "
           "intellectual_properties ip ON ip.id=w.intellectual_property_id AND "
           "ip.project_id=e.project_id WHERE e.project_id=?";
  if (kind == "line")
    return "SELECT "
           "e.id,e.name,w.title||' / "
           "'||s.name,e.description,e.structure_id,e.updated_at FROM "
           "narrative_lines e JOIN narrative_structures s ON "
           "s.id=e.structure_id AND s.project_id=e.project_id JOIN works w ON "
           "w.id=s.work_id JOIN intellectual_properties ip ON "
           "ip.id=w.intellectual_property_id AND ip.project_id=e.project_id "
           "WHERE "
           "e.project_id=?";
  throw std::runtime_error("Tipo de destino inválido");
}
project::TemporalTarget read_target(SqliteStatement &q,
                                    const std::string &kind) {
  json::Value::Array snapshot;
  for (int i = 0; i < 6; ++i)
    snapshot.emplace_back(q.column_text(i));
  return {kind, q.column_text(0), q.column_text(1), q.column_text(2),
          hash(json::serialize(snapshot, false))};
}
project::TemporalTarget target(SqliteDatabase &db, const project::Project &p,
                               const std::string &kind, const std::string &id) {
  auto q = db.prepare(target_query(kind) + " AND e.id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step())
    throw std::runtime_error("Destino temporal ausente neste Projeto");
  return read_target(q, kind);
}
project::TemporalAttachment read_attachment(SqliteStatement &q) {
  project::TemporalAttachment a;
  a.id = q.column_text(0);
  a.reference_id = q.column_text(1);
  int count = 0;
  const char *kinds[] = {"entity", "relation", "unit", "line"};
  for (int i = 0; i < 4; ++i)
    if (!q.column_is_null(2 + i)) {
      ++count;
      a.kind = kinds[i];
      a.target_id = q.column_text(2 + i);
    }
  a.role = q.column_text(6);
  a.note = q.column_text(7);
  a.token = q.column_text(8);
  a.meaning = q.column_text(9);
  if (count != 1)
    throw std::runtime_error("Vínculo temporal corrompido");
  project::validate(a);
  return a;
}
std::vector<TextDateUse> text_uses(SqliteDatabase &db,
                                   const project::Project &p,
                                   const std::string &ref) {
  auto q = db.prepare(
      "SELECT "
      "b.id,b.document_id,b.annotation_id,d.title,b.role,s.payload,d.content,d."
      "updated_at "
      "FROM semantic_date_bindings b JOIN documents d ON d.id=b.document_id "
      "AND d.project_id=b.project_id "
      "JOIN document_semantics s ON s.document_id=b.document_id AND "
      "s.project_id=b.project_id "
      "WHERE b.project_id=? AND b.reference_id=? ORDER BY b.id LIMIT 501");
  q.bind(1, p.manifest().project_id);
  q.bind(2, ref);
  std::vector<TextDateUse> out;
  while (q.step())
    out.push_back({q.column_text(0), q.column_text(1), q.column_text(2),
                   q.column_text(3), q.column_text(4),
                   hash(json::serialize(
                       json::Value::Array{q.column_text(3), q.column_text(5),
                                          q.column_text(6), q.column_text(7)},
                       false))});
  if (out.size() > 500)
    throw std::runtime_error("Mais de 500 usos textuais desta data");
  return out;
}
std::optional<project::TemporalAttachment> attachment(SqliteDatabase &db,
                                                      const project::Project &p,
                                                      const std::string &id) {
  auto q = db.prepare("SELECT "
                      "id,reference_id,entity_id,relation_id,unit_id,line_id,"
                      "role,note,token,meaning "
                      "FROM temporal_attachments WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step())
    return {};
  return read_attachment(q);
}
std::int64_t checked_add(std::int64_t a,std::int64_t b) {
  if((b>0 && a>std::numeric_limits<std::int64_t>::max()-b) ||
     (b<0 && a<std::numeric_limits<std::int64_t>::min()-b)) throw std::runtime_error("Alinhamento temporal excede limites");
  return a+b;
}
std::int64_t checked_subtract(std::int64_t a,std::int64_t b) {
  if((b>0 && a<std::numeric_limits<std::int64_t>::min()+b) ||
     (b<0 && a>std::numeric_limits<std::int64_t>::max()+b)) throw std::runtime_error("Alinhamento temporal excede limites");
  return a-b;
}
std::optional<project::TemporalWindow> reference_window(SqliteDatabase &db,
    const project::Project &p, const project::TemporalReference &r) {
  auto source = read_calendar(db,p,r.calendar_id);
  auto base = read_binding(db,p,r.axis_id);
  auto history = read_adoptions(db,p,r.axis_id);
  std::vector<std::int64_t> epochs;
  if(base && base->calendar_id==r.calendar_id) epochs.push_back(base->epoch_ordinal);
  for(const auto &a:history) if(a.calendar_id==r.calendar_id) epochs.push_back(a.epoch_ordinal);
  std::sort(epochs.begin(),epochs.end()); epochs.erase(std::unique(epochs.begin(),epochs.end()),epochs.end());
  if(epochs.empty() && base) {
    try {
      auto origin=read_calendar(db,p,base->calendar_id);
      const project::CalendarDate anchor{source.engine=="chinese"?1901:1,1,project::calendar_first_day(source)};
      const project::CalendarDate origin_anchor{origin.engine=="chinese"?1901:1,1,project::calendar_first_day(origin)};
      auto offset=checked_subtract(project::calendar_ordinal(origin,origin_anchor,base->epoch_ordinal),project::terrestrial_day(origin,origin_anchor));
      epochs.push_back(checked_subtract(checked_add(offset,project::terrestrial_day(source,anchor)),project::calendar_ordinal(source,anchor)));
    } catch(const std::exception &) {}
  }
  if(epochs.size()!=1) return {};
  return project::temporal_window(source,r,epochs.front());
}
void sort_reference_dates(SqliteDatabase &db, const project::Project &p,
                          std::vector<project::TemporalReference> &refs) {
  std::map<std::string,std::optional<project::TemporalWindow>> windows;
  for(const auto &r:refs) windows[r.id]=reference_window(db,p,r);
  std::stable_sort(refs.begin(),refs.end(),[&](const auto &a,const auto &b) {
    if(a.axis_id!=b.axis_id) return a.axis_id<b.axis_id;
    const auto &aw=windows.at(a.id),&bw=windows.at(b.id);
    auto rank=[](const auto &w){return !w || !w->comparable?2:!w->lower_bound?0:1;};
    if(rank(aw)!=rank(bw)) return rank(aw)<rank(bw);
    if(rank(aw)==1) {
      auto order=project::compare_temporal_coordinates(*aw->lower_bound,*bw->lower_bound);
      if(order) return order<0;
    }
    return std::tie(a.calendar_id,a.year,a.month,a.day,a.clock,a.label,a.id)<
           std::tie(b.calendar_id,b.year,b.month,b.day,b.clock,b.label,b.id);
  });
}
} // namespace
const project::Project &WorldbuildingService::owner() const {
  if (!session_.current())
    throw std::runtime_error("Nenhum Projeto aberto");
  return *session_.current();
}
std::vector<project::EntityAlias>
WorldbuildingService::aliases(const std::string &entity) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  exists(db, p, "entities", entity);
  auto q = db.prepare(
      "SELECT id,entity_id,name,kind,note,updated_at,token,normalized FROM "
      "entity_aliases WHERE project_id=? AND entity_id=? ORDER BY "
      "normalized,id LIMIT 65");
  q.bind(1, p.manifest().project_id);
  q.bind(2, entity);
  std::vector<project::EntityAlias> out;
  while (q.step())
    out.push_back(read_alias(q));
  if (out.size() > 64)
    throw std::runtime_error("Entidade excede 64 aliases");
  return out;
}
project::EntityAlias WorldbuildingService::save_alias(
    project::EntityAlias a,
    std::optional<project::EntityAlias> expected) const {
  if (a.id.empty())
    a.id = project::new_uuid();
  a.name = project::trim_world_text(a.name);
  a.note = project::trim_world_text(a.note);
  project::validate(a);
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  exists(db, p, "entities", a.entity_id);
  auto old = alias(db, p, a.id);
  if (old != expected || (old && old->entity_id != a.entity_id))
    throw std::runtime_error(
        "Conflito no alias; reabra a ficha. Nada foi alterado.");
  auto count = db.prepare(
      "SELECT count(*) FROM entity_aliases WHERE project_id=? AND entity_id=?");
  count.bind(1, p.manifest().project_id);
  count.bind(2, a.entity_id);
  count.step();
  if (!old && count.column_integer(0) >= 64)
    throw std::runtime_error("Máximo de 64 aliases por entidade");
  auto canonical =
      db.prepare("SELECT name FROM entities WHERE project_id=? AND id=?");
  canonical.bind(1, p.manifest().project_id);
  canonical.bind(2, a.entity_id);
  canonical.step();
  const auto canonical_name = canonical.column_text(0);
  if (canonical_name.size() <= 512 &&
      project::alias_key(canonical_name) == project::alias_key(a.name))
    throw std::runtime_error("Esse já é o nome canônico da entidade");
  auto duplicate =
      db.prepare("SELECT 1 FROM entity_aliases WHERE project_id=? AND "
                 "entity_id=? AND normalized=? AND id<>? LIMIT 1");
  duplicate.bind(1, p.manifest().project_id);
  duplicate.bind(2, a.entity_id);
  duplicate.bind(3, project::alias_key(a.name));
  duplicate.bind(4, a.id);
  if (duplicate.step())
    throw std::runtime_error(
        "Esse nome alternativo já está cadastrado nesta entidade. Edite o nome "
        "existente ou escolha outro.");
  tx.record_project_operation(old ? "Editar nome alternativo"
                                  : "Adicionar nome alternativo");
  a.updated_at = project::utc_now();
  a.token = project::new_uuid();
  auto q = db.prepare(
      "INSERT INTO "
      "entity_aliases(id,project_id,entity_id,name,normalized,kind,note,"
      "updated_at,token) VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE "
      "SET "
      "name=excluded.name,normalized=excluded.normalized,kind=excluded.kind,"
      "note=excluded.note,updated_at=excluded.updated_at,token=excluded.token");
  q.bind(1, a.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, a.entity_id);
  q.bind(4, a.name);
  q.bind(5, project::alias_key(a.name));
  q.bind(6, a.kind);
  q.bind(7, a.note);
  q.bind(8, a.updated_at);
  q.bind(9, a.token);
  q.run();
  tx.commit();
  return a;
}
void WorldbuildingService::remove_alias(
    const project::EntityAlias &expected) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  if (alias(db, p, expected.id) != expected)
    throw std::runtime_error("Alias mudou ou foi removido; reabra a ficha");
  tx.record_project_operation("Remover nome alternativo");
  auto q = db.prepare("DELETE FROM entity_aliases WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, expected.id);
  q.run();
  tx.commit();
}
std::vector<project::FictionalCalendar>
WorldbuildingService::calendars() const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  auto q = db.prepare("SELECT id FROM fictional_calendars WHERE project_id=? "
                      "ORDER BY name,id LIMIT 65");
  q.bind(1, p.manifest().project_id);
  std::vector<project::FictionalCalendar> out;
  while (q.step())
    out.push_back(read_calendar(db, p, q.column_text(0)));
  if (out.size() > 64)
    throw std::runtime_error("Projeto excede 64 calendários");
  return out;
}
project::FictionalCalendar
WorldbuildingService::calendar(const std::string &id) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  return read_calendar(db, p, id);
}
project::FictionalCalendar
WorldbuildingService::create_calendar(project::FictionalCalendar c,
                                      std::optional<std::string> parent) const {
  c.id = project::new_uuid();
  c.name = project::trim_world_text(c.name);
  auto payload = encode(c);
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  if (parent)
    (void)read_calendar(db, p, *parent);
  auto count =
      db.prepare("SELECT count(*) FROM fictional_calendars WHERE project_id=?");
  count.bind(1, p.manifest().project_id);
  count.step();
  if (count.column_integer(0) >= 64)
    throw std::runtime_error("Máximo de 64 calendários por Projeto");
  tx.record_project_operation(parent ? "Criar revisão de sistema temporal"
                                     : "Criar sistema temporal");
  auto q = db.prepare(
      "INSERT INTO fictional_calendars(id,project_id,name,payload,sha256) "
      "VALUES(?,?,?,?,?)");
  q.bind(1, c.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, c.name);
  q.bind(4, payload);
  q.bind(5, hash(payload));
  q.run();
  if (parent) {
    auto revision = db.prepare(
        "INSERT INTO calendar_revisions(calendar_id,project_id,parent_id) "
        "VALUES(?,?,?)");
    revision.bind(1, c.id);
    revision.bind(2, p.manifest().project_id);
    revision.bind(3, *parent);
    revision.run();
  }
  tx.commit();
  return c;
}
std::optional<std::string>
WorldbuildingService::calendar_parent(const std::string &id) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  (void)read_calendar(db, p, id);
  auto q = db.prepare("SELECT parent_id FROM calendar_revisions WHERE "
                      "project_id=? AND calendar_id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step())
    return {};
  return q.column_text(0);
}
std::vector<project::CalendarAdoption>
WorldbuildingService::adoptions(const std::string &axis) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  exists(db, p, "fictional_time_axes", axis);
  return read_adoptions(db, p, axis);
}
project::CalendarAdoption WorldbuildingService::adopt_calendar(
    project::CalendarAdoption a, const project::AxisCalendar &base,
    const std::vector<project::CalendarAdoption> &expected) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  if (a.axis_id != base.axis_id || read_binding(db, p, a.axis_id) != base ||
      read_adoptions(db, p, a.axis_id) != expected)
    throw std::runtime_error(
        "Histórico temporal mudou; reabra a prévia. Nada foi alterado.");
  if (a.mode != "retire" && a.mode != "coexist")
    throw std::runtime_error("Escolha aposentar ou coexistir");
  if (expected.size() >= 128 ||
      (!expected.empty() && a.from_ordinal <= expected.back().from_ordinal))
    throw std::runtime_error(
        "A nova adoção deve suceder a última; reveja o histórico");
  auto old = effective_binding(base, expected, a.from_ordinal);
  if (old.calendar_id == a.calendar_id)
    throw std::runtime_error(
        "O sistema já está vigente; escolha outro ou uma revisão");
  auto c = read_calendar(db, p, a.calendar_id);
  (void)project::calendar_date(c, a.from_ordinal, a.epoch_ordinal);
  a.id = project::new_uuid();
  tx.record_project_operation(a.mode == "retire"
                                  ? "Adotar sistema e aposentar antecessores"
                                  : "Adotar sistema com calendários paralelos");
  auto q = db.prepare("INSERT INTO "
                      "calendar_adoptions(id,project_id,axis_id,calendar_id,"
                      "from_ordinal,epoch_ordinal,mode) VALUES(?,?,?,?,?,?,?)");
  q.bind(1, a.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, a.axis_id);
  q.bind(4, a.calendar_id);
  q.bind(5, a.from_ordinal);
  q.bind(6, a.epoch_ordinal);
  q.bind(7, a.mode);
  q.run();
  tx.commit();
  return a;
}
std::vector<std::string> WorldbuildingService::parallel_dates(
    const project::FictionalTimePoint &point) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  guard(db, p);
  auto base = read_binding(db, p, point.axis_id);
  if (!base)
    return {"Ordem " + std::to_string(point.ordinal)};
  std::vector<project::AxisCalendar> views{*base};
  for (const auto &a : read_adoptions(db, p, point.axis_id)) {
    if (a.from_ordinal > point.ordinal)
      break;
    if (a.mode == "retire")
      views.clear();
    views.push_back({point.axis_id, a.calendar_id, a.epoch_ordinal, {}});
  }
  std::vector<std::string> out;
  for (const auto &b : views) {
    auto c = read_calendar(db, p, b.calendar_id);
    out.push_back(
        c.name + " — " +
        project::format_calendar_date(
            c, project::calendar_date(c, point.ordinal, b.epoch_ordinal)));
  }
  tx.commit();
  return out;
}
std::vector<project::TemporalReference>
WorldbuildingService::temporal_references(const std::string &axis) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  guard(db, p);
  exists(db, p, "fictional_time_axes", axis);
  auto q = db.prepare(
      "SELECT id,axis_id,calendar_id,label,year,month,day,clock,note,token "
      "FROM temporal_references WHERE project_id=? AND axis_id=? ORDER BY "
      "label,id LIMIT 501");
  q.bind(1, p.manifest().project_id);
  q.bind(2, axis);
  std::vector<project::TemporalReference> out;
  while (q.step())
    out.push_back(read_reference(db, p, q));
  if (out.size() > 500)
    throw std::runtime_error("Eixo excede 500 referências temporais");
  sort_reference_dates(db,p,out);
  tx.commit();
  return out;
}
project::TemporalReference
WorldbuildingService::temporal_reference(const std::string &id) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  auto r = reference(db, p, id);
  if (!r)
    throw std::runtime_error("Referência temporal ausente");
  return *r;
}
std::vector<TextDateUse>
WorldbuildingService::text_date_uses(const std::string &id) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  return text_uses(db, p, id);
}
std::vector<TemporalContinuityNotice>
WorldbuildingService::continuity_notices(project::Document d) const {
  project::validate_semantic_structure(d);
  SemanticAnchorEngine::inspect(d);
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  guard(db, p);
  std::vector<TemporalContinuityNotice> out;
  auto epoch =
      [&](const project::TemporalReference &r) -> std::optional<std::int64_t> {
    auto q = db.prepare("SELECT epoch_ordinal FROM axis_calendars WHERE "
                        "project_id=? AND axis_id=? AND calendar_id=? UNION "
                        "SELECT epoch_ordinal FROM calendar_adoptions WHERE "
                        "project_id=? AND axis_id=? AND calendar_id=?");
    q.bind(1, p.manifest().project_id);
    q.bind(2, r.axis_id);
    q.bind(3, r.calendar_id);
    q.bind(4, p.manifest().project_id);
    q.bind(5, r.axis_id);
    q.bind(6, r.calendar_id);
    if (!q.step())
      return {};
    auto value = q.column_integer(0);
    if (q.step())
      return {};
    return value;
  };
  for (const auto &a : d.semantics.annotations) {
    if (a.status != "active" ||
        a.type_key != "inde.semantic.temporal-context" || a.dates.empty())
      continue;
    auto e =
        std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                     [&](const auto &v) { return v.anchor_id == a.anchor_id; });
    if (e == d.semantics.anchors.end() ||
        e->state != project::AnchorResolution::Resolved)
      continue;
    auto participants = a.fields.find("participants");
    if (participants == a.fields.end())
      continue;
    std::vector<project::SemanticDateBinding> scene;
    for (const auto &b : a.dates)
      if (b.role == "scene")
        scene.push_back(b);
    if (scene.size() != 1)
      continue; // never select a winning temporal context
    auto current = reference(db, p, scene.front().reference_id);
    if (!current)
      throw std::runtime_error("Data textual ausente");
    const bool exception = a.fields.contains("exception") &&
                           !a.fields.at("exception").values.empty();
    for (const auto &entity : participants->second.values) {
      auto dest = target(db, p, "entity", entity);
      auto q = db.prepare(
          "SELECT reference_id FROM temporal_attachments WHERE project_id=? "
          "AND entity_id=? AND meaning='birth' ORDER BY id LIMIT 2");
      q.bind(1, p.manifest().project_id);
      q.bind(2, entity);
      if (!q.step())
        continue;
      auto birth = reference(db, p, q.column_text(0));
      if (q.step())
        continue;
      if (!birth || birth->axis_id != current->axis_id)
        continue;
      std::int64_t ce = 0, be = 0;
      if (birth->calendar_id != current->calendar_id) {
        auto cv = epoch(*current), bv = epoch(*birth);
        if (!cv || !bv)
          continue;
        ce = *cv;
        be = *bv;
      }
      auto cw = project::temporal_window(
          read_calendar(db, p, current->calendar_id), *current, ce);
      auto bw = project::temporal_window(
          read_calendar(db, p, birth->calendar_id), *birth, be);
      if (!cw.comparable || !bw.comparable || !cw.latest || !bw.earliest ||
          project::compare_temporal_coordinates(*cw.upper_bound,
                                                *bw.lower_bound) > 0)
        continue;
      out.push_back(
          {a.id, entity, current->id, birth->id,
           "Aviso: presença declarada de " + dest.label + " em «" +
               current->label + "» (" +
               project::format_temporal_reference(
                   read_calendar(db, p, current->calendar_id), *current) +
               ") antecede o nascimento formal «" + birth->label + "» (" +
               project::format_temporal_reference(
                   read_calendar(db, p, birth->calendar_id), *birth) +
               "). Isso não impede salvar: viagem no tempo ou outro contexto "
               "pode justificar a diferença." +
               (exception ? " O trecho já declara um contexto excepcional."
                          : ""),
           exception});
      if (out.size() >= 200) {
        tx.commit();
        return out;
      }
    }
  }
  tx.commit();
  return out;
}
static project::TemporalReference save_reference(
    SqliteDatabase &db, const project::Project &p, SqliteTransaction &tx,
    project::TemporalReference r,
    std::optional<project::TemporalReference> expected,
    const std::vector<project::TemporalAttachment> &expected_links,
    const std::vector<TextDateUse> &expected_text,
    const std::string &operation = {}) {
  guard(db, p);
  exists(db, p, "fictional_time_axes", r.axis_id);
  project::validate(read_calendar(db, p, r.calendar_id), r);
  if (r.id.empty())
    r.id = project::new_uuid();
  if (!g_uuid_string_is_valid(r.id.c_str()))
    throw std::runtime_error("Identidade de referência inválida");
  auto old = reference(db, p, r.id);
  if (old != expected || (old && old->axis_id != r.axis_id))
    throw std::runtime_error("Referência mudou; reabra antes de salvar");
  auto links_query = db.prepare(
      "SELECT "
      "id,reference_id,entity_id,relation_id,unit_id,line_id,role,"
      "note,token,meaning FROM temporal_attachments WHERE project_id=? AND "
      "reference_id=? ORDER BY role,id LIMIT 501");
  links_query.bind(1, p.manifest().project_id);
  links_query.bind(2, r.id);
  std::vector<project::TemporalAttachment> links;
  while (links_query.step())
    links.push_back(read_attachment(links_query));
  if (links != expected_links)
    throw std::runtime_error("Os vínculos desta data mudaram; reabra e confira "
                             "o impacto antes de editar");
  if (text_uses(db, p, r.id) != expected_text)
    throw std::runtime_error(
        "Os usos desta data em documentos mudaram; reabra e confira o impacto");
  auto count = db.prepare("SELECT count(*) FROM temporal_references WHERE "
                          "project_id=? AND axis_id=?");
  count.bind(1, p.manifest().project_id);
  count.bind(2, r.axis_id);
  count.step();
  if (!old && count.column_integer(0) >= 500)
    throw std::runtime_error("Máximo de 500 referências neste eixo");
  json::Value::Array units;
  for (auto n : r.clock)
    units.emplace_back(n);
  r.token = project::new_uuid();
  tx.record_project_operation(!operation.empty() ? operation : old ? "Editar data e horário autoral"
                                  : "Criar data e horário autoral");
  auto q =
      db.prepare("INSERT INTO "
                 "temporal_references(id,project_id,axis_id,calendar_id,label,"
                 "year,month,day,clock,note,token) "
                 "VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
                 "calendar_id=excluded.calendar_id,label=excluded.label,year="
                 "excluded.year,month=excluded.month,day=excluded.day,clock="
                 "excluded.clock,note=excluded.note,token=excluded.token");
  q.bind(1, r.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, r.axis_id);
  q.bind(4, r.calendar_id);
  q.bind(5, r.label);
  q.bind(6, r.year);
  if (r.month)
    q.bind(7, *r.month);
  else
    q.bind_null(7);
  if (r.day)
    q.bind(8, *r.day);
  else
    q.bind_null(8);
  q.bind(9, json::serialize(units, false));
  q.bind(10, r.note);
  q.bind(11, r.token);
  q.run();
  auto ext = db.prepare(
      "INSERT INTO temporal_reference_extents(reference_id,project_id,payload) "
      "VALUES(?,?,?) ON CONFLICT(reference_id) DO UPDATE SET "
      "payload=excluded.payload");
  ext.bind(1, r.id);
  ext.bind(2, p.manifest().project_id);
  ext.bind(3, temporal_extent_codec::encode(r));
  ext.run();
  return r;
}
project::TemporalReference WorldbuildingService::save_temporal_reference(
    project::TemporalReference r, std::optional<project::TemporalReference> expected,
    const std::vector<project::TemporalAttachment> &links,
    const std::vector<TextDateUse> &text) const {
  const auto &p = owner(); auto db = open(p); SqliteTransaction tx(db);
  r = save_reference(db, p, tx, r, expected, links, text);
  tx.commit(); return r;
}

void WorldbuildingService::remove_temporal_reference(
    const project::TemporalReference &expected) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  if (reference(db, p, expected.id) != expected)
    throw std::runtime_error("Referência mudou; reabra antes de remover");
  auto uses = db.prepare("SELECT count(*) FROM temporal_attachments WHERE "
                         "project_id=? AND reference_id=?");
  uses.bind(1, p.manifest().project_id);
  uses.bind(2, expected.id);
  uses.step();
  if (uses.column_integer(0))
    throw std::runtime_error(
        "Remova explicitamente os vínculos antes de apagar esta data");
  if (!text_uses(db, p, expected.id).empty())
    throw std::runtime_error(
        "Remova os usos em documentos antes de apagar esta data");
  tx.record_project_operation("Remover data e horário autoral");
  auto q =
      db.prepare("DELETE FROM temporal_references WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, expected.id);
  q.run();
  tx.commit();
}
std::vector<project::TemporalTarget>
WorldbuildingService::temporal_targets(const std::string &kind,
                                       const std::string &query) const {
  if (query.size() > 256 ||
      !g_utf8_validate(query.data(), query.size(), nullptr) ||
      query.find('\0') != std::string::npos)
    throw std::runtime_error("Busca inválida ou acima de 256 bytes");
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  auto q = db.prepare("WITH targets(id,label,context,a,b,c) AS (" +
                      target_query(kind) +
                      ") SELECT * FROM targets WHERE "
                      "instr(lower(label||char(10)||context),lower(?))>0 ORDER "
                      "BY label,id LIMIT 101");
  q.bind(1, p.manifest().project_id);
  q.bind(2, project::trim_world_text(query));
  std::vector<project::TemporalTarget> out;
  while (q.step())
    out.push_back(read_target(q, kind));
  if (out.size() > 100)
    throw std::runtime_error("Mais de 100 resultados: refine a busca; nenhum "
                             "resultado foi escolhido automaticamente");
  return out;
}
project::TemporalTarget
WorldbuildingService::temporal_target(const std::string &kind,
                                      const std::string &id) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  return target(db, p, kind, id);
}
std::vector<project::TemporalAttachment>
WorldbuildingService::temporal_attachments(const std::string &ref) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  if (!reference(db, p, ref))
    throw std::runtime_error("Referência ausente");
  auto q = db.prepare(
      "SELECT "
      "id,reference_id,entity_id,relation_id,unit_id,line_id,role,"
      "note,token,meaning FROM temporal_attachments WHERE project_id=? AND "
      "reference_id=? ORDER BY role,id LIMIT 501");
  q.bind(1, p.manifest().project_id);
  q.bind(2, ref);
  std::vector<project::TemporalAttachment> out;
  while (q.step())
    out.push_back(read_attachment(q));
  if (out.size() > 500)
    throw std::runtime_error("Mais de 500 vínculos para esta data");
  return out;
}
std::vector<project::TemporalReference> WorldbuildingService::target_dates(
    const project::TemporalTarget &expected) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  guard(db, p);
  if (target(db, p, expected.kind, expected.id) != expected)
    throw std::runtime_error("Destino mudou; consulte novamente");
  auto q = db.prepare("SELECT DISTINCT "
                      "r.id,r.axis_id,r.calendar_id,r.label,r.year,r.month,r."
                      "day,r.clock,r.note,r.token FROM temporal_references r "
                      "JOIN temporal_attachments a ON a.reference_id=r.id AND "
                      "a.project_id=r.project_id WHERE r.project_id=? AND a." +
                      target_column(expected.kind) +
                      "=? ORDER BY r.label,r.id LIMIT 501");
  q.bind(1, p.manifest().project_id);
  q.bind(2, expected.id);
  std::vector<project::TemporalReference> out;
  while (q.step())
    out.push_back(read_reference(db, p, q));
  if (out.size() > 500)
    throw std::runtime_error("Consulta excede 500 datas");
  sort_reference_dates(db,p,out);
  tx.commit();
  return out;
}
static project::TemporalAttachment attach_reference(
    SqliteDatabase &db, const project::Project &p, SqliteTransaction &tx,project::TemporalAttachment a,
                                  const project::TemporalReference &expected,
                                  const project::TemporalTarget &dest,
                                  bool record_operation = true) {
  guard(db, p);
  if (a.reference_id != expected.id || a.kind != dest.kind ||
      a.target_id != dest.id || reference(db, p, expected.id) != expected ||
      target(db, p, dest.kind, dest.id) != dest)
    throw std::runtime_error("Data ou destino mudou; reabra antes de vincular");
  if (!a.id.empty())
    throw std::runtime_error("Novo vínculo não aceita identidade existente");
  a.id = project::new_uuid();
  a.token = project::new_uuid();
  a.role = project::trim_world_text(a.role);
  project::validate(a);
  auto count =
      db.prepare("SELECT max((SELECT count(*) FROM temporal_attachments WHERE "
                 "project_id=? AND reference_id=?),(SELECT count(*) FROM "
                 "temporal_attachments WHERE project_id=? AND " +
                 target_column(dest.kind) + "=?))");
  count.bind(1, p.manifest().project_id);
  count.bind(2, expected.id);
  count.bind(3, p.manifest().project_id);
  count.bind(4, dest.id);
  count.step();
  if (count.column_integer(0) >= 500)
    throw std::runtime_error("Limite de 500 vínculos para data ou destino");
  if (record_operation) tx.record_project_operation("Vincular data — " + a.role);
  auto q = db.prepare(
      "INSERT INTO "
      "temporal_attachments(id,project_id,reference_id,entity_id,relation_id,"
      "unit_id,line_id,role,note,token,meaning) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
  q.bind(1, a.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, a.reference_id);
  int i = 4;
  for (auto kind : {"entity", "relation", "unit", "line"}) {
    if (a.kind == kind)
      q.bind(i, a.target_id);
    else
      q.bind_null(i);
    ++i;
  }
  q.bind(8, a.role);
  q.bind(9, a.note);
  q.bind(10, a.token);
  q.bind(11, a.meaning);
  q.run();
  return a;
}
project::TemporalAttachment WorldbuildingService::attach_date(
    project::TemporalAttachment a, const project::TemporalReference &r,
    const project::TemporalTarget &dest) const {
  const auto &p = owner(); auto db = open(p); SqliteTransaction tx(db);
  a = attach_reference(db, p, tx, a, r, dest);
  tx.commit(); return a;
}
project::TemporalAttachment WorldbuildingService::create_attached_date(
    project::TemporalReference r, const project::FictionalCalendar &calendar,
    project::TemporalAttachment a, const project::TemporalTarget &dest) const {
  const auto &p = owner(); auto db = open(p); SqliteTransaction tx(db);
  guard(db, p);
  if (!r.id.empty() || read_calendar(db,p,r.calendar_id) != calendar)
    throw std::runtime_error("Calendário mudou ou referência não é nova");
  r = save_reference(db,p,tx,r,{}, {}, {}, "Criar data e vincular elemento");
  a.reference_id = r.id;
  a = attach_reference(db,p,tx,a,r,dest,false);
  tx.commit(); return a;
}

void WorldbuildingService::remove_date_attachment(
    const project::TemporalAttachment &expected) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  if (attachment(db, p, expected.id) != expected)
    throw std::runtime_error("Vínculo mudou; reabra antes de remover");
  tx.record_project_operation("Remover vínculo temporal — " + expected.role);
  auto q = db.prepare(
      "DELETE FROM temporal_attachments WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, expected.id);
  q.run();
  tx.commit();
}
std::optional<project::AxisCalendar>
WorldbuildingService::binding(const std::string &axis) const {
  const auto &p = owner();
  auto db = open(p);
  guard(db, p);
  exists(db, p, "fictional_time_axes", axis);
  return read_binding(db, p, axis);
}
void WorldbuildingService::remove_last_adoption(
    const project::AxisCalendar &base,
    const std::vector<project::CalendarAdoption> &expected) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  if (expected.empty() || read_binding(db, p, base.axis_id) != base ||
      read_adoptions(db, p, base.axis_id) != expected)
    throw std::runtime_error("Histórico temporal mudou ou está vazio; reabra antes de remover a última adoção.");
  tx.record_project_operation("Remover última adoção de calendário; fatos preservados");
  auto q = db.prepare("DELETE FROM calendar_adoptions WHERE project_id=? AND axis_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, base.axis_id);
  q.bind(3, expected.back().id);
  q.run();
  if (db.changes() != 1) throw std::runtime_error("Adoção não encontrada");
  tx.commit();
}
std::optional<project::AxisCalendar> WorldbuildingService::assign_calendar(
    const std::string &axis, std::optional<std::string> cal, std::int64_t epoch,
    const std::optional<project::AxisCalendar> &expected) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  exists(db, p, "fictional_time_axes", axis);
  if (read_binding(db, p, axis) != expected)
    throw std::runtime_error("Calendário deste eixo mudou; reabra a "
                             "configuração antes de confirmar");
  if (!read_adoptions(db, p, axis).empty())
    throw std::runtime_error("Este eixo tem adoções históricas; use Uso e "
                             "mudanças, não substitua sua origem");
  if (cal)
    (void)read_calendar(db, p, *cal);
  tx.record_project_operation(cal ? "Associar calendário ao eixo"
                                  : "Desassociar calendário do eixo");
  auto remove =
      db.prepare("DELETE FROM axis_calendars WHERE project_id=? AND axis_id=?");
  remove.bind(1, p.manifest().project_id);
  remove.bind(2, axis);
  remove.run();
  std::optional<project::AxisCalendar> out;
  if (cal) {
    out = project::AxisCalendar{axis, *cal, epoch, project::new_uuid()};
    auto q = db.prepare("INSERT INTO "
                        "axis_calendars(axis_id,project_id,calendar_id,epoch_"
                        "ordinal,token) VALUES(?,?,?,?,?)");
    q.bind(1, axis);
    q.bind(2, p.manifest().project_id);
    q.bind(3, *cal);
    q.bind(4, epoch);
    q.bind(5, out->token);
    q.run();
  }
  tx.commit();
  return out;
}
std::string WorldbuildingService::point_date(
    const project::FictionalTimePoint &point) const {
  return point_dates({point}).at(point.id);
}
std::map<std::string, std::string> WorldbuildingService::point_dates(
    const std::vector<project::FictionalTimePoint> &points) const {
  if (points.size() > 500)
    throw std::runtime_error("Consulta de datas excede 500 pontos");
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  guard(db, p);
  std::map<std::string, std::optional<project::AxisCalendar>> axes;
  std::map<std::string, std::vector<project::CalendarAdoption>> histories;
  std::map<std::string, project::FictionalCalendar> definitions;
  std::map<std::string, std::string> out;
  for (const auto &point : points) {
    if (!axes.contains(point.axis_id)) {
      exists(db, p, "fictional_time_axes", point.axis_id);
      axes[point.axis_id] = read_binding(db, p, point.axis_id);
      histories[point.axis_id] = read_adoptions(db, p, point.axis_id);
    }
    auto b = axes.at(point.axis_id);
    if (b)
      b = effective_binding(*b, histories.at(point.axis_id), point.ordinal);
    if (!b) {
      out[point.id] = "Ordem " + std::to_string(point.ordinal);
      continue;
    }
    if (!definitions.contains(b->calendar_id))
      definitions[b->calendar_id] = read_calendar(db, p, b->calendar_id);
    const auto &c = definitions.at(b->calendar_id);
    try {
      out[point.id] = project::format_calendar_date(
          c, project::calendar_date(c, point.ordinal, b->epoch_ordinal));
    } catch (const std::exception &) {
      out[point.id] = "Ordem " + std::to_string(point.ordinal) +
                      " (fora dos anos do calendário)";
    }
  }
  tx.commit();
  return out;
}
project::FictionalTimePoint WorldbuildingService::create_dated_point(
    const project::AxisCalendar &expected, project::CalendarDate date,
    std::string label, std::string description) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db);
  guard(db, p);
  exists(db, p, "fictional_time_axes", expected.axis_id);
  const auto base = read_binding(db, p, expected.axis_id);
  if (!base)
    throw std::runtime_error("Este eixo não tem calendário declarado");
  auto c = read_calendar(db, p, expected.calendar_id);
  project::FictionalTimePoint point{
      project::new_uuid(),
      expected.axis_id,
      project::calendar_ordinal(c, date, expected.epoch_ordinal),
      std::move(label),
      std::move(description),
      project::utc_now(),
      project::utc_now()};
  project::validate(point);
  const auto effective = effective_binding(
      *base, read_adoptions(db, p, expected.axis_id), point.ordinal);
  if (effective != expected)
    throw std::runtime_error(
        "O sistema escolhido não está vigente nessa data ou a adoção mudou. "
        "Confira o sistema e reabra a prévia; nenhum ponto foi criado.");
  auto duplicate =
      db.prepare("SELECT 1 FROM fictional_time_points WHERE project_id=? AND "
                 "axis_id=? AND ordinal=? LIMIT 1");
  duplicate.bind(1, p.manifest().project_id);
  duplicate.bind(2, point.axis_id);
  duplicate.bind(3, point.ordinal);
  if (duplicate.step())
    throw std::runtime_error("Já existe um ponto nessa data neste eixo. Abra o "
                             "ponto existente ou escolha outra data.");
  tx.record_project_operation("Criar ponto por data ficcional");
  auto q =
      db.prepare("INSERT INTO "
                 "fictional_time_points(id,project_id,axis_id,ordinal,label,"
                 "description,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?)");
  q.bind(1, point.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, point.axis_id);
  q.bind(4, point.ordinal);
  q.bind(5, point.label);
  q.bind(6, point.description);
  q.bind(7, point.created_at);
  q.bind(8, point.updated_at);
  q.run();
  tx.commit();
  return point;
}
CalendarMonthView
WorldbuildingService::month_view(const project::AxisCalendar &expected,
                                 std::int64_t year, int month,
                                 const std::string &regime) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  guard(db, p);
  exists(db, p, "fictional_time_axes", expected.axis_id);
  if (read_binding(db, p, expected.axis_id) != expected) {
    bool declared = false;
    for (const auto &a : read_adoptions(db, p, expected.axis_id))
      if (project::AxisCalendar{a.axis_id, a.calendar_id, a.epoch_ordinal, a.id} == expected)
        declared = true;
    if (!declared)
      throw std::runtime_error("Associação mudou; reabra a consulta mensal");
  }
  auto c = read_calendar(db, p, expected.calendar_id);
  auto start = project::calendar_ordinal(
           c, {year, month, project::calendar_first_day(c)},
           expected.epoch_ordinal),
       end = project::calendar_ordinal(c,
                                       {year, month,
                                        project::days_in_month(c, year, month) +
                                            project::calendar_first_day(c) - 1},
                                       expected.epoch_ordinal);
  CalendarMonthView out;
  if (regime != "all" && regime != "active" && regime != "parallel" && regime != "outside")
    throw std::runtime_error("Filtro de vigência inválido");
  const auto base = read_binding(db,p,expected.axis_id);
  const auto history = read_adoptions(db,p,expected.axis_id);
  std::vector<std::int64_t> visible;
  for (auto at = start;; ++at) {
    std::vector<project::AxisCalendar> views;
    if (base) views.push_back(*base);
    for (const auto &a : history) {
      if (a.from_ordinal > at) break;
      if (a.mode == "retire") views.clear();
      views.push_back({a.axis_id,a.calendar_id,a.epoch_ordinal,a.id});
    }
    const bool present = std::find(views.begin(),views.end(),expected) != views.end();
    const std::string state = !views.empty() && views.back() == expected ? "active" : present ? "parallel" : "outside";
    const int day = project::calendar_date(c,at,expected.epoch_ordinal).day;
    out.regimes[day] = state;
    if (regime == "all" || regime == state) visible.push_back(at);
    if (at == end) break;
  }
  for (const auto &a : history) if (a.from_ordinal >= start && a.from_ordinal <= end) {
    const int day = project::calendar_date(c,a.from_ordinal,expected.epoch_ordinal).day;
    out.cuts[day] = "Adoção: " + read_calendar(db,p,a.calendar_id).name +
        (a.mode == "retire" ? " — substitui os anteriores" : " — mantém apresentação paralela");
  }
  std::string where =
      " WHERE p.project_id=? AND p.axis_id=? AND p.ordinal BETWEEN ? AND ?";
  if (regime != "all") {
    if (visible.empty()) where += " AND 0";
    else {
      where += " AND p.ordinal IN(";
      for (std::size_t i=0;i<visible.size();++i) where += (i?",":"") + std::to_string(visible[i]);
      where += ")";
    }
  }
  auto bind = [&](SqliteStatement &q) {
    q.bind(1, p.manifest().project_id);
    q.bind(2, expected.axis_id);
    q.bind(3, start);
    q.bind(4, end);
  };
  auto count =
      db.prepare("SELECT count(*) FROM fictional_time_points p" + where);
  bind(count);
  count.step();
  out.matching_points = count.column_integer(0);
  auto points =
      db.prepare("SELECT "
                 "p.id,p.axis_id,p.ordinal,p.label,p.description,p.created_at,"
                 "p.updated_at FROM fictional_time_points p" +
                 where + " ORDER BY p.ordinal,p.id LIMIT 500");
  bind(points);
  while (points.step())
    out.points.push_back({points.column_text(0), points.column_text(1),
                          points.column_integer(2), points.column_text(3),
                          points.column_text(4), points.column_text(5),
                          points.column_text(6)});
  const std::string joins =
      " FROM fictional_time_points p JOIN event_occurrences o ON "
      "o.time_point_id=p.id AND o.project_id=p.project_id JOIN entities e ON "
      "e.id=o.event_entity_id AND e.project_id=o.project_id";
  auto total = db.prepare("SELECT count(*)" + joins + where);
  bind(total);
  total.step();
  out.matching_events = total.column_integer(0);
  auto events = db.prepare("SELECT p.id,e.name,p.ordinal" + joins + where +
                           " ORDER BY p.ordinal,o.id LIMIT 500");
  bind(events);
  while (events.step())
    out.events.push_back({events.column_text(0), events.column_text(1),
                          events.column_integer(2)});
  auto refs = db.prepare("SELECT id,axis_id,calendar_id,label,year,month,day,clock,note,token FROM temporal_references WHERE project_id=? AND axis_id=? ORDER BY year,month,day,label,id LIMIT 501");
  refs.bind(1,p.manifest().project_id); refs.bind(2,expected.axis_id);
  std::size_t reference_count{};
  std::map<std::string,project::FictionalCalendar> calendars{{c.id,c}};
  while (refs.step()) {
    if (++reference_count > 500) throw std::runtime_error("Consulta excede 500 referências; não é completa");
    auto r = read_reference(db,p,refs);
    if (!calendars.contains(r.calendar_id)) calendars[r.calendar_id] = read_calendar(db,p,r.calendar_id);
    const auto &source = calendars.at(r.calendar_id);
    std::vector<std::int64_t> epochs;
    if (base && base->calendar_id == r.calendar_id) epochs.push_back(base->epoch_ordinal);
    for (const auto &a : history) if (a.calendar_id == r.calendar_id) epochs.push_back(a.epoch_ordinal);
    std::sort(epochs.begin(),epochs.end()); epochs.erase(std::unique(epochs.begin(),epochs.end()),epochs.end());
    if (epochs.empty()) {
      try {
        const project::CalendarDate anchor{source.engine == "chinese" ? 1901 : 1,1,project::calendar_first_day(source)};
        const project::CalendarDate display{year,month,project::calendar_first_day(c)};
        auto offset = checked_subtract(start,project::terrestrial_day(c,display));
        auto terrestrial = project::terrestrial_day(source,anchor);
        // Both operands are bounded by the supported calendar engines.
        epochs.push_back(checked_subtract(checked_add(offset,terrestrial),project::calendar_ordinal(source,anchor)));
      } catch (const std::exception &) {}
    }
    if (epochs.size() != 1) { if(regime=="all") out.unpositioned.push_back(r); continue; }
    auto window = project::temporal_window(source,r,epochs.front());
    if (!window.comparable) { if(regime=="all") out.unpositioned.push_back(r); continue; }
    CalendarMonthView::ReferenceHit hit{r,{}};
    for (const auto at : visible) {
      if (at == std::numeric_limits<std::int64_t>::max()) throw std::runtime_error("Limite temporal excedido");
      if ((!window.lower_bound || project::compare_temporal_coordinates(*window.lower_bound,{at+1,0,1}) < 0) &&
          (!window.upper_bound || project::compare_temporal_coordinates(*window.upper_bound,{at,0,1}) > 0))
        hit.days.push_back(project::calendar_date(c,at,expected.epoch_ordinal).day);
    }
    if (!hit.days.empty()) out.references.push_back(std::move(hit));
  }
  std::vector<project::TemporalReference> ordered;
  for(const auto &hit:out.references) ordered.push_back(hit.reference);
  sort_reference_dates(db,p,ordered);
  std::map<std::string,std::size_t> position;
  for(std::size_t i=0;i<ordered.size();++i) position[ordered[i].id]=i;
  std::stable_sort(out.references.begin(),out.references.end(),[&](const auto &a,const auto &b) {
    return position.at(a.reference.id)<position.at(b.reference.id);
  });
  tx.commit();
  return out;
}
} // namespace inde::application

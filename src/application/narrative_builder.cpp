#include "inde/application/narrative_builder.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/writing.hpp"
#include "narrative_builder_support.hpp"
#include <algorithm>
#include <glib.h>
#include <limits>
#include <stdexcept>
namespace inde::application {
namespace narrative_builder_detail {
using namespace persistence;
using Row = std::vector<json::Value>;
SqliteDatabase open(const project::Project &p) {
  const auto path = ProjectDatabaseRepository::database_path(p.path());
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Banco do Projeto ausente");
  return SqliteDatabase(path);
}
void guard(SqliteDatabase &db, const project::Project &p) {
  if (SchemaMigrator{}.current_version(db) != current_database_schema_version ||
      db.query_text("SELECT id FROM projects") != p.manifest().project_id)
    throw std::runtime_error("Projeto/schema incompatível; reabra o Projeto");
}
std::vector<Row> rows(SqliteDatabase &db, const std::string &sql,
                      const std::string &id, int columns) {
  auto q = db.prepare(sql);
  q.bind(1, id);
  std::vector<Row> result;
  std::size_t bytes{};
  while (q.step()) {
    if (result.size() >= 10000)
      throw std::runtime_error(
          "Consulta excede 10.000 associações; não é uma leitura completa");
    Row r;
    for (int c = 0; c < columns; ++c) {
      if (q.column_is_null(c))
        r.emplace_back();
      else {
        auto text = q.column_text(c);
        bytes += text.size();
        r.emplace_back(std::move(text));
      }
    }
    if (bytes > 4 * 1024 * 1024)
      throw std::runtime_error(
          "Consulta de planejamento excede 4 MiB; reduza o escopo");
    result.push_back(std::move(r));
  }
  return result;
}
const std::string &str(const Row &r, std::size_t n) {
  return r.at(n).as_string();
}
NarrativeBuilderSnapshot read(SqliteDatabase &db, const project::Project &p,
                              const std::string &id) {
  guard(db, p);
  auto owner = db.prepare("SELECT id,work_id,name,description FROM "
                          "narrative_structures WHERE id=? AND project_id=?");
  owner.bind(1, id);
  owner.bind(2, p.manifest().project_id);
  if (!owner.step())
    throw std::runtime_error("Estrutura narrativa ausente neste Projeto");
  NarrativeBuilderSnapshot s;
  s.project_id = p.manifest().project_id;
  s.structure_id = id;
  s.work_id = owner.column_text(1);
  s.name = owner.column_text(2);
  s.description = owner.column_text(3);
  json::Value::Array evidence{json::Value(s.project_id), json::Value(id),
                              json::Value(s.work_id), json::Value(s.name),
                              json::Value(s.description)};
  auto capture = [&](const std::string &sql, int columns) {
    auto result = rows(db, sql, id, columns);
    json::Value::Array v;
    for (const auto &r : result)
      v.emplace_back(r);
    evidence.emplace_back(std::move(v));
    return result;
  };
  auto units = capture(
      "SELECT "
      "id,structure_id,designator,title,summary,purpose,perspective,"
      "CAST(position AS TEXT),created_at,updated_at FROM narrative_units WHERE "
      "structure_id=? ORDER BY position,id",
      10);
  if (units.size() > 2000)
    throw std::runtime_error("Construtor limitado a 2.000 unidades por "
                             "estrutura; nada foi truncado");
  for (const auto &r : units)
    s.units.push_back({str(r, 0), str(r, 1), str(r, 2), str(r, 3), str(r, 4),
                       str(r, 5), str(r, 6), std::stoll(str(r, 7)), str(r, 8),
                       str(r, 9)});
  for (const auto &r :
       capture("SELECT "
               "id,structure_id,name,description,CAST(position AS "
               "TEXT),created_at,updated_at "
               "FROM narrative_lines WHERE structure_id=? ORDER BY position,id",
               7))
    s.lines.push_back({str(r, 0), str(r, 1), str(r, 2), str(r, 3),
                       std::stoll(str(r, 4)), str(r, 5), str(r, 6)});
  for (const auto &r : capture(
           "SELECT "
           "c.unit_id,c.location_entity_id,e.name,CAST(c.writing_position AS "
           "TEXT),c.token "
           "FROM narrative_unit_contexts c JOIN narrative_units u ON "
           "u.id=c.unit_id LEFT JOIN entities e ON e.id=c.location_entity_id "
           "WHERE u.structure_id=? ORDER BY c.unit_id",
           5))
    s.contexts.push_back({str(r, 0), r[1].is_null() ? "" : str(r, 1),
                          r[2].is_null() ? "" : str(r, 2),
                          r[3].is_null() ? std::optional<std::int64_t>{}
                                         : std::stoll(str(r, 3))});
  for (const auto &r :
       capture("SELECT "
               "r.id,r.unit_id,r.document_id,d.title,d.updated_at,r.note FROM "
               "narrative_realizations r JOIN narrative_units u ON "
               "u.id=r.unit_id JOIN documents d ON d.id=r.document_id WHERE "
               "u.structure_id=? ORDER BY r.id",
               6))
    s.realizations.push_back(
        {str(r, 0), str(r, 1), str(r, 2), str(r, 3), str(r, 5)});
  for (const auto &r : capture(
           "SELECT x.id,x.unit_id,x.relation_id,a.name||' → '||b.name||' · "
           "'||t.name,r.updated_at FROM narrative_unit_relations x JOIN "
           "narrative_units u ON u.id=x.unit_id JOIN relations r ON "
           "r.id=x.relation_id JOIN entities a ON a.id=r.source_entity_id JOIN "
           "entities b ON b.id=r.target_entity_id JOIN relation_types t ON "
           "t.id=r.relation_type_id WHERE u.structure_id=? ORDER BY x.id",
           5))
    s.relations.push_back({str(r, 0), str(r, 1), str(r, 2), str(r, 3)});
  for (const auto &r : capture(
           "SELECT m.unit_id,m.line_id,CAST(m.position AS TEXT) FROM "
           "narrative_unit_lines m "
           "JOIN narrative_units u ON u.id=m.unit_id WHERE u.structure_id=? "
           "ORDER BY m.unit_id,m.line_id",
           3))
    s.memberships.push_back({str(r, 0), str(r, 1), std::stoll(str(r, 2))});
  for (const auto &r : capture(
           "SELECT m.unit_id,m.entity_id,m.role FROM narrative_unit_entities m "
           "JOIN narrative_units u ON u.id=m.unit_id WHERE u.structure_id=? "
           "ORDER BY m.unit_id,m.entity_id,m.role",
           3))
    s.entities.push_back({str(r, 0), str(r, 1), str(r, 2)});
  for (const auto &r :
       capture("SELECT "
               "id,source_unit_id,target_unit_id,kind,label,created_at,updated_"
               "at FROM "
               "narrative_links WHERE structure_id=? ORDER BY id",
               7))
    s.links.push_back({str(r, 0), id, str(r, 1), str(r, 2),
                       project::narrative_link_kind_from_string(str(r, 3)),
                       str(r, 4), str(r, 5), str(r, 6)});
  for (const auto &r :
       capture("SELECT "
               "a.id,a.unit_id,a.line_id,a.reference_id,a.role,a.note,a."
               "meaning,a.token FROM "
               "temporal_attachments a WHERE a.unit_id IN(SELECT id FROM "
               "narrative_units WHERE structure_id=?1) OR a.line_id IN(SELECT "
               "id FROM narrative_lines WHERE structure_id=?1) ORDER BY a.id",
               8))
    s.dates.push_back({str(r, 0), r[1].is_null() ? "" : str(r, 1),
                       r[2].is_null() ? "" : str(r, 2), str(r, 3), str(r, 4),
                       str(r, 5), str(r, 6)});
  for (const auto &r :
       capture("SELECT "
               "id,role_id,entity_id,unit_id,notes,created_at,updated_at FROM "
               "narrative_role_assignments WHERE structure_id=? ORDER BY id",
               7))
    s.roles.push_back({str(r, 0), str(r, 1), str(r, 2), s.work_id, id,
                       r[3].is_null() ? std::nullopt : std::optional{str(r, 3)},
                       str(r, 4), str(r, 5), str(r, 6)});
  for (const auto &r :
       capture("SELECT d.link_id,d.scope FROM narrative_dependency_scopes d "
               "JOIN narrative_links l ON l.id=d.link_id WHERE "
               "l.structure_id=? ORDER BY d.link_id",
               2))
    s.scopes.push_back({str(r, 0), str(r, 1)});
  for (const auto &r :
       capture("SELECT observation_json,source_revision,source_structure_id "
               "FROM narrative_branch_provenance WHERE structure_id=?",
               3))
    s.provenance_json = str(r, 0);
  auto payload = json::serialize(evidence, false);
  payload += read_targets(db, p, s);
  if (payload.size() > 8 * 1024 * 1024)
    throw std::runtime_error("Snapshot excede 8 MiB; nada foi truncado");
  auto *hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256, payload.c_str(),
                                             payload.size());
  s.revision = hash;
  g_free(hash);
  return s;
}
void check(SqliteDatabase &db, const project::Project &p,
           const NarrativeBuilderSnapshot &s) {
  if (s.project_id != p.manifest().project_id ||
      read(db, p, s.structure_id).revision != s.revision)
    throw std::runtime_error("Planejamento mudou desde a consulta; atualize "
                             "antes de confirmar. Nada foi alterado.");
}
void unit(SqliteDatabase &db, const project::Project &p,
          const NarrativeBuilderSnapshot &s, const std::string &id) {
  auto q = db.prepare("SELECT id FROM narrative_units WHERE id=? AND "
                      "structure_id=? AND project_id=?");
  q.bind(1, id);
  q.bind(2, s.structure_id);
  q.bind(3, p.manifest().project_id);
  if (!q.step())
    throw std::runtime_error("Unidade ausente da estrutura observada");
}
void context(SqliteDatabase &db, const project::Project &p,
             const std::string &id) {
  auto q = db.prepare(
      "INSERT INTO narrative_unit_contexts(unit_id,project_id,token) "
      "VALUES(?,?,?) ON CONFLICT(unit_id) DO UPDATE SET token=excluded.token");
  q.bind(1, id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, project::new_uuid());
  q.run();
}
void attach(SqliteDatabase &db, const project::Project &p, const std::string &u,
            const std::string &d) {
  auto q = db.prepare(
      "INSERT INTO narrative_realizations(id,project_id,unit_id,document_id) "
      "VALUES(?,?,?,?)");
  q.bind(1, project::new_uuid());
  q.bind(2, p.manifest().project_id);
  q.bind(3, u);
  q.bind(4, d);
  q.run();
}
} // namespace narrative_builder_detail
using namespace narrative_builder_detail;
const project::Project &NarrativeBuilder::owner() const {
  if (!session_.current())
    throw std::runtime_error("Abra um Projeto");
  return *session_.current();
}
NarrativeBuilderSnapshot
NarrativeBuilder::snapshot(const std::string &id) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  auto s = read(db, p, id);
  tx.commit();
  return s;
}
project::NarrativeUnit
NarrativeBuilder::create_unit(const NarrativeBuilderSnapshot &s,
                              project::NarrativeUnit v,
                              const std::string &line) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Criar unidade na montagem narrativa");
  check(db, p, s);
  const auto current = read(db, p, s.structure_id);
  if (current.units.size() >= 2000)
    throw std::runtime_error("Limite de 2.000 unidades; nada foi criado");
  v.id = project::new_uuid();
  v.structure_id = s.structure_id;
  v.created_at = v.updated_at = project::utc_now();
  const auto last = current.units.empty() ? 0 : current.units.back().position;
  if (last > std::numeric_limits<std::int64_t>::max() - 1000)
    throw std::runtime_error("Posição excessiva; reorganize a montagem");
  v.position = last + 1000;
  project::validate(v);
  auto q = db.prepare("INSERT INTO "
                      "narrative_units(id,project_id,structure_id,designator,"
                      "title,summary,purpose,perspective,position,created_at,"
                      "updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?)");
  q.bind(1, v.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, s.structure_id);
  q.bind(4, v.designator);
  q.bind(5, v.title);
  q.bind(6, v.summary);
  q.bind(7, v.purpose);
  q.bind(8, v.perspective);
  q.bind(9, v.position);
  q.bind(10, v.created_at);
  q.bind(11, v.updated_at);
  q.run();
  if (!line.empty()) {
    auto at = std::find_if(current.lines.begin(), current.lines.end(),
                           [&](auto &l) { return l.id == line; });
    if (at == current.lines.end())
      throw std::runtime_error("Linha ausente; a unidade não foi criada");
    auto m = db.prepare(
        "INSERT INTO narrative_unit_lines(project_id,unit_id,line_id,position) "
        "SELECT ?,?,?,coalesce(max(position),0)+1000 FROM narrative_unit_lines "
        "WHERE line_id=?");
    m.bind(1, p.manifest().project_id);
    m.bind(2, v.id);
    m.bind(3, line);
    m.bind(4, line);
    m.run();
  }
  tx.commit();
  return v;
}
void NarrativeBuilder::update_unit(const NarrativeBuilderSnapshot &s,
                                   project::NarrativeUnit v) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Editar unidade narrativa");
  check(db, p, s);
  unit(db, p, s, v.id);
  const auto current = read(db, p, s.structure_id);
  auto old = std::find_if(current.units.begin(), current.units.end(),
                          [&](auto &u) { return u.id == v.id; });
  // Caller fields never authorize changes of ownership or any order.
  if (old == current.units.end() || v.structure_id != s.structure_id ||
      v.position != old->position)
    throw std::runtime_error(
        "Use o comando da ordem escolhida para mover a unidade");
  project::validate(v);
  auto q = db.prepare("UPDATE narrative_units SET "
                      "designator=?,title=?,summary=?,purpose=?,perspective=?,"
                      "updated_at=? WHERE id=?");
  q.bind(1, v.designator);
  q.bind(2, v.title);
  q.bind(3, v.summary);
  q.bind(4, v.purpose);
  q.bind(5, v.perspective);
  q.bind(6, project::utc_now());
  q.bind(7, v.id);
  q.run();
  context(db, p, v.id);
  tx.commit();
}
void NarrativeBuilder::update_line(const NarrativeBuilderSnapshot &s,
                                   project::NarrativeLine v) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Editar linha narrativa");
  check(db, p, s);
  const auto current = read(db, p, s.structure_id);
  auto at = std::find_if(current.lines.begin(), current.lines.end(),
                         [&](auto &l) { return l.id == v.id; });
  if (at == current.lines.end() || v.structure_id != s.structure_id ||
      v.position != at->position)
    throw std::runtime_error("Linha incompatível com a consulta");
  project::validate(v);
  auto q = db.prepare("UPDATE narrative_lines SET "
                      "name=?,description=?,updated_at=? WHERE id=?");
  q.bind(1, v.name);
  q.bind(2, v.description);
  q.bind(3, project::utc_now());
  q.bind(4, v.id);
  q.run();
  tx.commit();
}
void NarrativeBuilder::queue_unit(const NarrativeBuilderSnapshot &s,
                                  const std::string &id, bool include) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, include ? "Adicionar à fila de escrita"
                                   : "Retirar da fila de escrita");
  check(db, p, s);
  unit(db, p, s, id);
  context(db, p, id);
  if (include) {
    auto maximum = db.prepare("SELECT coalesce(max(c.writing_position),0) FROM "
                              "narrative_unit_contexts c JOIN narrative_units "
                              "u ON u.id=c.unit_id WHERE u.structure_id=?");
    maximum.bind(1, s.structure_id);
    maximum.step();
    if (maximum.column_integer(0) >
        std::numeric_limits<std::int64_t>::max() - 1000)
      throw std::runtime_error("Fila de escrita excede o limite de posições");
  }
  auto q = db.prepare(
      include ? "UPDATE narrative_unit_contexts SET writing_position=(SELECT "
                "coalesce(max(c.writing_position),0)+1000 FROM "
                "narrative_unit_contexts c JOIN narrative_units u ON "
                "u.id=c.unit_id WHERE u.structure_id=?) WHERE unit_id=? AND "
                "writing_position IS NULL"
              : "UPDATE narrative_unit_contexts SET writing_position=NULL "
                "WHERE ? IS NOT NULL AND unit_id=?");
  q.bind(1, s.structure_id);
  q.bind(2, id);
  q.run();
  tx.commit();
}
void NarrativeBuilder::move_unit(const NarrativeBuilderSnapshot &s,
                                 const std::string &id, BuilderOrder order,
                                 int direction) const {
  if (direction != -1 && direction != 1)
    throw std::runtime_error("Direção inválida");
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, order == BuilderOrder::Narrative
                               ? "Mover na montagem narrativa"
                               : "Mover na fila de escrita");
  check(db, p, s);
  unit(db, p, s, id);
  const auto current = read(db, p, s.structure_id);
  std::vector<std::pair<std::int64_t, std::string>> list;
  if (order == BuilderOrder::Narrative) {
    for (auto &u : current.units)
      list.emplace_back(u.position, u.id);
  } else if (order == BuilderOrder::WritingQueue) {
    for (auto &c : current.contexts)
      if (c.writing_position)
        list.emplace_back(*c.writing_position, c.unit_id);
  } else {
    throw std::runtime_error("Ordem inválida");
  }
  std::sort(list.begin(), list.end());
  auto at = std::find_if(list.begin(), list.end(),
                         [&](auto &x) { return x.second == id; });
  if (at == list.end())
    throw std::runtime_error("Unidade não está na ordem escolhida");
  auto index = std::distance(list.begin(), at), next = index + direction;
  if (next < 0 || next >= static_cast<std::ptrdiff_t>(list.size())) {
    tx.commit();
    return;
  }
  std::swap(list[index], list[next]);
  auto q = db.prepare(
      order == BuilderOrder::Narrative
          ? "UPDATE narrative_units SET position=?,updated_at=? WHERE id=?"
          : "UPDATE narrative_unit_contexts SET writing_position=?,token=? "
            "WHERE unit_id=?");
  for (std::size_t i = 0; i < list.size(); ++i) {
    q.bind(1, static_cast<std::int64_t>((i + 1) * 1000));
    q.bind(2, order == BuilderOrder::Narrative ? project::utc_now()
                                               : project::new_uuid());
    q.bind(3, list[i].second);
    q.run();
    q.reset();
  }
  if (order == BuilderOrder::Narrative)
    context(db, p, id);
  tx.commit();
}
void NarrativeBuilder::set_location(const NarrativeBuilderSnapshot &s,
                                    const std::string &id,
                                    const std::string &location) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Contexto de Local da unidade");
  check(db, p, s);
  unit(db, p, s, id);
  context(db, p, id);
  auto q = db.prepare("UPDATE narrative_unit_contexts SET location_entity_id=? "
                      "WHERE unit_id=?");
  if (location.empty())
    q.bind_null(1);
  else
    q.bind(1, location);
  q.bind(2, id);
  q.run();
  tx.commit();
}
void NarrativeBuilder::attach_document(const NarrativeBuilderSnapshot &s,
                                       const std::string &id,
                                       const std::string &doc) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Vincular realização textual");
  check(db, p, s);
  unit(db, p, s, id);
  attach(db, p, id, doc);
  tx.commit();
}
std::string NarrativeBuilder::create_document(const NarrativeBuilderSnapshot &s,
                                              const std::string &id,
                                              std::string title) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Criar Documento e realização narrativa");
  check(db, p, s);
  unit(db, p, s, id);
  project::Document d{};
  d.id = project::new_uuid();
  d.title = std::move(title);
  d.created_at = d.updated_at = project::utc_now();
  project::validate(d);
  auto q = db.prepare("INSERT INTO "
                      "documents(id,project_id,title,content,created_at,"
                      "updated_at) VALUES(?,?,?,'',?,?)");
  q.bind(1, d.id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, d.title);
  q.bind(4, d.created_at);
  q.bind(5, d.updated_at);
  q.run();
  attach(db, p, id, d.id);
  tx.commit();
  return d.id;
}
void NarrativeBuilder::attach_relation(const NarrativeBuilderSnapshot &s,
                                       const std::string &id,
                                       const std::string &relation) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Relação em foco na unidade");
  check(db, p, s);
  unit(db, p, s, id);
  auto q = db.prepare(
      "INSERT INTO narrative_unit_relations(id,project_id,unit_id,relation_id) "
      "VALUES(?,?,?,?)");
  q.bind(1, project::new_uuid());
  q.bind(2, p.manifest().project_id);
  q.bind(3, id);
  q.bind(4, relation);
  q.run();
  tx.commit();
}
void NarrativeBuilder::detach(const NarrativeBuilderSnapshot &s,
                              const std::string &kind,
                              const std::string &id) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Retirar associação da unidade");
  check(db, p, s);
  const auto table = kind == "document"   ? "narrative_realizations"
                     : kind == "target" ? "narrative_text_targets"
                     : kind == "relation" ? "narrative_unit_relations"
                                          : nullptr;
  if (!table)
    throw std::runtime_error("Associação inválida");
  if(kind=="document" || kind=="target")record_text_detach(db,p,s,id);
  auto q = db.prepare("DELETE FROM " + std::string(table) +
                      " WHERE id=? AND unit_id IN(SELECT id FROM "
                      "narrative_units WHERE structure_id=?)");
  q.bind(1, id);
  q.bind(2, s.structure_id);
  q.run();
  if (db.changes() != 1)
    throw std::runtime_error("Associação ausente da estrutura");
  tx.commit();
}
} // namespace inde::application

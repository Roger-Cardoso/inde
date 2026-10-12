#include "inde/persistence/semantic_store.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/semantic_codec.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace inde::persistence {
namespace {
std::string owner(SqliteDatabase &db) {
  auto q = db.prepare("SELECT id FROM projects");
  if (!q.step())
    throw std::runtime_error("Projeto ausente");
  auto id = q.column_text(0);
  if (q.step())
    throw std::runtime_error("Mais de um Projeto no banco");
  return id;
}
std::vector<project::AnnotationTypeDefinition> types(SqliteDatabase &db,
                                                     const std::string &p) {
  auto out = project::builtin_annotation_types();
  auto q = db.prepare("SELECT definition_json FROM semantic_type_definitions "
                      "WHERE project_id=? ORDER BY type_key,type_version");
  q.bind(1, p);
  while (q.step()) {
    auto t = decode_semantic_type(q.column_text(0));
    if (t.system || t.key.starts_with("inde."))
      throw std::runtime_error(
          "Definição externa tentou substituir namespace protegido");
    out.push_back(std::move(t));
  }
  return out;
}
void validate_fields(SqliteDatabase &db, const std::string &p,
                     const project::Document &d) {
  auto ts = types(db, p);
  for (auto &diagnostic : project::semantic_diagnostics(d, ts))
    if (diagnostic.integrity)
      throw std::runtime_error(diagnostic.message);
  struct Interval {
    std::size_t start, end;
    bool restricted;
  };
  std::vector<Interval> intervals;
  for (auto &a : d.semantics.annotations) {
    auto *t = project::find_annotation_type(ts, a.type_key, a.type_version);
    auto anchor = std::find_if(d.anchors.begin(), d.anchors.end(),
                               [&](auto &v) { return v.id == a.anchor_id; });
    auto e =
        std::find_if(d.semantics.anchors.begin(), d.semantics.anchors.end(),
                     [&](auto &v) { return v.anchor_id == a.anchor_id; });
    if (t && a.status != "archived" && anchor != d.anchors.end() &&
        e != d.semantics.anchors.end() &&
        e->state == project::AnchorResolution::Resolved)
      intervals.push_back(
          {anchor->start_offset, anchor->end_offset, !t->allow_overlap});
  }
  std::sort(intervals.begin(), intervals.end(),
            [](auto &a, auto &b) { return a.start < b.start; });
  std::size_t max_end{}, restricted_end{};
  for (auto &i : intervals) {
    if (i.start < restricted_end || (i.restricted && i.start < max_end))
      throw std::runtime_error(
          "Sobreposição proibida pela definição do tipo semântico");
    max_end = std::max(max_end, i.end);
    if (i.restricted)
      restricted_end = std::max(restricted_end, i.end);
  }
  for (auto &r : d.semantics.relations)
    if (std::none_of(d.semantics.annotations.begin(),
                     d.semantics.annotations.end(),
                     [&](auto &a) { return a.id == r.target_annotation_id; })) {
      auto q = db.prepare(
          "SELECT id FROM semantic_annotations WHERE project_id=? AND id=?");
      q.bind(1, p);
      q.bind(2, r.target_annotation_id);
      if (!q.step())
        throw std::runtime_error("Anotação alvo ausente no Projeto");
    }
  for (auto &a : d.semantics.annotations) {
    auto *t = project::find_annotation_type(ts, a.type_key, a.type_version);
    // Unknown definitions remain opaque and recoverable. Typed entity IDs are
    // still real FKs.
    for (const auto &b : a.dates) {
      auto q=db.prepare("SELECT id FROM temporal_references WHERE project_id=? AND id=?");
      q.bind(1,p);q.bind(2,b.reference_id);
      if(!q.step()) throw std::runtime_error("Data textual ausente neste Projeto; escolha uma referência existente");
    }
    for (const auto &b : a.fictional_time) {
      auto point = [&](const std::string &id) {
        auto q = db.prepare("SELECT axis_id,ordinal FROM fictional_time_points "
                            "WHERE project_id=? AND id=?");
        q.bind(1, p);
        q.bind(2, id);
        if (!q.step() || q.column_text(0) != b.fictional_axis_id)
          throw std::runtime_error("Vínculo temporal exige pontos existentes "
                                   "no eixo ficcional declarado");
        return q.column_integer(1);
      };
      const auto start = point(b.fictional_start_point_id);
      if (b.fictional_end_point_id && point(*b.fictional_end_point_id) < start)
        throw std::runtime_error(
            "Fim temporal anterior ao início; nenhum vínculo aplicado");
    }
    for (auto &[key, v] : a.fields) {
      const project::SemanticFieldDefinition *f = nullptr;
      if (t) {
        auto it = std::find_if(t->fields.begin(), t->fields.end(),
                               [&](auto &x) { return x.key == key; });
        if (it != t->fields.end())
          f = &*it;
      }
      if (f) {
        if (f->kind != v.kind || (!f->multiple && v.values.size() > 1))
          throw std::runtime_error(
              "Campo semântico incompatível com a definição");
        if (v.kind == project::SemanticFieldKind::Enum)
          for (auto &x : v.values)
            if (std::find(f->values.begin(), f->values.end(), x) ==
                f->values.end())
              throw std::runtime_error("Opção semântica desconhecida");
      }
      if (v.kind == project::SemanticFieldKind::Entity)
        for (auto &id : v.values) {
          auto q =
              db.prepare("SELECT t.key FROM entities e JOIN entity_types t ON "
                         "t.id=e.entity_type_id AND t.project_id=e.project_id "
                         "WHERE e.project_id=? AND e.id=?");
          q.bind(1, p);
          q.bind(2, id);
          if (!q.step())
            throw std::runtime_error(
                "Referência semântica a entidade ausente no Projeto");
          if (f && !f->entity_type_keys.empty() &&
              std::find(f->entity_type_keys.begin(), f->entity_type_keys.end(),
                        q.column_text(0)) == f->entity_type_keys.end())
            throw std::runtime_error(
                "Tipo de entidade incompatível com o campo semântico");
        }
    }
    if (t && a.status == "active")
      for (auto &f : t->fields)
        if (f.required) {
          auto v = a.fields.find(f.key);
          if (v == a.fields.end() || v->second.values.empty())
            throw std::runtime_error("Anotação ativa sem campo obrigatório; "
                                     "salve explicitamente como incompleta");
        }
  }
}
} // namespace
void SemanticStore::validate(const std::filesystem::path &p,
                             const project::Document &d) const {
  project::validate(d);
  SqliteDatabase db(ProjectDatabaseRepository::database_path(p));
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  validate_fields(db, owner(db), d);
  tx.commit();
}
std::vector<project::AnnotationTypeDefinition>
SemanticStore::types(const std::filesystem::path &p) const {
  SqliteDatabase db(ProjectDatabaseRepository::database_path(p));
  return persistence::types(db, owner(db));
}
void SemanticStore::add_type(const std::filesystem::path &p,
                             const project::AnnotationTypeDefinition &t) const {
  project::validate(t);
  if (t.system || !t.key.starts_with("project."))
    throw std::runtime_error("Tipos personalizados usam namespace project.; "
                             "tipos nativos são protegidos");
  SqliteDatabase db(ProjectDatabaseRepository::database_path(p));
  SqliteTransaction tx(db);
  auto q = db.prepare("INSERT INTO "
                      "semantic_type_definitions(project_id,type_key,type_"
                      "version,definition_json) VALUES(?,?,?,?)");
  q.bind(1, owner(db));
  q.bind(2, t.key);
  q.bind(3, std::int64_t(t.version));
  q.bind(4, encode_semantic_type(t));
  q.run();
  tx.commit();
}
void load_semantic_layers(SqliteDatabase &db, const std::string &p,
                          project::Document &d) {
  auto q = db.prepare("SELECT payload FROM document_semantics WHERE "
                      "project_id=? AND document_id=?");
  q.bind(1, p);
  q.bind(2, d.id);
  if (q.step())
    d.semantics = decode_semantics(q.column_text(0));
}
void save_semantic_layers(SqliteDatabase &db, const std::string &p,
                          const project::Document &d) {
  project::validate_semantic_structure(d);
  validate_fields(db, p, d);
  for (auto table :
       {"semantic_entity_bindings", "semantic_annotation_relations",
        "semantic_fictional_time_bindings", "semantic_date_bindings"}) {
    auto q = db.prepare("DELETE FROM " + std::string(table) +
                        " WHERE project_id=? AND document_id=?");
    q.bind(1, p);
    q.bind(2, d.id);
    q.run();
  }
  std::set<std::string> retained;
  for (auto &a : d.semantics.annotations)
    retained.insert(a.id);
  auto old = db.prepare("SELECT id FROM semantic_annotations WHERE "
                        "project_id=? AND document_id=?");
  old.bind(1, p);
  old.bind(2, d.id);
  std::vector<std::string> removed;
  while (old.step())
    if (!retained.contains(old.column_text(0)))
      removed.push_back(old.column_text(0));
  for (auto &id : removed) {
    auto q = db.prepare("DELETE FROM semantic_annotations WHERE project_id=? "
                        "AND document_id=? AND id=?");
    q.bind(1, p);
    q.bind(2, d.id);
    q.bind(3, id);
    q.run();
  }
  // Temporarily archive global projections so upsert order cannot violate the
  // one-current-perspective invariant. This entire aggregate is transactional.
  auto reset_global =
      db.prepare("UPDATE semantic_annotations SET status='archived' WHERE "
                 "project_id=? AND document_id=? AND scope='document' AND type_key='inde.semantic.perspective'");
  reset_global.bind(1, p);
  reset_global.bind(2, d.id);
  reset_global.run();
  for (auto &a : d.semantics.annotations) {
    std::string category;
    auto c = a.fields.find("category");
    if (c != a.fields.end() && !c->second.values.empty())
      category = c->second.values.front();
    auto q = db.prepare(
        "INSERT INTO "
        "semantic_annotations(id,project_id,document_id,anchor_id,type_key,"
        "type_version,status,category,scope) VALUES(?,?,?,?,?,?,?,?,?) ON "
        "CONFLICT(id) "
        "DO UPDATE SET "
        "anchor_id=excluded.anchor_id,type_key=excluded.type_key,type_version="
        "excluded.type_version,status=excluded.status,scope=excluded.scope,"
        "category=excluded."
        "category WHERE semantic_annotations.project_id=excluded.project_id "
        "AND semantic_annotations.document_id=excluded.document_id");
    q.bind(1, a.id);
    q.bind(2, p);
    q.bind(3, d.id);
    if (a.scope == project::AnnotationScope::Document)
      q.bind_null(4);
    else
      q.bind(4, a.anchor_id);
    q.bind(5, a.type_key);
    q.bind(6, std::int64_t(a.type_version));
    q.bind(7, a.status);
    q.bind(8, category);
    q.bind(9, project::to_string(a.scope));
    q.run();
    if (db.changes() != 1)
      throw std::runtime_error("Anotação já pertence a outro Documento");
    for(const auto &b:a.dates) {
      auto bind=db.prepare("INSERT INTO semantic_date_bindings(id,project_id,document_id,annotation_id,reference_id,role) VALUES(?,?,?,?,?,?)");
      bind.bind(1,b.id);bind.bind(2,p);bind.bind(3,d.id);bind.bind(4,a.id);bind.bind(5,b.reference_id);bind.bind(6,b.role);bind.run();
    }
    for (const auto &b : a.fictional_time) {
      auto bind =
          db.prepare("INSERT INTO "
                     "semantic_fictional_time_bindings(id,project_id,document_"
                     "id,annotation_id,fictional_axis_id,fictional_start_point_"
                     "id,fictional_end_point_id) VALUES(?,?,?,?,?,?,?)");
      bind.bind(1, b.id);
      bind.bind(2, p);
      bind.bind(3, d.id);
      bind.bind(4, a.id);
      bind.bind(5, b.fictional_axis_id);
      bind.bind(6, b.fictional_start_point_id);
      if (b.fictional_end_point_id)
        bind.bind(7, *b.fictional_end_point_id);
      else
        bind.bind_null(7);
      bind.run();
    }
    for (auto &[key, f] : a.fields)
      if (f.kind == project::SemanticFieldKind::Entity)
        for (std::size_t n = 0; n < f.values.size(); ++n) {
          auto bind = db.prepare(
              "INSERT INTO "
              "semantic_entity_bindings(project_id,document_id,annotation_id,"
              "field_key,ordinal,entity_id) VALUES(?,?,?,?,?,?)");
          bind.bind(1, p);
          bind.bind(2, d.id);
          bind.bind(3, a.id);
          bind.bind(4, key);
          bind.bind(5, std::int64_t(n));
          bind.bind(6, f.values[n]);
          bind.run();
        }
  }
  for (auto &r : d.semantics.relations) {
    auto q = db.prepare("INSERT INTO "
                        "semantic_annotation_relations(id,project_id,document_"
                        "id,source_id,target_id,type_key) VALUES(?,?,?,?,?,?)");
    q.bind(1, r.id);
    q.bind(2, p);
    q.bind(3, d.id);
    q.bind(4, r.source_annotation_id);
    q.bind(5, r.target_annotation_id);
    q.bind(6, r.type_key);
    q.run();
  }
  auto q = db.prepare(
      "INSERT INTO document_semantics(document_id,project_id,payload) "
      "VALUES(?,?,?) ON CONFLICT(document_id) DO UPDATE SET "
      "payload=excluded.payload WHERE "
      "document_semantics.project_id=excluded.project_id");
  q.bind(1, d.id);
  q.bind(2, p);
  q.bind(3, encode_semantics(d.semantics));
  q.run();
  if (db.changes() != 1)
    throw std::runtime_error("Identidade semântica do Documento divergente");
}
SemanticUsePage SemanticStore::uses(const std::filesystem::path &p,
                                    const SemanticUseQuery &filter) const {
  if (!filter.limit || filter.limit > 200 || filter.offset > 10000000)
    throw std::runtime_error("Paginação semântica inválida");
  SqliteDatabase db(ProjectDatabaseRepository::database_path(p));
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  std::string where = " WHERE a.project_id=?";
  std::vector<std::string> args{owner(db)};
  if (filter.fictional_point_id) {
    if (!filter.fictional_axis_id)
      throw std::runtime_error(
          "Declare o eixo ficcional para filtrar por ponto");
    auto point = db.prepare("SELECT axis_id FROM fictional_time_points WHERE "
                            "project_id=? AND id=?");
    point.bind(1, args.front());
    point.bind(2, *filter.fictional_point_id);
    if (!point.step() || point.column_text(0) != *filter.fictional_axis_id)
      throw std::runtime_error(
          "Ponto do filtro não pertence ao eixo ficcional declarado");
  }
  auto condition = [&](const std::optional<std::string> &v,
                       const std::string &sql) {
    if (v) {
      where += sql;
      args.push_back(*v);
    }
  };
  if (!filter.include_archived)
    where += " AND a.status!='archived'";
  condition(filter.document_id, " AND a.document_id=?");
  condition(filter.annotation_id, " AND a.id=?");
  condition(filter.exclude_document_id, " AND a.document_id!=?");
  condition(filter.type_key, " AND a.type_key=?");
  condition(filter.category, " AND a.category=?");
  if (filter.annotation_scope)
    condition(project::to_string(*filter.annotation_scope), " AND a.scope=?");
  if (filter.fictional_axis_id) {
    where += " AND EXISTS(SELECT 1 FROM semantic_fictional_time_bindings bt "
             "WHERE bt.project_id=a.project_id AND bt.annotation_id=a.id AND "
             "bt.fictional_axis_id=?";
    args.push_back(*filter.fictional_axis_id);
    if (filter.fictional_point_id) {
      where +=
          " AND (SELECT ordinal FROM fictional_time_points WHERE id=?) BETWEEN "
          "(SELECT ordinal FROM fictional_time_points WHERE "
          "id=bt.fictional_start_point_id) AND (SELECT ordinal FROM "
          "fictional_time_points WHERE "
          "id=COALESCE(bt.fictional_end_point_id,bt.fictional_start_point_id))";
      args.push_back(*filter.fictional_point_id);
    }
    where += ")";
  }
  condition(
      filter.work_id,
      " AND EXISTS(SELECT 1 FROM documents scope_doc JOIN editorial_nodes "
      "scope_node ON scope_node.id=scope_doc.editorial_node_id WHERE "
      "scope_doc.id=a.document_id AND scope_doc.project_id=a.project_id AND "
      "scope_node.work_id=?)");
  condition(
      filter.entity_id,
      " AND EXISTS(SELECT 1 FROM semantic_entity_bindings b WHERE "
      "b.project_id=a.project_id AND b.annotation_id=a.id AND b.entity_id=?)");
  condition(
      filter.target_annotation_id,
      " AND EXISTS(SELECT 1 FROM semantic_annotation_relations r WHERE "
      "r.project_id=a.project_id AND r.source_id=a.id AND r.target_id=?)");
  auto bind = [&](SqliteStatement &q) {
    int i = 1;
    for (auto &s : args)
      q.bind(i++, s);
  };
  auto count =
      db.prepare("SELECT count(*) FROM semantic_annotations a" + where);
  bind(count);
  count.step();
  SemanticUsePage out;
  out.total = count.column_integer(0);
  auto q = db.prepare(
      "SELECT "
      "a.id,a.document_id,d.title,COALESCE(a.anchor_id,''),a.type_key,a.status,"
      "a.category,"
      "COALESCE(k.start_offset,0),COALESCE(k.end_offset,0),a.scope FROM "
      "semantic_annotations a JOIN "
      "documents d "
      "ON d.id=a.document_id AND d.project_id=a.project_id LEFT JOIN "
      "document_anchors k ON k.id=a.anchor_id AND k.project_id=a.project_id" +
      where +
      " ORDER BY d.title COLLATE NOCASE,d.id,k.start_offset,a.id LIMIT ? "
      "OFFSET ?");
  bind(q);
  q.bind(args.size() + 1, std::int64_t(filter.limit));
  q.bind(args.size() + 2, std::int64_t(filter.offset));
  while (q.step())
    out.rows.push_back({q.column_text(0), q.column_text(1), q.column_text(2),
                        q.column_text(3), q.column_text(4), q.column_text(5),
                        q.column_text(6), std::size_t(q.column_integer(7)),
                        std::size_t(q.column_integer(8)),
                        project::annotation_scope(q.column_text(9))});
  tx.commit();
  return out;
}
} // namespace inde::persistence

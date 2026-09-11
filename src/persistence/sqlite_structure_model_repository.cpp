#include "inde/persistence/sqlite_structure_model_repository.hpp"

#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"

#include <stdexcept>

namespace inde::persistence {
namespace {

SqliteDatabase open_database(const std::filesystem::path &project_path) {
  const auto path = ProjectDatabaseRepository::database_path(project_path);
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("O banco de dados do projeto não foi encontrado");
  return SqliteDatabase(path);
}

void bind_optional(SqliteStatement &statement, int index,
                   const std::optional<std::string> &value) {
  if (value)
    statement.bind(index, *value);
  else
    statement.bind_null(index);
}

project::EditorialStructure read_editorial_structure(SqliteStatement &s) {
  project::EditorialStructure value{
      s.column_text(0),
      s.column_text(1),
      s.column_text(2),
      s.column_text(3),
      s.column_is_null(4) ? std::nullopt : std::optional{s.column_text(4)},
      s.column_is_null(5) ? std::nullopt : std::optional{s.column_text(5)},
      project::structure_creation_kind_from_string(s.column_text(6)),
      s.column_integer(7) != 0,
      s.column_text(8),
      s.column_text(9)};
  project::validate(value);
  return value;
}

project::NarrativeStructure read_narrative_structure(SqliteStatement &s) {
  project::NarrativeStructure value{
      s.column_text(0),
      s.column_text(1),
      s.column_text(2),
      s.column_text(3),
      s.column_is_null(4) ? std::nullopt : std::optional{s.column_text(4)},
      s.column_is_null(5) ? std::nullopt : std::optional{s.column_text(5)},
      project::structure_creation_kind_from_string(s.column_text(6)),
      s.column_integer(7) != 0,
      s.column_text(8),
      s.column_text(9)};
  project::validate(value);
  return value;
}

void insert_editorial_structure(SqliteDatabase &database,
                                const std::string &project_id,
                                const project::EditorialStructure &value) {
  project::validate(value);
  auto s = database.prepare(
      "INSERT INTO editorial_structures "
      "(id,project_id,work_id,name,description,model_id,derived_from_id,"
      "creation_kind,is_active,created_at,updated_at) VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?)");
  s.bind(1, value.id);
  s.bind(2, project_id);
  s.bind(3, value.work_id);
  s.bind(4, value.name);
  s.bind(5, value.description);
  bind_optional(s, 6, value.model_id);
  bind_optional(s, 7, value.derived_from_id);
  s.bind(8, project::to_string(value.creation_kind));
  s.bind(9, static_cast<std::int64_t>(value.is_active));
  s.bind(10, value.created_at);
  s.bind(11, value.updated_at);
  s.run();
}

void insert_narrative_structure(SqliteDatabase &database,
                                const std::string &project_id,
                                const project::NarrativeStructure &value) {
  project::validate(value);
  auto s = database.prepare(
      "INSERT INTO narrative_structures "
      "(id,project_id,work_id,name,description,model_id,derived_from_id,"
      "creation_kind,is_active,created_at,updated_at) VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?)");
  s.bind(1, value.id);
  s.bind(2, project_id);
  s.bind(3, value.work_id);
  s.bind(4, value.name);
  s.bind(5, value.description);
  bind_optional(s, 6, value.model_id);
  bind_optional(s, 7, value.derived_from_id);
  s.bind(8, project::to_string(value.creation_kind));
  s.bind(9, static_cast<std::int64_t>(value.is_active));
  s.bind(10, value.created_at);
  s.bind(11, value.updated_at);
  s.run();
}

void insert_line(SqliteDatabase &database, const std::string &project_id,
                 const project::NarrativeLine &value) {
  project::validate(value);
  auto s = database.prepare(
      "INSERT INTO narrative_lines "
      "(id,project_id,structure_id,name,description,position,created_at,"
      "updated_at) "
      "VALUES (?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
      "name=excluded.name,description=excluded.description,"
      "position=excluded.position,updated_at=excluded.updated_at");
  s.bind(1, value.id);
  s.bind(2, project_id);
  s.bind(3, value.structure_id);
  s.bind(4, value.name);
  s.bind(5, value.description);
  s.bind(6, value.position);
  s.bind(7, value.created_at);
  s.bind(8, value.updated_at);
  s.run();
}

void insert_unit(SqliteDatabase &database, const std::string &project_id,
                 const project::NarrativeUnit &value) {
  project::validate(value);
  auto s = database.prepare(
      "INSERT INTO narrative_units "
      "(id,project_id,structure_id,designator,title,summary,purpose,"
      "perspective,"
      "position,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(id) DO UPDATE SET designator=excluded.designator,"
      "title=excluded.title,summary=excluded.summary,purpose=excluded.purpose,"
      "perspective=excluded.perspective,position=excluded.position,"
      "updated_at=excluded.updated_at");
  s.bind(1, value.id);
  s.bind(2, project_id);
  s.bind(3, value.structure_id);
  s.bind(4, value.designator);
  s.bind(5, value.title);
  s.bind(6, value.summary);
  s.bind(7, value.purpose);
  s.bind(8, value.perspective);
  s.bind(9, value.position);
  s.bind(10, value.created_at);
  s.bind(11, value.updated_at);
  s.run();
}

void insert_membership(SqliteDatabase &database, const std::string &project_id,
                       const project::NarrativeUnitLine &value) {
  project::validate(value);
  auto s = database.prepare(
      "INSERT INTO narrative_unit_lines(project_id,unit_id,line_id,position) "
      "VALUES (?,?,?,?) ON CONFLICT(unit_id,line_id) DO UPDATE SET "
      "position=excluded.position");
  s.bind(1, project_id);
  s.bind(2, value.unit_id);
  s.bind(3, value.line_id);
  s.bind(4, value.position);
  s.run();
}

void insert_unit_entity(SqliteDatabase &database, const std::string &project_id,
                        const project::NarrativeUnitEntity &value) {
  project::validate(value);
  auto s = database.prepare(
      "INSERT INTO narrative_unit_entities(project_id,unit_id,entity_id,role) "
      "VALUES (?,?,?,?) ON CONFLICT(unit_id,entity_id,role) DO NOTHING");
  s.bind(1, project_id);
  s.bind(2, value.unit_id);
  s.bind(3, value.entity_id);
  s.bind(4, value.role);
  s.run();
}

void insert_link(SqliteDatabase &database, const std::string &project_id,
                 const project::NarrativeLink &value) {
  project::validate(value);
  auto s = database.prepare(
      "INSERT INTO narrative_links "
      "(id,project_id,structure_id,source_unit_id,target_unit_id,kind,label,"
      "created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(id) DO UPDATE SET kind=excluded.kind,label=excluded.label,"
      "updated_at=excluded.updated_at");
  s.bind(1, value.id);
  s.bind(2, project_id);
  s.bind(3, value.structure_id);
  s.bind(4, value.source_unit_id);
  s.bind(5, value.target_unit_id);
  s.bind(6, project::to_string(value.kind));
  s.bind(7, value.label);
  s.bind(8, value.created_at);
  s.bind(9, value.updated_at);
  s.run();
}

void require_removed(SqliteDatabase &database, const char *message) {
  if (database.changes() != 1)
    throw std::runtime_error(message);
}

} // namespace

void SqliteStructureModelRepository::initialize(
    const std::filesystem::path &path) const {
  auto db = open_database(path);
  SqliteTransaction transaction(db);
  const auto project_id = db.query_text("SELECT id FROM projects");
  const auto now = project::utc_now();
  struct ModelSeed {
    const char *id;
    const char *layer;
    const char *key;
    const char *name;
    const char *description;
  };
  const ModelSeed models[] = {
      {"20000000-0000-4000-9000-000000000001", "editorial", "three-acts",
       "Três atos", "Três atos editoriais sem impor capítulos."},
      {"20000000-0000-4000-9000-000000000002", "editorial", "simple-novel",
       "Romance simples", "Dez capítulos editoriais independentes."},
      {"20000000-0000-4000-9000-000000000003", "editorial",
       "parts-and-chapters", "Partes e capítulos",
       "Três partes com cinco capítulos em cada uma."},
      {"20000000-0000-4000-9000-000000000004", "narrative", "linear",
       "Linha narrativa simples",
       "Uma linha principal pronta para receber unidades."},
      {"20000000-0000-4000-9000-000000000005", "narrative", "three-movements",
       "Três movimentos narrativos",
       "Preparação, confrontação e resolução como unidades editáveis."}};
  auto model = db.prepare("INSERT OR IGNORE INTO structure_models "
                          "(id,project_id,layer,model_key,name,description,is_"
                          "builtin,created_at,updated_at) "
                          "VALUES (?,?,?,?,?,?,1,?,?)");
  for (const auto &value : models) {
    model.bind(1, value.id);
    model.bind(2, project_id);
    model.bind(3, value.layer);
    model.bind(4, value.key);
    model.bind(5, value.name);
    model.bind(6, value.description);
    model.bind(7, now);
    model.bind(8, now);
    model.run();
    model.reset();
  }
  struct ItemSeed {
    const char *id;
    const char *model;
    const char *kind;
    const char *type;
    const char *designator;
    const char *title;
    std::int64_t position;
  };
  const ItemSeed items[] = {
      {"21000000-0000-4000-9000-000000000001", models[0].id, "editorial-node",
       "act", "I", "Sem título", 1000},
      {"21000000-0000-4000-9000-000000000002", models[0].id, "editorial-node",
       "act", "II", "Sem título", 2000},
      {"21000000-0000-4000-9000-000000000003", models[0].id, "editorial-node",
       "act", "III", "Sem título", 3000},
      {"21000000-0000-4000-9000-000000000004", models[3].id, "narrative-line",
       "", "", "Linha principal", 1000},
      {"21000000-0000-4000-9000-000000000005", models[4].id, "narrative-line",
       "", "", "Linha principal", 1000},
      {"21000000-0000-4000-9000-000000000006", models[4].id, "narrative-unit",
       "", "1", "Preparação", 2000},
      {"21000000-0000-4000-9000-000000000007", models[4].id, "narrative-unit",
       "", "2", "Confrontação", 3000},
      {"21000000-0000-4000-9000-000000000008", models[4].id, "narrative-unit",
       "", "3", "Resolução", 4000}};
  auto item = db.prepare(
      "INSERT OR IGNORE INTO structure_model_items "
      "(id,project_id,model_id,parent_id,item_kind,type_key,designator,title,"
      "position,created_at,updated_at) VALUES (?,?,?,NULL,?,?,?,?,?,?,?)");
  for (const auto &value : items) {
    item.bind(1, value.id);
    item.bind(2, project_id);
    item.bind(3, value.model);
    item.bind(4, value.kind);
    item.bind(5, value.type);
    item.bind(6, value.designator);
    item.bind(7, value.title);
    item.bind(8, value.position);
    item.bind(9, now);
    item.bind(10, now);
    item.run();
    item.reset();
  }
  struct RoleSeed {
    const char *id;
    const char *name;
    const char *description;
  };
  const RoleSeed roles[] = {
      {"22000000-0000-4000-9000-000000000001", "Protagonista",
       "Papel contextual de protagonismo."},
      {"22000000-0000-4000-9000-000000000002", "Antagonista",
       "Papel contextual de oposição."},
      {"22000000-0000-4000-9000-000000000003", "Testemunha",
       "Papel contextual de testemunho."},
      {"22000000-0000-4000-9000-000000000004", "Mentor",
       "Papel contextual de mentoria."},
      {"22000000-0000-4000-9000-000000000005", "Ponto de vista (PoV)",
       "Papel contextual de ponto de vista; não substitui toda perspectiva."}};
  auto role = db.prepare(
      "INSERT OR IGNORE INTO narrative_roles "
      "(id,project_id,name,description,is_builtin,created_at,updated_at) "
      "VALUES (?,?,?,?,1,?,?)");
  for (const auto &value : roles) {
    role.bind(1, value.id);
    role.bind(2, project_id);
    role.bind(3, value.name);
    role.bind(4, value.description);
    role.bind(5, now);
    role.bind(6, now);
    role.run();
    role.reset();
  }
  transaction.commit();
}

std::vector<project::StructureModel>
SqliteStructureModelRepository::models(const std::filesystem::path &path,
                                       project::StructureLayer layer) const {
  auto db = open_database(path);
  auto s = db.prepare(
      "SELECT "
      "id,layer,model_key,name,description,is_builtin,created_at,updated_at "
      "FROM structure_models WHERE layer=? ORDER BY is_builtin DESC,name,id");
  s.bind(1, project::to_string(layer));
  std::vector<project::StructureModel> out;
  while (s.step()) {
    project::StructureModel v{
        s.column_text(0),
        project::structure_layer_from_string(s.column_text(1)),
        s.column_text(2),
        s.column_text(3),
        s.column_text(4),
        s.column_integer(5) != 0,
        s.column_text(6),
        s.column_text(7)};
    project::validate(v);
    out.push_back(std::move(v));
  }
  return out;
}

std::vector<project::StructureModelItem>
SqliteStructureModelRepository::model_items(const std::filesystem::path &path,
                                            const std::string &id) const {
  auto db = open_database(path);
  auto s = db.prepare(
      "SELECT "
      "id,model_id,parent_id,item_kind,type_key,designator,title,summary,"
      "purpose,perspective,position,created_at,updated_at FROM "
      "structure_model_items "
      "WHERE model_id=? ORDER BY position,id");
  s.bind(1, id);
  std::vector<project::StructureModelItem> out;
  while (s.step()) {
    project::StructureModelItem v{
        s.column_text(0),
        s.column_text(1),
        s.column_is_null(2) ? std::nullopt : std::optional{s.column_text(2)},
        project::structure_model_item_kind_from_string(s.column_text(3)),
        s.column_text(4),
        s.column_text(5),
        s.column_text(6),
        s.column_text(7),
        s.column_text(8),
        s.column_text(9),
        s.column_integer(10),
        s.column_text(11),
        s.column_text(12)};
    project::validate(v);
    out.push_back(std::move(v));
  }
  return out;
}

std::vector<project::StructureModelItemLink>
SqliteStructureModelRepository::model_item_links(
    const std::filesystem::path &path, const std::string &id) const {
  auto db = open_database(path);
  auto s = db.prepare(
      "SELECT id,model_id,source_item_id,target_item_id,kind,label,position "
      "FROM structure_model_item_links WHERE model_id=? "
      "ORDER BY kind,position,id");
  s.bind(1, id);
  std::vector<project::StructureModelItemLink> out;
  while (s.step()) {
    project::StructureModelItemLink value{s.column_text(0),   s.column_text(1),
                                          s.column_text(2),   s.column_text(3),
                                          s.column_text(4),   s.column_text(5),
                                          s.column_integer(6)};
    project::validate(value);
    out.push_back(std::move(value));
  }
  return out;
}

void SqliteStructureModelRepository::save_model_bundle(
    const std::filesystem::path &path, const project::StructureModel &value,
    const std::vector<project::StructureModelItem> &items,
    const std::vector<project::StructureModelItemLink> &links) const {
  project::validate(value);
  for (const auto &item : items)
    project::validate(item);
  for (const auto &link : links)
    project::validate(link);
  auto db = open_database(path);
  SqliteTransaction tx(db);
  const auto pid = db.query_text("SELECT id FROM projects");
  auto s = db.prepare(
      "INSERT INTO "
      "structure_models(id,project_id,layer,model_key,name,description,is_"
      "builtin,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?)");
  s.bind(1, value.id);
  s.bind(2, pid);
  s.bind(3, project::to_string(value.layer));
  s.bind(4, value.key);
  s.bind(5, value.name);
  s.bind(6, value.description);
  s.bind(7, static_cast<std::int64_t>(value.is_builtin));
  s.bind(8, value.created_at);
  s.bind(9, value.updated_at);
  s.run();
  auto i = db.prepare(
      "INSERT INTO "
      "structure_model_items(id,project_id,model_id,parent_id,item_kind,type_"
      "key,designator,title,summary,purpose,perspective,position,created_at,"
      "updated_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  for (const auto &v : items) {
    i.bind(1, v.id);
    i.bind(2, pid);
    i.bind(3, v.model_id);
    bind_optional(i, 4, v.parent_id);
    i.bind(5, project::to_string(v.kind));
    i.bind(6, v.type_key);
    i.bind(7, v.designator);
    i.bind(8, v.title);
    i.bind(9, v.summary);
    i.bind(10, v.purpose);
    i.bind(11, v.perspective);
    i.bind(12, v.position);
    i.bind(13, v.created_at);
    i.bind(14, v.updated_at);
    i.run();
    i.reset();
  }
  auto link = db.prepare(
      "INSERT INTO "
      "structure_model_item_links(id,project_id,model_id,source_item_id,target_"
      "item_id,kind,label,position) VALUES (?,?,?,?,?,?,?,?)");
  for (const auto &v : links) {
    link.bind(1, v.id);
    link.bind(2, pid);
    link.bind(3, v.model_id);
    link.bind(4, v.source_item_id);
    link.bind(5, v.target_item_id);
    link.bind(6, v.kind);
    link.bind(7, v.label);
    link.bind(8, v.position);
    link.run();
    link.reset();
  }
  tx.commit();
}

void SqliteStructureModelRepository::remove_model(
    const std::filesystem::path &p, const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare("DELETE FROM structure_models WHERE id=?");
  s.bind(1, id);
  s.run();
  require_removed(db, "Modelo estrutural não encontrado ou protegido");
  tx.commit();
}

std::vector<project::EditorialStructure>
SqliteStructureModelRepository::editorial_structures(
    const std::filesystem::path &p, const std::string &work) const {
  auto db = open_database(p);
  auto s = db.prepare(
      "SELECT "
      "id,work_id,name,description,model_id,derived_from_id,creation_kind,is_"
      "active,created_at,updated_at FROM editorial_structures WHERE work_id=? "
      "ORDER BY is_active DESC,updated_at DESC,id");
  s.bind(1, work);
  std::vector<project::EditorialStructure> out;
  while (s.step())
    out.push_back(read_editorial_structure(s));
  return out;
}
std::vector<project::NarrativeStructure>
SqliteStructureModelRepository::narrative_structures(
    const std::filesystem::path &p, const std::string &work) const {
  auto db = open_database(p);
  auto s = db.prepare(
      "SELECT "
      "id,work_id,name,description,model_id,derived_from_id,creation_kind,is_"
      "active,created_at,updated_at FROM narrative_structures WHERE work_id=? "
      "ORDER BY is_active DESC,updated_at DESC,id");
  s.bind(1, work);
  std::vector<project::NarrativeStructure> out;
  while (s.step())
    out.push_back(read_narrative_structure(s));
  return out;
}

void SqliteStructureModelRepository::save_editorial_structure_bundle(
    const std::filesystem::path &p, const project::EditorialStructure &value,
    const std::vector<project::StructuralNode> &nodes) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  const auto pid = db.query_text("SELECT id FROM projects");
  insert_editorial_structure(db, pid, value);
  auto s = db.prepare(
      "INSERT INTO "
      "editorial_nodes(id,work_id,parent_id,type,title,subtitle,synopsis,"
      "custom_type_name,status,position,created_at,updated_at,structural_type_"
      "id,designator,structure_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  for (const auto &v : nodes) {
    project::validate(v);
    s.bind(1, v.id);
    s.bind(2, v.work_id);
    bind_optional(s, 3, v.parent_id);
    s.bind(4, project::to_string(v.type));
    s.bind(5, v.title);
    s.bind(6, v.subtitle);
    s.bind(7, v.synopsis);
    s.bind(8, v.custom_type_name);
    s.bind(9, v.status);
    s.bind(10, v.position);
    s.bind(11, v.created_at);
    s.bind(12, v.updated_at);
    s.bind(13, v.structural_type_id);
    s.bind(14, v.designator);
    s.bind(15, v.structure_id);
    s.run();
    s.reset();
  }
  tx.commit();
}

void SqliteStructureModelRepository::save_narrative_structure_bundle(
    const std::filesystem::path &p, const project::NarrativeStructure &value,
    const std::vector<project::NarrativeLine> &lines,
    const std::vector<project::NarrativeUnit> &units,
    const std::vector<project::NarrativeUnitLine> &memberships,
    const std::vector<project::NarrativeUnitEntity> &entities,
    const std::vector<project::NarrativeLink> &links) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  const auto pid = db.query_text("SELECT id FROM projects");
  insert_narrative_structure(db, pid, value);
  for (const auto &v : lines)
    insert_line(db, pid, v);
  for (const auto &v : units)
    insert_unit(db, pid, v);
  for (const auto &v : memberships)
    insert_membership(db, pid, v);
  for (const auto &v : entities)
    insert_unit_entity(db, pid, v);
  for (const auto &v : links)
    insert_link(db, pid, v);
  tx.commit();
}

void SqliteStructureModelRepository::activate_editorial(
    const std::filesystem::path &p, const std::string &work,
    const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto clear =
      db.prepare("UPDATE editorial_structures SET is_active=0,updated_at=? "
                 "WHERE work_id=? AND is_active=1");
  clear.bind(1, project::utc_now());
  clear.bind(2, work);
  clear.run();
  auto set = db.prepare("UPDATE editorial_structures SET "
                        "is_active=1,updated_at=? WHERE id=? AND work_id=?");
  set.bind(1, project::utc_now());
  set.bind(2, id);
  set.bind(3, work);
  set.run();
  require_removed(db, "Estrutura editorial alvo não encontrada");
  tx.commit();
}
void SqliteStructureModelRepository::activate_narrative(
    const std::filesystem::path &p, const std::string &work,
    const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto clear =
      db.prepare("UPDATE narrative_structures SET is_active=0,updated_at=? "
                 "WHERE work_id=? AND is_active=1");
  clear.bind(1, project::utc_now());
  clear.bind(2, work);
  clear.run();
  auto set = db.prepare("UPDATE narrative_structures SET "
                        "is_active=1,updated_at=? WHERE id=? AND work_id=?");
  set.bind(1, project::utc_now());
  set.bind(2, id);
  set.bind(3, work);
  set.run();
  require_removed(db, "Estrutura narrativa alvo não encontrada");
  tx.commit();
}
std::size_t
SqliteStructureModelRepository::document_count(const std::filesystem::path &p,
                                               const std::string &id) const {
  auto db = open_database(p);
  auto s = db.prepare("SELECT count(*) FROM documents d JOIN editorial_nodes n "
                      "ON n.id=d.editorial_node_id WHERE n.structure_id=?");
  s.bind(1, id);
  if (!s.step())
    return 0;
  return static_cast<std::size_t>(s.column_integer(0));
}
void SqliteStructureModelRepository::remove_editorial_structure(
    const std::filesystem::path &p, const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto nodes = db.prepare(
      "DELETE FROM editorial_nodes WHERE structure_id=? AND EXISTS (SELECT 1 "
      "FROM editorial_structures s WHERE s.id=? AND s.is_active=0)");
  nodes.bind(1, id);
  nodes.bind(2, id);
  nodes.run();
  auto s =
      db.prepare("DELETE FROM editorial_structures WHERE id=? AND is_active=0");
  s.bind(1, id);
  s.run();
  require_removed(db, "A estrutura editorial não existe ou está ativa");
  tx.commit();
}
void SqliteStructureModelRepository::remove_narrative_structure(
    const std::filesystem::path &p, const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s =
      db.prepare("DELETE FROM narrative_structures WHERE id=? AND is_active=0");
  s.bind(1, id);
  s.run();
  require_removed(db, "A estrutura narrativa não existe ou está ativa");
  tx.commit();
}

std::vector<project::NarrativeLine>
SqliteStructureModelRepository::lines(const std::filesystem::path &p,
                                      const std::string &id) const {
  auto db = open_database(p);
  auto s = db.prepare(
      "SELECT id,structure_id,name,description,position,created_at,updated_at "
      "FROM narrative_lines WHERE structure_id=? ORDER BY position,id");
  s.bind(1, id);
  std::vector<project::NarrativeLine> out;
  while (s.step()) {
    project::NarrativeLine v{s.column_text(0),    s.column_text(1),
                             s.column_text(2),    s.column_text(3),
                             s.column_integer(4), s.column_text(5),
                             s.column_text(6)};
    project::validate(v);
    out.push_back(std::move(v));
  }
  return out;
}
std::vector<project::NarrativeUnit>
SqliteStructureModelRepository::units(const std::filesystem::path &p,
                                      const std::string &id) const {
  auto db = open_database(p);
  auto s =
      db.prepare("SELECT "
                 "id,structure_id,designator,title,summary,purpose,perspective,"
                 "position,created_at,updated_at FROM narrative_units WHERE "
                 "structure_id=? ORDER BY position,id");
  s.bind(1, id);
  std::vector<project::NarrativeUnit> out;
  while (s.step()) {
    project::NarrativeUnit v{s.column_text(0), s.column_text(1),
                             s.column_text(2), s.column_text(3),
                             s.column_text(4), s.column_text(5),
                             s.column_text(6), s.column_integer(7),
                             s.column_text(8), s.column_text(9)};
    project::validate(v);
    out.push_back(std::move(v));
  }
  return out;
}
std::vector<project::NarrativeUnitLine>
SqliteStructureModelRepository::unit_lines(const std::filesystem::path &p,
                                           const std::string &id) const {
  auto db = open_database(p);
  auto s = db.prepare(
      "SELECT m.unit_id,m.line_id,m.position FROM narrative_unit_lines m JOIN "
      "narrative_units u ON u.id=m.unit_id WHERE u.structure_id=? ORDER BY "
      "m.line_id,m.position,m.unit_id");
  s.bind(1, id);
  std::vector<project::NarrativeUnitLine> out;
  while (s.step())
    out.push_back({s.column_text(0), s.column_text(1), s.column_integer(2)});
  return out;
}
std::vector<project::NarrativeUnitEntity>
SqliteStructureModelRepository::unit_entities(const std::filesystem::path &p,
                                              const std::string &id) const {
  auto db = open_database(p);
  auto s = db.prepare(
      "SELECT m.unit_id,m.entity_id,m.role FROM narrative_unit_entities m JOIN "
      "narrative_units u ON u.id=m.unit_id WHERE u.structure_id=? ORDER BY "
      "m.unit_id,m.role,m.entity_id");
  s.bind(1, id);
  std::vector<project::NarrativeUnitEntity> out;
  while (s.step())
    out.push_back({s.column_text(0), s.column_text(1), s.column_text(2)});
  return out;
}
std::vector<project::NarrativeLink>
SqliteStructureModelRepository::links(const std::filesystem::path &p,
                                      const std::string &id) const {
  auto db = open_database(p);
  auto s =
      db.prepare("SELECT "
                 "id,structure_id,source_unit_id,target_unit_id,kind,label,"
                 "created_at,updated_at FROM narrative_links WHERE "
                 "structure_id=? ORDER BY kind,source_unit_id,target_unit_id");
  s.bind(1, id);
  std::vector<project::NarrativeLink> out;
  while (s.step()) {
    project::NarrativeLink v{
        s.column_text(0),
        s.column_text(1),
        s.column_text(2),
        s.column_text(3),
        project::narrative_link_kind_from_string(s.column_text(4)),
        s.column_text(5),
        s.column_text(6),
        s.column_text(7)};
    project::validate(v);
    out.push_back(std::move(v));
  }
  return out;
}

void SqliteStructureModelRepository::save(
    const std::filesystem::path &p, const project::NarrativeLine &v) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  insert_line(db, db.query_text("SELECT id FROM projects"), v);
  tx.commit();
}
void SqliteStructureModelRepository::save(
    const std::filesystem::path &p, const project::NarrativeUnit &v) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  insert_unit(db, db.query_text("SELECT id FROM projects"), v);
  tx.commit();
}
void SqliteStructureModelRepository::save(
    const std::filesystem::path &p, const project::NarrativeUnitLine &v) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  insert_membership(db, db.query_text("SELECT id FROM projects"), v);
  tx.commit();
}
void SqliteStructureModelRepository::save(
    const std::filesystem::path &p,
    const project::NarrativeUnitEntity &v) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  insert_unit_entity(db, db.query_text("SELECT id FROM projects"), v);
  tx.commit();
}
void SqliteStructureModelRepository::save(
    const std::filesystem::path &p, const project::NarrativeLink &v) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  insert_link(db, db.query_text("SELECT id FROM projects"), v);
  tx.commit();
}
void SqliteStructureModelRepository::remove_line(const std::filesystem::path &p,
                                                 const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare("DELETE FROM narrative_lines WHERE id=?");
  s.bind(1, id);
  s.run();
  require_removed(db, "Linha narrativa não encontrada");
  tx.commit();
}
void SqliteStructureModelRepository::remove_unit(const std::filesystem::path &p,
                                                 const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare("DELETE FROM narrative_units WHERE id=?");
  s.bind(1, id);
  s.run();
  require_removed(db, "Unidade narrativa não encontrada");
  tx.commit();
}
void SqliteStructureModelRepository::remove_unit_line(
    const std::filesystem::path &p, const std::string &u,
    const std::string &l) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare(
      "DELETE FROM narrative_unit_lines WHERE unit_id=? AND line_id=?");
  s.bind(1, u);
  s.bind(2, l);
  s.run();
  require_removed(db, "Pertencimento narrativo não encontrado");
  tx.commit();
}
void SqliteStructureModelRepository::remove_unit_entity(
    const std::filesystem::path &p, const std::string &u, const std::string &e,
    const std::string &r) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare("DELETE FROM narrative_unit_entities WHERE unit_id=? AND "
                      "entity_id=? AND role=?");
  s.bind(1, u);
  s.bind(2, e);
  s.bind(3, r);
  s.run();
  require_removed(db, "Vínculo entre unidade e realidade não encontrado");
  tx.commit();
}
void SqliteStructureModelRepository::remove_link(const std::filesystem::path &p,
                                                 const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare("DELETE FROM narrative_links WHERE id=?");
  s.bind(1, id);
  s.run();
  require_removed(db, "Vínculo narrativo não encontrado");
  tx.commit();
}

std::vector<project::NarrativeRole>
SqliteStructureModelRepository::roles(const std::filesystem::path &p) const {
  auto db = open_database(p);
  auto s =
      db.prepare("SELECT id,name,description,is_builtin,created_at,updated_at "
                 "FROM narrative_roles ORDER BY is_builtin DESC,name,id");
  std::vector<project::NarrativeRole> out;
  while (s.step()) {
    project::NarrativeRole v{s.column_text(0), s.column_text(1),
                             s.column_text(2), s.column_integer(3) != 0,
                             s.column_text(4), s.column_text(5)};
    project::validate(v);
    out.push_back(std::move(v));
  }
  return out;
}
std::vector<project::NarrativeRoleAssignment>
SqliteStructureModelRepository::role_assignments(
    const std::filesystem::path &p,
    const std::optional<std::string> &work) const {
  auto db = open_database(p);
  auto s =
      db.prepare("SELECT "
                 "id,role_id,entity_id,work_id,structure_id,unit_id,notes,"
                 "created_at,updated_at FROM narrative_role_assignments WHERE "
                 "(? IS NULL OR work_id=?) ORDER BY updated_at DESC,id");
  bind_optional(s, 1, work);
  bind_optional(s, 2, work);
  std::vector<project::NarrativeRoleAssignment> out;
  while (s.step()) {
    project::NarrativeRoleAssignment v{
        s.column_text(0),
        s.column_text(1),
        s.column_text(2),
        s.column_text(3),
        s.column_is_null(4) ? std::nullopt : std::optional{s.column_text(4)},
        s.column_is_null(5) ? std::nullopt : std::optional{s.column_text(5)},
        s.column_text(6),
        s.column_text(7),
        s.column_text(8)};
    project::validate(v);
    out.push_back(std::move(v));
  }
  return out;
}
void SqliteStructureModelRepository::save(
    const std::filesystem::path &p, const project::NarrativeRole &v) const {
  project::validate(v);
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare(
      "INSERT INTO "
      "narrative_roles(id,project_id,name,description,is_builtin,created_at,"
      "updated_at) VALUES (?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
      "name=excluded.name,description=excluded.description,updated_at=excluded."
      "updated_at");
  s.bind(1, v.id);
  s.bind(2, db.query_text("SELECT id FROM projects"));
  s.bind(3, v.name);
  s.bind(4, v.description);
  s.bind(5, static_cast<std::int64_t>(v.is_builtin));
  s.bind(6, v.created_at);
  s.bind(7, v.updated_at);
  s.run();
  tx.commit();
}
void SqliteStructureModelRepository::save(
    const std::filesystem::path &p,
    const project::NarrativeRoleAssignment &v) const {
  project::validate(v);
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare(
      "INSERT INTO "
      "narrative_role_assignments(id,project_id,role_id,entity_id,work_id,"
      "structure_id,unit_id,notes,created_at,updated_at) VALUES "
      "(?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
      "role_id=excluded.role_id,entity_id=excluded.entity_id,work_id=excluded."
      "work_id,structure_id=excluded.structure_id,unit_id=excluded.unit_id,"
      "notes=excluded.notes,updated_at=excluded.updated_at");
  s.bind(1, v.id);
  s.bind(2, db.query_text("SELECT id FROM projects"));
  s.bind(3, v.role_id);
  s.bind(4, v.entity_id);
  s.bind(5, v.work_id);
  bind_optional(s, 6, v.structure_id);
  bind_optional(s, 7, v.unit_id);
  s.bind(8, v.notes);
  s.bind(9, v.created_at);
  s.bind(10, v.updated_at);
  s.run();
  tx.commit();
}
void SqliteStructureModelRepository::remove_role(const std::filesystem::path &p,
                                                 const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s =
      db.prepare("DELETE FROM narrative_roles WHERE id=? AND is_builtin=0");
  s.bind(1, id);
  s.run();
  require_removed(db, "Papel narrativo não encontrado, interno ou em uso");
  tx.commit();
}
void SqliteStructureModelRepository::remove_role_assignment(
    const std::filesystem::path &p, const std::string &id) const {
  auto db = open_database(p);
  SqliteTransaction tx(db);
  auto s = db.prepare("DELETE FROM narrative_role_assignments WHERE id=?");
  s.bind(1, id);
  s.run();
  require_removed(db, "Atribuição narrativa não encontrada");
  tx.commit();
}

} // namespace inde::persistence

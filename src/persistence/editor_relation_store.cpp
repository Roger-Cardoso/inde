#include "inde/persistence/sqlite_narrative_repository.hpp"
#include "narrative_repository_support.hpp"
#include "narrative_row_codec.hpp"
#include <stdexcept>
namespace inde::persistence {
void SqliteNarrativeRepository::create_relation_from_text(
    const std::filesystem::path &path,
    const project::TextRelationCreation &p) const {
  using namespace narrative_detail;
  auto db = open_database(path);
  const auto owner = project_id(db);
  SqliteTransaction tx(db, "Criar relação a partir do texto");
  for (const auto *e : {&p.source, &p.target}) {
    auto q = db.prepare(
        "SELECT id,entity_type_id,name,summary,created_at,updated_at FROM "
        "entities WHERE project_id=? AND id=?");
    q.bind(1, owner);
    q.bind(2, e->id);
    if (!q.step())
      throw std::runtime_error("Uma entidade da relação foi removida");
    auto current = read_entity(q);
    if (current.entity_type_id != e->entity_type_id ||
        current.name != e->name || current.summary != e->summary ||
        current.updated_at != e->updated_at)
      throw std::runtime_error(
          "Uma entidade da relação mudou; revise o formulário");
  }
  const auto &t = p.type;
  if (p.create_type) {
    auto q = db.prepare("INSERT INTO "
                        "relation_types(id,project_id,key,name,inverse_name,"
                        "description,directionality,is_builtin,created_at,"
                        "updated_at) VALUES(?,?,?,?,?,?,?,?,?,?)");
    q.bind(1, t.id);
    q.bind(2, owner);
    q.bind(3, t.key);
    q.bind(4, t.name);
    q.bind(5, t.inverse_name);
    q.bind(6, t.description);
    q.bind(7, project::to_string(t.directionality));
    q.bind(8, std::int64_t(0));
    q.bind(9, t.created_at);
    q.bind(10, t.updated_at);
    q.run();
    record_change(db, owner, "create_relation_type_from_text");
  } else {
    auto q = db.prepare("SELECT "
                        "id,key,name,inverse_name,description,directionality,"
                        "is_builtin,created_at,updated_at FROM relation_types "
                        "WHERE project_id=? AND id=?");
    q.bind(1, owner);
    q.bind(2, t.id);
    if (!q.step())
      throw std::runtime_error("Tipo de relação removido");
    auto current = read_relation_type(q);
    if (current.key != t.key || current.name != t.name ||
        current.inverse_name != t.inverse_name ||
        current.directionality != t.directionality ||
        current.description != t.description ||
        current.updated_at != t.updated_at)
      throw std::runtime_error("O tipo de relação mudou; revise o sentido");
  }
  const auto &r = p.relation;
  auto q = db.prepare(
      "INSERT INTO "
      "relations(id,project_id,relation_type_id,source_entity_id,target_entity_"
      "id,description,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?)");
  q.bind(1, r.id);
  q.bind(2, owner);
  q.bind(3, r.relation_type_id);
  q.bind(4, r.source_entity_id);
  q.bind(5, r.target_entity_id);
  q.bind(6, r.description);
  q.bind(7, r.created_at);
  q.bind(8, r.updated_at);
  q.run();
  record_change(db, owner, "create_relation_from_text");
  tx.commit();
}
} // namespace inde::persistence

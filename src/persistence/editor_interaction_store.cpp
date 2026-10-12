#include "editor_interaction_store.hpp"
#include "narrative_repository_support.hpp"
#include "narrative_row_codec.hpp"
#include <stdexcept>
namespace inde::persistence {
void insert_text_entity(SqliteDatabase &db, const std::string &owner,
                        const project::TextEntityCreation &plan) {
  const auto &t = plan.type;
  const auto &e = plan.entity;
  if (plan.create_type) {
    auto q =
        db.prepare("INSERT INTO "
                   "entity_types(id,project_id,key,name,description,is_builtin,"
                   "created_at,updated_at) VALUES(?,?,?,?,?,?,?,?)");
    q.bind(1, t.id);
    q.bind(2, owner);
    q.bind(3, t.key);
    q.bind(4, t.name);
    q.bind(5, t.description);
    q.bind(6, std::int64_t(0));
    q.bind(7, t.created_at);
    q.bind(8, t.updated_at);
    q.run();
    narrative_detail::record_change(db, owner, "create_entity_type_from_text");
  } else {
    auto q = db.prepare(
        "SELECT id,key,name,description,is_builtin,created_at,updated_at FROM "
        "entity_types WHERE project_id=? AND id=?");
    q.bind(1, owner);
    q.bind(2, t.id);
    if (!q.step())
      throw std::runtime_error(
          "O tipo de entidade foi removido; abra o formulário novamente");
    auto current = narrative_detail::read_entity_type(q);
    if (current.key != t.key || current.name != t.name ||
        current.description != t.description ||
        current.is_builtin != t.is_builtin ||
        current.updated_at != t.updated_at)
      throw std::runtime_error(
          "O tipo de entidade mudou; revise a escolha novamente");
  }
  auto q = db.prepare("INSERT INTO "
                      "entities(id,project_id,entity_type_id,name,summary,"
                      "created_at,updated_at) VALUES(?,?,?,?,?,?,?)");
  q.bind(1, e.id);
  q.bind(2, owner);
  q.bind(3, e.entity_type_id);
  q.bind(4, e.name);
  q.bind(5, e.summary);
  q.bind(6, e.created_at);
  q.bind(7, e.updated_at);
  q.run();
  narrative_detail::record_change(db, owner, "create_entity_from_text");
}
} // namespace inde::persistence

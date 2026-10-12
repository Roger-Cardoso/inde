#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/semantic_store.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include <stdexcept>

namespace inde::persistence {
SemanticRelationPage
SemanticStore::relations(const std::filesystem::path &path,
                         const SemanticRelationQuery &filter) const {
  if (filter.annotation_id.empty() || filter.annotation_id.size() > 128 ||
      !filter.limit || filter.limit > 200 || filter.offset > 10000000 ||
      (filter.direction != SemanticRelationDirection::Incoming &&
       filter.direction != SemanticRelationDirection::Outgoing))
    throw std::runtime_error("Consulta de relações semânticas inválida");
  SqliteDatabase db(ProjectDatabaseRepository::database_path(path));
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  auto owner = db.prepare("SELECT id FROM projects");
  if (!owner.step())
    throw std::runtime_error("Projeto ausente");
  const auto project_id = owner.column_text(0);
  if (owner.step())
    throw std::runtime_error("Mais de um Projeto no banco");
  std::string where = " WHERE r.project_id=? AND ";
  where += filter.direction == SemanticRelationDirection::Incoming
               ? "r.target_id=?"
               : "r.source_id=?";
  if (filter.exclude_source_document_id)
    where += " AND r.document_id!=?";
  const auto bind = [&](SqliteStatement &q) {
    q.bind(1, project_id);
    q.bind(2, filter.annotation_id);
    if (filter.exclude_source_document_id)
      q.bind(3, *filter.exclude_source_document_id);
  };
  auto count = db.prepare(
      "SELECT count(*) FROM semantic_annotation_relations r" + where);
  bind(count);
  count.step();
  SemanticRelationPage out;
  out.total = count.column_integer(0);
  const std::string columns =
      "a.id,a.document_id,d.title,COALESCE(a.anchor_id,''),a.type_key,a.status,"
      "a.category,"
      "COALESCE(k.start_offset,0),COALESCE(k.end_offset,0),a.scope";
  std::string target_columns = columns;
  for (std::size_t p = 0;
       (p = target_columns.find("a.", p)) != std::string::npos; p += 2)
    target_columns.replace(p, 2, "b.");
  for (std::size_t p = 0;
       (p = target_columns.find("d.", p)) != std::string::npos; p += 2)
    target_columns.replace(p, 2, "e.");
  for (std::size_t p = 0;
       (p = target_columns.find("k.", p)) != std::string::npos; p += 2)
    target_columns.replace(p, 2, "l.");
  auto q = db.prepare(
      "SELECT r.id,r.type_key," + columns + "," + target_columns +
      " FROM semantic_annotation_relations r "
      "JOIN semantic_annotations a ON a.id=r.source_id AND "
      "a.project_id=r.project_id "
      "JOIN semantic_annotations b ON b.id=r.target_id AND "
      "b.project_id=r.project_id "
      "JOIN documents d ON d.id=a.document_id AND d.project_id=r.project_id "
      "JOIN documents e ON e.id=b.document_id AND e.project_id=r.project_id "
      "LEFT JOIN document_anchors k ON k.id=a.anchor_id AND "
      "k.project_id=r.project_id "
      "LEFT JOIN document_anchors l ON l.id=b.anchor_id AND "
      "l.project_id=r.project_id" +
      where +
      " ORDER BY d.title COLLATE NOCASE,d.id,k.start_offset,r.id LIMIT ? "
      "OFFSET ?");
  bind(q);
  const int at = filter.exclude_source_document_id ? 4 : 3;
  q.bind(at, std::int64_t(filter.limit));
  q.bind(at + 1, std::int64_t(filter.offset));
  const auto endpoint = [&](int i) -> SemanticUse {
    return {q.column_text(i),
            q.column_text(i + 1),
            q.column_text(i + 2),
            q.column_text(i + 3),
            q.column_text(i + 4),
            q.column_text(i + 5),
            q.column_text(i + 6),
            std::size_t(q.column_integer(i + 7)),
            std::size_t(q.column_integer(i + 8)),
            project::annotation_scope(q.column_text(i + 9))};
  };
  while (q.step())
    out.rows.push_back(
        {q.column_text(0), q.column_text(1), endpoint(2), endpoint(12)});
  tx.commit();
  return out;
}
} // namespace inde::persistence

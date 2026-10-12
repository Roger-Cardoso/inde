#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/project/manifest.hpp"
#include <limits>
#include <tuple>
namespace inde::persistence {
namespace {
std::filesystem::path path(const project::Project &p) {
  auto file = ProjectDatabaseRepository::database_path(p.path());
  if (!std::filesystem::is_regular_file(file))
    throw std::runtime_error("Banco da Composição ausente");
  return file;
}
void owner(SqliteDatabase &db, const project::Project &p) {
  if (SchemaMigrator{}.current_version(db) != current_database_schema_version)
    throw std::runtime_error(
        "Schema da Composição incompatível; reabra o Projeto nesta versão");
  auto q = db.prepare("SELECT id FROM projects");
  if (!q.step() || q.column_text(0) != p.manifest().project_id || q.step())
    throw std::runtime_error("Identidade do Projeto incompatível");
}
project::StoredComposition read(SqliteStatement &q,
                                const std::string &project) {
  if (q.column_integer(6) >
      static_cast<std::int64_t>(project::composition_payload_limit))
    throw std::runtime_error("Payload de Composição excedido");
  project::StoredComposition c;
  c.project_id = project;
  c.revision = q.column_integer(2);
  c.sha256 = q.column_text(5);
  c.updated_at = q.column_text(3);
  c.value = decode_composition(q.column_text(4), c.sha256);
  if (c.value.blocks.composition_id() != q.column_text(0) ||
      c.value.name != q.column_text(1) || c.revision < 1)
    throw std::runtime_error("Metadados de Composição incompatíveis");
  return c;
}
} // namespace
std::vector<project::CompositionSummary>
CompositionStore::list(const project::Project &p, std::size_t offset) const {
  if (offset > 1000000)
    throw std::runtime_error("Paginação de Composições excedida");
  SqliteDatabase db(path(p));
  owner(db, p);
  auto q =
      db.prepare("SELECT id,name,revision,updated_at FROM compositions WHERE "
                 "project_id=? ORDER BY updated_at DESC,id LIMIT 30 OFFSET ?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, static_cast<std::int64_t>(offset));
  std::vector<project::CompositionSummary> out;
  while (q.step())
    out.push_back({q.column_text(0), q.column_text(1), q.column_text(3),
                   q.column_integer(2)});
  return out;
}
project::StoredComposition CompositionStore::load(const project::Project &p,
                                                  const std::string &id) const {
  SqliteDatabase db(path(p));
  owner(db, p);
  auto q = db.prepare(
      "SELECT id,name,revision,updated_at,payload,sha256,length(CAST(payload "
      "AS BLOB)) FROM compositions WHERE project_id=? AND id=?");
  q.bind(1, p.manifest().project_id);
  q.bind(2, id);
  if (!q.step())
    throw std::runtime_error("Composição ausente");
  return read(q, p.manifest().project_id);
}
std::vector<project::CompositionSummary> CompositionStore::search(const project::Project &p,
    const std::string &text,std::size_t limit) const {
  if(text.size()>256 || limit==0 || limit>50)throw std::runtime_error("Busca de Composição excede o limite");
  SqliteDatabase db(path(p));owner(db,p);
  auto q=db.prepare("SELECT id,name,revision,updated_at FROM compositions WHERE project_id=? AND instr(lower(name),lower(?))>0 ORDER BY name,id LIMIT ?");
  q.bind(1,p.manifest().project_id);q.bind(2,text);q.bind(3,static_cast<std::int64_t>(limit));
  std::vector<project::CompositionSummary> out;
  while(q.step())out.push_back({q.column_text(0),q.column_text(1),q.column_text(3),q.column_integer(2)});
  return out;
}
project::StoredComposition
CompositionStore::save(const project::Project &p, const project::Composition &c,
                       std::int64_t expected, const std::string &expected_hash,
                       bool draft_only,
                       const std::function<void(SqliteDatabase &)> &context) const {
  if (expected < 0 || expected == std::numeric_limits<std::int64_t>::max())
    throw std::runtime_error("Revisão da Composição inválida");
  const auto payload = encode_composition(c),
             hash = composition_checksum(payload), now = project::utc_now();
  SqliteDatabase db(path(p));
  SqliteTransaction tx(db);
  owner(db, p);
  if (context) {
    if (draft_only) throw std::runtime_error("Rascunho não pode alterar o planejamento");
    tx.record_project_operation("Separar realização de composição");
    context(db);
  }
  auto existing = db.prepare(
      "SELECT id,name,revision,updated_at,payload,sha256,length(CAST(payload "
      "AS BLOB)) FROM compositions WHERE project_id=? AND id=?");
  existing.bind(1, p.manifest().project_id);
  existing.bind(2, c.blocks.composition_id());
  if (existing.step()) {
    auto old = read(existing, p.manifest().project_id);
    if (old.revision != expected || old.sha256 != expected_hash)
      throw std::runtime_error(
          "Conflito: Composição mudou em outra sessão; rascunho local "
          "preservado. Recarregue explicitamente.");
    // Applied history and full frozen origins are append-only in v1. A valid
    // hash alone must not authorize rewriting an existing revision identity.
    for (const auto &[sha, origin] : old.value.origins)
      if (!c.origins.contains(sha) ||
          encode_document_snapshot(c.origins.at(sha)) !=
              encode_document_snapshot(origin))
        throw std::runtime_error(
            "Origem congelada não pode ser removida ou alterada");
    for (const auto &[id, block] : old.value.blocks.blocks()) {
      const auto &next = c.blocks.block(id);
      if (next.origin_document_id != block.origin_document_id ||
          next.origin_snapshot_sha256 != block.origin_snapshot_sha256 ||
          next.fork_origin != block.fork_origin ||
          next.revisions.size() < block.revisions.size())
        throw std::runtime_error(
            "Histórico aplicado do bloco deve ser preservado");
      for (std::size_t i = 0; i < block.revisions.size(); ++i) {
        const auto &a = block.revisions[i], &b = next.revisions[i];
        auto source = [](const auto &r) {
          return r.exact_source
                     ? std::tuple(r.exact_source->document_id,
                                  r.exact_source->source_body_sha256,
                                  r.exact_source->start, r.exact_source->end)
                     : std::tuple(std::string{}, std::string{}, std::size_t{},
                                  std::size_t{});
        };
        if (a.id != b.id || a.parent_id != b.parent_id || a.text != b.text ||
            source(a) != source(b) || a.layers.warnings != b.layers.warnings ||
            encode_document_snapshot(*a.layers.document) !=
                encode_document_snapshot(*b.layers.document))
          throw std::runtime_error(
              "Revisão aplicada é imutável; crie outra versão");
      }
    }
    if (draft_only) {
      auto check = c;
      check.drafts = old.value.drafts;
      if (encode_composition(check) != encode_composition(old.value))
        throw std::runtime_error(
            "Salvamento de rascunho tentou alterar a leitura");
    }
  } else if (expected != 0 || draft_only)
    throw std::runtime_error(
        "Composição removida; salvamento não pode recriá-la");
  if (!draft_only && !context)
    tx.record_project_operation(expected == 0 ? "Criar Composição"
                                              : "Alterar Composição");
  auto q = db.prepare(
      "INSERT INTO "
      "compositions(id,project_id,name,revision,updated_at,payload,sha256) "
      "VALUES(?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET "
      "name=excluded.name,revision=excluded.revision,updated_at=excluded."
      "updated_at,payload=excluded.payload,sha256=excluded.sha256");
  q.bind(1, c.blocks.composition_id());
  q.bind(2, p.manifest().project_id);
  q.bind(3, c.name);
  q.bind(4, expected + 1);
  q.bind(5, now);
  q.bind(6, payload);
  q.bind(7, hash);
  q.run();
  tx.commit();
  return {c, p.manifest().project_id, hash, now, expected + 1};
}
} // namespace inde::persistence

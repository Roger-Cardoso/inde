#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/document_snapshot.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/sqlite_writing_repository.hpp"
#include "inde/project/document_state.hpp"
#include "narrative_builder_support.hpp"
#include <algorithm>
#include <glib.h>
#include <map>
#include <stdexcept>

namespace inde::application {
namespace {
using namespace persistence;
using namespace narrative_builder_detail;
using J = json::Value;
std::string hash(const std::string &v) { return composition_checksum(v); }
void note_valid(const std::string &v) {
  if (v.size() > 2048 || v.find('\0') != std::string::npos ||
      !g_utf8_validate(v.data(), v.size(), nullptr))
    throw std::runtime_error("Nota inválida; máximo de 2.048 bytes UTF-8");
}
std::string selection(const project::Document &d,
                      const project::DocumentAnchor &a) {
  project::validate(d);
  if (a.start_offset >= a.end_offset ||
      a.end_offset > project::utf8_character_count(d.content))
    throw std::runtime_error("Escolha uma âncora com trecho não vazio");
  auto *first = g_utf8_offset_to_pointer(d.content.c_str(), a.start_offset),
       *last = g_utf8_offset_to_pointer(d.content.c_str(), a.end_offset);
  return std::string(first, last);
}
const project::DocumentAnchor &anchor(const project::Document &d,
                                      const std::string &id) {
  const auto a = std::find_if(d.anchors.begin(), d.anchors.end(),
                              [&](auto &v) { return v.id == id; });
  if (a == d.anchors.end())
    throw std::runtime_error("Âncora ausente; escolha outro trecho");
  for (const auto &v : d.semantics.anchors)
    if (v.anchor_id == id && v.state != project::AnchorResolution::Resolved)
      throw std::runtime_error(
          "Âncora semântica precisa ser reconciliada antes de associar");
  (void)selection(d, *a);
  return *a;
}
void expect_document(SqliteDatabase &db, const project::Project &p,
                     const project::Document &d) {
  const auto current = SqliteWritingRepository::load_in_transaction(
      db, p.manifest().project_id, d.id);
  if (!current || !project::same_document_state(*current, d))
    throw std::runtime_error(
        "Documento mudou; confira o trecho novamente. Nada foi alterado.");
}
void expect_composition(SqliteDatabase &db, const project::Project &p,
                        const project::StoredComposition &c) {
  auto q = db.prepare(
      "SELECT revision,sha256 FROM compositions WHERE id=? AND project_id=?");
  q.bind(1, c.value.blocks.composition_id());
  q.bind(2, p.manifest().project_id);
  if (c.project_id != p.manifest().project_id || !q.step() ||
      q.column_integer(0) != c.revision || q.column_text(1) != c.sha256 ||
      hash(encode_composition(c.value)) != c.sha256)
    throw std::runtime_error("Composição mudou; recarregue antes de associar");
}
BuilderRealization find(SqliteDatabase &db, const project::Project &p,
                        const NarrativeBuilderSnapshot &s,
                        const std::string &id) {
  const auto current = read(db, p, s.structure_id);
  auto at =
      std::find_if(current.realizations.begin(), current.realizations.end(),
                   [&](auto &r) { return r.id == id; });
  if (at == current.realizations.end())
    throw std::runtime_error("Realização ausente da estrutura");
  return *at;
}
void decision(SqliteDatabase &db, const project::Project &p,
              const std::string &u, const std::string &id,
              const std::string &action, const std::string &evidence) {
  if (evidence.size() > 8192)
    throw std::runtime_error("Evidência de decisão excede 8 KiB");
  auto q =
      db.prepare("INSERT INTO "
                 "narrative_text_decisions(id,project_id,unit_id,realization_"
                 "id,decision,evidence,created_at) VALUES(?,?,?,?,?,?,?)");
  q.bind(1, project::new_uuid());
  q.bind(2, p.manifest().project_id);
  q.bind(3, u);
  q.bind(4, id);
  q.bind(5, action);
  q.bind(6, evidence);
  q.bind(7, project::utc_now());
  q.run();
}
void put(SqliteDatabase &db, const project::Project &p,
         const NarrativeBuilderSnapshot &s, const std::string &u,
         const std::string &d, const std::string &a, const std::string &c,
         const std::string &o, const std::string &sha,
         const std::string &replace) {
  unit(db, p, s, u);
  std::string id = replace.empty() ? project::new_uuid() : replace;
  if (!replace.empty()) {
    const auto old = find(db, p, s, replace);
    if (old.unit_id != u)
      throw std::runtime_error("Realização pertence a outra unidade");
    auto q = db.prepare("DELETE FROM " + old.table + " WHERE id=?");
    q.bind(1, id);
    q.run();
    decision(db, p, u, id, "replace",
             json::serialize(J::Object{{"document", old.document_id},
                                       {"anchor", old.anchor_id},
                                       {"composition", old.composition_id},
                                       {"occurrence", old.occurrence_id},
                                       {"sha", old.current_sha}},
                             false));
  }
  auto q = db.prepare(
      "INSERT INTO "
      "narrative_text_targets(id,project_id,unit_id,document_id,anchor_id,"
      "composition_id,occurrence_id,observed_sha) VALUES(?,?,?,?,?,?,?,?)");
  q.bind(1, id);
  q.bind(2, p.manifest().project_id);
  q.bind(3, u);
  auto optional = [&](int n, const std::string &v) {
    if (v.empty())
      q.bind_null(n);
    else
      q.bind(n, v);
  };
  optional(4, d);
  optional(5, a);
  optional(6, c);
  optional(7, o);
  q.bind(8, sha);
  q.run();
  decision(db, p, u, id, "associate",
           json::serialize(J::Object{{"document", d},
                                     {"anchor", a},
                                     {"composition", c},
                                     {"occurrence", o},
                                     {"observed_sha", sha}},
                           false));
}
project::Document clone_document(const project::Document &source,
                                 const std::string &title,
                                 std::map<std::string, std::string> &anchors) {
  auto copy = source;
  copy.id = project::new_uuid();
  copy.title = title;
  copy.editorial_node_id.reset();
  copy.revision_of_id.reset();
  copy.revision_label.clear();
  copy.created_at = copy.updated_at = project::utc_now();
  for (auto &a : copy.anchors) {
    anchors[a.id] = project::new_uuid();
    a.id = anchors.at(a.id);
  }
  for (auto &r : copy.entity_references) {
    r.id = project::new_uuid();
    if (r.anchor_id)
      r.anchor_id = anchors.at(*r.anchor_id);
  }
  for (auto &a : copy.semantics.anchors)
    a.anchor_id = anchors.at(a.anchor_id);
  std::map<std::string, std::string> annotations;
  for (auto &a : copy.semantics.annotations) {
    annotations[a.id] = project::new_uuid();
    a.id = annotations.at(a.id);
    if (!a.anchor_id.empty())
      a.anchor_id = anchors.at(a.anchor_id);
    for (auto &b : a.fictional_time)
      b.id = project::new_uuid();
    for (auto &b : a.dates)
      b.id = project::new_uuid();
  }
  for (auto &r : copy.semantics.relations) {
    r.id = project::new_uuid();
    r.source_annotation_id = annotations.at(r.source_annotation_id);
    // Cross-document targets remain external canonical references; only
    // identities owned by this copied Document are remapped.
    if (annotations.contains(r.target_annotation_id))
      r.target_annotation_id = annotations.at(r.target_annotation_id);
  }
  project::validate(copy);
  return copy;
}
} // namespace

namespace narrative_builder_detail {
void record_text_detach(SqliteDatabase &db, const project::Project &p,
                        const NarrativeBuilderSnapshot &s,
                        const std::string &id) {
  const auto r = find(db, p, s, id);
  decision(db, p, r.unit_id, id, "unlink",
           json::serialize(J::Object{{"document", r.document_id},
                                     {"anchor", r.anchor_id},
                                     {"composition", r.composition_id},
                                     {"occurrence", r.occurrence_id},
                                     {"sha", r.current_sha},
                                     {"status", r.status},
                                     {"note", r.note}},
                           false));
}
std::string read_targets(SqliteDatabase &db, const project::Project &p,
                         NarrativeBuilderSnapshot &s) {
  J::Array evidence;
  auto q = db.prepare(
      "SELECT "
      "t.id,t.unit_id,t.document_id,t.anchor_id,t.composition_id,t.occurrence_"
      "id,t.observed_sha,t.note,t.review_state,t.provenance,coalesce(d.title,c."
      "name),a.start_offset,a.end_offset,CASE WHEN a.id IS NULL THEN NULL ELSE "
      "substr(d.content,a.start_offset+1,a.end_offset-a.start_offset) END FROM "
      "narrative_text_targets t JOIN narrative_units u ON u.id=t.unit_id LEFT "
      "JOIN documents d ON d.id=t.document_id LEFT JOIN document_anchors a ON "
      "a.id=t.anchor_id AND a.document_id=t.document_id LEFT JOIN compositions "
      "c ON c.id=t.composition_id WHERE u.structure_id=? ORDER BY t.id");
  q.bind(1, s.structure_id);
  std::map<std::string, project::Composition> compositions;
  std::size_t composition_bytes{};
  std::size_t count{};
  while (q.step()) {
    if (++count > 10000)
      throw std::runtime_error(
          "Mais de 10.000 realizações; consulta não foi truncada");
    auto optional = [&](int n) {
      return q.column_is_null(n) ? std::string{} : q.column_text(n);
    };
    BuilderRealization r;
    r.id = q.column_text(0);
    r.unit_id = q.column_text(1);
    r.document_id = optional(2);
    r.anchor_id = optional(3);
    r.composition_id = optional(4);
    r.occurrence_id = optional(5);
    r.observed_sha = q.column_text(6);
    r.note = q.column_text(7);
    r.review_state = q.column_text(8);
    r.provenance = q.column_text(9);
    r.title = q.column_text(10);
    r.table = "narrative_text_targets";
    std::string resolution;
    if (!r.document_id.empty()) {
      if (q.column_is_null(11))
        r.status = "missing";
      else {
        r.start = static_cast<std::size_t>(q.column_integer(11));
        r.end = static_cast<std::size_t>(q.column_integer(12));
        const auto text = q.column_text(13);
        if (text.size() > 16 * 1024 * 1024)
          throw std::runtime_error(
              "Trecho excede 16 MiB; consulta interrompida");
        r.current_sha = hash(text);
        r.status = r.start == r.end ? "empty" : "current";
        // Semantic evidence is read via the canonical Document loader below
        // only when this document has semantic layers; no independent anchor
        // engine.
        auto e = db.prepare("SELECT count(*) FROM semantic_annotations WHERE "
                            "document_id=? AND anchor_id=?");
        e.bind(1, r.document_id);
        e.bind(2, r.anchor_id);
        if (e.step() && e.column_integer(0)) {
          auto d = SqliteWritingRepository::load_in_transaction(
              db, p.manifest().project_id, r.document_id);
          for (const auto &v : d->semantics.anchors)
            if (v.anchor_id == r.anchor_id) {
              resolution = project::to_string(v.state);
              if (v.state != project::AnchorResolution::Resolved)
                r.status = "unresolved";
            }
        }
      }
    } else {
      if (!compositions.contains(r.composition_id)) {
        auto c = db.prepare("SELECT payload,sha256 FROM compositions WHERE "
                            "id=? AND project_id=?");
        c.bind(1, r.composition_id);
        c.bind(2, p.manifest().project_id);
        if (!c.step())
          throw std::runtime_error("Composição da realização ausente");
        const auto payload = c.column_text(0);
        composition_bytes += payload.size();
        if (composition_bytes > 16 * 1024 * 1024)
          throw std::runtime_error(
              "Composições da consulta excedem 16 MiB; reduza a estrutura");
        compositions.emplace(r.composition_id,
                             decode_composition(payload, c.column_text(1)));
      }
      auto &c = compositions.at(r.composition_id);
      auto it = std::find_if(c.blocks.placements().begin(),
                             c.blocks.placements().end(),
                             [&](auto &v) { return v.id == r.occurrence_id; });
      if (it == c.blocks.placements().end())
        r.status = "missing";
      else {
        auto &revision = c.blocks.revision(*it);
        r.current_sha = hash(revision.text);
        resolution =
            std::string(experimental::name(it->mode)) + ":" + revision.id +
            ":" + (c.blocks.visible(*it, c.profile) ? "visible" : "hidden") +
            ":" + (c.drafts.contains(it->block_id) ? "draft" : "applied");
        r.status = it->mode == experimental::PlacementMode::ReferenceOnly
                       ? "reference-only"
                   : !c.blocks.visible(*it, c.profile) ? "hidden"
                   : revision.text.empty()             ? "empty"
                                                       : "current";
        r.title += " · " + c.blocks.block(it->block_id).label;
      }
    }
    if (r.status == "current" && r.current_sha != r.observed_sha)
      r.status = "changed";
    evidence.emplace_back(
        J::Object{{"id", r.id},
                  {"unit", r.unit_id},
                  {"document", r.document_id},
                  {"anchor", r.anchor_id},
                  {"composition", r.composition_id},
                  {"occurrence", r.occurrence_id},
                  {"observed", r.observed_sha},
                  {"current", r.current_sha},
                  {"status", r.status},
                  {"review", r.review_state},
                  {"note", r.note},
                  {"provenance", r.provenance},
                  {"start", static_cast<std::int64_t>(r.start)},
                  {"end", static_cast<std::int64_t>(r.end)},
                  {"resolution", resolution}});
    s.realizations.push_back(std::move(r));
  }
  auto ex = db.prepare(
      "SELECT id,notice_key,evidence_sha,note FROM "
      "narrative_continuity_exceptions WHERE structure_id=? ORDER BY id");
  ex.bind(1, s.structure_id);
  while (ex.step())
    evidence.emplace_back(J::Array{ex.column_text(0), ex.column_text(1),
                                   ex.column_text(2), ex.column_text(3)});
  auto presence = db.prepare(
      "SELECT x.unit_id,x.entity_id FROM narrative_unit_presence_intents x "
      "JOIN narrative_units u ON u.id=x.unit_id WHERE u.structure_id=? ORDER "
      "BY x.unit_id,x.entity_id");
  presence.bind(1, s.structure_id);
  count = 0;
  while (presence.step()) {
    if (++count > 10000)
      throw std::runtime_error("Mais de 10.000 presenças planejadas");
    s.presences.push_back({presence.column_text(0), presence.column_text(1)});
    evidence.emplace_back(
        J::Array{presence.column_text(0), presence.column_text(1)});
  }
  return json::serialize(evidence, false);
}
} // namespace narrative_builder_detail

void NarrativeBuilder::attach_anchor(const NarrativeBuilderSnapshot &s,
                                     const std::string &u,
                                     const project::Document &d,
                                     const std::string &a,
                                     const std::string &replace) const {
  const auto &v = anchor(d, a);
  auto db = open(owner());
  SqliteTransaction tx(db, "Associar trecho à unidade");
  check(db, owner(), s);
  expect_document(db, owner(), d);
  put(db, owner(), s, u, d.id, a, "", "", hash(selection(d, v)), replace);
  tx.commit();
}
void NarrativeBuilder::attach_range(const NarrativeBuilderSnapshot &s,
                                    const std::string &u,
                                    const project::Document &d,
                                    std::size_t start, std::size_t end,
                                    const std::string &label) const {
  if (label.empty() || label.size() > 256)
    throw std::runtime_error("Identifique o trecho em até 256 bytes");
  auto next = d;
  project::DocumentAnchor a{project::new_uuid(), label, start, end};
  const auto sha = hash(selection(d, a));
  next.anchors.push_back(a);
  next.updated_at = project::utc_now();
  SqliteWritingRepository{}.save_with_context(
      owner().path(), next, &d, s.project_id,
      "Marcar trecho e associar unidade", [&](SqliteDatabase &db) {
        check(db, owner(), s);
        put(db, owner(), s, u, d.id, a.id, "", "", sha, "");
      });
}
void NarrativeBuilder::attach_occurrence(const NarrativeBuilderSnapshot &s,
                                         const std::string &u,
                                         const project::StoredComposition &c,
                                         const std::string &o,
                                         const std::string &replace) const {
  const auto &v = c.value.blocks.occurrence(o);
  if (v.mode == experimental::PlacementMode::ReferenceOnly)
    throw std::runtime_error("Referência de navegação não realiza prosa; "
                             "escolha ocorrência incorporada");
  auto db = open(owner());
  SqliteTransaction tx(db, "Associar ocorrência à unidade");
  check(db, owner(), s);
  expect_composition(db, owner(), c);
  put(db, owner(), s, u, "", "", c.value.blocks.composition_id(), o,
      hash(c.value.blocks.revision(v).text), replace);
  tx.commit();
}
void NarrativeBuilder::review_realization(const NarrativeBuilderSnapshot &s,
                                          const std::string &id,
                                          const std::string &note,
                                          bool divergent) const {
  note_valid(note);
  if (divergent && note.empty())
    throw std::runtime_error("Explique a divergência declarada");
  auto db = open(owner());
  SqliteTransaction tx(db, "Conferir realização textual");
  check(db, owner(), s);
  auto r = find(db, owner(), s, id);
  if (r.table != "narrative_text_targets" || r.status == "missing" ||
      r.status == "unresolved" || r.status == "empty")
    throw std::runtime_error(
        "Escolha um trecho/ocorrência válido antes de conferir");
  auto q = db.prepare("UPDATE narrative_text_targets SET "
                      "observed_sha=?,note=?,review_state=? WHERE id=?");
  q.bind(1, r.current_sha);
  q.bind(2, note);
  q.bind(3, divergent ? "diverged" : "confirmed");
  q.bind(4, id);
  q.run();
  decision(db, owner(), r.unit_id, id, divergent ? "diverged" : "confirmed",
           json::serialize(J::Object{{"sha", r.current_sha}, {"note", note}},
                           false));
  tx.commit();
}
void NarrativeBuilder::make_independent(const NarrativeBuilderSnapshot &s,
                                        const std::string &id,
                                        const std::string &title) const {
  const auto &p = owner();
  const auto current = snapshot(s.structure_id);
  if (current.revision != s.revision)
    throw std::runtime_error("Realizações mudaram; confira antes de separar");
  auto it =
      std::find_if(current.realizations.begin(), current.realizations.end(),
                   [&](auto &r) { return r.id == id; });
  if (it == current.realizations.end())
    throw std::runtime_error("Realização ausente");
  const auto r = *it;
  if (r.status == "missing" || r.status == "unresolved")
    throw std::runtime_error("Reconcilie o alvo antes de separar");
  if (!r.document_id.empty()) {
    const auto source =
        SqliteWritingRepository{}.document(p.path(), r.document_id);
    if (!source)
      throw std::runtime_error("Documento ausente");
    std::map<std::string, std::string> mapping;
    auto copy = clone_document(*source, title, mapping);
    SqliteWritingRepository{}.save_with_context(
        p.path(), copy, nullptr, s.project_id, "Separar prosa da realização",
        [&](SqliteDatabase &db) {
          check(db, p, s);
          expect_document(db, p, *source);
          const auto provenance = json::serialize(
              J::Object{{"source_document", source->id},
                        {"source_anchor", r.anchor_id},
                        {"source_sha", hash(encode_document_snapshot(*source))},
                        {"new_document", copy.id}},
              false);
          auto q =
              db.prepare("UPDATE " + r.table + " SET document_id=?" +
                         (r.anchor_id.empty() ? std::string{}
                                              : ",anchor_id=?,provenance=?") +
                         " WHERE id=?");
          q.bind(1, copy.id);
          if (r.anchor_id.empty())
            q.bind(2, r.id);
          else {
            q.bind(2, mapping.at(r.anchor_id));
            q.bind(3, provenance);
            q.bind(4, r.id);
          }
          q.run();
          decision(db, p, r.unit_id, r.id, "independent-document", provenance);
        });
  } else {
    auto c = CompositionStore{}.load(p, r.composition_id);
    const auto old = c.value.blocks.occurrence(r.occurrence_id);
    const auto revision = c.value.blocks.revision(old).id;
    auto next = c.value;
    auto occurrence = next.blocks.place(
        old.block_id, experimental::PlacementMode::Pinned, revision);
    next.blocks.fork(occurrence, revision);
    next.blocks.rename_block(next.blocks.occurrence(occurrence).block_id,
                             title);
    next.blocks.classify(occurrence, old.variant, old.layer);
    // Independent prose must not duplicate the public reading implicitly.
    // The author can explicitly replace/show the new occurrence in Composition.
    next.blocks.set_hidden(occurrence, true);
    // Add a fork to the same named alternative group without losing its reading
    // policy.
    auto state = next.blocks.snapshot();
    for (auto &v : state.placements)
      if (v.id == occurrence)
        v.variant_group = old.variant_group;
    next.blocks = experimental::TextBlocksLab::restore(std::move(state));
    const auto provenance =
        json::serialize(J::Object{{"composition", r.composition_id},
                                  {"source_occurrence", r.occurrence_id},
                                  {"source_revision", revision},
                                  {"source_sha", c.sha256},
                                  {"new_occurrence", occurrence}},
                        false);
    (void)CompositionStore{}.save(
        p, next, c.revision, c.sha256, false, [&](SqliteDatabase &db) {
          check(db, p, s);
          auto q = db.prepare("UPDATE narrative_text_targets SET "
                              "occurrence_id=?,provenance=? WHERE id=?");
          q.bind(1, occurrence);
          q.bind(2, provenance);
          q.bind(3, id);
          q.run();
          decision(db, p, r.unit_id, id, "independent-occurrence", provenance);
        });
  }
}
void NarrativeBuilder::declare_presence(const NarrativeBuilderSnapshot &s,
                                        const std::string &u,
                                        const std::string &entity,
                                        bool present) const {
  auto db = open(owner());
  SqliteTransaction tx(db, "Declarar presença planejada");
  check(db, owner(), s);
  unit(db, owner(), s, u);
  if (present) {
    auto q = db.prepare("INSERT INTO "
                        "narrative_unit_presence_intents(id,project_id,unit_id,"
                        "entity_id) VALUES(?,?,?,?)");
    q.bind(1, project::new_uuid());
    q.bind(2, s.project_id);
    q.bind(3, u);
    q.bind(4, entity);
    q.run();
  } else {
    auto q = db.prepare("DELETE FROM narrative_unit_presence_intents WHERE "
                        "unit_id=? AND entity_id=?");
    q.bind(1, u);
    q.bind(2, entity);
    q.run();
  }
  tx.commit();
}
std::vector<project::RemovalUse>
NarrativeBuilder::text_uses(const std::string &document,
                            const std::string &composition,
                            const std::string &occurrence) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  const bool is_document = !document.empty();
  if (is_document == !composition.empty() ||
      (!is_document && occurrence.empty()) ||
      (is_document && !occurrence.empty()))
    throw std::runtime_error("Escolha Documento ou ocorrência");
  auto q = db.prepare(
      "SELECT "
      "x.id,x.unit_id,u.structure_id,s.work_id,u.title,s.name,w.title,x.target_"
      "table "
      "FROM (SELECT id,unit_id,'narrative_realizations' AS target_table FROM "
      "narrative_realizations WHERE "
      "document_id=?1 UNION ALL SELECT id,unit_id,'narrative_text_targets' "
      "FROM narrative_text_targets "
      "WHERE document_id=?1 OR (composition_id=?2 AND occurrence_id=?3)) x "
      "JOIN narrative_units u ON u.id=x.unit_id JOIN narrative_structures s ON "
      "s.id=u.structure_id JOIN works w ON w.id=s.work_id WHERE "
      "u.project_id=?4 ORDER BY w.title,s.name,u.title,x.id LIMIT 10001");
  if (is_document)
    q.bind(1, document);
  else
    q.bind_null(1);
  if (is_document) {
    q.bind_null(2);
    q.bind_null(3);
  } else {
    q.bind(2, composition);
    q.bind(3, occurrence);
  }
  q.bind(4, p.manifest().project_id);
  std::vector<project::RemovalUse> out;
  while (q.step()) {
    if (out.size() >= 10000)
      throw std::runtime_error(
          "Mais de 10.000 usos textuais; consulta não foi truncada");
    project::RemovalUse use;
    use.table = q.column_text(7);
    use.record_id = q.column_text(0);
    use.location_table = "narrative_units";
    use.location_id = q.column_text(1);
    use.structure_id = q.column_text(2);
    use.work_id = q.column_text(3);
    use.name = q.column_text(4);
    use.path = q.column_text(6) + " · " + q.column_text(5);
    out.push_back(std::move(use));
  }
  tx.commit();
  return out;
}
} // namespace inde::application

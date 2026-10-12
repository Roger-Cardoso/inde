#include "../persistence/calendar_codec.hpp"
#include "inde/persistence/composition_store.hpp"
#include "inde/persistence/json.hpp"
#include "narrative_builder_support.hpp"
#include "temporal_extent_codec.hpp"
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <stdexcept>

namespace inde::application {
namespace {
using namespace persistence;
using namespace narrative_builder_detail;
struct Date {
  project::TemporalReference reference;
  project::FictionalCalendar calendar;
  std::optional<std::int64_t> epoch;
  bool ambiguous_epoch{};
  std::string evidence;
};
Date load_date(SqliteDatabase &db, const std::string &id) {
  auto q = db.prepare(
      "SELECT "
      "r.axis_id,r.calendar_id,r.label,r.year,r.month,r.day,r.clock,r.token,e."
      "payload,c.payload,c.sha256 FROM temporal_references r JOIN "
      "fictional_calendars c ON c.id=r.calendar_id LEFT JOIN "
      "temporal_reference_extents e ON e.reference_id=r.id WHERE r.id=?");
  q.bind(1, id);
  if (!q.step())
    throw std::runtime_error("Data declarada ausente");
  Date d;
  auto &r = d.reference;
  r.id = id;
  r.axis_id = q.column_text(0);
  r.calendar_id = q.column_text(1);
  r.label = q.column_text(2);
  r.year = q.column_integer(3);
  if (!q.column_is_null(4))
    r.month = static_cast<int>(q.column_integer(4));
  if (!q.column_is_null(5))
    r.day = static_cast<int>(q.column_integer(5));
  const auto clock = json::parse(q.column_text(6));
  for (const auto &v : clock.as_array())
    r.clock.push_back(static_cast<int>(v.as_integer()));
  r.token = q.column_text(7);
  if (!q.column_is_null(8))
    temporal_extent_codec::decode(q.column_text(8), r);
  d.calendar = calendar_codec::decode(q.column_text(9), q.column_text(10));
  project::validate(d.calendar, r);
  auto epoch =
      db.prepare("SELECT epoch_ordinal FROM axis_calendars WHERE axis_id=?1 "
                 "AND calendar_id=?2 UNION SELECT epoch_ordinal FROM "
                 "calendar_adoptions WHERE axis_id=?1 AND calendar_id=?2");
  epoch.bind(1, r.axis_id);
  epoch.bind(2, r.calendar_id);
  if (epoch.step()) {
    auto n = epoch.column_integer(0);
    if (!epoch.step())
      d.epoch = n;
    else
      d.ambiguous_epoch = true;
  }
  d.evidence = r.label + " [" + r.id + "] · " +
               project::format_temporal_reference(d.calendar, r) + " · eixo " +
               r.axis_id + " · calendário " + r.calendar_id + " · precisão " +
               r.qualifier + " · evidência " + r.token + "/" +
               q.column_text(10) + "/" + temporal_extent_codec::encode(r) +
               " · época " +
               (d.epoch             ? std::to_string(*d.epoch)
                : d.ambiguous_epoch ? "correlação ambígua"
                                    : "não correlacionada");
  return d;
}
std::optional<std::pair<project::TemporalWindow, project::TemporalWindow>>
windows(const Date &a, const Date &b) {
  if (a.reference.axis_id != b.reference.axis_id || a.ambiguous_epoch ||
      b.ambiguous_epoch)
    return {};
  std::int64_t ae{}, be{};
  if (a.reference.calendar_id != b.reference.calendar_id) {
    if (!a.epoch || !b.epoch)
      return {};
    ae = *a.epoch;
    be = *b.epoch;
  }
  const auto x = project::temporal_window(a.calendar, a.reference, ae),
             y = project::temporal_window(b.calendar, b.reference, be);
  if (!x.comparable || !y.comparable)
    return {};
  return std::pair{x, y};
}
std::vector<BuilderNotice> inspect(SqliteDatabase &db,
                                   const NarrativeBuilderSnapshot &s) {
  std::vector<BuilderNotice> out;
  std::size_t work{};
  auto spend = [&] {
    if (++work > 2000000)
      throw std::runtime_error(
          "Continuidade excede 2.000.000 etapas de consulta; reduza o escopo. "
          "Nenhum resultado parcial foi apresentado.");
  };
  auto add = [&](std::string key, std::string kind, std::string a,
                 std::string b, std::string title, std::string evidence) {
    if (out.size() >= 4000)
      throw std::runtime_error("Continuidade excede 4.000 avisos; reduza a "
                               "estrutura. Consulta não foi truncada.");
    if (evidence.size() > 8192)
      throw std::runtime_error(
          "Cadeia de evidência excede 8 KiB; reduza o escopo");
    out.push_back({std::move(key), std::move(kind), std::move(a), std::move(b),
                   std::move(title), evidence, composition_checksum(evidence)});
  };
  std::map<std::string, std::string> names, locations;
  std::map<std::string, std::int64_t> narrative, writing;
  for (auto &u : s.units) {
    names[u.id] = u.title + " [" + u.id + "]";
    narrative[u.id] = u.position;
  }
  for (auto &c : s.contexts) {
    locations[c.unit_id] = c.location_id;
    if (c.writing_position)
      writing[c.unit_id] = *c.writing_position;
  }
  std::map<std::string, Date> dates;
  auto date = [&](const std::string &id) -> const Date & {
    if (!dates.contains(id))
      dates.emplace(id, load_date(db, id));
    return dates.at(id);
  };
  std::map<std::string, std::vector<std::string>> unit_dates;
  for (const auto &d : s.dates)
    if (!d.unit_id.empty() && d.meaning == "occurrence")
      unit_dates[d.unit_id].push_back(d.reference_id);
  auto scene = [&](const std::string &u) -> const Date * {
    const auto &v = unit_dates[u];
    return v.size() == 1 ? &date(v.front()) : nullptr;
  };
  for (const auto &r : s.realizations)
    if (r.table == "narrative_text_targets") {
      auto identity = "Unidade " + names.at(r.unit_id) + " · realização " +
                      r.id + " · Documento " + r.document_id + " · âncora " +
                      r.anchor_id + " · composição " + r.composition_id +
                      " · ocorrência " + r.occurrence_id + " · SHA conferido " +
                      r.observed_sha + " · SHA atual " + r.current_sha +
                      " · estado " + r.status + " · nota " + r.note;
      if (r.status == "missing" || r.status == "empty" ||
          r.status == "unresolved")
        add("text:" + r.id, "reconciliation", r.unit_id, "",
            "Trecho/ocorrência precisa de reconciliação", identity);
      else if (r.status == "changed")
        add("text:" + r.id, "review", r.unit_id, "",
            "Texto mudou desde a associação/conferência",
            identity + ". Alteração não significa divergência semântica.");
      else if (r.status == "hidden" || r.status == "reference-only")
        add("text:" + r.id, "review", r.unit_id, "",
            "Ocorrência não integra a leitura atual", identity);
      if (r.review_state == "diverged")
        add("diverged:" + r.id, "author-declared", r.unit_id, "",
            "Divergência declarada pelo autor", identity);
    }
  struct Edge {
    std::string to, id;
  };
  std::map<std::string, std::map<std::string, std::vector<Edge>>> graphs;
  for (const auto &l : s.links) {
    std::string scope = "unclassified";
    for (const auto &v : s.scopes)
      if (v.link_id == l.id)
        scope = v.scope;
    if (l.kind != project::NarrativeLinkKind::Precedes &&
        l.kind != project::NarrativeLinkKind::Depends)
      continue;
    if (scope == "unclassified") {
      add("unclassified:" + l.id, "insufficient", l.source_unit_id,
          l.target_unit_id, "Dependência sem autoridade declarada",
          l.id + " · " + names.at(l.source_unit_id) + " → " +
              names.at(l.target_unit_id) + " · " + l.label +
              ". Não recebe semântica cronológica.");
      continue;
    }
    const auto a = l.kind == project::NarrativeLinkKind::Depends
                       ? l.target_unit_id
                       : l.source_unit_id,
               b = l.kind == project::NarrativeLinkKind::Depends
                       ? l.source_unit_id
                       : l.target_unit_id;
    graphs[scope][a].push_back({b, l.id});
  }
  for (const auto &[scope, graph] : graphs) {
    // Iterative breadth-first reachability: complete paths, not direct
    // neighbors alone. The four authority graphs are never merged into a single
    // order.
    for (const auto &[root, edges] : graph) {
      (void)edges;
      std::deque<std::string> queue{root};
      bool cycle_reported = false;
      std::map<std::string, std::pair<std::string, std::string>> parent{
          {root, {"", ""}}};
      while (!queue.empty()) {
        auto at = queue.front();
        queue.pop_front();
        auto found = graph.find(at);
        if (found == graph.end())
          continue;
        for (const auto &edge : found->second) {
          spend();
          if (edge.to != root && parent.contains(edge.to))
            continue;
          if (edge.to == root && cycle_reported)
            continue;
          auto evidence = [&] {
            std::vector<std::string> path{edge.id};
            auto previous = at;
            while (previous != root) {
              path.push_back(parent.at(previous).second);
              previous = parent.at(previous).first;
            }
            std::reverse(path.begin(), path.end());
            std::string result = scope + " · " + names.at(root) + " → " +
                                 names.at(edge.to) + " · vínculos";
            for (const auto &id : path)
              result += " → " + id;
            return result;
          };
          if (edge.to == root) {
            cycle_reported = true;
            add("cycle:" + scope + ":" + root, "cycle", root, root,
                "Ciclo de dependência na autoridade declarada", evidence());
            continue;
          }
          if (parent.contains(edge.to))
            continue;
          parent[edge.to] = {at, edge.id};
          queue.push_back(edge.to);
          auto key = "chain:" + scope + ":" + root + ":" + edge.to;
          if (scope == "narrative" || scope == "writing") {
            const auto &rank = scope == "narrative" ? narrative : writing;
            if (!rank.contains(root) || !rank.contains(edge.to))
              add(key, "insufficient", root, edge.to,
                  "Dependência inclui unidade fora da fila", evidence());
            else if (rank.at(root) >= rank.at(edge.to))
              add(key, "order", root, edge.to,
                  "Ordem contraria cadeia declarada",
                  evidence() + " · posições " + std::to_string(rank.at(root)) +
                      " / " + std::to_string(rank.at(edge.to)));
          } else if (scope == "chronology") {
            const auto *a = scene(root), *b = scene(edge.to);
            if (!a || !b) {
              add(key, "insufficient", root, edge.to,
                  "Cronologia sem contexto de ocorrência único",
                  evidence() + ". Declare exatamente uma data com significado "
                               "Ocorrência por unidade.");
              continue;
            }
            auto with_dates = [&] {
              return evidence() + "\n" + a->evidence + "\n" + b->evidence;
            };
            auto w = windows(*a, *b);
            if (!w || !w->first.lower_bound || !w->first.upper_bound ||
                !w->second.lower_bound || !w->second.upper_bound)
              add(key, "insufficient", root, edge.to,
                  "Datas não comparáveis ou sem limites suficientes",
                  with_dates());
            else if (project::compare_temporal_coordinates(
                         *w->first.lower_bound, *w->second.upper_bound) >= 0)
              add(key, "chronology", root, edge.to,
                  "Datas contrariam precedência cronológica declarada",
                  with_dates());
            else if (project::compare_temporal_coordinates(
                         *w->first.upper_bound, *w->second.lower_bound) > 0)
              add(key, "insufficient", root, edge.to,
                  "Precisão das datas não estabelece precedência",
                  with_dates());
          } else {
            add(key, "insufficient", root, edge.to,
                "Precedência editorial não demonstrada pela colocação textual",
                evidence() + ". A estrutura editorial tem autoridade própria; "
                             "associação de texto não define uma sequência "
                             "pública única.");
          }
        }
      }
    }
  }
  for (std::size_t i = 0; i < s.presences.size(); ++i) {
    const auto &v = s.presences[i];
    const auto *a = scene(v.unit_id);
    const auto local = locations[v.unit_id];
    const auto intro = "Presença planejada explicitamente: entidade " +
                       v.entity_id + " · " + names.at(v.unit_id) + " · Local " +
                       local;
    if (!a || local.empty()) {
      add("presence-context:" + v.unit_id + ":" + v.entity_id, "insufficient",
          v.unit_id, "", "Presença planejada sem tempo/Local suficiente",
          intro + ". Vínculo de entidade não foi usado como presença.");
      continue;
    }
    auto birth =
        db.prepare("SELECT reference_id FROM temporal_attachments WHERE "
                   "entity_id=? AND meaning='birth' ORDER BY id LIMIT 2");
    birth.bind(1, v.entity_id);
    if (birth.step()) {
      const auto id = birth.column_text(0);
      if (birth.step())
        add("birth:" + v.unit_id + ":" + v.entity_id, "insufficient", v.unit_id,
            "", "Há mais de um nascimento formal; nenhum foi escolhido", intro);
      else {
        const auto &b = date(id);
        auto w = windows(*a, b);
        if (!w || !w->first.upper_bound || !w->second.lower_bound)
          add("birth:" + v.unit_id + ":" + v.entity_id, "insufficient",
              v.unit_id, "",
              "Nascimento e cena não são suficientemente comparáveis",
              intro + "\n" + a->evidence + "\n" + b.evidence);
        else if (project::compare_temporal_coordinates(
                     *w->first.upper_bound, *w->second.lower_bound) <= 0)
          add("birth:" + v.unit_id + ":" + v.entity_id, "presence", v.unit_id,
              "", "Presença planejada antecede nascimento formal",
              intro + "\n" + a->evidence + "\n" + b.evidence +
                  ". Viagem no tempo pode justificar; aviso não bloqueia "
                  "escrita.");
      }
    }
    for (std::size_t j = i + 1; j < s.presences.size(); ++j) {
      spend();
      const auto &v2 = s.presences[j];
      if (v.entity_id != v2.entity_id || locations[v2.unit_id].empty() ||
          local == locations[v2.unit_id])
        continue;
      const auto *b = scene(v2.unit_id);
      if (!b)
        continue;
      auto w = windows(*a, *b);
      auto key = "presence:" + v.unit_id + ":" + v2.unit_id + ":" + v.entity_id;
      auto evidence = intro + "\n" + names.at(v2.unit_id) + " · Local " +
                      locations[v2.unit_id] + "\n" + a->evidence + "\n" +
                      b->evidence;
      if (!w || !w->first.lower_bound || !w->first.upper_bound ||
          !w->second.lower_bound || !w->second.upper_bound)
        add(key, "insufficient", v.unit_id, v2.unit_id,
            "Presenças em locais diferentes sem comparação temporal suficiente",
            evidence);
      else if (project::compare_temporal_coordinates(
                   *w->first.lower_bound, *w->second.upper_bound) < 0 &&
               project::compare_temporal_coordinates(*w->second.lower_bound,
                                                     *w->first.upper_bound) < 0)
        add(key, "presence", v.unit_id, v2.unit_id,
            "Conferir possível sobreposição em locais diferentes",
            evidence + ". Janelas sobrepostas não provam simultaneidade; "
                       "conferir precisão, deslocamento ou exceção. Distância "
                       "não determina duração de viagem.");
    }
    auto canonical =
        db.prepare("SELECT "
                   "p.id,p.location_entity_id,s.axis_id,s.ordinal,e.ordinal,p."
                   "updated_at FROM entity_presences p JOIN "
                   "fictional_time_points s ON s.id=p.start_time_point_id LEFT "
                   "JOIN fictional_time_points e ON e.id=p.end_time_point_id "
                   "WHERE p.entity_id=? ORDER BY p.id LIMIT 10001");
    canonical.bind(1, v.entity_id);
    std::size_t count{};
    while (canonical.step()) {
      spend();
      if (++count > 10000)
        throw std::runtime_error(
            "Presenças canônicas excedem 10.000; consulta incompleta recusada");
      if (canonical.column_text(1) == local ||
          canonical.column_text(2) != a->reference.axis_id)
        continue;
      const auto key =
          "canonical-presence:" + v.unit_id + ":" + canonical.column_text(0);
      auto evidence = intro + "\n" + a->evidence + "\nPresença canônica " +
                      canonical.column_text(0) + " · Local " +
                      canonical.column_text(1) + " · início ordinal " +
                      std::to_string(canonical.column_integer(3)) + " · fim " +
                      (canonical.column_is_null(4)
                           ? "aberto"
                           : std::to_string(canonical.column_integer(4))) +
                      " · revisão " + canonical.column_text(5);
      if (!a->epoch) {
        add(key, "insufficient", v.unit_id, "",
            "Presença canônica exige correlação explícita entre data e eixo",
            evidence);
        continue;
      }
      const auto w =
          project::temporal_window(a->calendar, a->reference, *a->epoch);
      if (!w.comparable || !w.lower_bound || !w.upper_bound) {
        add(key, "insufficient", v.unit_id, "",
            "Janela planejada não permite avaliar presença canônica", evidence);
        continue;
      }
      const bool before_end = canonical.column_is_null(4) ||
                              w.lower_bound->day <= canonical.column_integer(4);
      const project::TemporalCoordinate start{canonical.column_integer(3), 0,
                                              1};
      if (before_end &&
          project::compare_temporal_coordinates(*w.upper_bound, start) > 0)
        add(key, "presence", v.unit_id, "",
            "Conferir Local planejado frente a presença canônica",
            evidence + ". Comparação aponta sobreposição possível, não "
                       "impossibilidade de viagem.");
    }
  }
  auto exceptions = db.prepare(
      "SELECT notice_key,evidence_sha,note FROM "
      "narrative_continuity_exceptions WHERE structure_id=? ORDER BY id");
  exceptions.bind(1, s.structure_id);
  while (exceptions.step()) {
    const auto key = exceptions.column_text(0), sha = exceptions.column_text(1),
               note = exceptions.column_text(2);
    auto at = std::find_if(out.begin(), out.end(),
                           [&](auto &n) { return n.key == key; });
    if (at != out.end() && at->evidence_sha == sha)
      at->exception_note = note;
    else
      add("stale-exception:" + key, "review",
          at == out.end() ? "" : at->unit_id, "",
          "Exceção anterior precisa de nova conferência",
          "Aviso original " + key + " · SHA " + sha + " · justificativa " +
              note +
              ". Evidência mudou ou o aviso não existe mais; exceção não foi "
              "reaplicada.");
  }
  return out;
}
} // namespace
std::vector<BuilderNotice>
NarrativeBuilder::continuity(const NarrativeBuilderSnapshot &s) const {
  auto db = open(owner());
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  check(db, owner(), s);
  auto out = inspect(db, read(db, owner(), s.structure_id));
  tx.commit();
  return out;
}
void NarrativeBuilder::set_exception(const NarrativeBuilderSnapshot &s,
                                     const BuilderNotice &notice,
                                     const std::string &note) const {
  if (note.size() > 2048 || note.find('\0') != std::string::npos ||
      !g_utf8_validate(note.data(), note.size(), nullptr))
    throw std::runtime_error("Justificativa inválida; máximo 2.048 bytes");
  auto db = open(owner());
  SqliteTransaction tx(db, "Justificar aviso de continuidade");
  check(db, owner(), s);
  if (note.empty()) {
    auto q = db.prepare("DELETE FROM narrative_continuity_exceptions WHERE "
                        "structure_id=? AND notice_key=?");
    q.bind(1, s.structure_id);
    q.bind(2, notice.key);
    q.run();
  } else {
    const auto current = inspect(db, read(db, owner(), s.structure_id));
    auto at = std::find_if(current.begin(), current.end(), [&](auto &n) {
      return n.key == notice.key && n.evidence_sha == notice.evidence_sha;
    });
    if (at == current.end())
      throw std::runtime_error(
          "Evidência do aviso mudou; confira novamente antes de justificar");
    if (at->kind == "insufficient" || at->kind == "reconciliation" ||
        at->kind == "review")
      throw std::runtime_error("Informação insuficiente/pendência exige "
                               "conferência, não uma exceção de continuidade");
    auto q = db.prepare(
        "INSERT INTO "
        "narrative_continuity_exceptions(id,project_id,structure_id,notice_key,"
        "evidence_sha,note,created_at) VALUES(?,?,?,?,?,?,?) ON "
        "CONFLICT(structure_id,notice_key) DO UPDATE SET "
        "evidence_sha=excluded.evidence_sha,note=excluded.note,created_at="
        "excluded.created_at");
    q.bind(1, project::new_uuid());
    q.bind(2, owner().manifest().project_id);
    q.bind(3, s.structure_id);
    q.bind(4, at->key);
    q.bind(5, at->evidence_sha);
    q.bind(6, note);
    q.bind(7, project::utc_now());
    q.run();
  }
  tx.commit();
}
} // namespace inde::application

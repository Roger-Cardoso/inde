#include "inde/persistence/json.hpp"
#include "narrative_builder_support.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace inde::application {
namespace {
using namespace persistence;
using namespace narrative_builder_detail;
using J = json::Value;
using O = J::Object;
void run(SqliteDatabase &db, const std::string &sql,
         std::initializer_list<J> args) {
  auto q = db.prepare(sql);
  int i = 1;
  for (const auto &a : args) {
    if (a.is_null())
      q.bind_null(i);
    else if (std::holds_alternative<std::string>(a.storage()))
      q.bind(i, a.as_string());
    else
      q.bind(i, a.as_integer());
    ++i;
  }
  q.run();
}
O origins(const NarrativeBuilderSnapshot &s, const std::string &key) {
  if (s.provenance_json.empty())
    return {};
  return json::parse(s.provenance_json).at(key).as_object();
}
std::string root_id(const O &mapping, const std::string &id) {
  const auto at = mapping.find(id);
  return at == mapping.end() ? id : at->second.as_string();
}
std::string joined(std::vector<std::string> v) {
  std::sort(v.begin(), v.end());
  return json::serialize(J::Array(v.begin(), v.end()), false);
}
// Identity correspondence only. Titles and prose never match unrelated items.
O normalized(const NarrativeBuilderSnapshot &s) {
  auto um = origins(s, "units"), lm = origins(s, "lines");
  O result, units, lines;
  std::map<std::string, std::size_t> queue;
  std::vector<std::pair<std::int64_t, std::string>> positions;
  for (auto &c : s.contexts)
    if (c.writing_position)
      positions.emplace_back(*c.writing_position, c.unit_id);
  std::sort(positions.begin(), positions.end());
  for (std::size_t i = 0; i < positions.size(); ++i)
    queue[positions[i].second] = i + 1;
  std::map<std::string, std::vector<std::pair<std::int64_t, std::string>>>
      line_positions;
  for (const auto &m : s.memberships)
    line_positions[m.line_id].emplace_back(m.position, m.unit_id);
  std::map<std::pair<std::string, std::string>, std::size_t> line_rank;
  for (auto &[line, list] : line_positions) {
    std::sort(list.begin(), list.end());
    for (std::size_t i = 0; i < list.size(); ++i)
      line_rank[{line, list[i].second}] = i + 1;
  }
  for (std::size_t i = 0; i < s.units.size(); ++i) {
    auto &u = s.units[i];
    O fields{{"Título", u.title},
             {"Designador", u.designator},
             {"Resumo", u.summary},
             {"Intenção", u.purpose},
             {"Perspectiva", u.perspective},
             {"Montagem narrativa", std::to_string(i + 1)},
             {"Fila de escrita", queue.contains(u.id)
                                     ? std::to_string(queue[u.id])
                                     : "fora da fila"},
             {"Local", "não declarado"}};
    for (auto &c : s.contexts)
      if (c.unit_id == u.id && !c.location_id.empty())
        fields["Local"] = c.location_id;
    std::vector<std::string> members, entities, relations, docs, dates, roles;
    for (auto &m : s.memberships)
      if (m.unit_id == u.id) {
        members.push_back(root_id(lm, m.line_id) + " · posição " +
                          std::to_string(line_rank.at({m.line_id, u.id})));
      }
    for (auto &e : s.entities)
      if (e.unit_id == u.id)
        entities.push_back(e.entity_id + " · " + e.role);
    for (auto &r : s.relations)
      if (r.unit_id == u.id)
        relations.push_back(r.relation_id);
    for (auto &r : s.realizations)
      if (r.unit_id == u.id)
        docs.push_back(r.table=="narrative_realizations"?r.document_id + " · " + r.note:
                       r.document_id + " · " + r.anchor_id + " · " + r.composition_id + " · " +
                       r.occurrence_id + " · " + r.note + " · " + r.review_state);
    for (auto &r : s.dates)
      if (r.unit_id == u.id)
        dates.push_back(r.reference_id + " · " + r.role + " · " + r.meaning +
                        " · " + r.note);
    for (auto &r : s.roles)
      if (r.unit_id && *r.unit_id == u.id)
        roles.push_back(r.role_id + " · " + r.entity_id + " · " + r.notes);
    fields["Linhas e posições próprias"] = joined(members);
    fields["Entidades e funções"] = joined(entities);
    fields["Relações em foco"] = joined(relations);
    fields["Documentos associados"] = joined(docs);
    fields["Referências temporais"] = joined(dates);
    fields["Papéis narrativos"] = joined(roles);
    std::vector<std::string> presences;
    for(const auto &v:s.presences)if(v.unit_id==u.id)presences.push_back(v.entity_id);
    fields["Presenças planejadas"] = joined(presences);
    units[root_id(um, u.id)] =
        O{{"id", u.id}, {"title", u.title}, {"fields", fields}};
  }
  for (auto &l : s.lines) {
    std::vector<std::string> dates;
    for (auto &r : s.dates)
      if (r.line_id == l.id)
        dates.push_back(r.reference_id + " · " + r.role + " · " + r.meaning +
                        " · " + r.note);
    lines[root_id(lm, l.id)] =
        O{{"id", l.id},
          {"title", l.name},
          {"fields", O{{"Nome da linha", l.name},
                       {"Objetivo / descrição", l.description},
                       {"Referências temporais", joined(dates)}}}};
  }
  std::vector<std::string> links, roles;
  for (auto &l : s.links) {
    std::string scope = "unclassified";
    for (auto &d : s.scopes)
      if (d.link_id == l.id)
        scope = d.scope;
    links.push_back(
        root_id(um, l.source_unit_id) + " → " + root_id(um, l.target_unit_id) +
        " · " + project::to_string(l.kind) + " · " + scope + " · " + l.label);
  }
  for (auto &r : s.roles)
    if (!r.unit_id)
      roles.push_back(r.role_id + " · " + r.entity_id + " · " + r.notes);
  result["units"] = units;
  result["lines"] = lines;
  result["links"] = joined(links);
  result["roles"] = joined(roles);
  return result;
}
BuilderComparison compare_values(const O &a, const O &b, std::string an,
                                 std::string bn, bool origin) {
  BuilderComparison r{std::move(an), std::move(bn), {}, origin};
  auto append = [&](BuilderDifference d) {
    if (r.differences.size() >= 4000)
      throw std::runtime_error("Comparação excede 4.000 diferenças; reduza o "
                               "escopo. Nada foi aplicado.");
    // Keep the complete evidence, not an implicitly truncated preview.
    r.differences.push_back(std::move(d));
  };
  for (const auto &kind : {std::string("units"), std::string("lines")}) {
    const auto &left = a.at(kind).as_object(), &right = b.at(kind).as_object();
    std::set<std::string> ids;
    for (auto &[id, v] : left)
      ids.insert(id);
    for (auto &[id, v] : right)
      ids.insert(id);
    for (auto &id : ids) {
      auto x = left.find(id), y = right.find(id);
      const bool is_unit = kind == "units";
      if (x == left.end() || y == right.end()) {
        auto &v = x == left.end() ? y->second : x->second;
        append({x == left.end() ? "Adicionado" : "Ausente no destino",
                is_unit && x != left.end() ? v.at("id").as_string() : "",
                is_unit && y != right.end() ? v.at("id").as_string() : "",
                v.at("title").as_string(),
                is_unit ? "Unidade identificada por proveniência; nenhuma "
                          "correspondência por nome."
                        : "Linha identificada por proveniência."});
        continue;
      }
      const auto &xf = x->second.at("fields").as_object(),
                 &yf = y->second.at("fields").as_object();
      std::set<std::string> fields;
      for(const auto &[field,value]:xf){(void)value;fields.insert(field);}
      for(const auto &[field,value]:yf){(void)value;fields.insert(field);}
      for (const auto &field : fields) {
        auto value=[&](const O &v){const auto at=v.find(field);return at==v.end()?
          (field=="Presenças planejadas"?std::string("[]"):std::string("não registrado na origem")):at->second.as_string();};
        if (value(xf) != value(yf))
          append({field, is_unit ? x->second.at("id").as_string() : "",
                  is_unit ? y->second.at("id").as_string() : "",
                  y->second.at("title").as_string(),
                  "Origem: " + value(xf) +
                      "\nDestino: " + value(yf)});
      }
    }
  }
  for (auto field : {"links", "roles"})
    if (a.at(field).as_string() != b.at(field).as_string())
      append({field == std::string("links") ? "Encadeamento e autoridades"
                                            : "Papéis da estrutura",
              "", "", "Estrutura",
              "Origem: " + a.at(field).as_string() +
                  "\nDestino: " + b.at(field).as_string()});
  return r;
}
} // namespace

project::NarrativeStructure
NarrativeBuilder::derive(const NarrativeBuilderSnapshot &expected,
                         std::string name, bool derived) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Alternativa narrativa com contexto completo");
  check(db, p, expected);
  const auto s = read(db, p, expected.structure_id);
  const auto now = project::utc_now();
  project::NarrativeStructure target{
      project::new_uuid(),
      s.work_id,
      std::move(name),
      s.description,
      std::nullopt,
      s.structure_id,
      derived ? project::StructureCreationKind::Derived
              : project::StructureCreationKind::Duplicated,
      false,
      now,
      now};
  project::validate(target);
  auto model =
      db.prepare("SELECT model_id FROM narrative_structures WHERE id=?");
  model.bind(1, s.structure_id);
  model.step();
  if (!model.column_is_null(0))
    target.model_id = model.column_text(0);
  std::map<std::string, std::string> units, lines, links;
  O um, lm;
  const auto old_um = origins(s, "units"), old_lm = origins(s, "lines");
  for (auto &u : s.units) {
    units[u.id] = project::new_uuid();
    um[units[u.id]] = root_id(old_um, u.id);
  }
  for (auto &l : s.lines) {
    lines[l.id] = project::new_uuid();
    lm[lines[l.id]] = root_id(old_lm, l.id);
  }
  auto observation = json::serialize(O{{"version", 1},
                                       {"units", um},
                                       {"lines", lm},
                                       {"baseline", normalized(s)}},
                                     false);
  if (observation.size() > 4 * 1024 * 1024)
    throw std::runtime_error(
        "Proveniência excede 4 MiB; nenhuma alternativa foi criada.");
  run(db,
      "INSERT INTO "
      "narrative_structures(id,project_id,work_id,name,description,model_id,"
      "derived_from_id,creation_kind,is_active,created_at,updated_at) SELECT "
      "?,project_id,work_id,?,description,model_id,id,?,0,?,? FROM "
      "narrative_structures WHERE id=?",
      {target.id, target.name, project::to_string(target.creation_kind), now,
       now, s.structure_id});
  for (auto &l : s.lines)
    run(db,
        "INSERT INTO "
        "narrative_lines(id,project_id,structure_id,name,description,position,"
        "created_at,updated_at) SELECT "
        "?,project_id,?,name,description,position,?,? FROM narrative_lines "
        "WHERE id=?",
        {lines.at(l.id), target.id, now, now, l.id});
  for (auto &u : s.units)
    run(db,
        "INSERT INTO "
        "narrative_units(id,project_id,structure_id,designator,title,summary,"
        "purpose,perspective,position,created_at,updated_at) SELECT "
        "?,project_id,?,designator,title,summary,purpose,perspective,position,?"
        ",? FROM narrative_units WHERE id=?",
        {units.at(u.id), target.id, now, now, u.id});
  for (auto &m : s.memberships)
    run(db,
        "INSERT INTO narrative_unit_lines(project_id,unit_id,line_id,position) "
        "VALUES(?,?,?,?)",
        {s.project_id, units.at(m.unit_id), lines.at(m.line_id), m.position});
  for (auto &e : s.entities)
    run(db,
        "INSERT INTO "
        "narrative_unit_entities(project_id,unit_id,entity_id,role) "
        "VALUES(?,?,?,?)",
        {s.project_id, units.at(e.unit_id), e.entity_id, e.role});
  for (auto &l : s.links) {
    links[l.id] = project::new_uuid();
    run(db,
        "INSERT INTO "
        "narrative_links(id,project_id,structure_id,source_unit_id,target_unit_"
        "id,kind,label,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?)",
        {links[l.id], s.project_id, target.id, units.at(l.source_unit_id),
         units.at(l.target_unit_id), project::to_string(l.kind), l.label, now,
         now});
  }
  for (auto &c : s.contexts)
    run(db,
        "INSERT INTO "
        "narrative_unit_contexts(unit_id,project_id,location_entity_id,writing_"
        "position,token) VALUES(?,?,?,?,?)",
        {units.at(c.unit_id), s.project_id,
         c.location_id.empty() ? J{} : J{c.location_id},
         c.writing_position ? J{*c.writing_position} : J{},
         project::new_uuid()});
  for (auto &r : s.realizations) {
    if(r.table=="narrative_text_targets")
      run(db,"INSERT INTO narrative_text_targets(id,project_id,unit_id,document_id,anchor_id,composition_id,occurrence_id,observed_sha,note,review_state,provenance) VALUES(?,?,?,?,?,?,?,?,?,?,?)",
          {project::new_uuid(),s.project_id,units.at(r.unit_id),r.document_id.empty()?J{}:J{r.document_id},
           r.anchor_id.empty()?J{}:J{r.anchor_id},r.composition_id.empty()?J{}:J{r.composition_id},
           r.occurrence_id.empty()?J{}:J{r.occurrence_id},r.observed_sha,r.note,r.review_state,r.provenance});
    else run(db,
        "INSERT INTO "
        "narrative_realizations(id,project_id,unit_id,document_id,note) "
        "VALUES(?,?,?,?,?)",
        {project::new_uuid(), s.project_id, units.at(r.unit_id), r.document_id,
         r.note});
  }
  for (auto &r : s.relations)
    run(db,
        "INSERT INTO "
        "narrative_unit_relations(id,project_id,unit_id,relation_id) "
        "VALUES(?,?,?,?)",
        {project::new_uuid(), s.project_id, units.at(r.unit_id),
         r.relation_id});
  for(const auto &v:s.presences)
    run(db,"INSERT INTO narrative_unit_presence_intents(id,project_id,unit_id,entity_id) VALUES(?,?,?,?)",
        {project::new_uuid(),s.project_id,units.at(v.unit_id),v.entity_id});
  for (auto &r : s.roles)
    run(db,
        "INSERT INTO "
        "narrative_role_assignments(id,project_id,role_id,entity_id,work_id,"
        "structure_id,unit_id,notes,created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?)",
        {project::new_uuid(), s.project_id, r.role_id, r.entity_id, s.work_id,
         target.id, r.unit_id ? J{units.at(*r.unit_id)} : J{}, r.notes, now,
         now});
  for (auto &a : s.dates)
    run(db,
        "INSERT INTO "
        "temporal_attachments(id,project_id,reference_id,unit_id,line_id,role,"
        "note,token,meaning) VALUES(?,?,?,?,?,?,?,?,?)",
        {project::new_uuid(), s.project_id, a.reference_id,
         a.unit_id.empty() ? J{} : J{units.at(a.unit_id)},
         a.line_id.empty() ? J{} : J{lines.at(a.line_id)}, a.role, a.note,
         project::new_uuid(), a.meaning});
  for (auto &d : s.scopes)
    run(db,
        "INSERT INTO narrative_dependency_scopes(link_id,project_id,scope) "
        "VALUES(?,?,?)",
        {links.at(d.link_id), s.project_id, d.scope});
  run(db,
      "INSERT INTO "
      "narrative_branch_provenance(structure_id,project_id,source_structure_id,"
      "source_revision,observation_json,created_at) VALUES(?,?,?,?,?,?)",
      {target.id, s.project_id, s.structure_id, s.revision, observation, now});
  // Validate the whole new plan before commit. Bounds are identical on restart.
  (void)read(db, p, target.id);
  tx.commit();
  return target;
}
BuilderComparison NarrativeBuilder::compare(const std::string &source,
                                            const std::string &target) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  const auto a = read(db, p, source), b = read(db, p, target);
  if (a.work_id != b.work_id)
    throw std::runtime_error("Compare alternativas da mesma Obra.");
  auto result =
      compare_values(normalized(a), normalized(b), a.name, b.name, false);
  tx.commit();
  return result;
}
BuilderComparison
NarrativeBuilder::compare_to_origin(const std::string &target) const {
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  const auto s = read(db, p, target);
  if (s.provenance_json.empty())
    throw std::runtime_error("Esta estrutura não tem ponto de derivação "
                             "registrado; escolha outra alternativa.");
  auto result = compare_values(
      json::parse(s.provenance_json).at("baseline").as_object(), normalized(s),
      "Ponto de derivação registrado", s.name, true);
  // Historical source IDs are not navigation targets in this read-only lens.
  for (auto &d : result.differences)
    d.unit_id.clear();
  tx.commit();
  return result;
}
void NarrativeBuilder::move_in_line(const NarrativeBuilderSnapshot &expected,
                                    const std::string &id,
                                    const std::string &line,
                                    int direction) const {
  if (direction != -1 && direction != 1)
    throw std::runtime_error("Direção inválida");
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Mover na ordem própria da linha");
  check(db, p, expected);
  unit(db, p, expected, id);
  const auto s = read(db, p, expected.structure_id);
  std::vector<std::pair<std::int64_t, std::string>> list;
  for (auto &m : s.memberships)
    if (m.line_id == line)
      list.emplace_back(m.position, m.unit_id);
  std::sort(list.begin(), list.end());
  auto at = std::find_if(list.begin(), list.end(),
                         [&](auto &x) { return x.second == id; });
  if (at == list.end())
    throw std::runtime_error("Unidade não pertence à linha observada");
  auto index = std::distance(list.begin(), at), next = index + direction;
  if (next >= 0 && next < static_cast<std::ptrdiff_t>(list.size())) {
    std::swap(list[index], list[next]);
    for (std::size_t i = 0; i < list.size(); ++i)
      run(db,
          "UPDATE narrative_unit_lines SET position=? WHERE unit_id=? AND "
          "line_id=?",
          {static_cast<std::int64_t>((i + 1) * 1000), list[i].second, line});
    context(db, p, id);
  }
  tx.commit();
}
void NarrativeBuilder::set_dependency_scope(const NarrativeBuilderSnapshot &s,
                                            const std::string &link,
                                            const std::string &scope) const {
  const std::set<std::string> valid{"unclassified", "narrative", "chronology",
                                    "writing", "editorial"};
  if (!valid.contains(scope))
    throw std::runtime_error("Autoridade do vínculo inválida");
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, "Declarar autoridade do vínculo narrativo");
  check(db, p, s);
  auto q = db.prepare(
      "SELECT id FROM narrative_links WHERE id=? AND structure_id=?");
  q.bind(1, link);
  q.bind(2, s.structure_id);
  if (!q.step())
    throw std::runtime_error("Vínculo ausente da estrutura observada");
  run(db,
      "INSERT INTO narrative_dependency_scopes(link_id,project_id,scope) "
      "VALUES(?,?,?) ON CONFLICT(link_id) DO UPDATE SET scope=excluded.scope",
      {link, s.project_id, scope});
  tx.commit();
}
BuilderMoveImpact
NarrativeBuilder::preview_move(const NarrativeBuilderSnapshot &expected,
                               const std::string &id, BuilderOrder order,
                               int direction) const {
  if (direction != -1 && direction != 1)
    throw std::runtime_error("Direção inválida");
  const auto &p = owner();
  auto db = open(p);
  SqliteTransaction tx(db, SqliteTransaction::Mode::Deferred);
  check(db, p, expected);
  unit(db, p, expected, id);
  const auto s = read(db, p, expected.structure_id);
  std::vector<std::pair<std::int64_t, std::string>> list;
  if (order == BuilderOrder::Narrative)
    for (auto &u : s.units)
      list.emplace_back(u.position, u.id);
  else if (order == BuilderOrder::WritingQueue) {
    for (auto &c : s.contexts)
      if (c.writing_position)
        list.emplace_back(*c.writing_position, c.unit_id);
  } else
    throw std::runtime_error("Ordem inválida");
  std::sort(list.begin(), list.end());
  auto at = std::find_if(list.begin(), list.end(),
                         [&](auto &x) { return x.second == id; });
  if (at == list.end())
    throw std::runtime_error("Unidade fora da ordem escolhida");
  auto index = std::distance(list.begin(), at), next = index + direction;
  BuilderMoveImpact impact;
  if (next < 0 || next >= static_cast<std::ptrdiff_t>(list.size())) {
    impact.warnings.push_back(
        "Unidade já está no limite desta ordem; nenhuma posição mudará.");
    tx.commit();
    return impact;
  }
  const auto neighbor = list[next].second;
  std::swap(list[index], list[next]);
  std::map<std::string, std::size_t> positions;
  for (std::size_t i = 0; i < list.size(); ++i)
    positions[list[i].second] = i;
  std::set<std::string> connected{id, neighbor}, docs;
  bool changed=true;
  while(changed) {
    const auto count=connected.size();
    for(const auto &l:s.links)if(connected.contains(l.source_unit_id) || connected.contains(l.target_unit_id)) {
      connected.insert(l.source_unit_id);connected.insert(l.target_unit_id);
    }
    changed=count!=connected.size();
  }
  for (auto &u : s.units)
    if (connected.contains(u.id))
      impact.connected_units.push_back(u.title + " [" + u.id + "]");
  for (auto &r : s.realizations)
    if (connected.contains(r.unit_id) && !r.document_id.empty())
      docs.insert(r.document_id);
  impact.documents = docs.size();
  for (auto &d : s.dates)
    if (connected.contains(d.unit_id))
      ++impact.dates;
  for (auto &l : s.links) {
    if (!connected.contains(l.source_unit_id) ||
        !connected.contains(l.target_unit_id))
      continue;
    std::string scope = "unclassified";
    for (auto &d : s.scopes)
      if (d.link_id == l.id)
        scope = d.scope;
    if (scope == "unclassified") {
      impact.warnings.push_back("Vínculo sem autoridade declarada: " + l.label +
                                " [" + l.id +
                                "]. Não é tratado como cronologia.");
      continue;
    }
    if (scope != (order == BuilderOrder::Narrative ? "narrative" : "writing"))
      continue;
    if (l.kind != project::NarrativeLinkKind::Precedes &&
        l.kind != project::NarrativeLinkKind::Depends)
      continue;
    auto first = l.kind == project::NarrativeLinkKind::Depends
                     ? l.target_unit_id
                     : l.source_unit_id;
    auto last = l.kind == project::NarrativeLinkKind::Depends
                    ? l.source_unit_id
                    : l.target_unit_id;
    if (!positions.contains(first) || !positions.contains(last))
      impact.warnings.push_back(
          "Dependência nesta fila inclui unidade sem posição [" + l.id +
          "]. Informação insuficiente.");
    else if (positions[first] >= positions[last])
      impact.warnings.push_back(
          "Ordem proposta contraria vínculo declarado nesta autoridade: " +
          l.label + " [" + l.id +
          "]. É um aviso; o autor pode manter a escolha.");
  }
  impact.warnings.push_back(
      "Cronologia e editorial não são reordenados nem validados por este "
      "movimento. Causalidade não implica ordem de leitura.");
  tx.commit();
  return impact;
}
} // namespace inde::application

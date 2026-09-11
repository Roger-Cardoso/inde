#include "inde/application/structure_model_service.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace inde::application {
namespace {

std::string trim(std::string value) {
  const auto content = [](unsigned char c) { return !std::isspace(c); };
  const auto begin = std::find_if(value.begin(), value.end(), content);
  const auto end = std::find_if(value.rbegin(), value.rend(), content).base();
  return begin < end ? std::string(begin, end) : std::string{};
}

template <class T>
const T &find_by_id(const std::vector<T> &values, const std::string &id,
                    const char *message) {
  const auto found = std::find_if(values.begin(), values.end(),
                                  [&](const auto &v) { return v.id == id; });
  if (found == values.end())
    throw std::runtime_error(message);
  return *found;
}

std::string structural_type_id_from_key(const std::string &key) {
  return project::builtin_structural_type_id(
      project::structural_node_type_from_string(key));
}

} // namespace

const project::Project &StructureModelService::require_project() const {
  const auto *value = session_.current();
  if (!value)
    throw std::runtime_error("Nenhum projeto está aberto");
  return *value;
}
void StructureModelService::require_work(const std::string &id) const {
  static_cast<void>(require_project());
  if (std::none_of(session_.catalog().works.begin(),
                   session_.catalog().works.end(),
                   [&](const auto &work) { return work.id == id; }))
    throw std::runtime_error("A Obra escolhida não existe");
}
void StructureModelService::reload_structural_nodes() {
  session_.structural_nodes() =
      structural_store_.load(require_project().path());
}

std::vector<project::StructureModel>
StructureModelService::models(project::StructureLayer layer) const {
  return store_.models(require_project().path(), layer);
}
void StructureModelService::delete_model(const std::string &id) {
  const auto editorial = models(project::StructureLayer::Editorial);
  const auto narrative = models(project::StructureLayer::Narrative);
  const auto find_model = [&](const auto &values) {
    return std::find_if(values.begin(), values.end(),
                        [&](const auto &value) { return value.id == id; });
  };
  auto found = find_model(editorial);
  if (found != editorial.end()) {
    if (found->is_builtin)
      throw std::runtime_error("Modelos estruturais do sistema são protegidos");
  } else {
    const auto narrative_found = find_model(narrative);
    if (narrative_found == narrative.end())
      throw std::runtime_error("Modelo estrutural não encontrado");
    if (narrative_found->is_builtin)
      throw std::runtime_error("Modelos estruturais do sistema são protegidos");
  }
  store_.remove_model(require_project().path(), id);
}
std::vector<project::EditorialStructure>
StructureModelService::editorial_structures(const std::string &work) const {
  require_work(work);
  return store_.editorial_structures(require_project().path(), work);
}
std::vector<project::NarrativeStructure>
StructureModelService::narrative_structures(const std::string &work) const {
  require_work(work);
  return store_.narrative_structures(require_project().path(), work);
}
std::optional<project::EditorialStructure>
StructureModelService::active_editorial(const std::string &work) const {
  const auto all = editorial_structures(work);
  const auto at = std::find_if(all.begin(), all.end(),
                               [](const auto &v) { return v.is_active; });
  return at == all.end() ? std::nullopt : std::optional{*at};
}
std::size_t
StructureModelService::editorial_node_count(const std::string &id) const {
  return static_cast<std::size_t>(std::count_if(
      session_.structural_nodes().begin(), session_.structural_nodes().end(),
      [&](const auto &node) { return node.structure_id == id; }));
}
std::optional<project::NarrativeStructure>
StructureModelService::active_narrative(const std::string &work) const {
  const auto all = narrative_structures(work);
  const auto at = std::find_if(all.begin(), all.end(),
                               [](const auto &v) { return v.is_active; });
  return at == all.end() ? std::nullopt : std::optional{*at};
}

project::EditorialStructure StructureModelService::create_editorial(
    const std::string &work, std::string name, std::string description) {
  require_work(work);
  const auto now = project::utc_now();
  project::EditorialStructure value{project::new_uuid(),
                                    work,
                                    trim(std::move(name)),
                                    trim(std::move(description)),
                                    std::nullopt,
                                    std::nullopt,
                                    project::StructureCreationKind::Blank,
                                    false,
                                    now,
                                    now};
  project::validate(value);
  store_.save_editorial_structure_bundle(require_project().path(), value, {});
  return value;
}

project::EditorialStructure StructureModelService::instantiate_editorial(
    const std::string &work, const std::string &model_id, std::string name) {
  require_work(work);
  const auto available = models(project::StructureLayer::Editorial);
  const auto &model =
      find_by_id(available, model_id, "Modelo editorial não encontrado");
  const auto now = project::utc_now();
  project::EditorialStructure structure{
      project::new_uuid(),
      work,
      trim(std::move(name)),
      model.description,
      model.id,
      std::nullopt,
      project::StructureCreationKind::Instantiated,
      false,
      now,
      now};
  auto items = store_.model_items(require_project().path(), model.id);
  if (items.empty() && model.key == "simple-novel")
    for (int i = 1; i <= 10; ++i)
      items.push_back({project::new_uuid(), model.id, std::nullopt,
                       project::StructureModelItemKind::EditorialNode,
                       "chapter", std::to_string(i), "Sem título", "", "", "",
                       i * project::structural_position_step, now, now});
  if (items.empty() && model.key == "parts-and-chapters") {
    for (int part = 1; part <= 3; ++part) {
      const auto parent = project::new_uuid();
      items.push_back({parent, model.id, std::nullopt,
                       project::StructureModelItemKind::EditorialNode, "part",
                       std::to_string(part), "Sem título", "", "", "",
                       part * project::structural_position_step, now, now});
      for (int chapter = 1; chapter <= 5; ++chapter)
        items.push_back({project::new_uuid(), model.id, parent,
                         project::StructureModelItemKind::EditorialNode,
                         "chapter", std::to_string(chapter), "Sem título", "",
                         "", "", chapter * project::structural_position_step,
                         now, now});
    }
  }
  std::unordered_map<std::string, std::string> ids;
  for (const auto &item : items)
    if (item.kind == project::StructureModelItemKind::EditorialNode)
      ids.emplace(item.id, project::new_uuid());
  const auto structural_types =
      structural_store_.load_types(require_project().path());
  std::vector<project::StructuralNode> nodes;
  for (const auto &item : items) {
    if (item.kind != project::StructureModelItemKind::EditorialNode)
      continue;
    const auto managed = std::find_if(
        structural_types.begin(), structural_types.end(),
        [&](const auto &candidate) { return candidate.id == item.type_key; });
    const auto type =
        managed == structural_types.end()
            ? project::structural_node_type_from_string(item.type_key)
            : project::legacy_structural_type_for_id(managed->id);
    const auto type_id = managed == structural_types.end()
                             ? structural_type_id_from_key(item.type_key)
                             : managed->id;
    const auto type_name = managed == structural_types.end()
                               ? project::display_name(type)
                               : managed->name;
    nodes.push_back(
        {ids.at(item.id), work,
         item.parent_id ? std::optional{ids.at(*item.parent_id)} : std::nullopt,
         type, item.title, "", item.summary,
         type == project::StructuralNodeType::Custom ? type_name : "",
         "Planejamento", item.position, now, now, type_id, type_name,
         item.designator, structure.id});
  }
  store_.save_editorial_structure_bundle(require_project().path(), structure,
                                         nodes);
  reload_structural_nodes();
  return structure;
}

project::EditorialStructure
StructureModelService::duplicate_editorial(const std::string &source_id,
                                           std::string name, bool derived) {
  std::optional<project::EditorialStructure> source;
  for (const auto &work : session_.catalog().works)
    for (const auto &value : editorial_structures(work.id))
      if (value.id == source_id)
        source = value;
  if (!source)
    throw std::runtime_error("Estrutura editorial de origem não encontrada");
  const auto now = project::utc_now();
  project::EditorialStructure copy{
      project::new_uuid(),
      source->work_id,
      trim(std::move(name)),
      source->description,
      source->model_id,
      source->id,
      derived ? project::StructureCreationKind::Derived
              : project::StructureCreationKind::Duplicated,
      false,
      now,
      now};
  std::vector<project::StructuralNode> originals;
  for (const auto &node : session_.structural_nodes())
    if (node.structure_id == source_id)
      originals.push_back(node);
  std::unordered_map<std::string, std::string> ids;
  for (const auto &node : originals)
    ids.emplace(node.id, project::new_uuid());
  std::vector<project::StructuralNode> nodes;
  for (auto node : originals) {
    node.id = ids.at(node.id);
    if (node.parent_id)
      node.parent_id = ids.at(*node.parent_id);
    node.structure_id = copy.id;
    node.created_at = now;
    node.updated_at = now;
    nodes.push_back(std::move(node));
  }
  store_.save_editorial_structure_bundle(require_project().path(), copy, nodes);
  reload_structural_nodes();
  return copy;
}

project::StructureModel StructureModelService::capture_editorial_model(
    const std::string &id, std::string name, std::string description) {
  std::optional<project::EditorialStructure> source;
  for (const auto &work : session_.catalog().works)
    for (const auto &value : editorial_structures(work.id))
      if (value.id == id)
        source = value;
  if (!source)
    throw std::runtime_error("Estrutura editorial não encontrada");
  const auto now = project::utc_now();
  project::StructureModel model{project::new_uuid(),
                                project::StructureLayer::Editorial,
                                "user-" + project::new_uuid(),
                                trim(std::move(name)),
                                trim(std::move(description)),
                                false,
                                now,
                                now};
  std::unordered_map<std::string, std::string> item_ids;
  for (const auto &node : session_.structural_nodes())
    if (node.structure_id == id)
      item_ids.emplace(node.id, project::new_uuid());
  std::vector<project::StructureModelItem> items;
  for (const auto &node : session_.structural_nodes()) {
    if (node.structure_id != id)
      continue;
    items.push_back({item_ids.at(node.id), model.id,
                     node.parent_id
                         ? std::optional{item_ids.at(*node.parent_id)}
                         : std::nullopt,
                     project::StructureModelItemKind::EditorialNode,
                     node.structural_type_id, node.designator, node.title,
                     node.synopsis, "", "", node.position, now, now});
  }
  project::validate(model);
  store_.save_model_bundle(require_project().path(), model, items, {});
  return model;
}

project::StructureActivationImpact
StructureModelService::editorial_activation_impact(
    const std::string &target) const {
  for (const auto &work : session_.catalog().works) {
    const auto all = editorial_structures(work.id);
    const auto at = std::find_if(all.begin(), all.end(),
                                 [&](const auto &v) { return v.id == target; });
    if (at == all.end())
      continue;
    const auto current = std::find_if(
        all.begin(), all.end(), [](const auto &v) { return v.is_active; });
    std::size_t count = 0;
    for (const auto &node : session_.structural_nodes())
      if (node.structure_id == target)
        ++count;
    return {current == all.end() ? std::string{} : current->id, target,
            current == all.end()
                ? 0
                : store_.document_count(require_project().path(), current->id),
            count};
  }
  throw std::runtime_error("Estrutura editorial alvo não encontrada");
}
void StructureModelService::activate_editorial(const std::string &target) {
  const auto impact = editorial_activation_impact(target);
  std::string work;
  for (const auto &w : session_.catalog().works)
    for (const auto &v : editorial_structures(w.id))
      if (v.id == target)
        work = w.id;
  store_.activate_editorial(require_project().path(), work, target);
}
void StructureModelService::delete_editorial(const std::string &id) {
  store_.remove_editorial_structure(require_project().path(), id);
  reload_structural_nodes();
}

project::NarrativeStructure StructureModelService::create_narrative(
    const std::string &work, std::string name, std::string description) {
  require_work(work);
  const auto now = project::utc_now();
  project::NarrativeStructure value{project::new_uuid(),
                                    work,
                                    trim(std::move(name)),
                                    trim(std::move(description)),
                                    std::nullopt,
                                    std::nullopt,
                                    project::StructureCreationKind::Blank,
                                    false,
                                    now,
                                    now};
  project::validate(value);
  store_.save_narrative_structure_bundle(require_project().path(), value, {},
                                         {}, {}, {}, {});
  return value;
}

project::NarrativeStructure StructureModelService::instantiate_narrative(
    const std::string &work, const std::string &model_id, std::string name) {
  require_work(work);
  const auto available = models(project::StructureLayer::Narrative);
  const auto &model =
      find_by_id(available, model_id, "Modelo narrativo não encontrado");
  const auto now = project::utc_now();
  project::NarrativeStructure structure{
      project::new_uuid(),
      work,
      trim(std::move(name)),
      model.description,
      model.id,
      std::nullopt,
      project::StructureCreationKind::Instantiated,
      false,
      now,
      now};
  const auto items = store_.model_items(require_project().path(), model.id);
  const auto item_links =
      store_.model_item_links(require_project().path(), model.id);
  std::vector<project::NarrativeLine> lines;
  std::vector<project::NarrativeUnit> units;
  std::unordered_map<std::string, std::string> ids;
  for (const auto &item : items) {
    const auto id = project::new_uuid();
    ids.emplace(item.id, id);
    if (item.kind == project::StructureModelItemKind::NarrativeLine)
      lines.push_back({id, structure.id, item.title, item.summary,
                       item.position, now, now});
    else if (item.kind == project::StructureModelItemKind::NarrativeUnit)
      units.push_back({id, structure.id, item.designator, item.title,
                       item.summary, item.purpose, item.perspective,
                       item.position, now, now});
  }
  std::vector<project::NarrativeUnitLine> memberships;
  std::vector<project::NarrativeLink> links;
  for (const auto &link : item_links) {
    if (!ids.contains(link.source_item_id) ||
        !ids.contains(link.target_item_id))
      continue;
    if (link.kind == "membership")
      memberships.push_back({ids.at(link.source_item_id),
                             ids.at(link.target_item_id), link.position});
    else
      links.push_back({project::new_uuid(), structure.id,
                       ids.at(link.source_item_id), ids.at(link.target_item_id),
                       project::narrative_link_kind_from_string(link.kind),
                       link.label, now, now});
  }
  if (item_links.empty() && !lines.empty())
    for (const auto &unit : units)
      memberships.push_back({unit.id, lines.front().id, unit.position});
  store_.save_narrative_structure_bundle(require_project().path(), structure,
                                         lines, units, memberships, {}, links);
  return structure;
}

project::NarrativeStructure
StructureModelService::duplicate_narrative(const std::string &source_id,
                                           std::string name, bool derived) {
  std::optional<project::NarrativeStructure> source;
  for (const auto &work : session_.catalog().works)
    for (const auto &v : narrative_structures(work.id))
      if (v.id == source_id)
        source = v;
  if (!source)
    throw std::runtime_error("Estrutura narrativa de origem não encontrada");
  const auto now = project::utc_now();
  project::NarrativeStructure copy{
      project::new_uuid(),
      source->work_id,
      trim(std::move(name)),
      source->description,
      source->model_id,
      source->id,
      derived ? project::StructureCreationKind::Derived
              : project::StructureCreationKind::Duplicated,
      false,
      now,
      now};
  auto old_lines = lines(source_id);
  auto old_units = units(source_id);
  auto old_members = unit_lines(source_id);
  auto old_entities = unit_entities(source_id);
  auto old_links = links(source_id);
  std::unordered_map<std::string, std::string> line_ids, unit_ids;
  for (auto &line : old_lines) {
    const auto old = line.id;
    line.id = project::new_uuid();
    line.structure_id = copy.id;
    line.created_at = now;
    line.updated_at = now;
    line_ids.emplace(old, line.id);
  }
  for (auto &unit : old_units) {
    const auto old = unit.id;
    unit.id = project::new_uuid();
    unit.structure_id = copy.id;
    unit.created_at = now;
    unit.updated_at = now;
    unit_ids.emplace(old, unit.id);
  }
  for (auto &m : old_members) {
    m.unit_id = unit_ids.at(m.unit_id);
    m.line_id = line_ids.at(m.line_id);
  }
  for (auto &e : old_entities)
    e.unit_id = unit_ids.at(e.unit_id);
  for (auto &link : old_links) {
    link.id = project::new_uuid();
    link.structure_id = copy.id;
    link.source_unit_id = unit_ids.at(link.source_unit_id);
    link.target_unit_id = unit_ids.at(link.target_unit_id);
    link.created_at = now;
    link.updated_at = now;
  }
  store_.save_narrative_structure_bundle(require_project().path(), copy,
                                         old_lines, old_units, old_members,
                                         old_entities, old_links);
  return copy;
}

project::StructureModel StructureModelService::capture_narrative_model(
    const std::string &id, std::string name, std::string description) {
  bool found = false;
  for (const auto &work : session_.catalog().works)
    for (const auto &value : narrative_structures(work.id))
      found = found || value.id == id;
  if (!found)
    throw std::runtime_error("Estrutura narrativa não encontrada");
  const auto now = project::utc_now();
  project::StructureModel model{project::new_uuid(),
                                project::StructureLayer::Narrative,
                                "user-" + project::new_uuid(),
                                trim(std::move(name)),
                                trim(std::move(description)),
                                false,
                                now,
                                now};
  const auto source_lines = lines(id);
  const auto source_units = units(id);
  std::unordered_map<std::string, std::string> ids;
  for (const auto &line : source_lines)
    ids.emplace(line.id, project::new_uuid());
  for (const auto &unit : source_units)
    ids.emplace(unit.id, project::new_uuid());
  std::vector<project::StructureModelItem> items;
  for (const auto &line : source_lines)
    items.push_back({ids.at(line.id), model.id, std::nullopt,
                     project::StructureModelItemKind::NarrativeLine, "", "",
                     line.name, line.description, "", "", line.position, now,
                     now});
  for (const auto &unit : source_units)
    items.push_back({ids.at(unit.id), model.id, std::nullopt,
                     project::StructureModelItemKind::NarrativeUnit, "",
                     unit.designator, unit.title, unit.summary, unit.purpose,
                     unit.perspective, unit.position, now, now});
  std::vector<project::StructureModelItemLink> model_links;
  for (const auto &membership : unit_lines(id))
    model_links.push_back(
        {project::new_uuid(), model.id, ids.at(membership.unit_id),
         ids.at(membership.line_id), "membership", "", membership.position});
  for (const auto &link : links(id))
    model_links.push_back({project::new_uuid(), model.id,
                           ids.at(link.source_unit_id),
                           ids.at(link.target_unit_id),
                           project::to_string(link.kind), link.label, 0});
  project::validate(model);
  store_.save_model_bundle(require_project().path(), model, items, model_links);
  return model;
}

project::StructureActivationImpact
StructureModelService::narrative_activation_impact(
    const std::string &target) const {
  for (const auto &work : session_.catalog().works) {
    const auto all = narrative_structures(work.id);
    const auto at = std::find_if(all.begin(), all.end(),
                                 [&](const auto &v) { return v.id == target; });
    if (at == all.end())
      continue;
    const auto current = std::find_if(
        all.begin(), all.end(), [](const auto &v) { return v.is_active; });
    return {current == all.end() ? std::string{} : current->id, target, 0,
            units(target).size()};
  }
  throw std::runtime_error("Estrutura narrativa alvo não encontrada");
}
void StructureModelService::activate_narrative(const std::string &target) {
  static_cast<void>(narrative_activation_impact(target));
  for (const auto &w : session_.catalog().works)
    for (const auto &v : narrative_structures(w.id))
      if (v.id == target) {
        store_.activate_narrative(require_project().path(), w.id, target);
        return;
      }
}
void StructureModelService::delete_narrative(const std::string &id) {
  store_.remove_narrative_structure(require_project().path(), id);
}

std::vector<project::NarrativeLine>
StructureModelService::lines(const std::string &id) const {
  return store_.lines(require_project().path(), id);
}
std::vector<project::NarrativeUnit>
StructureModelService::units(const std::string &id) const {
  return store_.units(require_project().path(), id);
}
std::vector<project::NarrativeUnitLine>
StructureModelService::unit_lines(const std::string &id) const {
  return store_.unit_lines(require_project().path(), id);
}
std::vector<project::NarrativeUnitEntity>
StructureModelService::unit_entities(const std::string &id) const {
  return store_.unit_entities(require_project().path(), id);
}
std::vector<project::NarrativeLink>
StructureModelService::links(const std::string &id) const {
  return store_.links(require_project().path(), id);
}
project::NarrativeLine
StructureModelService::create_line(const std::string &id, std::string name,
                                   std::string description) {
  const auto all = lines(id);
  std::int64_t position = 0;
  for (const auto &v : all)
    position = std::max(position, v.position);
  const auto now = project::utc_now();
  project::NarrativeLine v{project::new_uuid(),
                           id,
                           trim(std::move(name)),
                           trim(std::move(description)),
                           position + 1000,
                           now,
                           now};
  project::validate(v);
  store_.save(require_project().path(), v);
  return v;
}
project::NarrativeUnit StructureModelService::create_unit(
    const std::string &id, std::string designator, std::string title,
    std::string summary, std::string purpose, std::string perspective) {
  const auto all = units(id);
  std::int64_t position = 0;
  for (const auto &v : all)
    position = std::max(position, v.position);
  const auto now = project::utc_now();
  project::NarrativeUnit v{project::new_uuid(),
                           id,
                           trim(std::move(designator)),
                           trim(std::move(title)),
                           trim(std::move(summary)),
                           trim(std::move(purpose)),
                           trim(std::move(perspective)),
                           position + 1000,
                           now,
                           now};
  project::validate(v);
  store_.save(require_project().path(), v);
  return v;
}
void StructureModelService::update_line(project::NarrativeLine v) {
  v.name = trim(v.name);
  v.description = trim(v.description);
  v.updated_at = project::utc_now();
  project::validate(v);
  store_.save(require_project().path(), v);
}
void StructureModelService::update_unit(project::NarrativeUnit v) {
  v.designator = trim(v.designator);
  v.title = trim(v.title);
  v.summary = trim(v.summary);
  v.purpose = trim(v.purpose);
  v.perspective = trim(v.perspective);
  v.updated_at = project::utc_now();
  project::validate(v);
  store_.save(require_project().path(), v);
}
void StructureModelService::delete_line(const std::string &id) {
  store_.remove_line(require_project().path(), id);
}
void StructureModelService::delete_unit(const std::string &id) {
  store_.remove_unit(require_project().path(), id);
}
void StructureModelService::add_unit_to_line(const std::string &unit,
                                             const std::string &line) {
  const auto all_lines = [&]() {
    for (const auto &w : session_.catalog().works)
      for (const auto &s : narrative_structures(w.id)) {
        const auto us = units(s.id);
        const auto ls = lines(s.id);
        if (std::any_of(us.begin(), us.end(),
                        [&](const auto &v) { return v.id == unit; }) &&
            std::any_of(ls.begin(), ls.end(),
                        [&](const auto &v) { return v.id == line; }))
          return std::optional{s.id};
      }
    return std::optional<std::string>{};
  }();
  if (!all_lines)
    throw std::runtime_error(
        "Linha e unidade precisam pertencer à mesma estrutura");
  std::int64_t position = 1000;
  for (const auto &m : unit_lines(*all_lines))
    if (m.line_id == line)
      position = std::max(position, m.position + 1000);
  store_.save(require_project().path(),
              project::NarrativeUnitLine{unit, line, position});
}
void StructureModelService::remove_unit_from_line(const std::string &u,
                                                  const std::string &l) {
  store_.remove_unit_line(require_project().path(), u, l);
}
void StructureModelService::add_entity_to_unit(const std::string &u,
                                               const std::string &e,
                                               std::string role) {
  store_.save(require_project().path(),
              project::NarrativeUnitEntity{u, e, trim(std::move(role))});
}
void StructureModelService::remove_entity_from_unit(const std::string &u,
                                                    const std::string &e,
                                                    const std::string &r) {
  store_.remove_unit_entity(require_project().path(), u, e, r);
}
project::NarrativeLink StructureModelService::create_link(
    const std::string &s, const std::string &source, const std::string &target,
    project::NarrativeLinkKind kind, std::string label) {
  const auto all = units(s);
  if (std::none_of(all.begin(), all.end(),
                   [&](const auto &v) { return v.id == source; }) ||
      std::none_of(all.begin(), all.end(),
                   [&](const auto &v) { return v.id == target; }))
    throw std::runtime_error("As duas unidades precisam pertencer à estrutura");
  const auto now = project::utc_now();
  project::NarrativeLink value{
      project::new_uuid(),    s,   source, target, kind,
      trim(std::move(label)), now, now};
  project::validate(value);
  store_.save(require_project().path(), value);
  return value;
}
void StructureModelService::delete_link(const std::string &id) {
  store_.remove_link(require_project().path(), id);
}

std::vector<project::NarrativeRole> StructureModelService::roles() const {
  return store_.roles(require_project().path());
}
std::vector<project::NarrativeRoleAssignment>
StructureModelService::role_assignments(
    const std::optional<std::string> &work) const {
  return store_.role_assignments(require_project().path(), work);
}
project::NarrativeRole
StructureModelService::create_role(std::string name, std::string description) {
  const auto now = project::utc_now();
  project::NarrativeRole v{project::new_uuid(),
                           trim(std::move(name)),
                           trim(std::move(description)),
                           false,
                           now,
                           now};
  project::validate(v);
  store_.save(require_project().path(), v);
  return v;
}
void StructureModelService::update_role(project::NarrativeRole v) {
  if (v.is_builtin)
    throw std::runtime_error("Papéis narrativos do sistema são protegidos");
  v.name = trim(v.name);
  v.description = trim(v.description);
  v.updated_at = project::utc_now();
  project::validate(v);
  store_.save(require_project().path(), v);
}
void StructureModelService::delete_role(const std::string &id) {
  store_.remove_role(require_project().path(), id);
}
project::NarrativeRoleAssignment StructureModelService::assign_role(
    const std::string &role, const std::string &entity, const std::string &work,
    std::optional<std::string> structure, std::optional<std::string> unit,
    std::string notes) {
  require_work(work);
  const auto now = project::utc_now();
  project::NarrativeRoleAssignment v{project::new_uuid(),
                                     role,
                                     entity,
                                     work,
                                     std::move(structure),
                                     std::move(unit),
                                     trim(std::move(notes)),
                                     now,
                                     now};
  project::validate(v);
  store_.save(require_project().path(), v);
  return v;
}
void StructureModelService::remove_role_assignment(const std::string &id) {
  store_.remove_role_assignment(require_project().path(), id);
}

} // namespace inde::application

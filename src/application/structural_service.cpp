#include "inde/application/structural_service.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace inde::application {
namespace {

std::string trim_ascii(std::string value) {
  const auto content = [](unsigned char character) {
    return !std::isspace(character);
  };
  const auto begin = std::find_if(value.begin(), value.end(), content);
  const auto end = std::find_if(value.rbegin(), value.rend(), content).base();
  return begin < end ? std::string(begin, end) : std::string{};
}

} // namespace

project::Project &StructuralService::require_project() {
  auto *project = session_.current();
  if (!project)
    throw std::runtime_error("Nenhum projeto está aberto");
  return *project;
}

const project::Project &StructuralService::require_project() const {
  const auto *project = session_.current();
  if (!project)
    throw std::runtime_error("Nenhum projeto está aberto");
  return *project;
}

std::vector<project::StructuralNode>
StructuralService::nodes_for_work(const std::string &work_id) const {
  static_cast<void>(require_project());
  if (!structure_models_) {
    std::vector<project::StructuralNode> legacy;
    std::copy_if(session_.structural_nodes().begin(),
                 session_.structural_nodes().end(), std::back_inserter(legacy),
                 [&](const auto &value) { return value.work_id == work_id; });
    std::sort(legacy.begin(), legacy.end(), [](const auto &a, const auto &b) {
      return a.position < b.position;
    });
    return legacy;
  }
  const auto structures = structure_models_->editorial_structures(
      require_project().path(), work_id);
  const auto active =
      std::find_if(structures.begin(), structures.end(),
                   [](const auto &value) { return value.is_active; });
  if (active == structures.end())
    throw std::runtime_error("A Obra não possui estrutura editorial ativa");
  std::vector<project::StructuralNode> result;
  std::copy_if(
      session_.structural_nodes().begin(), session_.structural_nodes().end(),
      std::back_inserter(result), [&](const auto &value) {
        return value.work_id == work_id && value.structure_id == active->id;
      });
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    return a.position < b.position;
  });
  return result;
}

std::vector<project::StructuralElementType>
StructuralService::element_types() const {
  return repository_.load_types(require_project().path());
}

project::StructuralElementType
StructuralService::require_element_type(const std::string &id) const {
  const auto values = element_types();
  const auto found =
      std::find_if(values.begin(), values.end(),
                   [&](const auto &value) { return value.id == id; });
  if (found == values.end())
    throw std::runtime_error("O tipo estrutural escolhido não existe");
  return *found;
}

project::StructuralElementType
StructuralService::create_element_type(std::string name) {
  static_cast<void>(require_project());
  const auto now = project::utc_now();
  project::StructuralElementType value{project::new_uuid(), trim_ascii(name),
                                       false, now, now};
  project::validate(value);
  repository_.save_type(session_.current()->path(), value);
  return value;
}

project::StructuralElementType StructuralService::update_element_type(
    const project::StructuralElementType &input) {
  const auto stored = require_element_type(input.id);
  if (stored.is_builtin)
    throw std::runtime_error("Tipos estruturais do sistema são protegidos");
  auto value = stored;
  value.name = trim_ascii(input.name);
  value.updated_at = project::utc_now();
  project::validate(value);
  repository_.save_type(session_.current()->path(), value);
  for (auto &node : session_.structural_nodes()) {
    if (node.structural_type_id != value.id)
      continue;
    node.structural_type_name = value.name;
    node.custom_type_name = value.name;
  }
  return value;
}

void StructuralService::delete_element_type(const std::string &id) {
  const auto stored = require_element_type(id);
  if (stored.is_builtin)
    throw std::runtime_error("Tipos estruturais do sistema são protegidos");
  repository_.remove_type(session_.current()->path(), id);
}

const project::StructuralNode &StructuralService::create_node(
    std::string work_id, std::optional<std::string> parent_id,
    std::string structural_type_id, std::string designator, std::string title) {
  static_cast<void>(require_project());
  const auto structural_type = require_element_type(structural_type_id);
  const auto structures = structure_models_
                              ? structure_models_->editorial_structures(
                                    require_project().path(), work_id)
                              : std::vector<project::EditorialStructure>{};
  const auto active =
      std::find_if(structures.begin(), structures.end(),
                   [](const auto &value) { return value.is_active; });
  if (structure_models_ && active == structures.end())
    throw std::runtime_error("A Obra não possui estrutura editorial ativa");
  const std::string active_id =
      active == structures.end() ? std::string{} : active->id;
  if (!std::any_of(session_.catalog().works.begin(),
                   session_.catalog().works.end(),
                   [&](const auto &work) { return work.id == work_id; }))
    throw std::runtime_error("A obra escolhida não existe");
  if (parent_id) {
    const auto parent = std::find_if(
        session_.structural_nodes().begin(), session_.structural_nodes().end(),
        [&](const auto &node) { return node.id == *parent_id; });
    if (parent == session_.structural_nodes().end() ||
        parent->work_id != work_id ||
        (!active_id.empty() && parent->structure_id != active_id))
      throw std::runtime_error("O elemento pai não pertence à mesma obra");
  }
  std::int64_t position = 0;
  for (const auto &node : session_.structural_nodes())
    if (node.work_id == work_id &&
        (active_id.empty() || node.structure_id == active_id) &&
        node.parent_id == parent_id)
      position = std::max(position, node.position);
  const auto now = project::utc_now();
  const auto legacy_type =
      project::legacy_structural_type_for_id(structural_type.id);
  project::StructuralNode value{
      project::new_uuid(),
      std::move(work_id),
      std::move(parent_id),
      legacy_type,
      trim_ascii(title),
      "",
      "",
      legacy_type == project::StructuralNodeType::Custom ? structural_type.name
                                                         : std::string{},
      "Planejamento",
      position + project::structural_position_step,
      now,
      now,
      structural_type.id,
      structural_type.name,
      trim_ascii(designator),
      active_id};
  project::validate(value);
  repository_.save(session_.current()->path(), value);
  session_.structural_nodes().push_back(std::move(value));
  return session_.structural_nodes().back();
}

const project::StructuralNode &StructuralService::create_node(
    std::string work_id, std::optional<std::string> parent_id,
    project::StructuralNodeType type, std::string title,
    std::string custom_type_name) {
  if (type != project::StructuralNodeType::Custom)
    return create_node(std::move(work_id), std::move(parent_id),
                       project::builtin_structural_type_id(type), "",
                       std::move(title));

  const auto wanted = trim_ascii(custom_type_name);
  const auto values = element_types();
  const auto found =
      std::find_if(values.begin(), values.end(), [&](const auto &value) {
        return !value.is_builtin && value.name == wanted;
      });
  const auto structural_type =
      found == values.end() ? create_element_type(wanted) : *found;
  return create_node(std::move(work_id), std::move(parent_id),
                     structural_type.id, "", std::move(title));
}

void StructuralService::update_node(const project::StructuralNode &input) {
  static_cast<void>(require_project());
  auto found = std::find_if(
      session_.structural_nodes().begin(), session_.structural_nodes().end(),
      [&](const auto &node) { return node.id == input.id; });
  if (found == session_.structural_nodes().end())
    throw std::runtime_error("Elemento estrutural não encontrado");
  auto value = input;
  value.id = found->id;
  value.work_id = found->work_id;
  value.parent_id = found->parent_id;
  value.position = found->position;
  value.created_at = found->created_at;
  value.updated_at = project::utc_now();
  if (value.structural_type_id.empty()) {
    if (value.type == project::StructuralNodeType::Custom) {
      const auto wanted = trim_ascii(value.custom_type_name);
      const auto values = element_types();
      const auto type = std::find_if(
          values.begin(), values.end(), [&](const auto &candidate) {
            return !candidate.is_builtin && candidate.name == wanted;
          });
      if (type == values.end())
        throw std::runtime_error("O tipo personalizado não existe");
      value.structural_type_id = type->id;
      value.structural_type_name = type->name;
    } else {
      value.structural_type_id =
          project::builtin_structural_type_id(value.type);
      value.structural_type_name = project::display_name(value.type);
    }
  } else {
    const auto structural_type = require_element_type(value.structural_type_id);
    value.structural_type_name = structural_type.name;
    value.type = project::legacy_structural_type_for_id(structural_type.id);
    value.custom_type_name = value.type == project::StructuralNodeType::Custom
                                 ? structural_type.name
                                 : std::string{};
  }
  value.title = trim_ascii(value.title);
  value.designator = trim_ascii(value.designator);
  project::validate(value);
  repository_.save(session_.current()->path(), value);
  *found = std::move(value);
}

void StructuralService::move_node(const std::string &id,
                                  std::optional<std::string> new_parent_id) {
  static_cast<void>(require_project());
  auto found = std::find_if(session_.structural_nodes().begin(),
                            session_.structural_nodes().end(),
                            [&](const auto &node) { return node.id == id; });
  if (found == session_.structural_nodes().end())
    throw std::runtime_error("Elemento estrutural não encontrado");
  if (!project::StructuralTree(session_.structural_nodes())
           .can_move(id, new_parent_id))
    throw std::runtime_error("O movimento criaria um ciclo na estrutura");
  if (new_parent_id) {
    const auto parent = std::find_if(
        session_.structural_nodes().begin(), session_.structural_nodes().end(),
        [&](const auto &node) { return node.id == *new_parent_id; });
    if (parent == session_.structural_nodes().end() ||
        parent->work_id != found->work_id ||
        parent->structure_id != found->structure_id)
      throw std::runtime_error("O novo pai não pertence à mesma obra");
  }
  std::int64_t position = 0;
  for (const auto &node : session_.structural_nodes())
    if (node.work_id == found->work_id &&
        node.structure_id == found->structure_id &&
        node.parent_id == new_parent_id)
      position = std::max(position, node.position);
  auto value = *found;
  value.parent_id = std::move(new_parent_id);
  value.position = position + project::structural_position_step;
  value.updated_at = project::utc_now();
  repository_.save(session_.current()->path(), value);
  *found = std::move(value);
}

namespace {
void reorder_node(std::vector<project::StructuralNode> &nodes,
                  persistence::StructuralStore &repository,
                  const std::filesystem::path &path, const std::string &id,
                  int direction) {
  auto node = std::find_if(nodes.begin(), nodes.end(),
                           [&](const auto &value) { return value.id == id; });
  if (node == nodes.end())
    throw std::runtime_error("Elemento estrutural não encontrado");
  std::vector<project::StructuralNode *> siblings;
  for (auto &value : nodes)
    if (value.work_id == node->work_id &&
        value.structure_id == node->structure_id &&
        value.parent_id == node->parent_id)
      siblings.push_back(&value);
  std::sort(siblings.begin(), siblings.end(), [](const auto *a, const auto *b) {
    return a->position < b->position;
  });
  const auto at =
      std::find_if(siblings.begin(), siblings.end(),
                   [&](const auto *value) { return value->id == id; });
  if ((direction < 0 && at == siblings.begin()) ||
      (direction > 0 && std::next(at) == siblings.end()))
    return;
  auto other = direction < 0 ? std::prev(at) : std::next(at);
  auto reordered = **at;
  auto displaced = **other;
  std::swap(reordered.position, displaced.position);
  const auto now = project::utc_now();
  reordered.updated_at = now;
  displaced.updated_at = now;
  repository.save_many(path, {reordered, displaced});
  **at = std::move(reordered);
  **other = std::move(displaced);
}
} // namespace

void StructuralService::move_node_up(const std::string &id) {
  static_cast<void>(require_project());
  reorder_node(session_.structural_nodes(), repository_,
               session_.current()->path(), id, -1);
}
void StructuralService::move_node_down(const std::string &id) {
  static_cast<void>(require_project());
  reorder_node(session_.structural_nodes(), repository_,
               session_.current()->path(), id, 1);
}

void StructuralService::delete_branch(const std::string &id) {
  static_cast<void>(require_project());
  const auto found = std::find_if(
      session_.structural_nodes().begin(), session_.structural_nodes().end(),
      [&](const auto &node) { return node.id == id; });
  if (found == session_.structural_nodes().end())
    throw std::runtime_error("Elemento estrutural não encontrado");
  auto ids =
      project::StructuralTree(session_.structural_nodes()).descendants_of(id);
  ids.push_back(id);
  repository_.move_to_trash(session_.current()->path(), ids);
  std::erase_if(session_.structural_nodes(), [&](const auto &node) {
    return std::find(ids.begin(), ids.end(), node.id) != ids.end();
  });
}

bool StructuralService::restore_last_deletion() {
  static_cast<void>(require_project());
  if (!repository_.restore_latest_trash(session_.current()->path()))
    return false;
  session_.structural_nodes() = repository_.load(session_.current()->path());
  std::vector<std::string> work_ids;
  for (const auto &work : session_.catalog().works)
    work_ids.push_back(work.id);
  project::StructuralTree(session_.structural_nodes()).validate_all(work_ids);
  return true;
}

std::string StructuralService::duplicate_branch(const std::string &id) {
  static_cast<void>(require_project());
  const auto source = std::find_if(
      session_.structural_nodes().begin(), session_.structural_nodes().end(),
      [&](const auto &node) { return node.id == id; });
  if (source == session_.structural_nodes().end())
    throw std::runtime_error("Elemento estrutural não encontrado");
  auto ids =
      project::StructuralTree(session_.structural_nodes()).descendants_of(id);
  ids.insert(ids.begin(), id);
  std::unordered_map<std::string, std::string> replacements;
  for (const auto &old_id : ids)
    replacements.emplace(old_id, project::new_uuid());
  std::int64_t root_position = 0;
  for (const auto &node : session_.structural_nodes())
    if (node.work_id == source->work_id &&
        node.structure_id == source->structure_id &&
        node.parent_id == source->parent_id)
      root_position = std::max(root_position, node.position);
  std::vector<project::StructuralNode> copies;
  const auto now = project::utc_now();
  for (const auto &old_id : ids) {
    const auto old = std::find_if(
        session_.structural_nodes().begin(), session_.structural_nodes().end(),
        [&](const auto &node) { return node.id == old_id; });
    auto copy = *old;
    copy.id = replacements.at(old_id);
    copy.created_at = now;
    copy.updated_at = now;
    if (old_id == id) {
      copy.title += " — Cópia";
      copy.position = root_position + project::structural_position_step;
    }
    if (copy.parent_id && replacements.contains(*copy.parent_id))
      copy.parent_id = replacements.at(*copy.parent_id);
    copies.push_back(std::move(copy));
  }
  repository_.save_many(session_.current()->path(), copies);
  session_.structural_nodes().insert(session_.structural_nodes().end(),
                                     copies.begin(), copies.end());
  return replacements.at(id);
}

void StructuralService::create_template(const std::string &work_id,
                                        const std::string &name) {
  static_cast<void>(require_project());
  if (!std::any_of(session_.catalog().works.begin(),
                   session_.catalog().works.end(),
                   [&](const auto &work) { return work.id == work_id; }))
    throw std::runtime_error("A obra escolhida não existe");
  if (name != "three-acts" && name != "simple-novel" &&
      name != "parts-and-chapters")
    throw std::runtime_error("Template estrutural desconhecido");

  const auto structures = structure_models_
                              ? structure_models_->editorial_structures(
                                    require_project().path(), work_id)
                              : std::vector<project::EditorialStructure>{};
  const auto active =
      std::find_if(structures.begin(), structures.end(),
                   [](const auto &value) { return value.is_active; });
  if (structure_models_ && active == structures.end())
    throw std::runtime_error("A Obra não possui estrutura editorial ativa");
  const std::string active_id =
      active == structures.end() ? std::string{} : active->id;

  auto working = session_.structural_nodes();
  std::vector<project::StructuralNode> pending;
  auto append = [&](std::optional<std::string> parent_id,
                    project::StructuralNodeType type, std::string designator,
                    std::string title) {
    std::int64_t position = 0;
    for (const auto &node : working)
      if (node.work_id == work_id &&
          (active_id.empty() || node.structure_id == active_id) &&
          node.parent_id == parent_id)
        position = std::max(position, node.position);
    const auto now = project::utc_now();
    project::StructuralNode value{project::new_uuid(),
                                  work_id,
                                  std::move(parent_id),
                                  type,
                                  std::move(title),
                                  "",
                                  "",
                                  "",
                                  "Planejamento",
                                  position + project::structural_position_step,
                                  now,
                                  now,
                                  project::builtin_structural_type_id(type),
                                  project::display_name(type),
                                  std::move(designator),
                                  active_id};
    project::validate(value);
    const auto id = value.id;
    working.push_back(value);
    pending.push_back(std::move(value));
    return id;
  };

  if (name == "three-acts") {
    append(std::nullopt, project::StructuralNodeType::Act, "I", "Sem título");
    append(std::nullopt, project::StructuralNodeType::Act, "II", "Sem título");
    append(std::nullopt, project::StructuralNodeType::Act, "III", "Sem título");
  } else if (name == "simple-novel") {
    for (int i = 1; i <= 10; ++i) {
      append(std::nullopt, project::StructuralNodeType::Chapter,
             std::to_string(i), "Sem título");
    }
  } else {
    for (int part = 1; part <= 3; ++part) {
      const auto parent =
          append(std::nullopt, project::StructuralNodeType::Part,
                 std::to_string(part), "Sem título");
      for (int chapter = 1; chapter <= 5; ++chapter)
        append(parent, project::StructuralNodeType::Chapter,
               std::to_string(chapter), "Sem título");
    }
  }
  repository_.save_many(session_.current()->path(), pending);
  session_.structural_nodes().insert(session_.structural_nodes().end(),
                                     pending.begin(), pending.end());
}

} // namespace inde::application

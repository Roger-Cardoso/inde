#include "inde/project/structural_node.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <uuid/uuid.h>

namespace inde::project {

std::string to_string(StructuralNodeType type) {
    switch (type) {
    case StructuralNodeType::Saga: return "saga";
    case StructuralNodeType::Series: return "series";
    case StructuralNodeType::Volume: return "volume";
    case StructuralNodeType::Part: return "part";
    case StructuralNodeType::Act: return "act";
    case StructuralNodeType::Arc: return "arc";
    case StructuralNodeType::Chapter: return "chapter";
    case StructuralNodeType::Section: return "section";
    case StructuralNodeType::Scene: return "scene";
    case StructuralNodeType::Custom: return "custom";
    }
    throw std::runtime_error("Tipo estrutural desconhecido");
}

StructuralNodeType structural_node_type_from_string(const std::string& value) {
    if (value == "saga") return StructuralNodeType::Saga;
    if (value == "series") return StructuralNodeType::Series;
    if (value == "volume") return StructuralNodeType::Volume;
    if (value == "part") return StructuralNodeType::Part;
    if (value == "act") return StructuralNodeType::Act;
    if (value == "arc") return StructuralNodeType::Arc;
    if (value == "chapter") return StructuralNodeType::Chapter;
    if (value == "section") return StructuralNodeType::Section;
    if (value == "scene") return StructuralNodeType::Scene;
    if (value == "custom") return StructuralNodeType::Custom;
    throw std::runtime_error("Tipo estrutural inválido: " + value);
}

std::string display_name(StructuralNodeType type) {
    switch (type) {
    case StructuralNodeType::Saga: return "Saga";
    case StructuralNodeType::Series: return "Série";
    case StructuralNodeType::Volume: return "Volume";
    case StructuralNodeType::Part: return "Parte";
    case StructuralNodeType::Act: return "Ato";
    case StructuralNodeType::Arc: return "Arco";
    case StructuralNodeType::Chapter: return "Capítulo";
    case StructuralNodeType::Section: return "Seção";
    case StructuralNodeType::Scene: return "Cena";
    case StructuralNodeType::Custom: return "Personalizado";
    }
    return "Desconhecido";
}

const std::vector<BuiltinStructuralElementType>&
builtin_structural_element_types() {
    static const std::vector<BuiltinStructuralElementType> values{
        {"10000000-0000-4000-9000-000000000001",
         StructuralNodeType::Series, "Série"},
        {"10000000-0000-4000-9000-000000000002",
         StructuralNodeType::Saga, "Saga"},
        {"10000000-0000-4000-9000-000000000003",
         StructuralNodeType::Part, "Parte"},
        {"10000000-0000-4000-9000-000000000004",
         StructuralNodeType::Act, "Ato"},
        {"10000000-0000-4000-9000-000000000005",
         StructuralNodeType::Volume, "Volume"},
        {"10000000-0000-4000-9000-000000000006",
         StructuralNodeType::Arc, "Arco"},
        {"10000000-0000-4000-9000-000000000007",
         StructuralNodeType::Chapter, "Capítulo"},
        {"10000000-0000-4000-9000-000000000008",
         StructuralNodeType::Section, "Seção"},
        {"10000000-0000-4000-9000-000000000009",
         StructuralNodeType::Scene, "Cena"},
    };
    return values;
}

std::string builtin_structural_type_id(StructuralNodeType legacy_type) {
    const auto& values = builtin_structural_element_types();
    const auto found = std::find_if(values.begin(), values.end(),
                                    [&](const auto& value) {
                                        return value.legacy_type == legacy_type;
                                    });
    if (found == values.end())
        throw std::runtime_error(
            "Tipo personalizado não possui identidade interna fixa");
    return found->id;
}

StructuralNodeType legacy_structural_type_for_id(const std::string& id) {
    const auto& values = builtin_structural_element_types();
    const auto found = std::find_if(values.begin(), values.end(),
                                    [&](const auto& value) {
                                        return id == value.id;
                                    });
    return found == values.end() ? StructuralNodeType::Custom
                                 : found->legacy_type;
}

std::string structural_node_type_name(const StructuralNode& node) {
    if (!node.structural_type_name.empty()) return node.structural_type_name;
    if (node.type == StructuralNodeType::Custom &&
        !node.custom_type_name.empty())
        return node.custom_type_name;
    return display_name(node.type);
}

std::string structural_node_position_label(const StructuralNode& node) {
    auto result = structural_node_type_name(node);
    if (!node.designator.empty()) result += ": " + node.designator;
    return result;
}

std::unordered_map<std::string, std::string>
structural_node_path_labels(const std::vector<StructuralNode>& nodes) {
    std::unordered_map<std::string, const StructuralNode*> by_id;
    by_id.reserve(nodes.size());
    for (const auto& node : nodes) by_id.emplace(node.id, &node);
    std::unordered_map<std::string, std::string> paths;
    paths.reserve(nodes.size());
    std::unordered_set<std::string> visiting;
    std::function<std::string(const StructuralNode&)> build =
        [&](const StructuralNode& node) -> std::string {
        if (const auto cached = paths.find(node.id); cached != paths.end())
            return cached->second;
        if (!visiting.insert(node.id).second)
            return "Estrutura cíclica";
        std::string result;
        if (node.parent_id) {
            const auto parent = by_id.find(*node.parent_id);
            if (parent != by_id.end()) result = build(*parent->second);
        }
        if (!result.empty()) result += " > ";
        result += structural_node_position_label(node);
        visiting.erase(node.id);
        paths.emplace(node.id, result);
        return result;
    };
    for (const auto& node : nodes) static_cast<void>(build(node));
    std::unordered_map<std::string, std::string> labels;
    labels.reserve(nodes.size());
    for (const auto& node : nodes) {
        auto label = paths.at(node.id);
        if (!node.title.empty()) label += " — " + node.title;
        labels.emplace(node.id, std::move(label));
    }
    return labels;
}

std::string structural_node_path_label(const std::vector<StructuralNode>& nodes,
                                       const std::string& id) {
    const auto labels = structural_node_path_labels(nodes);
    const auto found = labels.find(id);
    return found == labels.end() ? "Elemento editorial indisponível"
                                 : found->second;
}

void validate(const StructuralNode& node) {
    uuid_t parsed{};
    if (uuid_parse(node.id.c_str(), parsed) != 0) throw std::runtime_error("Nó estrutural com UUID inválido");
    if (uuid_parse(node.work_id.c_str(), parsed) != 0) throw std::runtime_error("Obra do nó estrutural com UUID inválido");
    if (node.parent_id && uuid_parse(node.parent_id->c_str(), parsed) != 0) throw std::runtime_error("Pai do nó estrutural com UUID inválido");
    if (node.title.empty()) throw std::runtime_error("O título do elemento estrutural é obrigatório");
    if (node.status.empty()) throw std::runtime_error("O status do elemento estrutural é obrigatório");
    if (node.position < 0) throw std::runtime_error("A posição estrutural não pode ser negativa");
    if (node.type == StructuralNodeType::Custom && node.custom_type_name.empty())
        throw std::runtime_error("Um tipo estrutural personalizado precisa de nome");
    if (!node.structural_type_id.empty()) {
        if (uuid_parse(node.structural_type_id.c_str(), parsed) != 0)
            throw std::runtime_error(
                "Tipo do elemento estrutural possui UUID inválido");
        if (node.structural_type_name.empty())
            throw std::runtime_error(
                "Nome do tipo do elemento estrutural está ausente");
    }
    if (node.designator.size() > 64)
        throw std::runtime_error(
            "O número/designador do elemento excede 64 caracteres");
    if (node.created_at.empty() || node.updated_at.empty()) throw std::runtime_error("Datas do elemento estrutural ausentes");
}

void validate(const StructuralElementType& type) {
    uuid_t parsed{};
    if (uuid_parse(type.id.c_str(), parsed) != 0)
        throw std::runtime_error("Tipo estrutural com UUID inválido");
    if (type.name.empty() || type.name.size() > 128)
        throw std::runtime_error(
            "O nome do tipo estrutural deve ter entre 1 e 128 caracteres");
    if (type.created_at.empty() || type.updated_at.empty())
        throw std::runtime_error("Datas do tipo estrutural ausentes");
}

StructuralTree::StructuralTree(const std::vector<StructuralNode>& nodes) : nodes_(nodes) {}

void StructuralTree::validate_all(const std::vector<std::string>& work_ids) const {
    std::unordered_map<std::string, const StructuralNode*> by_id;
    const std::unordered_set<std::string> valid_works(work_ids.begin(), work_ids.end());
    for (const auto& node : nodes_) {
        validate(node);
        if (!by_id.emplace(node.id, &node).second) throw std::runtime_error("UUID estrutural duplicado: " + node.id);
        if (!valid_works.contains(node.work_id))
            throw std::runtime_error("Elemento estrutural ligado a uma obra inexistente");
    }
    for (const auto& node : nodes_) {
        if (!node.parent_id) continue;
        const auto parent = by_id.find(*node.parent_id);
        if (parent == by_id.end()) throw std::runtime_error("Elemento estrutural com pai inexistente");
        if (parent->second->work_id != node.work_id) throw std::runtime_error("Pai e filho pertencem a obras diferentes");
    }
    enum class Mark { Visiting, Visited };
    std::unordered_map<std::string, Mark> marks;
    std::function<void(const StructuralNode&)> visit = [&](const StructuralNode& node) {
        if (const auto mark = marks.find(node.id); mark != marks.end()) {
            if (mark->second == Mark::Visiting) throw std::runtime_error("Ciclo detectado na estrutura editorial");
            return;
        }
        marks[node.id] = Mark::Visiting;
        if (node.parent_id) visit(*by_id.at(*node.parent_id));
        marks[node.id] = Mark::Visited;
    };
    for (const auto& node : nodes_) visit(node);
}

std::vector<std::string> StructuralTree::descendants_of(const std::string& id) const {
    std::vector<std::string> result;
    std::unordered_map<std::string, std::vector<const StructuralNode*>> children;
    for (const auto& node : nodes_) if (node.parent_id) children[*node.parent_id].push_back(&node);
    std::function<void(const std::string&)> collect = [&](const std::string& parent) {
        const auto found = children.find(parent); if (found == children.end()) return;
        for (const auto* node : found->second) { result.push_back(node->id); collect(node->id); }
    };
    collect(id); return result;
}

bool StructuralTree::can_move(const std::string& id, const std::optional<std::string>& parent) const {
    if (!parent) return true;
    if (*parent == id) return false;
    const auto descendants = descendants_of(id);
    return std::find(descendants.begin(), descendants.end(), *parent) == descendants.end();
}

} // namespace inde::project

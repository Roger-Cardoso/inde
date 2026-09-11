#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace inde::project {

inline constexpr int current_structural_node_version = 1;
inline constexpr std::int64_t structural_position_step = 1000;

enum class StructuralNodeType {
  Saga,
  Series,
  Volume,
  Part,
  Act,
  Arc,
  Chapter,
  Section,
  Scene,
  Custom
};

struct StructuralElementType {
  std::string id;
  std::string name;
  bool is_builtin{};
  std::string created_at;
  std::string updated_at;
};

struct BuiltinStructuralElementType {
  const char *id;
  StructuralNodeType legacy_type;
  const char *name;
};

struct StructuralNode {
  std::string id;
  std::string work_id;
  std::optional<std::string> parent_id;
  StructuralNodeType type{StructuralNodeType::Chapter};
  std::string title;
  std::string subtitle;
  std::string synopsis;
  std::string custom_type_name;
  std::string status{"Planejamento"};
  std::int64_t position{structural_position_step};
  std::string created_at;
  std::string updated_at;
  // v11: o tipo é uma identidade gerenciável. Os campos legados acima são
  // mantidos como espelho de compatibilidade para importação v1.
  std::string structural_type_id;
  std::string structural_type_name;
  // Designador opcional e deliberadamente textual: aceita 1, 0, I, IV ou
  // convenções próprias sem fingir uma aritmética universal.
  std::string designator;
  // v12: cada nó pertence a uma instância editorial. Vazio só existe em
  // fontes legadas antes de a persistência atribuir a estrutura ativa.
  std::string structure_id;
};

[[nodiscard]] std::string to_string(StructuralNodeType type);
[[nodiscard]] StructuralNodeType
structural_node_type_from_string(const std::string &value);
[[nodiscard]] std::string display_name(StructuralNodeType type);
[[nodiscard]] const std::vector<BuiltinStructuralElementType> &
builtin_structural_element_types();
[[nodiscard]] std::string
builtin_structural_type_id(StructuralNodeType legacy_type);
[[nodiscard]] StructuralNodeType
legacy_structural_type_for_id(const std::string &id);
[[nodiscard]] std::string structural_node_type_name(const StructuralNode &node);
[[nodiscard]] std::string
structural_node_position_label(const StructuralNode &node);
// Rótulo humano para seletores de estruturas grandes. A identidade continua
// sendo o UUID; o texto explicita ancestralidade e ordem entre irmãos.
[[nodiscard]] std::string
structural_node_path_label(const std::vector<StructuralNode> &nodes,
                           const std::string &id);
[[nodiscard]] std::unordered_map<std::string, std::string>
structural_node_path_labels(const std::vector<StructuralNode> &nodes);
void validate(const StructuralNode &node);
void validate(const StructuralElementType &type);

class StructuralTree {
public:
  explicit StructuralTree(const std::vector<StructuralNode> &nodes);
  void validate_all(const std::vector<std::string> &valid_work_ids) const;
  [[nodiscard]] std::vector<std::string>
  descendants_of(const std::string &id) const;
  [[nodiscard]] bool
  can_move(const std::string &id,
           const std::optional<std::string> &new_parent_id) const;

private:
  const std::vector<StructuralNode> &nodes_;
};

} // namespace inde::project

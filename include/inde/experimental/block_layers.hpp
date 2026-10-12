#pragma once
#include "inde/project/writing.hpp"
#include <memory>
#include <string>
#include <vector>
namespace inde::experimental {
struct LayerSnapshot {
  std::shared_ptr<const project::Document> document;
  std::vector<std::string> warnings;
};
[[nodiscard]] LayerSnapshot slice_layers(const project::Document &, std::size_t, std::size_t);
[[nodiscard]] LayerSnapshot edit_layers(const LayerSnapshot &, const std::string &);
struct TextProjection;
class TextBlocksLab;
struct MaterializedDocument {
  project::Document document;
  std::vector<std::string> warnings;
  std::size_t incorporated_occurrences{}, reference_occurrences{};
};
[[nodiscard]] MaterializedDocument materialize(const TextBlocksLab &,
    const TextProjection &, const std::string &title);
}

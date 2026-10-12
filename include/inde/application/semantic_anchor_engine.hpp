#pragma once
#include "inde/project/writing.hpp"
namespace inde::application {
// Offsets are Unicode scalar positions, identical to GTK text-buffer offsets.
class SemanticAnchorEngine {
public:
  static project::SemanticAnchorEvidence
  evidence(const project::Document &, const project::DocumentAnchor &);
  // Transforms semantic anchors only. The caller transforms legacy layers
  // itself.
  static void edit(project::Document &, std::size_t start, std::size_t end,
                   const std::string &replacement);
  static void
  inspect(project::Document &); // fail-visible, never search/reattach
  static void reanchor(project::Document &, const std::string &annotation,
                       std::size_t start, std::size_t end);
};
} // namespace inde::application

#pragma once
#include "inde/project/project.hpp"
#include "inde/project/structural_node.hpp"
#include "inde/project/writing.hpp"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace inde::application {
struct DocumentSaveSnapshot {
  project::Project owner;
  std::vector<project::StructuralNode> structural_nodes;
  project::Document document;
  project::Document expected;
  std::uint64_t epoch{}, revision{};
  std::string recovery_session;
  std::size_t cursor{}, bound{};
  std::uint64_t durable_revision{};
};
struct DocumentSaveResult {
  std::string project_id, document_id;
  std::uint64_t epoch{}, revision{};
  // Last durable state, including an earlier successful write before a failure.
  std::optional<project::Document> persisted;
  std::string error;
  std::chrono::nanoseconds elapsed{};
  std::uint64_t persisted_revision{};
  std::string journal_error;
};
class DocumentSaveCoordinator final {
public:
  using Writer = std::function<project::Document(const DocumentSaveSnapshot &)>;
  explicit DocumentSaveCoordinator(Writer writer = {});
  ~DocumentSaveCoordinator();
  DocumentSaveCoordinator(const DocumentSaveCoordinator &) = delete;
  DocumentSaveCoordinator &operator=(const DocumentSaveCoordinator &) = delete;
  // One active write + one replaceable pending snapshot, same editing context.
  // Writes already started are never cancelled or treated as discarded analysis.
  void submit(DocumentSaveSnapshot snapshot);
  [[nodiscard]] std::optional<DocumentSaveResult> take_latest();
  [[nodiscard]] bool busy() const;
  void drain(); // Wait for durable work; a close/save/navigation barrier.
  void clear(); // Drain then release the old editing context.
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}

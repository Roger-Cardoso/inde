#pragma once
#include "inde/application/document_export.hpp"
#include "inde/application/project_session.hpp"
#include "inde/experimental/block_layers.hpp"
#include "inde/persistence/composition_store.hpp"
namespace inde::application {
struct PreparedComposition {
  persistence::CompositionOutputGuard source;
  experimental::MaterializedDocument output;
  DocumentExport reading;
};
class CompositionService {
public:
  explicit CompositionService(ProjectSession &session) : session_(session) {}
  std::vector<project::CompositionSummary> list(std::size_t offset = 0) const;
  std::vector<project::CompositionSummary> search(const std::string &,std::size_t limit = 50) const;
  project::StoredComposition load(const std::string &) const;
  project::StoredComposition create(const std::string &) const;
  project::StoredComposition save(const project::StoredComposition &,
                                  bool draft_only = false) const;
  std::string import(project::Composition &, const project::Document &,
                     std::size_t start, std::size_t end) const;
  PreparedComposition prepare(const project::StoredComposition &,
                              const std::string &title) const;
  project::Document materialize(const PreparedComposition &,
                                bool acknowledged) const;
  void export_reading(const PreparedComposition &,
                      const std::filesystem::path &new_folder,
                      bool acknowledged) const;

private:
  const project::Project &owner() const;
  void current(const persistence::CompositionOutputGuard &) const;
  ProjectSession &session_;
  persistence::CompositionStore store_;
};
} // namespace inde::application

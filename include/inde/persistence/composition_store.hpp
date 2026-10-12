#pragma once
#include "inde/project/composition.hpp"
#include "inde/project/project.hpp"
#include <functional>
namespace inde::persistence {
class SqliteDatabase;
std::string encode_composition(const project::Composition &);
project::Composition decode_composition(const std::string &,
                                        const std::string &sha256);
std::string composition_checksum(const std::string &);
struct CompositionOutputGuard {
  std::string project_id, composition_id, sha256, recipe;
  std::int64_t revision{};
};
class CompositionStore {
public:
  std::vector<project::CompositionSummary> list(const project::Project &,
                                                std::size_t offset = 0) const;
  std::vector<project::CompositionSummary> search(const project::Project &,
      const std::string &, std::size_t limit = 50) const;
  project::StoredComposition load(const project::Project &,
                                  const std::string &) const;
  project::StoredComposition save(const project::Project &,
                                  const project::Composition &,
                                  std::int64_t expected,
                                  const std::string &expected_sha256,
                                  bool draft_only = false,
                                  const std::function<void(SqliteDatabase &)> &context = {}) const;
};
} // namespace inde::persistence

#pragma once

#include "inde/project/linguistic.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace inde::persistence {

struct LinguisticCatalogPaths {
  std::filesystem::path lexical;
  std::filesystem::path constructions;

  [[nodiscard]] static LinguisticCatalogPaths from_environment();
};

class SqliteLinguisticCatalog final {
public:
  explicit SqliteLinguisticCatalog(
      LinguisticCatalogPaths paths = LinguisticCatalogPaths::from_environment())
      : paths_(std::move(paths)) {}

  [[nodiscard]] project::LinguisticCatalogStatus status() const;
  [[nodiscard]] project::LexicalQueryResult
  query_lexicon(const std::string &text, std::size_t limit = 50) const;
  [[nodiscard]] std::vector<project::LinguisticEntitySummary>
  linguistic_entities(const std::string &search = {},
                      std::size_t limit = 50) const;

  [[nodiscard]] const LinguisticCatalogPaths &paths() const noexcept {
    return paths_;
  }

private:
  LinguisticCatalogPaths paths_;
};

} // namespace inde::persistence

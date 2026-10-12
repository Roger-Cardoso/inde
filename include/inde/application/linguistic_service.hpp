#pragma once

#include "inde/persistence/sqlite_linguistic_catalog.hpp"

#include <utility>

namespace inde::application {

class LinguisticService final {
public:
  LinguisticService() = default;
  explicit LinguisticService(persistence::LinguisticCatalogPaths paths)
      : catalog_(std::move(paths)) {}

  [[nodiscard]] project::LinguisticCatalogStatus status() const {
    return catalog_.status();
  }

  [[nodiscard]] project::LexicalQueryResult
  query_lexicon(const std::string &text, std::size_t limit = 50) const {
    return catalog_.query_lexicon(text, limit);
  }

  [[nodiscard]] project::LexicalQueryResult
  query_quarantine(const std::string &text, std::size_t limit = 50) const {
    return catalog_.query_quarantine(text, limit);
  }

  [[nodiscard]] std::vector<project::LinguisticEntitySummary>
  linguistic_entities(const std::string &search = {},
                      std::size_t limit = 50) const {
    return catalog_.linguistic_entities(search, limit);
  }

private:
  persistence::SqliteLinguisticCatalog catalog_;
};

} // namespace inde::application

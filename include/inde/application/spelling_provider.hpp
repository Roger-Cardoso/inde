#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inde::application {

class SpellingProvider {
public:
  virtual ~SpellingProvider() = default;
  [[nodiscard]] virtual bool available() const noexcept = 0;
  [[nodiscard]] virtual std::string description() const = 0;
  [[nodiscard]] virtual bool check(const std::string &word) const = 0;
  [[nodiscard]] virtual std::vector<std::string>
  suggest(const std::string &word, std::size_t limit = 5) const = 0;
};

} // namespace inde::application

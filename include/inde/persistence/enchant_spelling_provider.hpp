#pragma once

#include "inde/application/spelling_provider.hpp"

#include <memory>
#include <string>

namespace inde::persistence {

class EnchantSpellingProvider final : public application::SpellingProvider {
public:
  explicit EnchantSpellingProvider(std::string language = "pt_BR");
  ~EnchantSpellingProvider() override;
  EnchantSpellingProvider(const EnchantSpellingProvider &) = delete;
  EnchantSpellingProvider &operator=(const EnchantSpellingProvider &) = delete;

  [[nodiscard]] bool available() const noexcept override;
  [[nodiscard]] std::string description() const override;
  [[nodiscard]] bool check(const std::string &word) const override;
  [[nodiscard]] std::vector<std::string>
  suggest(const std::string &word, std::size_t limit = 5) const override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace inde::persistence

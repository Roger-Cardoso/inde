#pragma once

#include "inde/project/dictionary.hpp"

#include <filesystem>
#include <vector>

namespace inde::persistence {

class DictionaryStore {
public:
  virtual ~DictionaryStore() = default;
  [[nodiscard]] virtual std::vector<project::LexicalDictionary>
  load() const = 0;
  virtual void
  save(const std::vector<project::LexicalDictionary> &dictionaries) const = 0;
};

class JsonDictionaryStore final : public DictionaryStore {
public:
  explicit JsonDictionaryStore(
      std::filesystem::path storage_path = default_path());

  [[nodiscard]] std::vector<project::LexicalDictionary> load() const override;
  void save(const std::vector<project::LexicalDictionary> &dictionaries)
      const override;

  [[nodiscard]] const std::filesystem::path &path() const noexcept {
    return storage_path_;
  }
  [[nodiscard]] static std::filesystem::path default_path();

private:
  std::filesystem::path storage_path_;
};

} // namespace inde::persistence

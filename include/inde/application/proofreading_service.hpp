#pragma once

#include "inde/application/spelling_provider.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/project/dictionary.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace inde::application {

class ProofreadingService {
public:
  ProofreadingService(persistence::DictionaryStore &store,
                      SpellingProvider &spelling);

  [[nodiscard]] const std::vector<project::LexicalDictionary> &
  dictionaries() const noexcept;
  [[nodiscard]] const project::LexicalDictionary &
  create_dictionary(std::string name, std::string language = "pt-BR");
  void set_dictionary_enabled(const std::string &id, bool enabled);
  void remove_dictionary(const std::string &id);
  void add_word(const std::string &dictionary_id, std::string word);
  void remove_word(const std::string &dictionary_id, const std::string &word);
  void add_to_personal_dictionary(std::string word);

  [[nodiscard]] std::vector<project::TextIssue>
  analyze(const std::string &text, std::size_t maximum_issues = 500) const;
  [[nodiscard]] bool spelling_available() const noexcept;
  [[nodiscard]] std::string provider_description() const;
  [[nodiscard]] const std::string &storage_error() const noexcept;

private:
  void persist();
  void rebuild_custom_words();
  [[nodiscard]] bool custom_word(const std::string &word) const;

  persistence::DictionaryStore &store_;
  SpellingProvider &spelling_;
  std::vector<project::LexicalDictionary> dictionaries_;
  std::unordered_set<std::string> custom_words_;
  mutable std::unordered_map<std::string, bool> spelling_cache_;
  std::string storage_error_;
};

} // namespace inde::application

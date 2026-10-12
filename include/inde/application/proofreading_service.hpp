#pragma once

#include "inde/application/spelling_provider.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/project/dictionary.hpp"

#include <cstddef>
#include <memory>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace inde::application {

// Cache and spelling provider belong to one analyzer/thread. The vocabulary
// snapshot is immutable and can be replaced without reading a store in a worker.
class ProofreadingAnalyzer {
public:
  explicit ProofreadingAnalyzer(SpellingProvider &spelling) : spelling_(spelling) {}
  [[nodiscard]] std::vector<project::TextIssue>
  analyze(const std::string &text,
          const std::unordered_set<std::string> &custom_words,
          std::size_t maximum_issues = 500,
          std::stop_token cancellation = {}) const;
  void clear_cache() { spelling_cache_.clear(); }
private:
  SpellingProvider &spelling_;
  mutable std::unordered_map<std::string, bool> spelling_cache_;
};

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
  [[nodiscard]] std::shared_ptr<const std::unordered_set<std::string>>
  vocabulary_snapshot() const noexcept { return custom_words_; }
  [[nodiscard]] bool spelling_available() const noexcept;
  [[nodiscard]] std::string provider_description() const;
  [[nodiscard]] const std::string &storage_error() const noexcept;

private:
  void persist();
  void rebuild_custom_words();

  persistence::DictionaryStore &store_;
  SpellingProvider &spelling_;
  std::vector<project::LexicalDictionary> dictionaries_;
  std::shared_ptr<const std::unordered_set<std::string>> custom_words_;
  ProofreadingAnalyzer analyzer_;
  std::string storage_error_;
};

} // namespace inde::application

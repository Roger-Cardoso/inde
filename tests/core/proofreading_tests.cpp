#include "inde/application/catalog_service.hpp"
#include "inde/application/project_service.hpp"
#include "inde/application/project_session.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/spelling_provider.hpp"
#include "inde/application/structural_service.hpp"
#include "inde/application/temporal_view.hpp"
#include "inde/application/timeline_projection.hpp"
#include "inde/persistence/catalog_repository.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/persistence/json.hpp"
#include "inde/persistence/project_database_importer.hpp"
#include "inde/persistence/project_database_repository.hpp"
#include "inde/persistence/project_repository.hpp"
#include "inde/persistence/recent_projects_store.hpp"
#include "inde/persistence/schema_migrator.hpp"
#include "inde/persistence/sqlite_catalog_repository.hpp"
#include "inde/persistence/sqlite_database.hpp"
#include "inde/persistence/sqlite_structural_repository.hpp"
#include "inde/persistence/structural_repository.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>

#include "core_test_cases.hpp"
#include "test_support.hpp"

namespace inde::tests {

namespace {

class DeterministicSpellingProvider final
    : public inde::application::SpellingProvider {
public:
  [[nodiscard]] bool available() const noexcept override { return true; }
  [[nodiscard]] std::string description() const override {
    return "Provedor ortográfico de teste";
  }
  [[nodiscard]] bool check(const std::string &word) const override {
    return word == "isto" || word == "casa" || word == "ação" ||
           word == "Ação";
  }
  [[nodiscard]] std::vector<std::string>
  suggest(const std::string &word, std::size_t limit) const override {
    if (word == "caza" && limit > 0)
      return {"casa"};
    return {};
  }
};

} // namespace

void dictionaries_and_proofreading_are_global_and_persistent() {
  TemporaryDirectory temporary;
  const auto path = temporary.path / "preferences" / "dictionaries.json";
  inde::persistence::JsonDictionaryStore store(path);
  DeterministicSpellingProvider spelling;
  inde::application::ProofreadingService service(store, spelling);

  assert(service.dictionaries().size() == 1);
  assert(service.dictionaries().front().category ==
         inde::project::DictionaryCategory::System);
  assert(service.spelling_available());

  const auto issues = service.analyze("isto  isto , caza.");
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Spelling &&
           issue.excerpt == "caza" && !issue.suggestions.empty() &&
           issue.suggestions.front() == "casa";
  }));

  const auto unicode_issues = service.analyze("ação.");
  assert(std::ranges::any_of(unicode_issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.start == 0 && issue.length == 1 && issue.excerpt == "a" &&
           issue.replacement == std::optional<std::string>{"A"};
  }));
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.message.find("espaços consecutivos") != std::string::npos;
  }));
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.message.find("Palavra repetida") != std::string::npos;
  }));
  assert(std::ranges::any_of(issues, [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Grammar &&
           issue.message.find("antes deste sinal") != std::string::npos;
  }));

  const auto dictionary_id =
      service.create_dictionary("Nomes de Arvoredo").id;
  service.add_word(dictionary_id, "Aurélin");
  service.add_word(dictionary_id, "d'Água");
  assert(std::ranges::none_of(service.analyze("Aurélin."), [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Spelling;
  }));
  assert(std::filesystem::is_regular_file(path));

  inde::persistence::JsonDictionaryStore reopened_store(path);
  inde::application::ProofreadingService reopened(reopened_store, spelling);
  assert(reopened.dictionaries().size() == 2);
  assert(reopened.dictionaries().back().words.size() == 2);
  assert(std::ranges::find(reopened.dictionaries().back().words, "Aurélin") !=
         reopened.dictionaries().back().words.end());
  reopened.set_dictionary_enabled(dictionary_id, false);
  assert(std::ranges::any_of(reopened.analyze("Aurélin."), [](const auto &issue) {
    return issue.kind == inde::project::TextIssueKind::Spelling;
  }));

  bool system_removal_rejected = false;
  try {
    reopened.remove_dictionary(reopened.dictionaries().front().id);
  } catch (const std::exception &) {
    system_removal_rejected = true;
  }
  assert(system_removal_rejected);

  bool connector_only_rejected = false;
  try {
    reopened.add_word(dictionary_id, "-");
  } catch (const std::exception &) {
    connector_only_rejected = true;
  }
  assert(connector_only_rejected);

  const auto damaged_path = temporary.path / "damaged.json";
  {
    std::ofstream damaged(damaged_path);
    damaged << "{ arquivo interrompido";
  }
  inde::persistence::JsonDictionaryStore damaged_store(damaged_path);
  inde::application::ProofreadingService guarded(damaged_store, spelling);
  assert(!guarded.storage_error().empty());
  bool overwrite_rejected = false;
  try {
    static_cast<void>(guarded.create_dictionary("Não sobrescrever"));
  } catch (const std::exception &) {
    overwrite_rejected = true;
  }
  assert(overwrite_rejected);
  assert(read_file(damaged_path) == "{ arquivo interrompido");

  inde::persistence::EnchantSpellingProvider system_spelling;
  if (system_spelling.available()) {
    assert(system_spelling.check("casa"));
    assert(!system_spelling.check("xptoqz"));
    assert(!system_spelling.suggest("caza").empty());
  }
}

} // namespace inde::tests

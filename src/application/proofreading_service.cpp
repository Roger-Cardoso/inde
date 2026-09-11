#include "inde/application/proofreading_service.hpp"

#include "inde/project/manifest.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>
#include <uuid/uuid.h>

namespace inde::application {
namespace {

constexpr const char *system_dictionary_id =
    "00000000-0000-4000-9000-000000000301";

struct Utf8Unit {
  char32_t value{};
  std::size_t byte{};
  std::size_t character{};
};

struct WordToken {
  std::string text;
  std::size_t start{};
  std::size_t length{};
  std::size_t first_unit{};
  std::size_t past_unit{};
};

std::vector<Utf8Unit> decode_utf8(const std::string &text) {
  std::vector<Utf8Unit> result;
  for (std::size_t byte = 0, character = 0; byte < text.size(); ++character) {
    const auto first = static_cast<unsigned char>(text[byte]);
    char32_t value = first;
    std::size_t count = 1;
    if ((first & 0xe0U) == 0xc0U && byte + 1 < text.size()) {
      value = first & 0x1fU;
      count = 2;
    } else if ((first & 0xf0U) == 0xe0U && byte + 2 < text.size()) {
      value = first & 0x0fU;
      count = 3;
    } else if ((first & 0xf8U) == 0xf0U && byte + 3 < text.size()) {
      value = first & 0x07U;
      count = 4;
    }
    bool valid = count > 1;
    for (std::size_t offset = 1; valid && offset < count; ++offset) {
      const auto continuation = static_cast<unsigned char>(text[byte + offset]);
      if ((continuation & 0xc0U) != 0x80U) {
        valid = false;
        break;
      }
      value = (value << 6U) | (continuation & 0x3fU);
    }
    if (!valid) {
      value = first;
      count = 1;
    }
    result.push_back({value, byte, character});
    byte += count;
  }
  return result;
}

bool is_letter(char32_t value) {
  return (value >= U'A' && value <= U'Z') || (value >= U'a' && value <= U'z') ||
         (value >= 0x00c0 && value <= 0x02af) ||
         (value >= 0x0300 && value <= 0x036f);
}

bool is_lower(char32_t value) {
  if (value >= U'a' && value <= U'z')
    return true;
  switch (value) {
  case U'à':
  case U'á':
  case U'â':
  case U'ã':
  case U'ä':
  case U'å':
  case U'ç':
  case U'è':
  case U'é':
  case U'ê':
  case U'ë':
  case U'ì':
  case U'í':
  case U'î':
  case U'ï':
  case U'ñ':
  case U'ò':
  case U'ó':
  case U'ô':
  case U'õ':
  case U'ö':
  case U'ù':
  case U'ú':
  case U'û':
  case U'ü':
  case U'ý':
  case U'ÿ':
    return true;
  default:
    return false;
  }
}

char32_t lower(char32_t value) {
  if (value >= U'A' && value <= U'Z')
    return value + 32;
  if (value >= 0x00c0 && value <= 0x00de && value != 0x00d7)
    return value + 32;
  return value;
}

char32_t upper(char32_t value) {
  if (value >= U'a' && value <= U'z')
    return value - 32;
  if (value >= 0x00e0 && value <= 0x00fe && value != 0x00f7)
    return value - 32;
  return value;
}

void append_utf8(std::string &out, char32_t value) {
  if (value <= 0x7f) {
    out.push_back(static_cast<char>(value));
  } else if (value <= 0x7ff) {
    out.push_back(static_cast<char>(0xc0 | (value >> 6)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else if (value <= 0xffff) {
    out.push_back(static_cast<char>(0xe0 | (value >> 12)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else {
    out.push_back(static_cast<char>(0xf0 | (value >> 18)));
    out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  }
}

std::string fold(const std::string &value) {
  std::string result;
  for (const auto &unit : decode_utf8(value))
    append_utf8(result, lower(unit.value));
  return result;
}

std::string trim_ascii(std::string value) {
  const auto visible = [](unsigned char c) { return !std::isspace(c); };
  const auto first = std::find_if(value.begin(), value.end(), visible);
  const auto last = std::find_if(value.rbegin(), value.rend(), visible).base();
  if (first >= last)
    return {};
  return {first, last};
}

std::vector<WordToken> words(const std::string &text,
                             const std::vector<Utf8Unit> &units) {
  std::vector<WordToken> result;
  for (std::size_t index = 0; index < units.size();) {
    if (!is_letter(units[index].value)) {
      ++index;
      continue;
    }
    const auto first = index;
    ++index;
    while (index < units.size()) {
      if (is_letter(units[index].value)) {
        ++index;
        continue;
      }
      const bool connector = units[index].value == U'-' ||
                             units[index].value == U'\'' ||
                             units[index].value == U'’';
      if (connector && index + 1 < units.size() &&
          is_letter(units[index + 1].value)) {
        index += 2;
        continue;
      }
      break;
    }
    const auto byte_start = units[first].byte;
    const auto byte_end =
        index < units.size() ? units[index].byte : text.size();
    result.push_back({text.substr(byte_start, byte_end - byte_start),
                      units[first].character, index - first, first, index});
  }
  return result;
}

bool is_all_caps_abbreviation(const WordToken &word,
                              const std::vector<Utf8Unit> &units) {
  if (word.length < 2 || word.length > 6)
    return false;
  for (std::size_t index = word.first_unit; index < word.past_unit; ++index) {
    const auto value = units[index].value;
    if (is_letter(value) && lower(value) == value)
      return false;
  }
  return true;
}

bool punctuation(char32_t value) {
  return value == U',' || value == U'.' || value == U';' || value == U':' ||
         value == U'!' || value == U'?';
}

bool valid_uuid(const std::string &value) {
  uuid_t parsed{};
  return uuid_parse(value.c_str(), parsed) == 0;
}

bool valid_personal_word(const std::string &word) {
  const auto units = decode_utf8(word);
  if (units.empty() || units.size() > 80 || !is_letter(units.front().value) ||
      !is_letter(units.back().value))
    return false;
  for (std::size_t index = 0; index < units.size(); ++index) {
    if (is_letter(units[index].value))
      continue;
    const bool connector = units[index].value == U'-' ||
                           units[index].value == U'\'' ||
                           units[index].value == U'’';
    if (!connector || index == 0 || index + 1 == units.size() ||
        !is_letter(units[index - 1].value) ||
        !is_letter(units[index + 1].value))
      return false;
  }
  return true;
}

} // namespace

ProofreadingService::ProofreadingService(persistence::DictionaryStore &store,
                                         SpellingProvider &spelling)
    : store_(store), spelling_(spelling) {
  dictionaries_.push_back({system_dictionary_id,
                           "Português (Brasil)",
                           "pt-BR",
                           project::DictionaryCategory::System,
                           true,
                           {}});
  try {
    auto loaded = store_.load();
    std::unordered_set<std::string> ids{system_dictionary_id};
    std::unordered_set<std::string> names{fold("Português (Brasil)")};
    for (const auto &dictionary : loaded) {
      if (dictionary.category != project::DictionaryCategory::User ||
          dictionary.language != "pt-BR" || !valid_uuid(dictionary.id) ||
          dictionary.name.empty() || !ids.insert(dictionary.id).second ||
          !names.insert(fold(dictionary.name)).second)
        throw std::runtime_error(
            "O arquivo contém dicionários inválidos ou duplicados");
      std::unordered_set<std::string> words_seen;
      for (const auto &word : dictionary.words) {
        if (!valid_personal_word(word) || !words_seen.insert(fold(word)).second)
          throw std::runtime_error(
              "O arquivo contém palavras pessoais inválidas ou duplicadas");
      }
    }
    dictionaries_.insert(dictionaries_.end(),
                         std::make_move_iterator(loaded.begin()),
                         std::make_move_iterator(loaded.end()));
  } catch (const std::exception &error) {
    storage_error_ = error.what();
  }
  rebuild_custom_words();
}

const std::vector<project::LexicalDictionary> &
ProofreadingService::dictionaries() const noexcept {
  return dictionaries_;
}

const project::LexicalDictionary &
ProofreadingService::create_dictionary(std::string name, std::string language) {
  if (!storage_error_.empty())
    throw std::runtime_error(
        "O arquivo de dicionários precisa ser recuperado: " + storage_error_);
  name = trim_ascii(std::move(name));
  if (name.empty() || decode_utf8(name).size() > 96)
    throw std::runtime_error(
        "O nome do dicionário deve ter entre 1 e 96 caracteres");
  if (language != "pt-BR")
    throw std::runtime_error(
        "Este primeiro corte aceita apenas Português (Brasil)");
  if (std::ranges::any_of(dictionaries_, [&](const auto &value) {
        return fold(value.name) == fold(name);
      }))
    throw std::runtime_error("Já existe um dicionário com esse nome");
  dictionaries_.push_back({project::new_uuid(),
                           std::move(name),
                           std::move(language),
                           project::DictionaryCategory::User,
                           true,
                           {}});
  try {
    persist();
  } catch (...) {
    dictionaries_.pop_back();
    throw;
  }
  return dictionaries_.back();
}

void ProofreadingService::set_dictionary_enabled(const std::string &id,
                                                 bool enabled) {
  const auto found =
      std::ranges::find(dictionaries_, id, &project::LexicalDictionary::id);
  if (found == dictionaries_.end())
    throw std::runtime_error("Dicionário não encontrado");
  if (found->category == project::DictionaryCategory::System)
    throw std::runtime_error(
        "O dicionário pt-BR do sistema permanece sempre ativo");
  const auto previous = found->enabled;
  found->enabled = enabled;
  try {
    persist();
  } catch (...) {
    found->enabled = previous;
    throw;
  }
  rebuild_custom_words();
}

void ProofreadingService::remove_dictionary(const std::string &id) {
  const auto found =
      std::ranges::find(dictionaries_, id, &project::LexicalDictionary::id);
  if (found == dictionaries_.end())
    throw std::runtime_error("Dicionário não encontrado");
  if (found->category == project::DictionaryCategory::System)
    throw std::runtime_error("O dicionário do sistema não pode ser removido");
  const auto index = static_cast<std::size_t>(found - dictionaries_.begin());
  const auto removed = *found;
  dictionaries_.erase(found);
  try {
    persist();
  } catch (...) {
    dictionaries_.insert(
        dictionaries_.begin() + static_cast<std::ptrdiff_t>(index), removed);
    throw;
  }
  rebuild_custom_words();
}

void ProofreadingService::add_word(const std::string &dictionary_id,
                                   std::string word) {
  if (!storage_error_.empty())
    throw std::runtime_error(
        "O arquivo de dicionários precisa ser recuperado: " + storage_error_);
  const auto found = std::ranges::find(dictionaries_, dictionary_id,
                                       &project::LexicalDictionary::id);
  if (found == dictionaries_.end())
    throw std::runtime_error("Dicionário não encontrado");
  if (found->category == project::DictionaryCategory::System)
    throw std::runtime_error(
        "Palavras pessoais devem entrar em um dicionário do usuário");
  word = trim_ascii(std::move(word));
  if (!valid_personal_word(word))
    throw std::runtime_error("Adicione uma única palavra de até 80 caracteres");
  const auto normalized = fold(word);
  if (std::ranges::none_of(found->words, [&](const auto &existing) {
        return fold(existing) == normalized;
      })) {
    const auto previous = found->words;
    found->words.push_back(std::move(word));
    std::ranges::sort(found->words, [](const auto &left, const auto &right) {
      return fold(left) < fold(right);
    });
    try {
      persist();
    } catch (...) {
      found->words = previous;
      throw;
    }
    rebuild_custom_words();
  }
}

void ProofreadingService::remove_word(const std::string &dictionary_id,
                                      const std::string &word) {
  const auto found = std::ranges::find(dictionaries_, dictionary_id,
                                       &project::LexicalDictionary::id);
  if (found == dictionaries_.end() ||
      found->category != project::DictionaryCategory::User)
    throw std::runtime_error("Dicionário pessoal não encontrado");
  const auto normalized = fold(word);
  const auto previous = found->words;
  std::erase_if(found->words, [&](const auto &existing) {
    return fold(existing) == normalized;
  });
  try {
    persist();
  } catch (...) {
    found->words = previous;
    throw;
  }
  rebuild_custom_words();
}

void ProofreadingService::add_to_personal_dictionary(std::string word) {
  auto found = std::ranges::find_if(dictionaries_, [](const auto &dictionary) {
    return dictionary.category == project::DictionaryCategory::User &&
           dictionary.name == "Vocabulário pessoal";
  });
  if (found == dictionaries_.end()) {
    const auto &created = create_dictionary("Vocabulário pessoal");
    add_word(created.id, std::move(word));
  } else {
    add_word(found->id, std::move(word));
  }
}

std::vector<project::TextIssue>
ProofreadingService::analyze(const std::string &text,
                             std::size_t maximum_issues) const {
  if (maximum_issues == 0 || text.empty())
    return {};
  const auto units = decode_utf8(text);
  const auto tokens = words(text, units);
  std::vector<project::TextIssue> issues;
  const auto append = [&](project::TextIssue issue) {
    if (issues.size() < maximum_issues)
      issues.push_back(std::move(issue));
  };

  for (std::size_t index = 0;
       index < units.size() && issues.size() < maximum_issues;) {
    if (units[index].value != U' ') {
      ++index;
      continue;
    }
    const auto first = index;
    while (index < units.size() && units[index].value == U' ')
      ++index;
    if (index - first > 1 && first > 0 && index < units.size() &&
        units[first - 1].value != U'\n' && units[index].value != U'\n') {
      append({project::TextIssueKind::Grammar,
              units[first].character,
              index - first,
              std::string(index - first, ' '),
              "Há espaços consecutivos entre palavras.",
              {},
              std::string(" ")});
    }
    if (index < units.size() && punctuation(units[index].value)) {
      append({project::TextIssueKind::Grammar,
              units[first].character,
              index - first,
              std::string(index - first, ' '),
              "Não se usa espaço antes deste sinal de pontuação.",
              {},
              std::string()});
    }
  }

  for (std::size_t index = 1;
       index < tokens.size() && issues.size() < maximum_issues; ++index) {
    const auto &previous = tokens[index - 1];
    const auto &current = tokens[index];
    if (fold(previous.text) != fold(current.text))
      continue;
    const auto previous_end = previous.past_unit;
    bool only_spacing = previous_end <= current.first_unit;
    for (auto unit = previous_end; only_spacing && unit < current.first_unit;
         ++unit)
      only_spacing = units[unit].value == U' ' || units[unit].value == U'\t' ||
                     units[unit].value == U'\n' || units[unit].value == U'\r';
    if (only_spacing)
      append({project::TextIssueKind::Grammar,
              current.start,
              current.length,
              current.text,
              "Palavra repetida em sequência.",
              {},
              std::string()});
  }

  bool sentence_start = true;
  for (std::size_t index = 0;
       index < units.size() && issues.size() < maximum_issues; ++index) {
    const auto value = units[index].value;
    if (sentence_start && is_letter(value)) {
      if (is_lower(value)) {
        const auto token = std::ranges::find_if(tokens, [&](const auto &word) {
          return word.start == units[index].character;
        });
        if (token != tokens.end()) {
          const auto first_byte = units[token->first_unit].byte;
          const auto next_byte = token->length > 1
                                     ? units[token->first_unit + 1].byte
                                     : first_byte + token->text.size();
          const auto first_letter =
              token->text.substr(0, next_byte - first_byte);
          std::string replacement;
          append_utf8(replacement, upper(value));
          append({project::TextIssueKind::Grammar,
                  token->start,
                  1,
                  first_letter,
                  "A frase começa com letra minúscula.",
                  {},
                  replacement});
        }
      }
      sentence_start = false;
    } else if (value == U'.' || value == U'!' || value == U'?') {
      sentence_start = true;
    } else if (sentence_start && !is_letter(value) && value != U' ' &&
               value != U'\t' && value != U'\r' && value != U'“' &&
               value != U'\'' && value != U'\"' && value != U'—') {
      sentence_start = false;
    }
  }

  for (std::size_t index = 0;
       index + 1 < units.size() && issues.size() < maximum_issues; ++index) {
    const auto value = units[index].value;
    if ((value != U',' && value != U';' && value != U':') ||
        !is_letter(units[index + 1].value))
      continue;
    std::string replacement;
    append_utf8(replacement, value);
    replacement.push_back(' ');
    append({project::TextIssueKind::Grammar,
            units[index].character,
            1,
            text.substr(units[index].byte, 1),
            "Falta um espaço depois deste sinal de pontuação.",
            {},
            replacement});
  }

  if (spelling_.available()) {
    for (const auto &word : tokens) {
      if (issues.size() >= maximum_issues)
        break;
      if (is_all_caps_abbreviation(word, units) || custom_word(word.text))
        continue;
      const auto key = fold(word.text);
      auto cached = spelling_cache_.find(key);
      if (cached == spelling_cache_.end())
        cached = spelling_cache_.emplace(key, spelling_.check(word.text)).first;
      if (!cached->second)
        append({project::TextIssueKind::Spelling, word.start, word.length,
                word.text, "Palavra não encontrada nos dicionários ativos.",
                spelling_.suggest(word.text), std::nullopt});
    }
  }

  std::ranges::stable_sort(issues, [](const auto &left, const auto &right) {
    if (left.start != right.start)
      return left.start < right.start;
    return left.kind == project::TextIssueKind::Spelling &&
           right.kind == project::TextIssueKind::Grammar;
  });
  return issues;
}

bool ProofreadingService::spelling_available() const noexcept {
  return spelling_.available();
}

std::string ProofreadingService::provider_description() const {
  return spelling_.description();
}

const std::string &ProofreadingService::storage_error() const noexcept {
  return storage_error_;
}

void ProofreadingService::persist() {
  if (!storage_error_.empty())
    throw std::runtime_error(
        "O arquivo de dicionários precisa ser recuperado: " + storage_error_);
  store_.save(dictionaries_);
}

void ProofreadingService::rebuild_custom_words() {
  custom_words_.clear();
  for (const auto &dictionary : dictionaries_) {
    if (dictionary.category != project::DictionaryCategory::User ||
        !dictionary.enabled)
      continue;
    for (const auto &word : dictionary.words)
      custom_words_.insert(fold(word));
  }
  spelling_cache_.clear();
}

bool ProofreadingService::custom_word(const std::string &word) const {
  return custom_words_.contains(fold(word));
}

} // namespace inde::application

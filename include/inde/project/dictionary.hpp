#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace inde::project {

enum class DictionaryCategory { System, User };

struct LexicalDictionary {
  std::string id;
  std::string name;
  std::string language;
  DictionaryCategory category{DictionaryCategory::User};
  bool enabled{true};
  std::vector<std::string> words;
};

enum class TextIssueKind { Spelling, Grammar };

struct TextIssue {
  TextIssueKind kind{TextIssueKind::Spelling};
  std::size_t start{};
  std::size_t length{};
  std::string excerpt;
  std::string message;
  std::vector<std::string> suggestions;
  std::optional<std::string> replacement;
};

} // namespace inde::project

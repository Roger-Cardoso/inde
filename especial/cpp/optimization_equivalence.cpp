#include "comic_model.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/text_search.hpp"
#include "inde/project/writing.hpp"
#include <iostream>
#include <random>

using inde::publication::A;
using inde::publication::O;
using inde::publication::V;
namespace {
struct Dictionary final : inde::application::SpellingProvider {
  bool available() const noexcept override { return true; }
  std::string description() const override { return "Synthetic"; }
  bool check(const std::string &word) const override { return word != "erraaada"; }
  std::vector<std::string> suggest(const std::string &, std::size_t) const override { return {"errada"}; }
};
V number(std::size_t n) { return static_cast<std::int64_t>(n); }
V inspect(const std::string &text) {
  const auto stats = inde::project::analyze_document_text(text, 200, 4);
  A repeated, paragraphs, searches, issues;
  for (const auto &r : stats.repeated_words)
    repeated.emplace_back(O{{"word", r.word}, {"count", number(r.count)}});
  for (const auto &p : stats.long_paragraphs)
    paragraphs.emplace_back(A{number(p.paragraph_number), number(p.word_count), number(p.start_offset)});
  for (const auto &q : {"a", "SS", "STRASSE", "ÁGUA", "cafe", "café", "İ", "日本語", "العربية", "🙂", "a_a"}) {
    for (bool sensitive : {false, true}) for (bool whole : {false, true}) {
      A matches;
      const auto result = inde::application::find_text(text, {q, sensitive, whole, 11});
      for (const auto &m : result.matches) matches.emplace_back(A{number(m.start), number(m.end)});
      searches.emplace_back(O{{"matches", std::move(matches)}, {"truncated", result.truncated}});
    }
  }
  Dictionary dictionary;
  inde::application::ProofreadingAnalyzer analyzer(dictionary);
  for (const auto &i : analyzer.analyze(text, {"ÁGUA"}, 10000)) {
    A suggestions;
    for (const auto &s : i.suggestions) suggestions.emplace_back(s);
    issues.emplace_back(O{{"kind", static_cast<int>(i.kind)}, {"start", number(i.start)},
      {"length", number(i.length)}, {"excerpt", i.excerpt}, {"message", i.message},
      {"suggestions", std::move(suggestions)}, {"replacement", i.replacement ? V(*i.replacement) : V(nullptr)}});
  }
  return O{{"words", number(stats.words)}, {"characters", number(stats.characters_with_spaces)},
    {"without_spaces", number(stats.characters_without_spaces)}, {"sentences", number(stats.sentences)},
    {"paragraphs", number(stats.paragraphs)}, {"minutes", number(stats.reading_minutes)},
    {"repeated", std::move(repeated)}, {"long", std::move(paragraphs)},
    {"searches", std::move(searches)}, {"issues", std::move(issues)}};
}
}
int main() {
  std::mt19937 random(2402);
  const std::vector<std::string> tokens{"Água", "água", "café", "café", "Straße", "STRASSE", "ß", "İ",
    "erraaada", "🙂", "日本語", "العربية", "한국어", "a_a", "AAAA", "repetida", "repetida", "um", "the",
    "primeira", "segunda", "terceira", "quarta", "quinta", "sexta", "sétima", "oitava", "nona", "décima"};
  const std::vector<std::string> separators{" ", "  ", "\n", ". ", ",", "! ", "\t", " ? ", "—"};
  A results;
  results.emplace_back(inspect(""));
  results.emplace_back(inspect("ß s ss Straße STRASSE café café a_a a-a 'a' 🙂 água\nágua"));
  for (int sample = 0; sample < 60; ++sample) {
    std::string text;
    for (int i = 0; i < 300; ++i)
      text += tokens[random() % tokens.size()] + separators[random() % separators.size()];
    results.emplace_back(inspect(text));
  }
  // Invalid byte handling belongs to the corrector/statistics; search rejects it.
  Dictionary dictionary;
  inde::application::ProofreadingAnalyzer analyzer(dictionary);
  for (const std::string &text : {std::string("a\xff z", 5), std::string("a\xf0\x80", 3)}) {
    A offsets;
    for (const auto &i : analyzer.analyze(text, {})) offsets.emplace_back(number(i.start));
    results.emplace_back(std::move(offsets));
    try { static_cast<void>(inde::application::find_text(text, {"a"})); return 2; }
    catch (const std::runtime_error &) {}
  }
  std::vector<inde::project::StructuralNode> nodes;
  for (int i = 0; i < 80; ++i) {
    inde::project::StructuralNode n;
    n.id = "unit-" + std::to_string(i);
    n.title = "Título " + std::to_string(i);
    n.position = i;
    if (i > 0 && i % 3 != 0) n.parent_id = "unit-" + std::to_string(i - 1);
    nodes.push_back(std::move(n));
  }
  A ordered;
  for (const auto &n : inde::application::production_order(nodes)) ordered.emplace_back(n.id);
  results.emplace_back(std::move(ordered));
  const auto serialized = inde::persistence::json::serialize(results, false);
  std::cout << inde::publication::hash(serialized) << '\n';
}

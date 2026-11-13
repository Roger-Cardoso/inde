#include "comic_model.hpp"
#include "inde/application/proofreading_service.hpp"
#include "inde/application/text_search.hpp"
#include "inde/project/writing.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <sys/resource.h>

namespace {
thread_local bool measuring{};
thread_local std::size_t allocations{}, allocated_bytes{};
}
void *operator new(std::size_t size) {
  if (void *p = std::malloc(size == 0 ? 1 : size)) {
    if (measuring) { ++allocations; allocated_bytes += size; }
    return p;
  }
  throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

namespace {
struct Dictionary final : inde::application::SpellingProvider {
  bool available() const noexcept override { return true; }
  std::string description() const override { return "Synthetic all-valid dictionary"; }
  bool check(const std::string &) const override { return true; }
  std::vector<std::string> suggest(const std::string &, std::size_t) const override { return {}; }
};
template<class F> void measure(const char *name, F operation) {
  const auto expected = operation(); // Warm caches outside the measured interval.
  std::vector<double> times;
  times.reserve(5);
  std::size_t total_allocations{}, total_bytes{};
  for (int i = 0; i < 5; ++i) {
    allocations = allocated_bytes = 0;
    measuring = true;
    const auto start = std::chrono::steady_clock::now();
    const auto result = operation();
    const auto end = std::chrono::steady_clock::now();
    measuring = false;
    if (result != expected) throw std::runtime_error("Non-deterministic benchmark");
    times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    total_allocations += allocations;
    total_bytes += allocated_bytes;
  }
  std::sort(times.begin(), times.end());
  std::cout << name << " median_ms=" << times[2]
            << " cpp_allocations_mean=" << total_allocations / 5
            << " cpp_requested_bytes_mean=" << total_bytes / 5
            << " result=" << expected << '\n';
}
}
int main() {
  std::string text;
  text.reserve(1024 * 1024 + 256);
  while (text.size() < 1024 * 1024)
    text += "A escritora observa a cidade e reescreve a cena. Cada parágrafo preserva contexto, ritmo e intenção.\n";
  measure("search_ascii_casefold", [&] {
    return inde::application::find_text(text, {"ESCRITORA", false, true}).matches.size();
  });
  measure("search_unicode_casefold", [&] {
    return inde::application::find_text(text, {"INTENÇÃO", false, true}).matches.size();
  });
  measure("text_statistics", [&] { return inde::project::analyze_document_text(text).words; });
  Dictionary dictionary;
  inde::application::ProofreadingAnalyzer analyzer(dictionary);
  measure("proofreading_warm", [&] { return analyzer.analyze(text, {}).size(); });
  auto book = inde::comic::example("synthetic-work");
  book.panels.clear();
  for (int i = 0; i < 80; ++i) {
    inde::comic::Panel p;
    p.id = "panel_" + std::to_string(i);
    p.title = "Um quadro";
    for (int j = 0; j < 16; ++j) {
      inde::comic::Element e;
      e.id = "element_" + std::to_string(i) + "_" + std::to_string(j);
      e.text = "O texto continua vivo no navegador. 日本語 العربية 한국어";
      p.elements.push_back(std::move(e));
    }
    book.panels.push_back(std::move(p));
  }
  measure("comic_validation", [&] { inde::comic::validate(book); return book.panels.size(); });
  measure("comic_json_roundtrip", [&] {
    return inde::comic::decode(inde::comic::encode(book)).panels.size();
  });
  std::vector<inde::project::StructuralNode> nodes;
  for (int i = 0; i < 10000; ++i) {
    inde::project::StructuralNode n;
    n.id = "node-" + std::to_string(i);
    n.title = "Uma unidade editorial com título longo para a medição";
    n.position = i;
    nodes.push_back(std::move(n));
  }
  measure("production_order_10000", [&] {
    return inde::application::production_order(nodes).size();
  });
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  std::cout << "max_rss_kib=" << usage.ru_maxrss << '\n';
}

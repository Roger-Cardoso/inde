#include "inde/application/proofreading_service.hpp"
#include "inde/persistence/dictionary_store.hpp"
#include "inde/persistence/enchant_spelling_provider.hpp"
#include "inde/project/manifest.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

int main() {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("inde-proofreading-benchmark-" + inde::project::new_uuid() + ".json");
  inde::persistence::JsonDictionaryStore store(path);
  inde::persistence::EnchantSpellingProvider spelling;
  inde::application::ProofreadingService service(store, spelling);

  std::string text;
  constexpr std::size_t paragraphs = 2500;
  const std::string paragraph =
      "Esta é uma frase em português brasileiro, com personagens e lugares. "
      "A narrativa preserva a voz artística do autor.\n";
  text.reserve(paragraph.size() * paragraphs);
  for (std::size_t index = 0; index < paragraphs; ++index)
    text += paragraph;

  const auto run = [&] {
    const auto started = std::chrono::steady_clock::now();
    const auto issues = service.analyze(text, 1000);
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started);
    return std::pair{issues.size(), elapsed.count()};
  };
  const auto cold = run();
  const auto warm = run();
  std::cout << "provider=" << service.provider_description() << '\n'
            << "characters=" << text.size() << " issues=" << warm.first
            << " cold_ms=" << cold.second << " warm_ms=" << warm.second << '\n';
  std::error_code error;
  std::filesystem::remove(path, error);
}

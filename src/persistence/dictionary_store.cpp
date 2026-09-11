#include "inde/persistence/dictionary_store.hpp"

#include "inde/persistence/json.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace inde::persistence {
namespace {

std::string read_all(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("Não foi possível abrir " + path.string());
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void write_atomic(const std::filesystem::path &path, const std::string &data) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    throw std::runtime_error(
        "Não foi possível criar a pasta dos dicionários: " + error.message());
  const auto temporary = path.string() + ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
      throw std::runtime_error("Não foi possível gravar os dicionários");
    output << data;
    output.close();
    if (!output) {
      std::filesystem::remove(temporary, error);
      throw std::runtime_error("A gravação dos dicionários ficou incompleta");
    }
  }
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    throw std::runtime_error("Não foi possível promover os dicionários: " +
                             error.message());
  }
}

} // namespace

JsonDictionaryStore::JsonDictionaryStore(std::filesystem::path storage_path)
    : storage_path_(std::move(storage_path)) {}

std::vector<project::LexicalDictionary> JsonDictionaryStore::load() const {
  if (!std::filesystem::exists(storage_path_))
    return {};
  const auto root = json::parse(read_all(storage_path_));
  if (root.at("format").as_string() != "INDE dictionaries" ||
      root.at("version").as_integer() != 1)
    throw std::runtime_error("Formato de dicionários do INDE incompatível");

  std::vector<project::LexicalDictionary> result;
  for (const auto &item : root.at("dictionaries").as_array()) {
    const auto &object = item.as_object();
    project::LexicalDictionary value;
    value.id = item.at("id").as_string();
    value.name = item.at("name").as_string();
    value.language = item.at("language").as_string();
    value.category = project::DictionaryCategory::User;
    value.enabled = item.at("enabled").as_boolean();
    for (const auto &word : item.at("words").as_array())
      value.words.push_back(word.as_string());
    if (value.id.empty() || value.name.empty() || value.language.empty())
      throw std::runtime_error("Dicionário pessoal possui campos vazios");
    result.push_back(std::move(value));
    static_cast<void>(object);
  }
  return result;
}

void JsonDictionaryStore::save(
    const std::vector<project::LexicalDictionary> &dictionaries) const {
  json::Value::Array encoded;
  for (const auto &dictionary : dictionaries) {
    if (dictionary.category != project::DictionaryCategory::User)
      continue;
    json::Value::Array words;
    for (const auto &word : dictionary.words)
      words.emplace_back(word);
    encoded.emplace_back(json::Value::Object{
        {"enabled", dictionary.enabled},
        {"id", dictionary.id},
        {"language", dictionary.language},
        {"name", dictionary.name},
        {"words", std::move(words)},
    });
  }
  write_atomic(storage_path_, json::serialize(json::Value::Object{
                                  {"dictionaries", std::move(encoded)},
                                  {"format", "INDE dictionaries"},
                                  {"version", 1},
                              }));
}

std::filesystem::path JsonDictionaryStore::default_path() {
  if (const char *config = std::getenv("XDG_CONFIG_HOME"))
    return std::filesystem::path(config) / "inde" / "dictionaries.json";
  if (const char *home = std::getenv("HOME"))
    return std::filesystem::path(home) / ".config" / "inde" /
           "dictionaries.json";
  return std::filesystem::temp_directory_path() / "inde-dictionaries.json";
}

} // namespace inde::persistence

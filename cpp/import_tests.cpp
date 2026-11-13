#include "publication_service.hpp"
#include <fstream>
#include <iostream>
using namespace inde;
using namespace inde::publication;
namespace fs = std::filesystem;
static std::string read(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Use o Projeto de uma fixture importada");
    fs::path path = argv[1];
    const auto before = hash(read(path / "data/project.sqlite3"));
    application::ProjectService source;
    source.open(path);
    Service pub(source);
    const auto receipt = persistence::json::parse(
        read(path / "data/importacao-literature.json"));
    Site s;
    s.name = "Conferência";
    s.author = "Autoria sintética";
    s.description = "Conferência de importação";
    s = pub.create_site(s);
    Edition e;
    e.site_id = s.id;
    e.title = "Edição importada";
    e.slug = "importada";
    e.work_id = receipt.at("work_id").as_string();
    e.structure_id = source.structures().active_editorial(e.work_id)->id;
    for (const auto &d : source.writing().documents())
      if (d.editorial_node_id)
        e.choices[*d.editorial_node_id].documents.push_back(d.id);
    e = pub.create_edition(e);
    bool blocked = false;
    try {
      pub.prepare(e.id, s.revision, e.revision);
    } catch (const std::exception &error) {
      blocked = std::string(error.what()).find("identidades públicas") !=
                std::string::npos;
    }
    if (!blocked || !pub.preparations(e.id).empty())
      throw std::runtime_error("A pendência pública não bloqueou a preparação");
    source.close();
    if (hash(read(path / "data/project.sqlite3")) != before)
      throw std::runtime_error("Conferir a publicação alterou o banco autoral");
    std::cout << "PUB-03A: identidades públicas pendentes bloqueiam a "
                 "publicação, preservando o banco autoral OK\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

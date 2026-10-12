#include "inde/application/project_service.hpp"
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  try {
    std::filesystem::path path(argv[1]);
    if (!path.is_absolute() || path.extension() != ".inde" || std::filesystem::exists(path) ||
        !std::filesystem::is_directory(path.parent_path()))
      throw std::runtime_error("Fixture exige destino absoluto novo .inde com pai existente; não sobrescreve");
    inde::application::ProjectService service;
    service.create(path, "Projeto vazio — laboratório de blocos sintético");
    if (service.writing().document_count() != 0) throw std::runtime_error("Fixture não vazia");
    service.close(); std::cout << "PASS: new synthetic empty fixture, no original modified\n"; return 0;
  } catch (const std::exception &e) { std::cerr << "FAIL fixture: " << e.what() << '\n'; return 1; }
}

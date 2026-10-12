#include "demo_project/demo_builder.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Uso: inde_demo_project_generator CAMINHO_DO_PROJETO.inde\n";
    return EXIT_FAILURE;
  }
  try {
    const auto output = std::filesystem::absolute(argv[1]).lexically_normal();
    inde::demo::DemoBuilder(output).build();
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "Falha ao criar projeto demonstrativo: " << error.what()
              << '\n';
    return EXIT_FAILURE;
  }
}

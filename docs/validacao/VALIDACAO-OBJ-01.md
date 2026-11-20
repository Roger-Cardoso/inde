# INDE-OBJ-01 — Relatório de validação inicial

Data: 2026-11-20. Escopo: **modelo em memória C++20 isolado**.

Comando executado em ambiente local de teste:

```sh
sh tests/run_object_model_tests.sh
```

Compilação com GCC 14.2.0, C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`, resultado:

```text
INDE-OBJ-01: 7 test groups passed
```

Os sete grupos abrangem grafo aninhado válido e reutilização de objeto, referências inválidas, identidades/ocorrências duplicadas ou órfãs, ciclos, geometria/grades, metadados de recursos e campos incompatíveis com o tipo. O alvo `inde_object_model_tests` também foi adicionado ao CMake e registrado com CTest quando `INDE_BUILD_CORE_TESTS` estiver habilitado.

**Não executados:** compilação GTK/CTest completo (gtkmm-4.0 indisponível no ambiente de validação), extração e inspeção integral do tar.gz schema 37, migradores SQLite, edição visual, undo/recovery do aplicativo, publicação HTML e integração de demos. Não houve modificação de schema nem migração de projetos. Resultados INT-01 anteriores não foram reexecutados.

**Aceite desta etapa:** válido somente como fundação de modelagem em memória; integração à persistência e migração exigem auditoria adicional e testes sobre projetos sintéticos.

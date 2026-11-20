# INDE-OBJ-01 — Relatório de validação inicial

Data: 2026-11-20. Escopo: **modelo em memória C++20 isolado**.

Comando executado em ambiente local de teste:

```sh
sh tests/run_object_model_tests.sh
```

Compilação independente com GCC 14.2.0, C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`. O validador reforçado foi ensaiado localmente em execução normal e com AddressSanitizer/UndefinedBehaviorSanitizer, ambos com 13 grupos aprovados. A suíte correspondente foi registrada na branch, assim como um workflow isolado de CI para execução em GitHub Actions. Resultado do ensaio:

```text
INDE-OBJ-01: 13 test groups passed
```

A entrega reforçada cobre **13 grupos de testes**: grafo válido e reuso de objetos; referências inválidas; identidades e ocorrências; ciclos; geometria; metadados; campos incompatíveis; múltiplos pais; layouts inalcançáveis; tipos desconhecidos; metadados e geometria adversariais; determinismo; e uma cadeia com 7.000 contêineres. Para reexecutar com sanitizadores: `SANITIZE=1 sh tests/run_object_model_tests.sh`. O alvo `inde_object_model_tests` também foi adicionado ao CMake e registrado com CTest quando `INDE_BUILD_CORE_TESTS` estiver habilitado.

**Não executados nesta etapa:** compilação GTK/CTest completo (gtkmm-4.0 indisponível no ambiente de validação), extração e inspeção integral do tar.gz schema 37, migradores SQLite, edição visual, undo/recovery do aplicativo, publicação HTML e integração de demos. Não houve modificação de schema nem migração de projetos. Resultados INT-01 anteriores não foram reexecutados.

Esses testes não demonstram resistência a documentos arbitrariamente grandes ou maliciosos nem verificam hashes contra bytes, pois o modelo atual mantém apenas metadados. O relatório não substitui resultados do workflow remoto até sua conclusão.

**Aceite desta etapa:** válido somente como fundação de modelagem em memória; integração à persistência e migração exigem auditoria adicional e testes sobre projetos sintéticos.

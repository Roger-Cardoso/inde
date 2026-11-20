# OBJ-01 — Validação da ponte somente de leitura do Documento

Data: 2026-11-20. Branch `feature/inde-obj-01-foundation`, ponto de partida `73dee10344535aedaeaa5d267d152e76329b53cf`. Corte de compatibilidade da Fase 1: `object_document_adapter.{hpp,cpp}`, duas suítes de teste e build mínimo do domínio/codec real. Sem escrita de volta, integração visual, alterações de schema ou conversão de projetos reais.

Código validado remotamente: `bdaa349f3d5b3c23050188f0a936b14aca8aa7dc`. O commit posterior acrescenta documentação/evidências; não muda a implementação. Os logs, resumo estruturado e hashes ficam em [evidências](evidencias-obj01-leitura/resultados.json), com [SHA256SUMS](evidencias-obj01-leitura/SHA256SUMS), conferíveis dessa pasta usando `sha256sum -c SHA256SUMS`.

## Escopo e autoridade

Foi implementada uma leitura experimental de `project::Document` para o grafo OBJ-01, com snapshot integral privado, IDs qualificados por projeto/documento/parágrafo, mapeamento de payload/offsets Unicode, grupos de leitura existentes e verificação de atualização de toda a fonte. O grafo e sua origem capturada são somente leitura; ensaios usam uma cópia independente com `CompositionEditor`.

Não se criou uma segunda autoridade de texto. O adaptador não contém operações de abrir/salvar projeto, SQLite, GTK, publicação ou migração; recebe um Documento já carregado. A identidade de projeto é fornecida pelo chamador, não comprovada por esta API. A verificação de atualização compara dados fornecidos em memória e não substitui a guarda dentro de uma futura transação de escrita.

O aplicativo `inde-especial` e a biblioteca de publicação não foram ligados ao adaptador. O schema permanece 37. O teste de integração usa serviços e SQLite reais exclusivamente em um diretório temporário novo, com perfil isolado, sem abrir projeto autoral. A criação/migração de fixture sintética pelo serviço existente não é alteração de migradores ou de projetos reais.

## Resultados locais

| Verificação | Resultado | Evidência |
| --- | --- | --- |
| Build mínimo, GCC 15.2.0, avisos como erros | 9 grupos aprovados | [gcc.txt](evidencias-obj01-leitura/gcc.txt) |
| Build mínimo, Clang 18.1.8, adaptador/testes rigorosos | 9 grupos aprovados | [clang.txt](evidencias-obj01-leitura/clang.txt) |
| ASan/UBSan, incluindo as 18 unidades do domínio/codec usadas pelo build mínimo | 9 grupos aprovados; LSan local desativado pelo ambiente com ptrace | [asan-ubsan.txt](evidencias-obj01-leitura/asan-ubsan.txt) |
| Ponte ligada ao núcleo completo e teste de repositório SQLite/Composição | 2/2 programas aprovados | [native-focal.txt](evidencias-obj01-leitura/native-focal.txt) |
| CTest completo do build nativo | **51/51 aprovados**, em 145,85 s | [native-ctest.txt](evidencias-obj01-leitura/native-ctest.txt) |
| CI ampliada da branch | **9/9 jobs aprovados**, incluindo LSan remoto | [execução 44765626359](https://github.com/Roger-Cardoso/inde/actions/runs/44765626359), [ci.json](evidencias-obj01-leitura/ci.json) |

Os 9 grupos da ponte cobrem:

1. Unicode, emojis, parágrafos vazios, LF final, texto repetido e renderização contínua exata.
2. Conservação integral de todos os campos do Documento, incluindo formatação, âncoras, referências, comandos inline, semântica, vínculos temporais/datas, metadados, parágrafos aposentados, folhas cobertas e perfis.
3. Captura independente: modificar metadados, prosa ou uma alocação mutável de folha coberta do chamador não modifica o snapshot.
4. IDs determinísticos, distinção entre parágrafos com a mesma prosa e qualificação por projeto/documento.
5. Verificação de fonte: mudanças apenas em título, timestamp, estilo, perfil, vínculo, semântica, folha privada ou IDs aposentados invalidam o snapshot; fonte/projeto/documento incompatíveis são recusados.
6. Recusas sem projeção parcial: falta de IDs, partição não canônica, IDs duplicados, corpo divergente, UTF-8 inválido e NUL.
7. Limites menores de parágrafos/strings/snapshot, limites inválidos, aceitação integral de 10.000 parágrafos e recusa de 10.001, sem truncamento.
8. Resma de várias linhas usando `reading_blocks`; reordenar uma cópia do grafo não muda prosa, resma ou snapshot de origem; undo de objetos continua independente.
9. **250 ordens de parágrafos geradas**, com seed fixa, conservando bindings por ID em vez de semelhança textual ou offsets.

O teste de repositório cria Documento formatado com âncora/perfil e Composição persistida com origem congelada, ocorrência live, pinned, reference-only e rascunho durável não aplicado. Antes/depois da leitura, reordenação/mudança de arranjo no grafo copiado e undo, compara **todos os arquivos do projeto byte a byte**. Verifica que Documento, revisão/hash/payload da Composição, perfis, rascunho e origens continuam iguais; executa `integrity_check`, `foreign_key_check` e confere schema 37. Depois simula outro escritor apenas na fixture, muda o título e comprova conflito da captura sem sobrescrever a fonte atualizada.

Esse teste demonstra conservação das autoridades existentes pelo adaptador; não demonstra tradução da Composição para objetos nem transação de escrita OBJ-01. A Composição ainda exige ponte própria.

## Compiladores, construção e exceção localizada

A fundação independente `tests/object_model` continua sem bibliotecas de domínio. O novo `tests/object_document` usa GLib/UUID e 18 unidades reais de domínio/codec do arquivo integrado, com hashes conferidos antes da configuração. Não configura GTK/SQLite. A receita existente de correções de diagnósticos é aplicada apenas à cópia do build.

A primeira compilação Clang encontrou `-Wsign-conversion` no código congelado preexistente (`writing.cpp`, percurso de bytes UTF-8), porque Clang inclui esse aviso em `-Wconversion`. O build mínimo recusa todos os demais avisos e aplica **somente ao alvo dos fontes antigos** `-Wno-sign-conversion` no Clang. O novo adaptador, a fundação e seus testes mantêm `-Wall -Wextra -Wpedantic -Wconversion -Werror` também no Clang. Não se alterou o pacote para contornar o diagnóstico, nem se declara que todo o legado passou sem essa exceção.

O build nativo completo reutiliza o núcleo Release previamente compilado com GCC, `INDE_BUILD_CORE_TESTS=ON`, `INDE_STRICT_WARNINGS=ON` e LTO desligado. Só o novo alvo e os testes precisaram ser compilados. A suíte completa foi reexecutada depois; o resultado anterior de 49 programas não foi reutilizado como validação dos dois novos programas.

## Reprodução

Build mínimo e sanitizadores:

```sh
python3 verificar_base.py --integrado --extrair
cmake -S tests/object_document -B build-obj01-document -G Ninja
cmake --build build-obj01-document --parallel 2
ctest --test-dir build-obj01-document --output-on-failure

cmake -S tests/object_document -B build-obj01-document-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-obj01-document-clang --parallel 2
ctest --test-dir build-obj01-document-clang --output-on-failure

cmake -S tests/object_document -B build-obj01-document-asan -G Ninja \
  -DOBJ01_SANITIZE=ON
cmake --build build-obj01-document-asan --parallel 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-obj01-document-asan --output-on-failure
```

O `detect_leaks=0` corresponde à limitação local sob ptrace já registrada na auditoria. A CI pede `detect_leaks=1`; os resultados remotos são registrados separadamente. Os sanitizadores do build mínimo instrumentam as unidades realmente incluídas; não equivalem a instrumentação do aplicativo completo.

Integração com o núcleo completo, a partir do build nativo preparado conforme a auditoria:

```sh
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_BUILD_CORE_TESTS=ON -DINDE_ENABLE_LTO=OFF
cmake --build build-nativo --target inde_object_document_adapter_tests \
  inde_object_document_service_tests --parallel 2
ctest --test-dir build-nativo --output-on-failure
```

A CI acrescenta quatro combinações GCC/Clang × sem/com ASan/UBSan ao conjunto anterior. Os jobs do adaptador usam o build mínimo; o teste de SQLite/Composição e as 51 verificações são evidência local do build completo. Não se atribui ao CI mínimo cobertura de banco/GTK que ele não possui.

## Limites e requisitos seguintes

Não foram reexecutados neste corte os 92 testes Python nem os percursos GTK: nenhuma chamada do adaptador foi inserida nesses fluxos, e os resultados anteriores permanecem históricos. O teste novo de repositório e a suíte CTest atual constituem a regressão pertinente desta alteração de núcleo. O driver histórico P03b e a ocorrência inicial PUB-02 continuam descritos na [auditoria anterior](VALIDACAO-OBJ-01.md), sem serem promovidos a testes aprovados por esta entrega.

Os limites de 10.000 parágrafos, 8 MiB de strings e 16 MiB de snapshot são limites explícitos da ponte, não aumento/redução dos limites do Documento. Preflight não é garantia de RSS, latência interativa, resistência a todo input hostil ou recuperação de falta de memória. Não foi implementado parser de arquivos, cache de projeções, seleção/filtragem visual, writeback, aplicação autoral de comandos, recovery OBJ-01, recursos de mídia ou exportação.

O caminho escolhido é completar a compatibilidade antes da integração visual: próximo corte próprio para `StoredComposition`, depois plano de mudanças com distinção visual/autoral e guarda transacional. Não é seguro usar o novo snapshot/stamp como autorização para gravar ou converter automaticamente a ordem de objetos em ordem de texto.

Ver [contrato da ponte](../arquitetura/ADAPTADOR-DOCUMENTO-OBJ-01.md). O relatório e os logs não substituem aceite humano nem promovem uma nova fase visual ou de persistência.

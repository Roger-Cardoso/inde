# OBJ-01 — Validação da ponte de leitura da Composição persistida

Data: 2026-11-20. Branch `feature/inde-obj-01-foundation`. Base: `6f4c216785a0d37c89d7392ff6e85f1b9e002beb`. Implementação: `0ca67284d0e087a782939e64dcbf81e3d9b0f60f`. Corte experimental de compatibilidade da Fase 1: leitura do agregado real, sem aplicação de rascunhos, escrita de volta, migração, GTK ou novo formato de projeto.

Os commits posteriores deste corte acrescentam documentação/evidências e não mudam a implementação validada. O [resumo estruturado](evidencias-obj01-composicao/resultados.json) e os logs possuem [SHA256SUMS](evidencias-obj01-composicao/SHA256SUMS), verificáveis a partir dessa pasta com `sha256sum -c SHA256SUMS`. São checksums de conteúdo, não atestação externa.

## O que foi implementado e conservado

O novo adaptador recebe `project::StoredComposition`, confere envelope/revisão/hash, usa o codec e a validação existentes, cria captura profunda própria e mapeia todas as ocorrências para o grafo. Conserva blocos/revisões sem uso ativo, origem congelada integral, faixas exatas, avisos de camadas, bifurcações, modos live/pinned/forked/reference-only, variantes/famílias/camadas, perfil e rascunhos não aplicados.

O grafo usa os tipos experimentais novos TextBlock e TextReference. Bloco de várias linhas não é tratado como parágrafo. Objetos de captura são qualificados por bloco/revisão; ocorrências preservam identidade derivada da colocação, independentemente da revisão que Live segue. O bloco autoritativo original permanece explícito no vínculo.

A leitura atual é a projeção exata do núcleo sobre o perfil capturado; não recebe separadores inventados. Referência visível tem largura zero. Itens ocultos/excluídos permanecem registrados no grafo, mas não são incorporados pelo resolver de texto. A inspeção explícita de revisão oculta e o snapshot completo são privados de autoria, não controle de acesso nem formato de publicação.

O preflight de strings/camadas de Documento foi extraído para um helper interno compartilhado. A ponte documental continua exigindo parágrafos canônicos e seus limites anteriores; a nova ponte aceita os Documentos de origem/camadas que o codec da Composição suporta, sem inventar parágrafos. Recodificação divergente é erro, não normalização silenciosa.

## Resultados observados

| Verificação | Resultado | Evidência |
| --- | --- | --- |
| Build mínimo com GCC 15.2.0 | 11 grupos da Composição + 9 do Documento aprovados, 2/2 programas | [gcc.txt](evidencias-obj01-composicao/gcc.txt) |
| Build mínimo com Clang 18.1.8 | 2/2 programas aprovados, 20 grupos | [clang.txt](evidencias-obj01-composicao/clang.txt) |
| ASan/UBSan locais, adaptadores e 25 unidades reais do domínio/codec | 2/2 aprovados; LSan local desativado sob ptrace | [asan-ubsan.txt](evidencias-obj01-composicao/asan-ubsan.txt) |
| Fundação, comandos, duas pontes e seus serviços, ligados ao núcleo completo | 6/6 programas aprovados | [native-focal.txt](evidencias-obj01-composicao/native-focal.txt) |
| CTest nativo completo | **53/53 aprovados**, em 183,21 s | [native-ctest.txt](evidencias-obj01-composicao/native-ctest.txt) |
| CI ampliada | **13/13 jobs aprovados**, incluindo LSan remoto | [execução 44770360362](https://github.com/Roger-Cardoso/inde/actions/runs/44770360362), [ci.json](evidencias-obj01-composicao/ci.json) |

Os 11 grupos novos verificam:

1. Revisão corrente versus fixada, bifurcação explícita, referência sem payload, tipos corretos e reutilização somente da mesma versão incorporada.
2. Leitura idêntica à produzida pelo proprietário, inclusive segmentos, coordenadas Unicode, faixas exatas e referências de largura zero; separação entre ocultação declarada, camada, variante global e família selecionada.
3. Igualdade exata do agregado pela recodificação, conservação de origens sem IDs de parágrafo, histórico/avisos e rascunho privado fora da leitura.
4. Independência de uma alocação mutável de Documento de revisão retida pelo chamador; a captura não acompanha mutação posterior.
5. Atualização: revisão diferente com hash igual, timestamp, nome, perfil e rascunho alterados; hash incompatível; outro projeto/Composição.
6. Recusas sem projeção parcial: envelope inválido, hash falso, origem ausente, revisão de rascunho inexistente, UTF-8 inválido e NUL.
7. Limites menores/ilegais, Composição vazia, **2.048 ocorrências intactas**, um único objeto reutilizado para o mesmo bloco vazio e distinção entre texto vazio e ausência; recusa sem truncamento acima do limite escolhido.
8. Bloco de biblioteca sem ocorrência e alternativa privada na origem integral permanecem na captura sem ganhar colocação artificial.
9. Reordenação, mudança de arranjo e undo em uma cópia do grafo deixam fonte, leitura congelada, revisão/hash e rascunho inalterados.
10. Ao avançar a revisão Live, a ocorrência mantém identidade, o objeto da captura muda de versão e a revisão Pinned continua a mesma; a captura anterior fica imutável e detecta desatualização.
11. **200 ordens de ocorrências geradas**, com seed fixa, preservando bindings por IDs/revisão, comparando leitura e segmentos com o núcleo e comprovando ausência de mutação.

Esses resultados não equivalem a prova formal, cobertura de todos os inputs ou benchmark de latência. A geração de ordens usa fixtures declaradas, não interpretação automática de conteúdo autoral.

## Prova com serviços e SQLite reais

`inde_object_composition_service_tests` cria um projeto temporário novo e perfil isolado, por meio dos serviços existentes. A fixture contém Documento com Unicode, formatação, resma/folha coberta e lente; Composição com todos os modos, bifurcação, alternativa de família, revisão aplicada e rascunho privado persistido.

Carrega o agregado pelo repositório real e passa-o ao novo adaptador. Antes/depois da captura, reordenação/mudança de arranjo em uma cópia do grafo e undo, compara **os bytes de todos os arquivos presentes no projeto**. Também confere igualdade do Documento, revisão/hash/payload integral da Composição, schema 37, `PRAGMA integrity_check` e `foreign_key_check`.

Depois altera apenas o título do Documento de origem na fixture e verifica que a captura da Composição continua atual: Live segue bloco, não Documento original posterior. Em seguida salva um novo rascunho por outro fluxo do serviço, na mesma fixture, e verifica conflito da revisão da Composição; a captura antiga continua intacta. As escritas concorrentes simuladas são sequenciais e sintéticas, não um ensaio de corrida entre threads/processos.

Não foi aberto/migrado projeto autoral, nem alterado codec, migrador, pacote de fontes, banco de produção ou `.inde` real. A criação da fixture usa o mecanismo de schema existente em diretório novo; não representa conversão de obras. A `main` permanece em `d41a5c9`, e sua alteração preexistente de permissão em `Preparar-ponte.sh` foi preservada.

## Construção e reprodução

```sh
python3 verificar_base.py --integrado --extrair
cmake -S tests/object_composition -B build-obj01-composition -G Ninja
cmake --build build-obj01-composition --parallel 2
ctest --test-dir build-obj01-composition --output-on-failure

cmake -S tests/object_composition -B build-obj01-composition-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-obj01-composition-clang --parallel 2
ctest --test-dir build-obj01-composition-clang --output-on-failure

cmake -S tests/object_composition -B build-obj01-composition-asan -G Ninja \
  -DOBJ01_SANITIZE=ON
cmake --build build-obj01-composition-asan --parallel 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-obj01-composition-asan --output-on-failure
```

O build mínimo estende o documental e compila 25 unidades reais do pacote verificado, sem configurar GTK/SQLite. A transformação preexistente de correções de diagnósticos só atinge a cópia do build. A exceção Clang para `-Wsign-conversion` nos fontes antigos continua restrita àquele alvo; adaptadores, helper, modelo e testes novos mantêm `-Wall -Wextra -Wpedantic -Wconversion -Werror`. Não se declara que o código antigo passou no Clang sem essa exceção.

A primeira ligação do build mínimo apontou a dependência real `text_delta` do motor de conservação de camadas. Foi incluída a unidade proprietária `text_edit_delta.cpp`, sem mock ou reimplementação, e o build completo mínimo foi reexecutado com sucesso. Essa dependência não torna o adaptador uma API de escrita: só os testes preparam estados por meio das operações existentes.

Para a prova de repositório, no build nativo completo preparado conforme os cortes anteriores:

```sh
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_BUILD_CORE_TESTS=ON -DINDE_ENABLE_LTO=OFF
cmake --build build-nativo --target inde_object_composition_adapter_tests \
  inde_object_composition_service_tests inde_object_document_adapter_tests \
  inde_object_document_service_tests inde_object_model_tests inde_object_commands_tests --parallel 2
ctest --test-dir build-nativo --output-on-failure
```

O núcleo do build completo é Release não instrumentado; os sanitizadores instrumentam os adaptadores e as unidades do build mínimo. O LSan local precisa de `detect_leaks=0` devido ao ambiente sob ptrace, conforme os relatórios anteriores. A CI pede `detect_leaks=1` e registra seu resultado separadamente. A CI mínima não executa o teste SQLite; a cobertura de repositório é local.

## Limites e trabalho seguinte

As quotas seguem o agregado existente: 256 blocos, 2.048 ocorrências, 4.096 revisões, snapshot até 2 MiB, strings até 8 MiB e 100.000 entradas de coleções do preflight. Podem ser reduzidas, não ampliadas nesta API. Nada é truncado. Não são quotas de RSS; o codec e a leitura do núcleo podem alocar temporários e a projeção textual tem limite próprio de 32 MiB.

Não foram reexecutados os 92 testes Python nem os percursos GTK, porque o adaptador não foi ligado à interface ou publicação. Foram reexecutados os programas CTest pertinentes, incluindo todas as regressões anteriores do núcleo. As ocorrências históricas P03b/PUB-02 continuam no relatório da auditoria; não são promovidas por estes resultados.

`check_source` verifica apenas o agregado fornecido, não o estado atual do banco, autorização ou uma transação de escrita. A captura privada não é pacote público. Não há aplicação de rascunhos, tradução de comandos do grafo em comandos autorais, alteração de origens, writeback, recuperação durável OBJ-01, mídia incorporada ou migração física.

As duas pontes estão prontas para o próximo requisito: **um plano explícito de alterações**, distinguindo apresentação visual de mudança autoral, com mapeamento de IDs, diagnóstico de operações não traduzíveis e delegação ao motor existente. Nenhuma escrita deve ser liberada só porque a árvore de objetos é válida ou o stamp conferiu fora de transação.

Ver [contrato da ponte](../arquitetura/ADAPTADOR-COMPOSICAO-OBJ-01.md). Evidências e hashes verificáveis estão nesta pasta; aprovação técnica não substitui aceite humano nem promove GTK ou arquivo único `.inde`.

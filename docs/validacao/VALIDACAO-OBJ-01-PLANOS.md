# OBJ-01 — Validação dos planos imutáveis

Data: 2026-11-20. Branch `feature/inde-obj-01-foundation`. Base: `5f2cff75e5bc9d03a050ac2d1d37ed473e6b47bf`. Corte da Fase 1: plano privado em memória com intenção explícita, fontes congeladas e tradução conservadora. Sem aplicação persistente, GTK, migração ou novo formato de projeto.

## Contratos ensaiados

Os dez grupos de testes novos cobrem os seguintes comportamentos:

1. O mesmo pedido de reordenação produz o mesmo grafo visual, mas somente a intenção autoral modifica o candidato textual. A fonte fornecida não muda. O candidato é comparado ao resultado exato do motor `move_reading_block`, inclusive seleção Unicode.
2. Resma de várias linhas exige seleção do grupo inteiro. Parte de grupo e destino no interior de outro grupo são recusados. Seleção explícita completa em ordem inversa conserva a ordem relativa original, IDs e folhas privadas.
3. A recusa do motor para inversão de intervalo marcado é conservada com diagnóstico, sem candidato parcial. Intenção visual pode reordenar o grafo mantendo o Documento intacto.
4. No-op mantém Documento e seleção; trocar parágrafos de prosa idêntica ainda muda a sequência de identidades e o hash.
5. Intenção desconhecida, comandos autorais sem tradução, lote documental autoral múltiplo, comando incompatível com arranjo, seleção Unicode inválida e quotas de comandos/IDs/bytes são recusados.
6. O plano documental sobrevive à projeção. Os estados anterior/candidato são ensaiados na sessão de edição proprietária: undo/redo restauram snapshots exatos em memória. Mudança posterior somente no título invalida o stamp e impede novo plano sobre a captura antiga.
7. Reordenação em lote da Composição conserva o agregado integral, exceto pela ordem; restaurar a ordem original reproduz os bytes originais. Intenção visual mantém a fonte inalterada. Leitura do candidato é a do núcleo, sem rascunho aplicado e sem fabricar revisão persistida futura.
8. Mover ocorrência oculta muda a ordem autoral mesmo sem mudar o texto visível. Revisão isolada e rascunho alterado invalidam a fonte. Arranjo autoral sem tradução não recebe fallback visual.
9. **150 movimentos documentais gerados**, seed fixa: candidatos comparados ao motor real, fonte inalterada antes de cada próximo estado sintético.
10. **150 movimentos da Composição gerados**, seed fixa: permutação comparada a um oráculo de sequência independente e payload completo ao codec proprietário.

As fixtures incluem Unicode, marcações, formatação, resma e folha privada, IDs de parágrafo, modos live/pinned/forked/reference-only, ocorrências ocultas, revisão aplicada, alternativa de família e rascunhos privados. A geração de movimentos usa estados sintéticos declarados; não infere narrativa ou significado.

## Prova com serviços e SQLite

`inde_object_change_plan_service_tests` cria um projeto novo em diretório temporário exclusivo e perfil isolado. Usa `ProjectService` real para persistir Documento e Composição com todos os modos, revisão aplicada e rascunho. Somente essa preparação e os escritores sintéticos posteriores gravam dados.

Depois carrega as fontes pelo repositório, captura as duas pontes e produz planos documentais visual/autoral e plano autoral da Composição, incluindo movimento de ocorrência oculta. Confere os candidatos, a seleção pelo motor, o conteúdo não aplicado do rascunho e a conservação de todos os demais campos. Destrói as projeções e confere que os planos continuam independentes.

**Todos os arquivos presentes no projeto são comparados byte a byte antes/depois do planejamento e de `check_source`.** Documento, hash/revisão/payload da Composição permanecem iguais. O teste verifica schema 37, `PRAGMA integrity_check` e `foreign_key_check`. Em seguida salva uma alteração de título documental e outra somente no rascunho da Composição, por fluxos do serviço, e exige conflito nos planos anteriores. Os snapshots dos planos permanecem congelados.

São escritores sequenciais em fixture, não corrida entre processos nem prova de escrita atômica. O planejador não possui API de save/apply. Nenhum projeto autoral é aberto ou migrado. Os arquivos compactados e o código proprietário permanecem inalterados.

## Reprodução

```sh
python3 verificar_base.py --integrado --extrair
cmake -S tests/object_change_plan -B build-obj01-plan -G Ninja
cmake --build build-obj01-plan --parallel 2
ctest --test-dir build-obj01-plan --output-on-failure

cmake -S tests/object_change_plan -B build-obj01-plan-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-obj01-plan-clang --parallel 2
ctest --test-dir build-obj01-plan-clang --output-on-failure

cmake -S tests/object_change_plan -B build-obj01-plan-asan -G Ninja \
  -DOBJ01_SANITIZE=ON
cmake --build build-obj01-plan-asan --parallel 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-obj01-plan-asan --output-on-failure
```

O build mínimo compila **26 unidades reais do proprietário**, modelo, comandos, duas pontes, planejador e três programas (9 grupos documentais, 11 da Composição e 10 de planos). Não configura GTK/SQLite. Os avisos rigorosos são erros no código novo. A exceção Clang de `-Wsign-conversion` é restrita ao alvo dos fontes antigos, como nos cortes anteriores. ASan/UBSan instrumentam também as unidades proprietárias do build mínimo. LSan local fica desativado devido ao ambiente sob ptrace; a CI pede `detect_leaks=1`.

No build nativo completo preparado conforme os cortes anteriores:

```sh
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_BUILD_CORE_TESTS=ON -DINDE_ENABLE_LTO=OFF
cmake --build build-nativo --target inde_object_change_plan_tests \
  inde_object_change_plan_service_tests --parallel 2
ctest --test-dir build-nativo -R inde_object_ --output-on-failure
ctest --test-dir build-nativo --output-on-failure
```

O núcleo nativo é Release sem sanitizadores; a prova SQLite é local, separada da matriz mínima da CI. A CI acrescenta quatro jobs de planos (GCC/Clang × sem/com ASan/UBSan/LSan) aos treze anteriores. A verificação dos três arquivos de fontes e os cinco testes de recusa do verificador continuam na CI.

## Fronteiras de aprovação

O plano compara a fonte fornecida em memória; não consulta a última revisão no banco, sincroniza threads, autentica o chamador ou fecha a janela entre checagem e gravação. Uma aplicação persistente futura exige contrato transacional próprio e integração com undo/recovery. Hash do candidato não é save, revisão futura nem aceite humano.

O suporte autoral documental é um grupo inteiro por plano; multi-grupo/lote é recusado. Composição admite somente reordenação na raiz. Arranjos, frames, pintura e transferência continuam visuais. Não houve conexão com GTK, alteração de publicação, mídia incorporada, unificação de bancos ou `.inde` único. Os 92 testes Python do aplicativo e percursos GTK não foram reexecutados neste corte, pois essas camadas não mudaram; os cinco testes Python específicos do verificador de fontes foram reexecutados.

O próximo requisito dentro da Fase 1 é especificar a aplicação controlada desses candidatos e sua relação com sessão/histórico, com conferência da fonte na autoridade correta. A validade do plano isolado não autoriza writeback.

Ver [contrato dos planos](../arquitetura/PLANO-ALTERACOES-OBJ-01.md), [inventário aprofundado](../arquitetura/INVENTARIO-OBJ-01.md) e [validação anterior](VALIDACAO-OBJ-01-COMPOSICAO.md). Resultados, commits e evidências verificáveis são registrados no complemento abaixo.

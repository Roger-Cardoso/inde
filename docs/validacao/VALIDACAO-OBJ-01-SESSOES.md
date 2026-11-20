# OBJ-01 — Validação da aplicação em sessões de memória

Data: 2026-11-20. Base: `b23a93dac441d6cd1397e4826fa815fa50d5513e`. Branch `feature/inde-obj-01-foundation`. Escopo da **Fase 1**: aplicar uma prévia explicitamente escolhida a uma autoridade privada em memória, com conferência da origem, revisão monotônica e histórico conjunto de fonte/grafo. Não há save, recovery durável, integração GTK ou migração.

## Implementação e risco resolvido

`object_change_session.*` acrescenta sessões separadas para Documento e Composição. Cada sessão conserva a origem externa inicial e o trabalho corrente. A preparação produz um ticket de prévia imutável ligado à identidade/revisão da sessão. Aplicação exige ticket ainda não usado, base interna correspondente e origem externa fornecida ainda igual à captura inicial. Undo/redo também exigem revisão e origem válidas.

A fonte e a apresentação entram juntas na linha do tempo. Isso evita desfazer texto sem o grafo correspondente quando há alterações visuais intercaladas. Uma mudança documental autoral é conferida por commit/undo/redo da sessão proprietária recriada para a transação; os replays autorais usam essa mesma rotina, com comparação exata de snapshot e seleção. A sessão de digitação GTK não é acessada nem substituída.

Os planejadores aceitam agora apresentação corrente validada da mesma captura plana. A comparação distingue armazenamento de vetores por ID da ordem lógica `Layout.children`, permitindo preparar outro movimento após reordenar a fonte. Modificar identidade/tipo/metadados da captura é recusado. Ordem visual divergente da fonte impede intenção autoral, evitando promoção implícita de mudanças visuais anteriores. Frames e pintura permanecem conservados quando a ordem anterior está alinhada.

A API mantém `origin()` como stamp externo original e `current()` como agregado de trabalho sem envelope salvo. A captura interna de Composição usa seu payload/hash de trabalho e contexto de revisão/timestamp de abertura somente para os codecs existentes; não é oferecida como revisão persistida futura.

## Nove grupos novos

1. Documento com Unicode, formatação e marcação: arranjo visual seguido de duas reordenações autorais; undo/redo conjunto exato, origem inalterada, dirty correto e revisão monotônica.
2. Tickets: outra sessão, consumo único, preparação concorrente na mesma revisão, stale após alteração, recusa mesmo depois de undo restaurar os bytes (ABA interno), no-op preservando redo/revisão e edição nova descartando redo.
3. Conflito externo somente em metadados e outro projeto: apply/undo recusados sem alterar fonte, grafo, revisão, histórico ou consumo do ticket.
4. Freeform com frames: reordenação autoral conserva geometria e pintura; reordenação visual posterior não é promovida para autoria; undo restaura alinhamento e permite novo plano. Apresentação inválida é recusada.
5. Resma de várias linhas e folha coberta privada: seleção parcial recusada, movimento de grupo completo aplicado, undo proprietário exato; intervalo marcado incompatível mantém a recusa original do motor.
6. Histórico: zero orçamento recusa mudança irreversível, mas admite no-op; limites máximos; fronteira exata de bytes; evicção por quantidade e bytes sempre remove estados inteiros, conservando o último par reversível.
7. Composição com live/pinned/forked/reference-only, revisão aplicada, ocorrência oculta, variante e rascunho: operações sucessivas conservam todo o agregado fora da ordem; undo/redo misto exato; conflito de revisão externa; stale e consumo único; envelope original intacto.
8. Composição: ordem visual divergente recusada para autoria, no-op conserva redo, grafo válido com tipo alterado é recusado por incompatibilidade com a captura, conflito de payload com mesma revisão não consome ticket, quota zero e teto de bytes.
9. **40 aplicações documentais + 40 de Composição**, seed fixa, seguidas de **80 undo e 80 redo**: cada estado, grafo e revisão comparado à sequência de snapshots guardada. Revisão final é 120 em cada sessão, sem retorno ao número antigo.

Os nove grupos se somam aos 30 grupos anteriores de duas pontes e planos, preservando testes de limites, resmas, fonte integral, variantes, referências, 150+150 movimentos planejados, 250 ordens documentais e 200 ordens de Composição. Não se declara prova formal nem cobertura de entradas arbitrárias.

## Prova com repositório real

`inde_object_change_session_service_tests` cria projeto novo em diretório temporário exclusivo e perfil isolado, pelos serviços reais. Persiste Documento com Unicode/formatação/marcação e Composição com todos os modos, revisão aplicada e rascunho privado. Carrega fontes pelo repositório e abre sessões isoladas.

Prepara/aplica mudança visual e reordenação autoral, recusa ticket preparado antes da mudança, destrói as projeções de origem e executa undo/redo dos passos mistos. Exige candidatos e grafos exatos, restauração da origem e dirty coerente. **Todos os arquivos do projeto são comparados byte a byte antes/depois desse percurso**; Documento e hash/revisão/payload da Composição persistida continuam iguais. Verifica schema 37, integridade SQLite e chaves estrangeiras.

Somente depois escreve uma alteração de título documental e outra de rascunho de Composição, por serviços da fixture, e verifica que undo é recusado sem mudar o trabalho da sessão. Escritores são sequenciais/sintéticos; o teste não é corrida entre processos nem comprovação de transação de escrita.

## Reprodução

```sh
python3 verificar_base.py --integrado --extrair
cmake -S tests/object_change_session -B build-obj01-session -G Ninja
cmake --build build-obj01-session --parallel 2
ctest --test-dir build-obj01-session --output-on-failure

cmake -S tests/object_change_session -B build-obj01-session-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-obj01-session-clang --parallel 2
ctest --test-dir build-obj01-session-clang --output-on-failure

cmake -S tests/object_change_session -B build-obj01-session-asan -G Ninja \
  -DOBJ01_SANITIZE=ON
cmake --build build-obj01-session-asan --parallel 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-obj01-session-asan --output-on-failure
```

O build mínimo inclui as mesmas **26 unidades reais do proprietário**, sem configurar GTK/SQLite, e quatro programas: 9 grupos documentais, 11 da Composição, 10 de planos, 9 de sessões. Todos os fontes novos mantêm avisos rigorosos como erros. A exceção Clang de conversão de sinal continua restrita aos fontes antigos. ASan/UBSan instrumentam também as unidades proprietárias. LSan local está desativado devido ao ambiente sob ptrace; na CI, `detect_leaks=1`.

Localmente, os builds Clang e ASan reutilizaram os diretórios mínimos anteriores, configurando `tests/object_change_plan` com `-DOBJ01_CHANGE_SESSION=ON`; a extensão configura os mesmos alvos e unidades. O GCC e a nova matriz da CI usam diretamente `tests/object_change_session`. Não houve substituição por mocks.

No build nativo completo preparado nos cortes anteriores:

```sh
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_BUILD_CORE_TESTS=ON -DINDE_ENABLE_LTO=OFF
cmake --build build-nativo --target inde_object_change_session_tests \
  inde_object_change_session_service_tests inde_object_change_plan_tests \
  inde_object_change_plan_service_tests --parallel 2
ctest --test-dir build-nativo -R inde_object_ --output-on-failure
ctest --test-dir build-nativo --output-on-failure
```

O núcleo nativo é Release não instrumentado; a prova SQLite é local. A CI acrescenta quatro combinações GCC/Clang × sem/com sanitizadores aos dezessete jobs anteriores. A verificação dos três pacotes de fontes e os cinco testes de recusa do verificador continuam na matriz.

## Limites observáveis

A quota é de representação histórica, não RSS; origem fixa, tickets do chamador, dados em memória do domínio e temporários podem ocupar mais. Padrão 64 passos/64 MiB, teto 64/128 MiB. Mudança que não cabe como par reversível é recusada. Sessão não aceita digitação/seleção externas, save, rebase, reconhecimento de persistência ou restauração após encerramento do processo.

Entradas precisam estar estáveis durante cada chamada. Não há sincronização entre threads, leitura automática da última versão no banco, lock, autorização ou fechamento da janela entre conferência e uma gravação externa futura. O ticket demonstra vínculo com esta sessão/revisão, não aceite humano nem autoridade de persistência. Mudança na origem bloqueia a sessão; o corte não sobrescreve/rebaseia conflitos.

Não houve injeção de falha de alocação, interrupção de processo, stress concorrente ou benchmark de latência. A preservação diante de alocação segue a preparação anterior à troca final, não um ensaio de todos os pontos de falha.

Os 92 testes Python do aplicativo e percursos GTK/publicação não foram reexecutados, pois não houve ligação nessas camadas. Os testes Python específicos do verificador de fontes foram reexecutados. Não foram abertos ou migrados projetos autorais, nem alterados pacotes de fontes, codecs ou migradores. A alteração preexistente de permissão em `Preparar-ponte.sh` no checkout local de main foi preservada.

O próximo requisito é o protocolo de persistência/reconhecimento de save e recovery, inicialmente em fixtures isoladas e sem migração. Aplicar em memória não deve ser confundido com gravar no projeto. GTK e arquivo único `.inde` continuam fora da fase atual.

Ver [contrato](../arquitetura/SESSOES-OBJ-01.md), [planos anteriores](VALIDACAO-OBJ-01-PLANOS.md) e [inventário](../arquitetura/INVENTARIO-OBJ-01.md). Resultados e evidências são consolidados no complemento deste relatório.

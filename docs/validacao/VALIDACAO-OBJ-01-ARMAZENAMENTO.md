# OBJ-01 — Validação do armazenamento durável de laboratório

Data: 2026-11-21. Base: `b5c9c820bd75beba2596a7b0328e5465c09df05b`. Branch `feature/inde-obj-01-foundation`. Escopo **Fase 1**, Linux, diretórios temporários novos. Nenhum armazenamento é aberto automaticamente pelo INDE ou em obra autoral.

## Implementação e invariantes

A biblioteca separada `object_checkpoint_store.*` conserva o codec privado anterior e adiciona raiz marcada por escopo/UUID, lease exclusivo, gerações imutáveis, publicação sem substituição e sync, inventário validado, quotas e plano de retenção explícito. Não altera projetos, codecs proprietários, migradores, SQLite, GTK ou o journal antigo.

O arquivo de geração é escrito integralmente sob nome temporário, sincronizado, renomeado atomicamente com `RENAME_NOREPLACE` e só confirmado como durável depois de sincronizar o diretório. Geração inválida não é reparada/ignorada; `latest` não faz fallback silencioso. Incompletos são conservados e contam na quota. Falha no fluxo de mutação exige reabrir antes de tentar outra mutação.

A retenção preserva pelo menos duas gerações válidas recentes e precisa de um plano ligado ao lease/inventário. Cada exclusão é sincronizada; o resultado pode ser parcial e informa durabilidade incerta quando aplicável. Não se declara atomicidade conjunta da retenção, nem com a escrita autoral.

## Seis grupos novos do build mínimo

1. Criação privada, gerações monotônicas e imutáveis com payloads diferentes, leitura por ID/latest, reabertura, marcador/escopo e exclusividade entre handles; 20 aberturas recusadas sem vazamento de descritores em `/proc/self/fd`. A transferência de propriedade dos descritores usa movimento explícito para evitar fechamento duplicado se a construção falhar.
2. Falhas injetadas em escrita (`ENOSPC`), sync do arquivo, publicação e sync do diretório (`EIO`). Verifica geração anterior intacta, visibilidade/durabilidade distintas, bloqueio de mutações depois de falha, reabertura e preservação de temporários. `EINTR` de escrita é repetido corretamente.
3. Corrupção da geração mais recente aparece no inventário; latest/publicação/retenção recusam, enquanto a antiga válida pode ser selecionada. Checksum inválido, links simbólicos, hard links e marcador modificado são recusados.
4. Caminho relativo, `..`, NUL e componente simbólico não podem inicializar caminho indevido. Diretório comum não é adotado. Quotas de arquivos/bytes recusam antes de criar outra tentativa ou apagar dados.
5. Retenção seleciona somente versões antigas; plano stale/consumido ou de outro lease é recusado. Falha intermediária de unlink reporta somente remoções efetivadas; falha no sync de retenção informa incerteza. As duas gerações recentes permanecem acessíveis.
6. Outro processo não abre o store enquanto o lease está ativo; handle herdado de fork não opera sob o lease do pai. **Seis processos novos** encerram em cada fronteira (`BeforeCreate`, `Created`, `Written`, `FileSynced`, `Published`, `DirectorySynced`). Cada reabertura conserva a geração anterior e encontra somente o conteúdo antigo ou o novo publicado completo, sem geração parcial nem promoção automática de temporários. Os payloads antigo/novo têm apresentações distintas.

São seis grupos novos e 52 no conjunto de seis programas mínimos, incluindo as 26 unidades reais do domínio/codec e todas as regressões de pontes, planos, sessões e save/recovery. Não são mocks de filesystem: arquivos, `flock`, `fsync`, rename e processos são reais; o hook de erro substitui explicitamente o resultado da operação selecionada.

Os filhos que encerram com `_exit` não executam destrutores ou a finalização de LSan; esse é o cenário de interrupção deliberado. Os pais e percursos normais são instrumentados, e a CI pede detecção de vazamentos nesses percursos. Não se afirma que um processo interrompido abruptamente teve sua memória validada no encerramento.

## Integração com projeto e CAS reais

`inde_object_checkpoint_store_service_tests` cria projeto sintético novo, perfil isolado e dois stores privados, fora da pasta do projeto. Documento contém Unicode, formatação/marcação; Composição inclui live/pinned/reference-only, ocultação, origem congelada e rascunho privado.

Aplica alterações privadas e ativa barreiras de save. Publica os checkpoints pendentes e confere que isso não alterou as fontes no SQLite. Só depois executa os serviços CAS existentes. Fecha/reabre os stores, recupera o caso commit-antes-de-ack, reconhece readbacks reais nas sessões vivas e publica a geração reconhecida. Exige duas gerações por fonte e retomada limpa pelo checkpoint mais recente.

Uma segunda alteração encontra `ENOSPC` injetado na escrita do checkpoint. O harness só chama o escritor autoral se a publicação foi durável; comprova que ele não foi chamado, que o banco permaneceu igual e que o pedido/trabalho continuaram preservados. Com a origem intacta, cancelamento explícito libera a barreira sem perder trabalho. O teste termina com schema 37, integridade SQLite e chaves estrangeiras conferidos.

Não há transação conjunta entre os dois stores ou as duas fontes. O store não consulta o banco para resolver conflitos; o CAS continua sendo a autoridade para escrever. Nenhuma obra real foi aberta ou migrada. O checkout local de main e sua alteração preexistente de permissão em `Preparar-ponte.sh` foram preservados.

## Reprodução

```sh
python3 verificar_base.py --integrado --extrair
cmake -S tests/object_checkpoint_store -B build-obj01-store -G Ninja
cmake --build build-obj01-store --parallel 2
ctest --test-dir build-obj01-store --output-on-failure

cmake -S tests/object_checkpoint_store -B build-obj01-store-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-obj01-store-clang --parallel 2
ctest --test-dir build-obj01-store-clang --output-on-failure

cmake -S tests/object_checkpoint_store -B build-obj01-store-asan -G Ninja \
  -DOBJ01_SANITIZE=ON
cmake --build build-obj01-store-asan --parallel 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-obj01-store-asan --output-on-failure
```

Localmente foram reutilizados os builds mínimos anteriores com `OBJ01_CHECKPOINT_STORE=ON`, `OBJ01_SAVE_PROTOCOL=ON` e `OBJ01_CHANGE_SESSION=ON` quando necessário. A entrada acima e a CI configuram os mesmos alvos desde checkout limpo. Todo o código novo usa avisos rigorosos como erros; a exceção Clang de conversão de sinal fica restrita aos fontes antigos. A dependência GLib do store é explícita no build nativo; não depende de include transitivo de outro alvo.

O núcleo nativo preparado nos cortes anteriores permanece Release sem sanitizadores; o teste com SQLite é local. Os sanitizadores do mínimo incluem store/processos e as 26 unidades proprietárias. LSan local fica desativado devido ao ambiente sob ptrace; a CI usa `detect_leaks=1`.

```sh
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_BUILD_CORE_TESTS=ON -DINDE_ENABLE_LTO=OFF
cmake --build build-nativo --target inde_object_checkpoint_store_tests \
  inde_object_checkpoint_store_service_tests --parallel 2
ctest --test-dir build-nativo --output-on-failure
```

A CI conserva os 21 jobs e amplia os quatro jobs de sessões para a entrada do store, executando seis programas com GCC/Clang e sanitizadores. O verificador dos três pacotes de fontes e seus cinco testes de recusa também continuam presentes.

## Fronteiras e lacunas

Este é um armazenamento Linux de laboratório, sem ligação à interface ou a um escritor automático do aplicativo. Requer filesystem com `renameat2(RENAME_NOREPLACE)` e sucesso dos syncs; não existe fallback que sobrescreva nomes. Lease é advisory e por diretório, não um lock global sobre Documento/Composição nem proteção contra código hostil do mesmo usuário.

Não foram testados queda de energia, hardware que não cumpra fsync, todos os pontos de erro, curto write injetado, falta de memória, stress concorrente amplo ou latência/RSS. Foram testados os erros e pontos de encerramento enumerados. A quota é volume de arquivos de dados, não espaço alocado do filesystem ou memória total. Estado visível sem sync confirmado é reportado como incerto, não como save concluído.

Temporários e evidência inválida não são removidos automaticamente; manutenção/exportação/descarte explícitos continuam futuros. Entradas inseguras/excessivas podem impedir reabertura até revisão. Retenção é explícita e pode terminar parcialmente. A recuperação ainda reinicia o histórico de undo/redo e não reutiliza tickets antigos.

Os 92 testes Python do aplicativo e percursos GTK/publicação não foram reexecutados, pois não houve integração nessas camadas. O próximo gate é coordenar escritor, gerações pendentes/reconhecidas, resultado incerto e manutenção segura no laboratório. Não se promove o corte a escrita em obras reais, migração, GTK ou arquivo único `.inde`.

Ver [contrato](../arquitetura/ARMAZENAMENTO-RECOVERY-OBJ-01.md), [protocolo anterior](VALIDACAO-OBJ-01-SALVAMENTO.md) e [inventário](../arquitetura/INVENTARIO-OBJ-01.md). Resultados/evidências são consolidados abaixo.

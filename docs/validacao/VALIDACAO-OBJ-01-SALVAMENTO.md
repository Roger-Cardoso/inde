# OBJ-01 — Validação do protocolo de save/recovery

Data: 2026-11-20. Base: `df13a5ff91e276e78a66a3bd74473db462cb4955`. Branch `feature/inde-obj-01-foundation`. **Fase 1, laboratório isolado**: protocolo puro nas sessões, escrita real somente nos serviços de fixtures novas, sem GTK, migração ou novo formato de projeto.

## Aprendizados e correções verificadas

O Documento existente oferece `update_document_if_unchanged`, com conferência de proprietário/estado dentro da transação `SqliteWritingRepository::save_checked`. A Composição confere projeto, revisão e hash dentro de `CompositionStore::save`, preservando revisões aplicadas e origens congeladas. O novo protocolo usa essas autoridades no teste; não acrescenta SQL de escrita próprio ou altera codecs/migradores.

O primeiro teste real encontrou normalização da ordem de armazenamento dos spans de formatação ao carregar do SQLite. O candidato e o readback representavam os mesmos spans/posições, mas seus bytes de snapshot diferiam. A comparação foi corrigida para a igualdade proprietária `project::same_document_state`, com adoção do readback carregado e conservação dos campos/IDs. Um teste específico cobre essa normalização e o undo/redo posterior. A precondição da origem ainda usa o stamp completo; não foi relaxado o controle de concorrência.

A atualização do timestamp depois de um save também exigiu reserva de representação no histórico. Sem ela, um save correto poderia ultrapassar uma quota exata ao ser reconhecido. O protocolo reserva 1.024 bytes por timestamp documental serializado e testa reconhecimento/replay na fronteira exata, sem descartar estados históricos.

O journal documental existente não possui representação do grafo ou da Composição OBJ-01. Foi conservado sem alteração. O novo codec produz somente checkpoint de laboratório privado, não save de apresentação no schema autoral.

## Sete grupos novos do build mínimo

1. Barreira de save, candidato/base congelados, recusa de segunda preparação/edição/undo, recibo incorreto, outra sessão, reconhecimento único, nova base/revisão e undo/redo depois do save.
2. Readback com spans em outra ordem de armazenamento, aceito somente por igualdade proprietária; o estado carregado passa a ser o corrente e o histórico permanece reversível.
3. Cancelamento somente com origem intacta, recusa diante de estado já alterado e reconhecimento documental com timestamp maior na quota exata.
4. Composição: mudança apenas visual não dispara save; payload/hash/revisão seguinte conferidos; hash falso ou revisão posterior recusados; apresentação segue dirty; undo/redo cruza o savepoint sem perder rascunhos/modos/histórico.
5. Checkpoint/reinício com Unicode, seleção e Freeform de precisão decimal: grafo exato conservado, nova sessão sem tickets/histórico, retomada não salva versus adoção explícita do candidato persistido, recusa de terceiro escritor e de revisão não correspondente.
6. Corrupção/checksum, versão, bytes finais, layout, seleção, frame e ID inválidos; profundidade JSON recusada antes do codec proprietário; **100 truncamentos gerados**, seed fixa, sem sessão parcial.
7. **20 savepoints documentais + 20 da Composição**, com undo/redo depois de cada reconhecimento e reinício de cada checkpoint reconhecido. Revisão local final 80 por sessão, incluindo aplicações, reconhecimentos e replays.

Esses grupos se somam aos 39 anteriores: são 46 grupos em cinco programas mínimos, com as mesmas 26 unidades reais do domínio/codec. Não há mocks de codecs ou do motor textual. Código novo mantém avisos rigorosos como erros; a exceção Clang de conversão de sinal continua restrita aos fontes antigos.

## Escrita real, conflitos e dois processos

`inde_object_save_service_tests` cria diretório exclusivo, perfil isolado e projeto sintético novo. Prepara Documento com múltiplas formatações/âncoras e Composição com modos live/pinned/reference-only, ocorrência oculta, origem congelada e rascunho privado. Aplica reordenações, salva por CAS, recarrega do repositório e reconhece os resultados, mantendo undo/redo.

Dois caminhos de escrita são exercitados sequencialmente: outro escritor confirma primeiro; a tentativa com `expected` antigo falha dentro da transação, e os snapshots/revisões/hashes do vencedor permanecem. Isso verifica o CAS real; não se afirma stress concorrente entre threads.

Em seguida um processo novo executado pelo mesmo binário prepara trabalho e pedidos, escreve checkpoints e confirma as duas fontes. Encerra com `_exit(73)` **antes dos reconhecimentos**. O pai recarrega apenas os arquivos/banco: retomada sobre base antiga é recusada; adoção explícita do candidato correspondente funciona. A apresentação Row continua privada/dirty, as fontes coincidem com o readback e o histórico reiniciado fica vazio.

Outro processo encerra com `_exit(74)` **entre os commits** do Documento e da Composição. A recuperação classifica separadamente o Documento correspondente ao candidato salvo e a Composição ainda não salva, preservando seu trabalho. Não se promete atomicidade conjunta.

O helper de arquivo do teste usa criação nova/exclusiva, permissão 0600, `O_NOFOLLOW`, limite de tamanho e `fsync` do arquivo e diretório. O protocolo público não recebe caminho nem executa I/O. O teste termina verificando schema 37, `PRAGMA integrity_check` e `foreign_key_check`.

## Reprodução

```sh
python3 verificar_base.py --integrado --extrair
cmake -S tests/object_save_protocol -B build-obj01-save -G Ninja
cmake --build build-obj01-save --parallel 2
ctest --test-dir build-obj01-save --output-on-failure

cmake -S tests/object_save_protocol -B build-obj01-save-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++
cmake --build build-obj01-save-clang --parallel 2
ctest --test-dir build-obj01-save-clang --output-on-failure

cmake -S tests/object_save_protocol -B build-obj01-save-asan -G Ninja \
  -DOBJ01_SANITIZE=ON
cmake --build build-obj01-save-asan --parallel 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-obj01-save-asan --output-on-failure
```

Os builds locais reutilizaram diretórios anteriores, configurando `OBJ01_SAVE_PROTOCOL=ON` (e `OBJ01_CHANGE_SESSION=ON` no build de planos). A entrada nova acima e a CI configuram os mesmos alvos desde um checkout limpo. ASan/UBSan instrumentam o protocolo e as 26 unidades proprietárias mínimas; LSan local permanece desativado pelo ambiente sob ptrace, e a CI pede `detect_leaks=1`.

No build nativo Release completo já preparado nos cortes anteriores:

```sh
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_BUILD_CORE_TESTS=ON -DINDE_ENABLE_LTO=OFF
cmake --build build-nativo --target inde_object_save_protocol_tests \
  inde_object_save_service_tests inde_object_change_session_tests \
  inde_object_change_session_service_tests --parallel 2
ctest --test-dir build-nativo --output-on-failure
```

A prova SQLite/dois processos é nativa local, sem sanitizadores. A matriz mínima da CI cobre os cinco programas com GCC/Clang e sanitizadores, conservando os 21 jobs anteriores e ampliando os quatro jobs de sessões para o protocolo completo. Fontes compactadas e os cinco testes do verificador também foram reconferidos.

## Limites reais

O checkpoint recupera origem, trabalho, seleção e apresentação; **não recupera undo/redo nem tickets de antes do reinício**. Reconhecimento na sessão viva conserva o histórico. O protocolo puro não autentica recibos, lê automaticamente o último estado do banco ou concede autorização; o teste fornece readback real e usa CAS existente.

Não há store durável de produção, atualização atômica de gerações, lease, retenção, autosave assíncrono integrado, cópia/rebase de conflito ou coordenação transacional entre Documento/Composição. Checksum não é assinatura. Recuperação com candidato coincidente demonstra equivalência, não autoria da escrita. A barreira só pode ser cancelada depois de encerrar/drenar o escritor e comprovar origem intacta.

Foram simulados pontos de encerramento com processo novo e fsync, sem ensaio de queda de energia, disco cheio, falha de fsync, alocação ou todos os pontos de crash. Não há benchmark de latência/RSS. O parser tem quotas próprias e pode recusar fonte válida que as exceda, sem truncá-la. A quota histórica é representação lógica, não memória total.

Não foram reexecutados os 92 testes Python do aplicativo ou percursos GTK/publicação. Foram reexecutadas as regressões CTest pertinentes e o verificador de fontes. Não foram abertos, escritos ou migrados projetos autorais. O checkout local de main e sua modificação preexistente de permissão em `Preparar-ponte.sh` permanecem preservados.

O próximo gate da Fase 1 é projetar/ensaiar o armazenamento durável versionado e sua coordenação/lease, ainda em fixtures. O protocolo atual não libera escrita em obras reais, integração GTK, migração ou arquivo único `.inde`.

Ver [contrato](../arquitetura/SALVAMENTO-RECOVERY-OBJ-01.md), [sessões anteriores](VALIDACAO-OBJ-01-SESSOES.md) e [inventário](../arquitetura/INVENTARIO-OBJ-01.md). Resultados e evidências seguem no complemento.

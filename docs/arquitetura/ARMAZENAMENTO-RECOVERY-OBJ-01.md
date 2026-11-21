# OBJ-01 — Armazenamento durável de checkpoints em laboratório

Data: 2026-11-21. Base: `b5c9c820bd75beba2596a7b0328e5465c09df05b`, branch `feature/inde-obj-01-foundation`. **Fase 1**, biblioteca Linux separada `object_checkpoint_store.*`. Não é aberta pelo aplicativo, não recebe projetos autorais automaticamente e não altera SQLite, journal antigo, GTK, publicação ou formato `.inde`.

## Raiz dedicada e escopo

`CheckpointStore::create_new(absolute_path, scope)` exige diretório ainda ausente e pai já existente. Cria raiz 0700, marcador privado versionado e arquivo de lease 0600, sincronizando arquivos e diretórios. Não inicializa diretório existente. `open_existing` exige marcador íntegro do mesmo projeto, tipo Documento/Composição e ID da fonte. Cada armazenamento possui UUID próprio, também presente no cabeçalho de suas gerações.

Não há caminho padrão, descoberta automática de obras ou criação dentro de `recovery/editor-v1`. Os testes fornecem diretórios exclusivos em fixtures temporárias, separados do projeto sintético. A biblioteca não migra nem reaproveita o journal documental existente.

A abertura percorre componentes absolutos por descritores, sem seguir links simbólicos, recusando `.`/`..`, NUL e caminhos excessivos. A raiz deve pertencer ao usuário atual e ser privada. Arquivos devem ser regulares, 0600, do usuário atual e ter um único hard link. Operações posteriores usam o descritor da raiz, não resolvem novamente caminhos de geração vindos do conteúdo.

Marcador e identidade do lease são conferidos durante operações. Entradas desconhecidas, links, diretórios ou permissões incompatíveis interrompem a operação, preservando evidência. Essa defesa não autentica conteúdo contra um processo hostil com as mesmas permissões do usuário.

## Lease e processos

Abrir o armazenamento adquire `flock(LOCK_EX | LOCK_NB)` no arquivo privado `.lease`, inclusive para leitura, e conserva o descritor até destruir o handle. Outro handle/processo cooperante recebe `store_busy`. Encerrar o processo libera o bloqueio pelo sistema operacional; não depende de remover um arquivo de lock supostamente obsoleto.

Descritores usam `CLOEXEC`. Um objeto herdado por `fork` recusa operar se o PID mudou; o filho deve abrir um novo handle após `exec`. A destruição não executa unlock explícito que pudesse liberar o lease do pai por meio do descritor herdado. A API não é reentrante nem sincronizador entre threads.

O lease é do **diretório de armazenamento**, não um lock global sobre a fonte autoral. Dois diretórios independentes podem declarar o mesmo escopo; o CAS real continua obrigatório no Documento/Composição. Não há transação conjunta entre armazenamento, SQLite ou duas fontes.

## Publicação de geração

Cada publicação valida checksum, codec, fonte completa, seleção, apresentação e escopo do checkpoint anterior à escrita. O corpo continua sendo `OBJ01-RECOVERY-LAB-1`. O arquivo geracional acrescenta cabeçalho `INDE-OBJ01-GENERATION-LAB-1`, ID do armazenamento, sequência e SHA-256 do checkpoint. Uma geração confirmada nunca é substituída.

| Etapa | Efeito |
| --- | --- |
| Validar entrada/inventário/quotas | Nenhum arquivo novo; fonte incompatível ou evidência inválida é recusa |
| Criar `.pending-<UUID>` | Arquivo novo/exclusivo e privado; não é uma geração recuperável |
| Gravar e `fsync` do arquivo | Conteúdo completo validado para publicação, ainda sem nome confirmado |
| `renameat2(RENAME_NOREPLACE)` na mesma raiz | Publica o nome geracional atomicamente, sem sobrescrever um nome existente |
| `fsync` do diretório | Só então o resultado informa `durable=true` |

A sequência é derivada do maior nome geracional confirmado, inclusive quando o registro está corrompido; não depende de um ponteiro HEAD mutável. Sequências de tentativas não publicadas podem ser reutilizadas, mas nomes/bytes de gerações confirmadas não. Esgotamento é recusa. Linux/filesystem sem suporte à publicação sem substituição falha fechado, sem fallback que sobrescreva arquivos.

`PublishResult` distingue `visible`, `durable`, sequência, etapa alcançada e diagnóstico. Falha depois do rename e antes do sync do diretório informa geração visível com durabilidade incerta. O handle fica bloqueado para novas mutações depois de falha no fluxo de publicação; fechar/reabrir exige inventário e validação novos. Não se começa uma gravação autoral somente porque um arquivo temporário existe ou um nome se tornou visível.

Arquivos incompletos permanecem identificados no inventário e contam nas quotas. Não viram geração nem são apagados silenciosamente. Este corte não implementa sua promoção, exportação ou descarte; exige manutenção explícita futura. Uma geração inválida também é preservada e bloqueia publicação/retenção.

## Leitura, corrupção e quotas

`inventory()` informa gerações em ordem, status/diagnóstico das inválidas, nomes incompletos e bytes dos arquivos de dados. `load(sequence)` escolhe uma versão explícita e a valida integralmente. `latest()` exige que a mais recente seja válida: não retorna uma anterior silenciosamente. Uma geração anterior válida pode ser escolhida explicitamente; entradas inseguras/excessivas exigem revisão do diretório antes de reabrir.

Limites padrão: **64 arquivos de dados e 256 MiB**, incluindo temporários. Opções admitem 2–128 arquivos e 1 KiB–512 MiB. Um arquivo geracional tem no máximo o limite do checkpoint (64 MiB) mais 256 bytes. Inventário absoluto também limita quantidade e volume. Marcador e lease têm tamanho fixo pequeno e não entram nessa quota de dados; ela não é limite de RSS, espaço alocado do filesystem ou custo dos temporários em memória.

Quota é conferida antes da criação de outra tentativa. Não provoca deleção automática. Limite insuficiente conserva o último estado recuperável. Ler pode permitir revisão/retenção quando uma configuração menor impedir nova publicação.

## Retenção explícita

`plan_retention(keep_latest)` conserva no mínimo as **duas gerações válidas mais recentes**, selecionando somente as anteriores. O plano é imutável, vinculado ao handle/lease e ao inventário observado. O chamador pode conferir IDs, hashes e bytes antes de `apply_retention`.

Plano consumido, de outro lease, inventário alterado ou evidência inválida é recusa antes de apagar. Cada geração selecionada é novamente carregada/conferida e removida por nome interno; o diretório é sincronizado depois de cada remoção. Arquivos incompletos, desconhecidos ou corrompidos não entram na seleção.

Retenção de várias versões **não é all-or-nothing**. Uma falha intermediária informa as remoções já observadas, diagnóstico e eventual incerteza de durabilidade. O handle passa a exigir reabertura. Os dois registros mais recentes nunca são candidatos desse plano, inclusive em resultado parcial. Não há rollback artificial de arquivos removidos nem exclusão silenciosa de evidência.

## Integração ensaiada e limites

O teste nativo conecta dois armazenamentos dedicados aos serviços reais de Documento e Composição de um projeto temporário. Publica a geração do save pendente e só executa CAS quando `durable=true`; após readback/ack, publica a segunda geração. As duas fases ficam disponíveis. Fechar/reabrir o armazenamento permite recuperar a janela commit-antes-de-ack e retomar o checkpoint reconhecido.

Uma falha ENOSPC injetada impede chamar o escritor autoral no teste e mantém trabalho/pedido. Esse sequenciamento é um **harness de laboratório**, não coordenador automático instalado no aplicativo. O protocolo anterior ainda define como reconhecer/cancelar/recuperar; o store não decide se um projeto mudou nem autoriza writeback.

Os testes mínimos também executam processos novos, interrupções nas seis fronteiras de publicação, competição por lease, I/O injetado, corrupção, quotas e retenção parcial. `fsync`/rename seguem o contrato observado do SO/filesystem. Não se afirma prova de queda de energia, integridade do hardware, todos os erros possíveis, stress concorrente amplo ou benchmark.

Continuam futuros: coordenação automática de escritor/checkpoint/ack, manutenção de temporários, política de retenção/exportação voltada ao autor, integração com recovery antigo/leases editoriais, interface e aceite humano. A restauração continua abrindo nova sessão sem undo/redo ou tickets antigos; nenhuma migração, integração GTK, publicação ou arquivo único `.inde` foi iniciada.

Ver [validação](../validacao/VALIDACAO-OBJ-01-ARMAZENAMENTO.md), [protocolo de save/recovery](SALVAMENTO-RECOVERY-OBJ-01.md) e [inventário](INVENTARIO-OBJ-01.md).

## Prioridade após a reconciliação

A [reconciliação de 2026-11-21](RECONCILIACAO-OBJ-01.md) preserva este store como laboratório e congela sua expansão automática. Coordenador, exportação/manutenção de incompletos e integração de produto não são pré-requisitos gerais do primeiro documento composto. Uma nova intervenção precisa demonstrar bloqueio do piloto e receber escopo confirmado. O store não deve ser convertido em banco autoral do documento.

# OBJ-01 — Protocolo de salvamento e recovery em laboratório

Data: 2026-11-20. Base `df13a5ff91e276e78a66a3bd74473db462cb4955`, branch `feature/inde-obj-01-foundation`. **Fase 1**, sem GTK, migração ou arquivo único `.inde`. As sessões continuam sem acesso a arquivos/bancos. Este corte acrescenta o protocolo puro de save/readback e um codec privado de checkpoint; somente os testes executam escrita pelos serviços existentes e I/O de checkpoints em projetos temporários exclusivos.

## Pedido, barreira e reconhecimento

`begin_save()` só admite fonte autoral logicamente alterada. Uma mudança exclusivamente visual não é gravável pelo schema autoral atual e não dispara um save. O pedido congela `expected` (última origem reconhecida) e `candidate` (trabalho atual), com identidade da sessão, intenção única e revisão. O pedido não pode ser alterado pelo chamador nem contém revisão futura inventada.

A sessão entra em barreira: preparar/aplicar mudanças e undo/redo são recusados enquanto houver save pendente. Não há fila assíncrona, substituição de candidato em voo ou edição concorrente neste protocolo. O escritor externo precisa concluir/drenar a operação antes de reconhecer ou cancelar.

| Etapa | Autoridade |
| --- | --- |
| Preparar pedido | Sessão privada: captura candidata/base e ativa barreira; nenhuma escrita |
| Gravar Documento | Serviço existente `update_document_if_unchanged(candidate, expected)`; comparação no repositório dentro de transação |
| Gravar Composição | Serviço/store existentes, com candidato + revisão/hash esperados; transação confere proprietário, concorrência e imutabilidade do histórico/origens |
| Recarregar | Repositório real devolve o estado efetivamente persistido |
| `acknowledge_save` | Protocolo confere esse readback e atualiza a origem da sessão, sem executar outra escrita |

Os adaptadores de escrita acima existem **somente no teste de integração** deste corte. A API da biblioteca não recebe caminho, serviço, conexão SQLite ou callback de escritor. Para a Composição, o envelope usado como pedido pelo serviço conserva revisão/hash esperados; ele não deve ser confundido com readback salvo. `candidate()` continua sendo `project::Composition` sem envelope de save.

Documento reconhece mesmo ID/projeto, `created_at` preservado e `updated_at` no formato UTC produzido pelo serviço. Os demais campos precisam representar o mesmo estado segundo `project::same_document_state`: essa é a regra real do domínio, que considera irrelevante a ordem de armazenamento de certas coleções de formatação/âncoras/referências. Não se usa comparação por prosa, título ou hash parcial. IDs e ordem de parágrafos, resmas, visibilidade e semântica continuam verificados pelo domínio.

Esse detalhe foi demonstrado no SQLite: depois de uma movimentação, o motor conservava os spans de formatação em sua ordem de armazenamento; o repositório os recarregava por posição. A primeira comparação literal recusou um save válido. O protocolo passou a usar a igualdade proprietária e a adotar o **readback carregado** como estado atual/base. O stamp da origem continua sendo hash integral da captura, para detectar alterações externas; igualdade de dirty/readback e precondição da origem são verificações com propósitos diferentes.

Composição exige payload integral exatamente igual ao candidato, projeto/ID correspondentes, hash real compatível e revisão persistida **expected + 1**. Timestamp UTC vem do serviço. Revisão arbitrariamente maior não é reconhecida como esse save.

Readback incorreto mantém barreira, pedido, histórico e revisão intactos. Resultado aceito consome o pedido, libera a barreira e incrementa a revisão da sessão, invalidando prévias antigas. Repetir reconhecimento ou usar pedido de outra sessão é recusa. Esses inputs não são recibos autenticados: o chamador é responsável por obtê-los da autoridade real; a biblioteca pura não prova que alguém executou uma escrita.

## Histórico, timestamps e cancelamento

Reconhecer um save conserva todos os estados de undo/redo. O Documento mantém em cada estado histórico o `updated_at` retornado pela persistência; o estado corrente adota os dados reais do readback. Alocações/validações precedem as trocas finais sem exceção. Composição só atualiza a origem/envelope reconhecido; seu payload histórico não recebe revisão artificial.

A quota documental agora reserva **1.024 bytes de representação para cada `updated_at` serializado** (ou seu tamanho anterior, se maior). Assim, substituir um timestamp curto por UTC não exige descartar histórico nem pode transformar um save já realizado em falha de reconhecimento por uma fronteira exata de quota. A soma continua sendo quota lógica, não RSS. Os limites de 64 passos/64 MiB padrão e teto 64/128 MiB permanecem.

`source_dirty()` documental usa a igualdade proprietária, incluindo timestamps; permutações de armazenamento de coleções não são apresentadas como edição autoral. A comparação gráfica ignora apenas a ordem dos vetores de armazenamento por ID; `Layout.children`, frames, arranjo e pintura continuam significativos. Depois de salvar texto, uma apresentação Row/Freeform ainda pode estar dirty, porque não foi persistida no schema autoral.

`cancel_save` exige pedido pendente válido e origem externa ainda igual à última base. Uma falha de escrita confirmada, com origem intacta, permite liberar a barreira sem perder trabalho/histórico. Não se cancela uma operação que ainda está em voo. Se o banco já mudou, inclusive porque o save pode ter sido confirmado, cancelamento é recusado: não se declara fracasso ou clean sem evidência. Conflitos preservam o pedido e o checkpoint para tratamento explícito; não há rebase/cópia automática neste corte.

## Checkpoint privado de laboratório

`checkpoint()` devolve bytes e SHA-256, sem I/O. O formato identificado por `OBJ01-RECOVERY-LAB-1` guarda:

- Tipo Documento/Composição e projeto; origem integral e trabalho integral pelos codecs proprietários, inclusive folhas privadas, revisões, origens e rascunhos.
- Revisão/timestamp persistidos da base da Composição e indicação de save pendente.
- Seleção Unicode documental e apresentação: arranjo/colunas, sequência de leitura, pintura, frames e ordem de armazenamento de objetos/ocorrências.

As entidades do grafo são reconstituídas pelos IDs da captura textual e validadas; o checkpoint não inventa outra autoridade de objetos. Não inclui credenciais, caminhos de escrita, exportação pública, tickets, histórico de undo/redo ou identidade da sessão antiga. É autoria privada, não pacote de publicação, assinatura ou novo formato `.inde`.

O enquadramento por comprimentos é iterativo, com versão estrita, checksum antes do parse, máximo de 64 MiB, fontes até 16 MiB cada (Composição também sujeita ao limite proprietário de 2 MiB), até 10.000 objetos/ocorrências por listas e IDs até 512 bytes. Antes do parser JSON proprietário, cada fonte passa por limite de profundidade 64 e um milhão de delimitadores estruturais fora de strings. IDs, UTF-8, identidades, hashes, seleção, frames, referências e regras do domínio são novamente validados. Nada é truncado. Campos incompletos, bytes finais, dimensões inválidas e versões desconhecidas são recusados. SHA-256 detecta corrupção; não autentica conteúdo contra quem pode reescrever bytes e checksum.

## Reinício e resultado incerto

`restore` cria uma sessão **nova**, com nova identidade, revisão local zero, sem barreira ativa nem tickets antigos. Conserva trabalho, seleção e apresentação. O histórico de undo/redo **não é recuperado**; esse limite é explícito, para não fabricar transações históricas a partir de um snapshot. O reconhecimento de save na sessão viva conserva o histórico; reiniciar é outro caso.

| Modo explícito | Condição externa | Resultado |
| --- | --- | --- |
| `ResumeUnsaved` | Origem ainda corresponde integralmente à base capturada | Retoma o trabalho pendente, mantendo fonte dirty quando necessário |
| `AdoptMatchingSave` | Checkpoint indicava save pendente e readback corresponde ao candidato; na Composição, revisão exatamente seguinte | Adota a base persistida correspondente, sem apagar apresentação visual pendente |
| Qualquer modo, com terceiro estado incompatível | Mudança externa não correspondente | Recusa sem escrever na origem nem descartar os bytes do checkpoint |

Não se escolhe modo automaticamente. Correspondência de candidato demonstra equivalência observada, não quem realizou a escrita. A restauração pressupõe que o escritor anterior foi encerrado/drenado; não há lease ou sincronização entre processos neste codec. Cada futuro save continua precisando do CAS real.

Documento e Composição possuem transações independentes. Se uma interrupção ocorre entre seus commits, uma fonte pode ser classificada como salva e outra retomada como trabalho não salvo. **Não existe atomicidade conjunta** entre essas duas autoridades neste protocolo.

## Prova e fronteira

O teste nativo cria somente projeto temporário novo. Exercita CAS real, readback, reconhecimento e undo/redo; prova conflitos com outro escritor; grava checkpoints novos com permissão 0600, criação exclusiva, `fsync` do arquivo/diretório e leitura limitada sem seguir links. Esses helpers são exclusivos do teste, não um serviço de recovery instalado.

Um processo novo via `fork`/`exec` executa os serviços e encerra com `_exit` depois dos commits, antes de reconhecer. Outro ensaio encerra entre o commit documental e o da Composição. O processo pai lê apenas o arquivo e o banco, sem herdar a sessão do filho. Isso demonstra os pontos ensaiados; não equivale a perda de energia, falha de disco ou todo possível ponto de crash.

O journal antigo `DocumentRecoveryStore` armazena Documento/seleção, mas não representa o grafo nem a Composição integral OBJ-01. Este corte não altera esse journal, não marca a apresentação como salva por ele e não migra seu schema. Um armazenamento durável de produção ainda exige gerações/atualização atômica do checkpoint, lease, retenção, exclusão/exportação, integração com o coordenador real e tratamento completo de I/O/resultado incerto. GTK, obras reais, publicação e arquivo único `.inde` permanecem fora do corte.

Ver [validação](../validacao/VALIDACAO-OBJ-01-SALVAMENTO.md), [sessões](SESSOES-OBJ-01.md) e [inventário](INVENTARIO-OBJ-01.md).

## Continuidade: armazenamento dedicado

O corte seguinte implementou [store durável de laboratório](ARMAZENAMENTO-RECOVERY-OBJ-01.md) para esses bytes, com gerações, lease, sync e retenção explícita. Os checkpoints continuam privados e a biblioteca de sessões permanece sem escritor de arquivos/bancos. Coordenação automática, manutenção de incompletos, interface e obras reais continuam fora da integração.

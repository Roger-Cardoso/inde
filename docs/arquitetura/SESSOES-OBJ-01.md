# OBJ-01 — Aplicação controlada em sessões de memória

Data: 2026-11-20. Base: `b23a93dac441d6cd1397e4826fa815fa50d5513e`, branch `feature/inde-obj-01-foundation`. Corte da **Fase 1**. `object_change_session.{hpp,cpp}` fornece sessões privadas, sem escrita em arquivos, SQLite, GTK, publicação ou recovery. Um estado aplicado à sessão continua sendo trabalho em memória.

## Autoridades e protocolo

`DocumentChangeSession` e `CompositionChangeSession` recebem uma projeção válida das pontes de leitura. Recapturam uma cópia profunda e conservam a origem inicial, seu stamp e o grafo inicial. A sessão passa a ser a única proprietária mutável do seu estado de trabalho; getters expõem acesso constante. Não recebe digitação externa, seleção pendente, atualização silenciosa de origem ou rebase.

| Operação | Pré-condições e resultado |
| --- | --- |
| `prepare(request)` | Planeja sobre fonte, seleção e apresentação atuais, sem alterar sessão/histórico. Devolve uma prévia imutável dentro de um ticket ligado à identidade e revisão desta sessão. |
| `apply(ticket, external_origin)` | A chamada é a escolha explícita de aplicar essa prévia. Confere sessão, uso anterior, revisão, origem externa inicial e base interna exata. Aplica fonte/grafo juntos ou recusa sem efeito. |
| `undo(expected, external_origin)` | Exige revisão atual e mesma origem inicial. Restaura conjuntamente fonte, seleção documental e grafo do passo anterior. |
| `redo(expected, external_origin)` | Mesmas conferências, restaurando conjuntamente o passo seguinte. |

Documento também exige o ID do projeto externo nas operações. Composição exige envelope `StoredComposition` externo completo. O chamador deve fornecer a origem atualmente carregada pela autoridade externa, **não o candidato alterado na sessão**. A comparação continua sendo com a captura de abertura, pois a sessão não salva nada nessa autoridade.

Tickets não podem ser copiados ou movidos como objetos; o `unique_ptr` proprietário pode ser transferido. Só a sessão os constrói. Sua prévia pode ser inspecionada antes da aplicação, sem setters para substituir candidato, intenção ou base. Um ticket aceito é consumido, inclusive no-op. Recusa não o consome. `apply` é uma ação explícita de API; ticket não é assinatura, autorização de acesso nem prova de aceite humano.

Cada alteração real, undo e redo incrementa a revisão monotonicamente. Uma prévia preparada antes de outra alteração continua recusada depois de desfazer essa alteração, mesmo que os bytes retornem ao estado antigo: a revisão não retrocede. Uma prévia de outra sessão também é recusada, ainda que as fontes sejam iguais. Lote vazio/no-op consome seu ticket sem mudar revisão, histórico ou redo. Edição nova após undo descarta o redo.

`current()` e `graph()` são vistas da sessão; o chamador deve considerá-las invalidadas por operação mutável posterior ou destruição. O ticket conserva cópias próprias da prévia, mas sua validade de aplicação depende da sessão/revisão. Referências antigas não servem como estado atual.

## Apresentação e ordem autoral

Os planejadores ganharam sobrecargas que recebem a apresentação corrente. As assinaturas anteriores continuam delegando à apresentação canônica da ponte, com o mesmo comportamento anterior.

A apresentação recebida é validada integralmente e deve preservar todas as entidades e metadados da captura plana. Pode variar arranjo, frames, pintura e sequência visual. Não pode introduzir entidades, trocar identidade, tipo ou conteúdo. A comparação de objetos/ocorrências/recursos normaliza somente a ordem de armazenamento dos vetores por ID: após reordenar a autoria, o adaptador pode emitir esses vetores em outra ordem, mas essa ordem de armazenamento não é a autoridade de leitura. `Layout.children` continua sendo comparado explicitamente.

Para intenção autoral, a sequência **anterior** da apresentação deve corresponder à sequência da fonte corrente. Se uma reordenação visual anterior divergiu da fonte, `presentation_reading_order_diverged` exige restaurá-la explicitamente ou desfazê-la primeiro. Um novo pedido autoral não promove alterações visuais anteriores para a fonte implicitamente. Mudança visual de arranjo, frame ou pintura sem divergência de leitura pode ser seguida de reordenação autoral, conservando geometria e pintura.

Continuam valendo todos os limites do planejador: um grupo documental inteiro por plano; destino no início de outro grupo ou fim; sem divisão/expansão automática de resmas; recusa do motor sobre intervalos; Composição com reordenação na raiz; transferências, frames, pintura e arranjos somente visuais. IDs autorais e públicos não são reinventados.

## Histórico conjunto e motor proprietário

A linha do tempo guarda snapshots imutáveis coordenados de fonte, seleção e grafo, compartilhados por ponteiros constantes. O passo é indivisível: desfazer texto sem seu grafo, ou grafo sem sua fonte correspondente, não é uma operação desta API. Composição restaura o agregado integral capturado, incluindo modos, versões, origens e rascunhos; não aplica rascunhos nem calcula novo texto por concatenação própria.

Para uma mudança documental autoral, antes de aceitar o candidato o executor recria uma `application::DocumentEditSession` isolada para essa transação, executa commit/undo/redo do proprietário e exige reprodução exata do snapshot e da seleção esperados. Ao desfazer/refazer um passo autoral, repete a verificação/replay pela sessão proprietária. Não implementa outro delta textual. A linha do tempo OBJ-01 coordena os snapshots gráficos com esse replay.

Essa sessão proprietária por transação **não é a sessão de digitação da interface**, não herda seu histórico e não consolida edições pendentes. Seu limite próprio de histórico reversível também pode recusar uma transação, antes de alterar a sessão OBJ-01. O corte é uma execução de laboratório separada da aplicação GTK.

Timestamps continuam com a autoridade de persistência. Os planos de reordenação conservam os timestamps de abertura/trabalho, e o replay proprietário conserva os timestamps atuais conforme seu contrato existente. Não se inventa data de salvamento.

## Quotas e atomicidade em memória

Padrão: **64 passos e 64 MiB** de representação histórica. O máximo configurável é 64 passos e 128 MiB; zero é permitido, mas nenhuma mudança real pode ser aceita sem caber como par reversível. No-op continua permitido. Estados antigos são removidos inteiros para respeitar quantidade e bytes. O passo mais recente precisa conservar anterior e posterior completos; se esse par exceder a quota, a aplicação é recusada.

`history_bytes()` soma, uma vez por estado único retido, bytes do snapshot canônico da fonte, structs lógicos do estado/grafo, IDs e relações do grafo. A linha do tempo inclui o estado corrente quando possui histórico; sem passos, informa zero. É uma quota dessa representação, **não RSS, capacidade dos alocadores, custo total de todas as camadas da fonte, tickets retidos pelo chamador, origem fixa ou temporários do motor**. Snapshots completos e cópias de codecs podem exigir mais memória que essa soma. O número de estados e os limites das pontes também limitam os dados admitidos.

Na aplicação, conferências, cópias do candidato, replay e preparação da nova linha do tempo precedem a troca final sem exceção do vetor de estados. Somente depois dessa troca avançam cursor/revisão e consumo do ticket. Falha de alocação pode propagar, conservando estado, histórico e ticket; outras falhas produzem diagnóstico privado. No replay, todas as verificações precedem a mudança do cursor. Não há estado parcial visível.

Entradas precisam permanecer estáveis durante a chamada. A API não sincroniza threads. Conferir uma origem fornecida em memória não consulta automaticamente a última revisão do banco nem impede outro processo de gravar depois da conferência. Revisão de sessão impede reuso de tickets; não equivale a transação SQLite ou detecção universal de ABA externo.

## Origem da Composição e fronteira persistente

`origin()` sempre expõe o stamp de abertura, sem revisão incrementada. `current()` devolve `project::Composition`, sem envelope de save. Para usar o codec/ponte existentes ao preparar planos sucessivos, o executor constrói internamente um contexto de captura com payload de trabalho e seu hash, conservando revisão/timestamp de abertura. Esse contexto não é retornado como `StoredComposition`, não representa estado salvo e não pode ser usado como revisão futura.

O `source()` do plano dentro do ticket descreve essa captura de trabalho; o `origin()` da sessão descreve a origem externa que precisa continuar igual. São propósitos distintos. Uma alteração externa somente em rascunho, metadados, revisão, hash ou timestamp impede apply/undo/redo. Nenhum conflito sobrescreve a origem ou atualiza silenciosamente a base.

Não há confirmação de save, refresh/rebase, executor persistente, registro de recovery ou serialização da sessão. Encerrar o processo perde esse trabalho em memória. Uma integração futura precisa definir transação/revisão na autoridade real, reconhecimento do save sem confundir base/trabalho, undo/recovery coerentes e tratamento de falhas antes de ligar a interface. Nenhum schema/projeto autoral é migrado por este corte.

Ver [validação das sessões](../validacao/VALIDACAO-OBJ-01-SESSOES.md), [planos](PLANO-ALTERACOES-OBJ-01.md) e [inventário](INVENTARIO-OBJ-01.md).

## Continuidade: save/readback e checkpoint

O corte seguinte adicionou o [protocolo de laboratório](SALVAMENTO-RECOVERY-OBJ-01.md): barreira de save, reconhecimento/cancelamento e checkpoint privado. `origin()` passa a representar a última origem reconhecida, inicialmente a abertura. O reconhecimento documental conserva histórico e atualiza timestamps, com reserva de representação; recuperação após reinício conserva trabalho/apresentação, mas abre nova sessão sem undo/redo anterior. Não há escritor externo na biblioteca nem integração com GTK.

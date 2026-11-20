# OBJ-01 — Planos imutáveis de alterações

Data: 2026-11-20. Base: `5f2cff75e5bc9d03a050ac2d1d37ed473e6b47bf`, branch `feature/inde-obj-01-foundation`. Corte da **Fase 1**, ainda em memória. Biblioteca separada `inde_object_change_plan`, em `cpp/object_change_plan.{hpp,cpp}`. Nenhuma conexão nova à interface, serviço de escrita, SQLite, recovery ou publicação.

## Intenção explícita e resultado verificável

`ChangeRequest` declara `VisualOnly` ou `AuthorialReorder`, além dos comandos OBJ-01. O tipo de comando, a posição, a similaridade textual e o arranjo não escolhem a intenção. Enum desconhecido é recusado. O padrão é visual.

| Intenção | Grafo após o pedido | Fonte da prévia |
| --- | --- | --- |
| VisualOnly | Cópia transformada pelos comandos transacionais existentes, incluindo suas regras de arranjo/frames/pintura | Documento ou Composição integral idêntico à captura, mesmo quando a ordem do grafo muda |
| AuthorialReorder / Documento | Ordem solicitada no layout raiz | Candidato produzido por `application::move_reading_block`, com seleção Unicode e todas as camadas do Documento |
| AuthorialReorder / Composição | Ordem solicitada no layout raiz | Cópia do agregado, com permutação exata das ocorrências pela operação proprietária `TextBlocksLab::reorder` |

`plan_document_change` e `plan_composition_change` recebem a projeção de leitura e a fonte atual **fornecida pelo chamador**. Primeiro verificam limites e correspondência integral da fonte com a captura. O grafo candidato é executado em `CompositionEditor` privado, sem histórico retido. Nenhuma recusa produz plano parcial; intenção autoral não suportada não vira visual silenciosamente.

Os resultados possuem `unique_ptr` para plano imutável, ou diagnósticos com código, ID e razão. O plano conserva fonte anterior, stamp original, grafos antes/depois, candidato e hash do candidato. `review()` informa separadamente `graph_changed` e `source_changed`. Comparar somente texto visível seria insuficiente: trocar dois parágrafos de prosa igual muda a sequência de identidades; reordenar uma ocorrência oculta muda a Composição mesmo quando a leitura visível é igual.

O plano é proprietário de suas cópias e sobrevive à destruição da projeção. Não armazena vistas para dados emprestados. A fonte e os candidatos são privados de autoria: contêm conteúdo oculto, histórico, origens e rascunhos. Diagnósticos do motor podem conter informação privada. Não são pacote público nem autorização de publicação.

## Tradução documental conservadora

Uma captura documental contém bindings de parágrafos e grupos obtidos da função real `application::reading_blocks`. Uma resma pode agrupar vários parágrafos. Por plano autoral admite-se **um comando que seleciona exatamente um grupo inteiro**; plano vazio é no-op aceito. O destino é o início de outro grupo ou o fim do Documento.

- Seleção parcial de resma, seleção de vários grupos ou destino no interior de grupo é recusado. Nenhuma expansão ou aproximação automática.
- A lista de IDs pode estar em outra ordem, mas o núcleo conserva a ordem relativa da fonte. ID não pertence à captura, duplicata, âncora selecionada e destino externo são recusados pelas camadas correspondentes.
- A tradução resolve IDs explícitos de ocorrências para os grupos proprietários. Não procura texto parecido nem substitui identidades por índices de vista filtrada.
- O motor existente conserva as identidades, remapeia formatação, seleção, resmas, marcações e evidências semânticas segundo suas regras. Se a movimentação dividir/inverter uma marcação, o plano devolve `source_text_engine_refused`, com a razão original. Não há segunda implementação textual nem fallback para substituição de strings.
- O candidato é novamente capturado pela ponte documental e sua ordem de IDs deve ser exatamente a solicitada no grafo. Divergência produz recusa, sem candidato parcial.

Lote documental autoral com mais de um comando é recusado (`document_authorial_batch_unsupported`). O corte não oferece permutações documentais arbitrárias: a validade de uma árvore não demonstra que uma sequência de alterações textuais preserva as camadas.

`before()` e `preview()` são `DocumentEditState`, com seleção Unicode válida. O teste integra esses estados à sessão de undo/redo existente, somente em memória. O plano não abre sessão, não altera o estado do chamador e não registra undo durável.

## Tradução da Composição

A captura contém **todas** as ocorrências, inclusive ocultas e referências, na ordem original. Lotes autorais de reordenação na raiz são executados no grafo e traduzidos para uma permutação completa dos IDs de colocação da fonte. A operação proprietária `reorder` altera a ordem do agregado candidato.

Como verificação adicional, o planejador restaura apenas a sequência original em outra cópia do candidato e exige que o codec produza os mesmos bytes do agregado original. Isso conserva modos live/pinned/forked/reference-only, versões aplicadas/históricas, biblioteca, classificações, perfil, origens congeladas e rascunhos. Nenhum rascunho é aplicado e nenhuma origem é atualizada implicitamente.

`before_reading()` e `preview_reading()` são projeções exatas do núcleo sobre o perfil capturado. `source_order()` e `preview_order()` expõem os IDs originais de colocação; no caso visual, ambas refletem a fonte inalterada, mesmo que o grafo tenha outra ordem.

O candidato é `project::Composition`, **sem envelope persistido futuro**. `source()` retém a revisão/hash/timestamp de origem; `preview_sha256()` é só o hash do payload candidato. Não se fabrica um `StoredComposition` com revisão incrementada, save realizado ou timestamp inventado.

Transferências, geometria, pintura e mudança de arranjo continuam disponíveis como intenção visual, respeitando o núcleo OBJ-01. Neste corte, sua tradução autoral é recusada (`unsupported_authorial_command`), pois não há contrato seguro entre essas operações e as autoridades existentes.

## Limites, atualização e aplicação futura

O pedido admite até **64 comandos, 20.000 entradas de IDs, 512 bytes por ID e 4 MiB agregados de IDs**. Conta IDs de layouts, âncoras, seleções, pintura e atribuições de frames antes de construir índices ou executar comandos. Os limites das pontes e do núcleo continuam valendo. Não são quotas de RSS nem limites de ingestão para um parser futuro.

`check_source` do plano revalida o agregado fornecido e compara o stamp original. Documento inclui hash integral; Composição inclui também projeto, ID, revisão e timestamp. Mudança somente em metadados ou rascunho invalida a correspondência. As entradas precisam permanecer estáveis durante a chamada; a API não sincroniza threads.

A verificação não consulta o banco, bloqueia outro escritor, concede permissão ou impede uma alteração entre checagem e save. Não existe `apply`/`save` neste módulo. Um futuro executor precisará conferir a fonte na autoridade e transação adequadas, distinguir estado apresentado/aprovado/aplicado, tratar falha sem sobrescrever concorrência e integrar o histórico/recovery pertinente. Validar o grafo ou comparar o hash em memória não libera uma escrita persistente.

Falha de alocação pode propagar; as fontes permanecem constantes. Outras falhas tornam-se diagnósticos privados, sem candidato parcial. Não há normalização silenciosa, alteração de schema, migração autoral ou mudanças em IDs públicos.

## Construção e evidência

O build mínimo `tests/object_change_plan` estende as duas pontes com a sessão de edição existente: **26 unidades reais do domínio/codec**, sem configurar GTK/SQLite. Compila modelo, comandos, pontes, planejador e testes com avisos como erros. A exceção Clang preexistente de conversão de sinal permanece restrita aos fontes antigos. GCC/Clang × sanitizadores compõem quatro jobs novos da CI.

O teste nativo adicional usa `ProjectService` e SQLite reais em diretório temporário recém-criado, compara todos os arquivos antes/depois do planejamento e verifica conflitos de fontes após escritores sequenciais sintéticos. Não abre uma obra autoral. O núcleo nativo completo é Release; a instrumentação ASan/UBSan fica no build mínimo, que inclui as rotinas proprietárias pertinentes.

Ver [validação e reprodução](../validacao/VALIDACAO-OBJ-01-PLANOS.md), [fundação](INDE-OBJ-01.md), [ponte documental](ADAPTADOR-DOCUMENTO-OBJ-01.md) e [ponte da Composição](ADAPTADOR-COMPOSICAO-OBJ-01.md).

## Continuidade: execução em memória

O corte seguinte acrescentou [sessões de memória](SESSOES-OBJ-01.md) que preparam/aplicam tickets, conferem origem e coordenam undo/redo. Os planos continuam imutáveis e sem API de save/apply própria. Novas sobrecargas permitem apresentação validada da mesma captura; intenção autoral exige ordem anterior alinhada à fonte. Escrita persistente continua fora da implementação.

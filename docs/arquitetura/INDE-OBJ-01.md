# INDE-OBJ-01 — Contrato da fundação de objetos e operações

Versão **0.7 experimental — pontes, planos, sessões e protocolo de save/recovery de laboratório**, 2026-11-20. Escopo: **Fase 1**, C++20 em memória. Implementação: `cpp/object_model.{hpp,cpp}` e `cpp/object_commands.{hpp,cpp}`. O núcleo de comandos permanece isolado. Bibliotecas separadas fazem leitura do Documento (`object_document_adapter.*`) e da Composição persistida (`object_composition_adapter.*`), sem escrita, GTK, alterações SQLite ou arquivo único `.inde`. Ver [ponte documental](ADAPTADOR-DOCUMENTO-OBJ-01.md), [ponte da Composição](ADAPTADOR-COMPOSICAO-OBJ-01.md) e [planos de alterações](PLANO-ALTERACOES-OBJ-01.md). O planejador separado traduz prévias autorais conservadoras pelo motor existente; não aplica alterações na fonte.

## Autoridades e identidades

Objeto tem identidade e tipo lógico (Paragraph, Image, Table, Equation, Container, TextBlock, TextReference). Ocorrência é um uso identificado do objeto em uma composição. O mesmo objeto pode ter diversas ocorrências; mover uma delas não modifica as demais. Recursos são metadados lógicos (ID, SHA-256 declarado, MIME, tamanho), sem caminhos autoritativos nem bytes incorporados.

A árvore ativa tem um layout raiz sem pai; cada outro layout tem exatamente um pai, e todos são alcançáveis. Ocorrências têm exatamente uma colocação. Contêiner aponta para layout interno exclusivo: reutilizar o mesmo objeto contêiner em duas ocorrências implicaria múltiplos pais e continua recusado. Instâncias reutilizáveis exigem contrato futuro, não relaxamento dessa invariante. Objetos/recursos de biblioteca sem ocorrência são admitidos; layouts órfãos não.

`objects::Composition` não substitui `project::Composition`. `ObjectKind::Paragraph` não contém texto nem referencia automaticamente um `DocumentParagraph`. IDs nesta fundação são opacos e únicos entre categorias; a ponte de leitura agora fornece mapas explícitos qualificados por projeto/documento, preservando IDs de parágrafos e o hash do snapshot completo. Esses mapas pertencem à projeção privada, não a uma nova autoridade de texto. Nenhuma equivalência é inferida de posição, nome ou conteúdo parecido.

TextBlock representa uma revisão capturada de bloco, que pode ter várias linhas; TextReference é navegação sem texto incorporado. Nas capturas de Composição, o objeto deriva de bloco/revisão e a ocorrência preserva o ID de colocação; o bloco lógico permanente permanece explícito no vínculo.

A ordem autoral do Documento continua com as estruturas textuais existentes. `Layout.children` é a ordem lógica de leitura **da composição OBJ-01**. `paint_order` é a pintura espacial de baixo para cima. Posição gráfica não define leitura, cronologia, causalidade, personagem, POV ou significado narrativo.

## Invariantes do validador

1. IDs não vazios/únicos e referências existentes, inclusive raiz e recursos.
2. Ocorrência em exatamente um layout; nenhum ciclo, pai duplo, raiz aninhada ou layout inalcançável. Travessias iterativas, sem recursão na pilha.
3. Container exige layout interno; demais tipos não admitem esse campo; Image exige recurso.
4. SHA-256 declarado contém 64 dígitos hexadecimais minúsculos; MIME usa forma conservadora `tipo/subtipo`; tamanho é positivo. Isto valida metadados, não os bytes do recurso. A conferência real de bytes entregue neste corte refere-se aos três pacotes de fontes.
5. Grid tem `columns > 0`; os outros layouts têm `columns == 0`.
6. Stack/Row/Grid não admitem frame absoluto. Overlay permite frame opcional. Freeform exige frame em cada ocorrência.
7. Posições, dimensões e arestas `x + width`, `y + height` finitas; dimensões estritamente positivas. Posições negativas são permitidas. Overflow das arestas é recusado.
8. `paint_order` não vazio só existe em Overlay/Freeform e é permutação exata dos filhos, sem duplicatas, faltas ou IDs externos. Vazio conserva o default anterior: pintar na ordem de `children`. Antes de uma alteração real da ordem de leitura, comandos espaciais congelam esse default para preservar a pintura.
9. Enums desconhecidos são recusados. Diagnósticos são determinísticos, por código e ID, sem reparos silenciosos. Validação não modifica o estado.
10. Preflight padrão limita a soma de objetos/ocorrências/layouts/recursos a 100.000 entidades, referências a 400.000 e strings a 32 MiB. Referências incluem filhos, pintura e vínculos. Contagem evita overflow e precede construção dos índices. `validate(c, limits)` permite limites explícitos para outros chamadores; `CompositionEditor` usa os limites padrão.

Esses limites são de entrada em memória já construída; não substituem um futuro parser com limites de ingestão. Complexidade esperada de validação: O(V + E + bytes), usando índices hash; colisões adversariais não têm garantia de tempo linear. O limite agregado de entidades não equivale ao limite textual existente de 100.000 identidades de parágrafos; esse mapeamento ainda precisa ser dimensionado na integração.

## Regras de arranjo e comandos

| Arranjo | Sequência / reordenação | Geometria e pintura |
| --- | --- | --- |
| Stack | Inserção antes de vizinho identificado ou ao fim; fluxo vertical | Sem frame ou pintura independente |
| Row | Mesma operação, interpretada como fluxo horizontal | Sem frame ou pintura independente |
| Grid | Sequência compacta em linhas; posição `i` corresponde a linha `i / columns`, coluna `i % columns` | Sem células reservadas, spans ou coordenadas; mover insere e desloca, não troca implicitamente |
| Overlay | Leitura independente do empilhamento | Frame opcional; pintura independente |
| Freeform | Leitura independente das coordenadas | Frame obrigatório; pintura independente |

`Frame` usa unidades abstratas **locais ao layout**, eixo x à direita e y para baixo. Não representa pixels, percentuais, centímetros nem coordenadas globais. Um contêiner movido conserva seus layouts/filhos locais; nenhuma transformação global é calculada. Limites de canvas, snap, colisão, recorte, alinhamento e mínimos percentuais do Comic não são importados como regras universais. Dimensões físicas e renderização permanecem fora desta fase.

| Comando C++ | Contrato |
| --- | --- |
| `ReorderOccurrences` | Seleção não vazia, sem duplicatas, toda no layout indicado. Reinsere na ordem relativa da origem antes de uma ocorrência não selecionada do mesmo layout, ou ao fim (`nullopt`). Ordem fornecida pelo chamador não inverte a seleção. Geometria, objetos e recursos permanecem iguais. |
| `TransferOccurrences` | Seleção de um único pai, destino diferente e âncora pertencente ao destino. Remove e insere atomicamente, conservando IDs, objetos e subárvores. Destino descendente do contêiner é recusado. Em destino espacial, itens transferidos entram no topo da pintura na ordem de leitura da origem; pintura dos demais permanece relativa. |
| `SetFrame` | Substitui o retângulo de uma ocorrência em layout espacial; serve a deslocamento/redimensionamento. Não altera leitura, pintura ou outros usos do objeto. Não executa snap implícito. |
| `SetPaintOrder` | Recebe permutação completa dos filhos em Overlay/Freeform. Não altera ordem de leitura ou frames. |
| `ChangeLayout` | Troca arranjo/colunas e aplica política explícita de frames. Valida todos os filhos. Entrada em espacial inicializa pintura se necessário; saída de espacial retira o campo de pintura, conforme o novo arranjo. Estado anterior continua no undo. Não cria posições ou dimensões. |

Transferência e mudança de arranjo usam `FramePolicy`:

- `Preserve` conserva valores locais existentes; não recebe substituições. Se o destino proibir frames ou exigir frames ausentes, a operação falha. Preservar valores em dois sistemas locais distintos é uma escolha explícita do chamador, não conversão de escala.
- `Clear` remove frames explicitamente; não recebe substituições. Freeform não aceita esse resultado enquanto houver filhos.
- `Replace` exige exatamente uma atribuição por ocorrência afetada, sem IDs duplicados/externos. Todos os frames precisam ser válidos e compatíveis com o destino.

Não existe perda silenciosa de geometria para fazer uma transferência passar. Mudar para fluxo dispensa a ordem de pintura por definição desse arranjo; voltar a espacial inicializa nova ordem, e desfazer restaura exatamente a anterior. Preservação de preferências de arranjos inativos exige modelo futuro separado.

Nenhum comando do núcleo modifica seleção GTK, texto, âncoras, alternativas ou identidades públicas. O planejador separado pode traduzir uma intenção autoral explícita em candidato privado pelo motor proprietário, sem modificar a fonte fornecida. Seleção aqui significa apenas conjunto explícito de IDs de ocorrência de um comando. Seleções misturando pais ou ancestrais/descendentes não são implicitamente expandidas. Âncora selecionada, ausente ou em outro layout é erro; índices de uma vista filtrada não fazem parte da API.

## Transações, revisão, histórico e falhas

`CompositionEditor` valida o estado inicial (construção inválida lança `invalid_argument`) e fornece apenas acesso constante ao estado atual. `apply(expected_revision, commands)` executa um lote em candidato privado, valida cada resultado intermediário completo e efetiva o lote somente quando todas as operações passam. Uma transferência individual já preserva a integridade sem expor seu estado parcial. Um lote não pode usar grafo intermediário inválido como ponte para outra operação.

Revisão começa em zero e cresce monotonicamente a cada mudança, undo e redo. Não é restaurada pelo histórico, prevenindo reutilização de revisão após desfazer. Revisão esperada divergente, revisão esgotada, comando inválido, estado inválido ou histórico vazio produzem resultado recusado com diagnósticos. Nenhuma recusa modifica estado, revisão, undo ou redo. Lote vazio e resultado estruturalmente idêntico são aceitos sem mudança de revisão, histórico ou redo.

Um lote bem sucedido forma uma unidade de undo. Undo/redo restauram snapshots exatos de IDs, relações, geometria, pintura e metadados; edição nova elimina redo. Histórico é limitado por **64 estados e 32 MiB** por padrão, configuráveis no construtor; tamanho conservador inclui capacidades de vetores/strings dos snapshots. Estados mais distantes são descartados para respeitar os limites. Se um snapshot sozinho exceder o orçamento, a edição é aceita mas pode ficar sem undo. Orçamento zero desativa retenção. O orçamento mede snapshots retidos, não RSS, capacidade vazia dos vetores de histórico, nem candidato temporário durante uma operação. Para composições grandes, cópias/deltas mais econômicos continuam trabalho futuro.

Máximo de 4.096 comandos por lote; o payload agregado dos comandos tem preflight de 400.000 entradas de seleção/pintura/atribuição e 32 MiB de strings antes de índices. Custo esperado de lote: O(B × (V + E + bytes)), além das cópias/histórico limitado. Testes de milhares de operações comprovam integridade nas entradas ensaiadas; não certificam latência interativa de documentos arbitrários.

Falhas de alocação podem lançar exceções, conservando o estado/histórico anterior: os trabalhos que alocam precedem o commit, e movimentos do agregado são verificados como `noexcept`. A API não é sincronizador entre threads; revisão esperada é proteção de comandos dentro da sessão. Não substitui lock, transação SQLite, revisão/hash persistido, recovery ou controle de acesso.

## Fronteira de compatibilidade e próximos gates

O `.inde` atual permanece diretório, com `data/project.sqlite3`, `data/publicacao.sqlite3`, `data/comics.sqlite3` e recovery próprio. Nenhum arquivo foi convertido. O schema autoral continua 37; códigos dos migradores, codecs e serviços existentes não foram alterados.

Antes de integrar, será necessário especificar e testar o vínculo com parágrafos/revisões e todas as autoridades inventariadas: texto ativo, folhas cobertas, referências semânticas, seleção/visibilidade, revisões aplicadas, origens congeladas, IDs públicos, undo/recovery e Save As. O grafo OBJ-01 ainda não possui payload textual, persistência, mídia incorporada, serializador ou exportador. A ponte separada resolve conteúdo de parágrafos sobre um snapshot integral privado, sem copiar texto para o grafo nem permitir escrita de volta. Recursos declarados não têm prova de integridade de bytes no modelo.

GTK, instâncias de contêiner, inclusão/remoção, wrap/unwrap, clipboard, geometria responsiva, spans de grade e arquivo único `.inde` permanecem futuros e não foram iniciados. Uma futura migração exigirá backup consistente SQLite (inclusive WAL), validação em cópias, reversibilidade e seleção explícita do conteúdo público. Visibilidade não autoriza publicar e arquivos de autoria/credenciais não pertencem ao pacote público.

Ver [inventário aprofundado](INVENTARIO-OBJ-01.md) e [relatório de validação](../validacao/VALIDACAO-OBJ-01.md).

O requisito de plano explícito está implementado de forma limitada e verificável: intenção visual preserva o agregado; intenção autoral admite um grupo documental inteiro ou reordenação de ocorrências da Composição. O planejador entrega candidatos imutáveis; a aplicação em memória pertence à biblioteca separada de sessões, sem escrita persistente. Ver [contrato](PLANO-ALTERACOES-OBJ-01.md) e [validação](../validacao/VALIDACAO-OBJ-01-PLANOS.md).

A aplicação controlada desses candidatos agora existe em [sessões privadas de memória](SESSOES-OBJ-01.md), com tickets vinculados à revisão, conferência da origem e histórico conjunto. Não aplica alterações a serviços, arquivos ou bancos. [Validação](../validacao/VALIDACAO-OBJ-01-SESSOES.md).

O [protocolo de save/readback e checkpoint de laboratório](SALVAMENTO-RECOVERY-OBJ-01.md) agora permite reconhecer gravações pelos serviços existentes e ensaiar reinício em fixtures. A biblioteca continua sem escrita em arquivos/bancos; o teste nativo é o único executor de escrita deste corte. [Validação](../validacao/VALIDACAO-OBJ-01-SALVAMENTO.md).

# OBJ-01 — aparência e medidas de tabelas

Data: 2026-11-22. Base `08c6e8a1f63aebac77d72316cbe0974c1de78ea0`; implementação `59d551207f326fd2a827c041c61b8edd99a233f6`; branch `feature/inde-obj-01-foundation`. [Roteiro utilizável](../../tools/obj01-pilot/TABELAS.md), [validação](../validacao/VALIDACAO-OBJ-01-TABELAS-VISUAIS.md) e [composição comum anterior](COMPOSICAO-REMOCAO-OBJ-01.md).

## Contrato

O usuário pediu aparência, estilo, customização, tamanho e alinhamento das tabelas. O conteúdo/identidades, fórmulas e arranjos existentes são conservados. O editor modal oferece prévia com os mesmos estilos usados na tabela real, sem motor paralelo para calcular fórmulas. O conteúdo da prévia vem do estado real; não é editado no modal. Cancelar/Esc conserva o estado anterior. Aplicar confirma o conjunto.

| Camada | Escolhas | Regra |
| --- | --- | --- |
| Tabela no documento | Largura, posição horizontal, texto geral e posição vertical no par | Usa a Presentation comum por ocorrência, conservando pares/leitura |
| Aparência | Fontes, tamanho/entrelinha, cores, faixas, cabeçalho e bordas | Modelo fechado, sem CSS livre; fontes locais |
| Título | Texto, visibilidade, alinhamento, acima/abaixo | Título semântico conservado quando oculto visualmente |
| Medidas internas | Espaços, alturas mínimas geral/por linha, proporções de colunas | Texto expande a linha; proporções completam 100% |
| Detalhes | Célula, linha ou coluna; alinhamento, negrito/itálico e cores | Mudança de uma propriedade conserva as demais; ausência significa herdar |

Quatro modelos C++: Editorial, Caderno, Grade e Noite. São pontos de partida completos da aparência, sem segundo modelo persistido de "preset". A interface conserva exceções de células/linhas/colunas ao escolher outro modelo. Restaurar aparência padrão limpa essas exceções, conservando título/cabeçalho e posição do objeto. A altura específica pode voltar à geral independentemente do restante.

A aparência é privada por ocorrência de tabela. Cores são `#RRGGBB`; enumerações e medidas são verificadas no servidor. Fonte 10–32, entrelinha 1–2,2, espaços 0–32, espessura 0–4, altura geral 24–240 e por linha 24–480. Pesos de colunas finitos/positivos, mesma quantidade das colunas, soma 100 com tolerância 1e-6. Identidades de linhas e células têm de pertencer à tabela; não há seleção por coordenada aproximada na persistência. Texto/conteúdo não recebe CSS ou HTML por concatenação.

## Fonte de verdade, operação e persistência

`edit_table_design` recebe id, título, cabeçalho, aparência completa e placement comum. Exige Composição, revisão corrente e tabela ativa. Campos desconhecidos/faltantes, tipos, cores, limites e identidades são recusados atomicamente. A revisão de payload só aumenta quando título/cabeçalho muda; fonte, textos de células e fórmulas não são reescritos pela aparência.

A operação passa pelo candidato/validação/checkpoint existentes. Undo/redo inclui aparência, medidas, título/cabeçalho e seleção, sem mexer em pares ou ordem de leitura. Remoção conserva aparência da tabela arquivada e restauração a reutiliza. Histórico em memória reinicia ao abrir, mas trabalho/estilos e itens retirados ficam persistidos.

Snapshot privado `INDE-OBJ01-COMPOSITE-P2D2-3` acrescenta `composition.table_styles`. Os formatos 1/2 continuam legíveis, estritos e sem conversão automática ao apenas abrir/salvar. A primeira edição usa V3; o binário V2 recusa a versão nova sem regravar origem/banco. P2-D2, linguagem de fórmulas 2 e SQLite auxiliar `user_version=5` permanecem; nenhuma tabela/coluna ou schema canônico mudou.

A API devolve aparência efetiva, defaults e modelos C++ como vistas. Capacidade `table_design_supported` impede oferecer os controles em motor anterior. O orçamento de estilos acompanha até 64 tabelas conservadas e suas linhas/células válidas, sem registros órfãos. Permanecem os limites de texto, fórmulas, memória e snapshot anteriores.

## Interface e limites

Três áreas: Aparência, Tamanho e posição, Células. Medidas redistribuídas atualizam campos e prévia. O editor espera operações pendentes antes de abrir, preservando texto recém-digitado. Células crescem com o texto e com a fonte, sem resize manual ou rolagem interna obrigatória. Fórmulas herdam cor/letra/alinhamento, conservando resultado protegido e acesso à fonte. O título escuro recebe o fundo correspondente para permanecer legível.

A criação estrutural continua 2×3. Fora do corte: acrescentar/remover linhas/colunas, mesclar, formatos numéricos, bordas/medidas independentes por célula, resize por alças, GTK, publicação canônica, migração de obras e `.inde` único. Medidas são da interface local, sem promessa de tipografia/exportação canônica. Validação técnica não é aceite humano ou conclusão de fase inteira.

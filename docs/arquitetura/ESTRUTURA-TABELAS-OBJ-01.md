# OBJ-01 — Estrutura recuperável das tabelas

Data: 2026-11-22. Continuação do [editor de aparência](TABELAS-VISUAIS-OBJ-01.md), sobre a base `26adaa22995def08b5ea2e269e9ef3a193d398c8`. Implementação `90939bb4652d1ba3452ccefb62174c8dea68ccbd`, branch `feature/inde-obj-01-foundation`. [Demonstração e roteiro](../../tools/obj01-pilot/ESTRUTURA-TABELAS.md); [validação](../validacao/VALIDACAO-OBJ-01-ESTRUTURA-TABELAS.md).

## Resultado e autoridade

O piloto cria tabelas com dimensões escolhidas e insere, retira e restaura linhas/colunas. A operação conserva o conteúdo e as identidades existentes. A aparência acompanha a estrutura atual. A fonte textual original, a sequência externa de objetos, os pares lado a lado e suas intenções não são reescritos por uma mudança interna de tabela.

Este é um refinamento do piloto composto já autorizado, no percurso reconciliado. Não promove o experimento a editor canônico, não declara concluída toda a composição documental e não inicia a Fase 3. O contrato universal e a auditoria histórica continuam sendo a referência; a capacidade nova exige prova no agregado isolado antes de qualquer integração.

## Grade conhecida e grade visível

`Table.rows`, `Table.columns` e `Table.cells` mantêm a grade completa conhecida, com IDs estáveis. `composition.table_structure[table_id]` registra listas de IDs retirados em `removed_rows` e `removed_columns`. A projeção entregue à interface exclui os eixos retirados e suas interseções, sem apagar os registros persistidos.

Invariantes:

- Entre 1 e 64 linhas conhecidas, entre 1 e 32 colunas conhecidas; exatamente uma célula por interseção, até 2.048 células por tabela. Os limites incluem eixos retirados.
- IDs conhecidos e únicos; máscaras sem duplicatas, sem referência estrangeira e com pelo menos um eixo visível de cada espécie. Metadados participam do limite agregado de bytes do documento.
- Inserir cria ID de eixo e IDs de células novos em todas as interseções conhecidas, inclusive com eixos retirados. Células novas começam vazias.
- Retirar acrescenta uma máscara; restaurar remove a máscara e devolve o eixo à posição relativa da grade conhecida. A operação não rearranja IDs remanescentes.
- O cabeçalho é a primeira linha visível. Não está vinculado permanentemente ao ID da primeira linha criada.
- Inserir/retirar/restaurar incrementa a revisão do payload da tabela, seleciona a tabela e limpa a seleção de célula.
- Tabela retirada inteira conserva sua estrutura como recurso retido. Não há descarte definitivo nem reaproveitamento silencioso de IDs.

As células no vetor persistido não precisam estar ordenadas por apresentação; a interface e o editor de aparência percorrem `row_ids` e `column_ids` e localizam as interseções por identidade.

## Operações e recusas

Todas exigem revisão esperada, perfil P2-D2 e instrumento Estrutura. O candidato completo é validado e confirmado pelo protocolo de checkpoint existente; qualquer recusa conserva o estado anterior.

| Operação | Entrada além de `op`/`expected` | Efeito e recusa |
| --- | --- | --- |
| `create_table_sized` | `before`, `title`, `header`, `rows`, `columns` | Contagens inteiras nos limites; células vazias. Criação exige alinhamento da leitura/apresentação, como a API anterior. |
| `insert_table_axis` | `id`, `axis`, `before` | `axis` é `row` ou `column`; `before` é ID de eixo visível da tabela ou vazio para o final conhecido. Recusa limite de retenção. |
| `remove_table_axis` | `id`, `axis`, `axis_id` | Retira eixo visível; recusa o último eixo visível e referências inexistentes/inativas. |
| `restore_table_axis` | `id`, `axis`, `axis_id` | Restaura somente um eixo atualmente retirado da tabela. |

A interface oferece inserir antes/depois e calcula o ID de referência. A API anterior `create_table`, com seis textos iniciais e criação 2×3, permanece inalterada para os perfis e clientes anteriores. Campos extras, tipos errados, alvos estrangeiros e revisão desatualizada são recusados.

`edit_cell`, `select_cell`, anexação/criação de fórmula e edição de texto misto exigem célula visível. Para editar conteúdo retirado, o autor restaura primeiro os eixos necessários.

## Fórmulas, aparência e histórico

Fórmulas são recursos lógicos independentes da posição da célula. As máscaras não removem fonte, bindings, resultados, frases ou dependências. Fórmulas retiradas continuam na biblioteca e podem alimentar outras fórmulas. Um comando retirado não conta como uso ativo na tabela; retirar/restaurar não executa nem consome esse comando.

O estilo canônico continua referido à grade conhecida. A projeção de aparência filtra exceções de células/alturas e normaliza as proporções das colunas visíveis. Ao aplicar uma edição visível, a mesclagem substitui somente as exceções visíveis e conserva as retiradas. IDs ocultos enviados diretamente pelo cliente são recusados. A fração de largura das colunas retiradas fica reservada; a edição redistribui a fração visível. Inserir coluna dá à nova coluna uma parcela igual a `100 / nova_quantidade_conhecida` e reduz proporcionalmente as anteriores. Distribuição igual sem personalização continua representada por lista vazia.

Proporções canônicas são finitas e estritamente positivas, com soma 100 dentro de tolerância `1e-6`; a interface oferece valores a partir de 0,1%. A projeção pode produzir frações menores, que não devem ser arredondadas destrutivamente no armazenamento.

Novas células herdam o estilo geral, sem inventar uma intenção permanente a partir de exceções antigas de célula. Aplicar um detalhe à linha/coluna inteira significa alterar as células visíveis atuais. Células novas exigem reaplicar essa escolha se desejado.

Cada operação interna forma um passo de undo/redo e usa o mesmo checkpoint do documento. A auditoria encontrou e corrigiu uma falha concreta: ao desfazer uma fórmula criada numa linha nova, a retenção de frases tentava copiar o endpoint futuro para um snapshot anterior à célula, tornando inválido o passo anterior de inserção. A retenção agora copia frases somente para endpoints existentes no snapshot de destino. O teste desfaz fórmula e inserção, depois refaz ambas e confere o binding. A frase do estado futuro continua no estado de refazer.

O histórico em memória continua limitado pela política existente e não sobrevive ao reinício. Conteúdo confirmado, máscaras, fórmulas e estilos são recuperados pelo checkpoint; Salvar e recuperação de trabalho permanecem distintos.

## Formato e preservação

Snapshot privado novo: `INDE-OBJ01-COMPOSITE-P2D2-4`. `composition.table_structure` é explícito e exige os recursos de composição/aparência. Formatos P2D2-1/2/3 continuam legíveis e mantêm sua representação até uma operação nova. O SQLite auxiliar continua `user_version=5`, sem DDL novo, e a linguagem de fórmulas continua 2. O motor da base V3 recusa V4 sem regravar origem ou banco.

Não mudam neste corte: arquivos compactados, metadados das fontes, modelo/comandos universais, adaptadores Documento/Composição, planos/sessões/escrita/checkpoint, motor de fórmulas, cartões e hub. As mudanças estão no agregado/UI do piloto, auxiliares tipados de aparência, testes e documentação. Main e projetos autorais ficam preservados.

## Riscos ligados às provas

| Risco | Invariante | Prova deste corte |
| --- | --- | --- |
| Índices novos trocarem textos/fórmulas | Células e eixos por ID estável | Inserções antes/depois, conteúdo Unicode, binding e estilo por ID; navegador 3×4 → 5×5 |
| Retirar linha e coluna perder a interseção | Grade conhecida completa, máscara separada | Retirada cruzada, inserção com eixos ocultos, reinício e restauração |
| Aparência visível apagar conteúdo oculto | Projeção + mesclagem por identidade | Cor/altura ocultas preservadas; tentativa direta em ID oculto recusada; pesos restaurados |
| Retirada consumir comando | Contagem apenas de usos visíveis; nenhuma execução implícita | Comando oculto permanece não consumido e variável original não muda |
| Histórico perder inserção anterior | Frases somente em endpoints conhecidos no snapshot | Regressão reproduzida antes da correção e sequência completa de undo/redo aprovada |
| Falha parcial substituir salvo | Candidato/checkpoint indivisível | Três operações × quatro janelas de crash; SIGKILL real e comparação antes/depois |
| Crescimento invisível sem limite | Retirados continuam na quota | Último eixo protegido; 64×32 aceito; retirada não libera quota |
| Motor antigo reinterpretar V4 | Versão explícita e recusa | Binário anterior contra cópia isolada, bytes de origem/banco inalterados |

Ficam pendentes mesclagem, reordenação interna por arraste, descarte definitivo, alças de resize, formatos numéricos, bordas individuais e integração GTK/publicação. Esta entrega não comprova todos os dispositivos, concorrência ampla, desempenho, todas as panes de armazenamento ou aceite humano.

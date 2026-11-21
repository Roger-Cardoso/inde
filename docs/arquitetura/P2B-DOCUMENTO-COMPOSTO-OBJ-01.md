# OBJ-01 — P2-B: tabela estruturada e edição reversível

Data: 2026-11-21. Base `8ea6c233f50529de9c9894a62b925d90941157a1`. Implementação principal `ee8e297`; correção de destino inicial da interface `e0aa9af`. Branch `feature/inde-obj-01-foundation`. Ver [como abrir](../../tools/obj01-pilot/P2B.md) e [validação](../validacao/VALIDACAO-OBJ-01-P2B.md).

O usuário esclareceu que a confirmação precavida servia à discussão anterior do planejamento e autorizou a continuidade do trabalho sem repetição desse procedimento. A continuidade segue o plano discutido e as restrições de preservação; aceite humano, execução técnica, merge e publicação continuam registros distintos.

## Conteúdo e identidades

O mesmo núcleo experimental admite agora perfis explícitos `P2A` e `P2B`. O perfil padrão continua P2-A. P2-B acrescenta tabela com ID de objeto/ocorrência, título, decisão de cabeçalho, revisão de conteúdo e vetores de IDs de linhas, colunas e células. Cada célula possui ID, linha, coluna e texto UTF-8. Há exatamente uma célula por par linha/coluna; o validador recusa colisões, referências inválidas ou forma incompleta.

A criação inicial é 2 × 3. O codec admite o perfil limitado a 64 linhas × 32 colunas, mas não há comandos de inserção/remoção de linhas/colunas neste corte. Uma tabela tem um uso incorporador; sua duplicação compartilhada fica pendente. Fórmulas, mesclagens e rich text não foram acrescentados.

Edição identifica ocorrência e célula, exige revisão esperada da sessão, modifica somente o texto escolhido e gera revisão de conteúdo quando o texto muda. A seleção é ocorrência + célula opcional; célula selecionada precisa pertencer à tabela ativa selecionada. O texto das células não é incorporado ao corpo/offsets do Documento proprietário.

A sequência autoral única continua incluindo objetos materiais e filtrando exatamente para a ordem de parágrafos da origem. A tabela só é inserida/movida entre grupos completos. Movimento visual e autoral permanecem explícitos; intenção autoral exige alinhamento prévio. Tabela fica na coluna principal; Row continua limitado ao par figura/parágrafo.

## Histórico, remoção e conservação

P2-B possui histórico privado de estados do agregado: texto capturado, payloads, objetos/usos, recursos, sequências, Rows e seleção. Revisão da sessão é monotônica inclusive em undo/redo; revisão de conteúdo pode retornar à revisão histórica restaurada. Save não apaga o histórico vivo. Reabrir salvo e iniciar outro processo abrem sessão sem histórico anterior.

Remover ocorrência de figura/tabela retira o uso das sequências e conserva o conteúdo em registros de biblioteca (`archived_figures`, `archived_tables`). Recursos PNG são bytes imutáveis compartilhados por ponteiros constantes entre estados. Remover o último uso ou desfazer a importação não apaga bytes nem a identidade lógica. Isso também permite salvar após undo sem gerar referência incompleta no banco.

A biblioteca conhecida é conservada ao reconstituir estados históricos; esses registros inativos não entram na ordem de leitura. Portanto, desfazer toda mudança visível pode ainda deixar novos registros de biblioteca para salvar. Não há coleta automática ou exclusão definitiva de recursos.

O histórico tem até 32 entradas e 32 MiB de metadados serializados, incluindo o estado corrente; a união de blobs retidos é limitada a 32 MiB. Entradas antigas são normalizadas contra a biblioteca conservada e retiradas quando excedem orçamento ou deixam de formar um estado válido. Somente nesse caminho privado, com mídia imutável previamente verificada, evita-se repetir decodificação PNG para cada entrada histórica. Importação, load, publicação do candidato e replay validam a mídia normalmente.

Uma nova edição após undo invalida redo. Seleção de ocorrência/célula não apaga redo. Edição sem mudança de texto não cria outra revisão de conteúdo ou entrada de edição. Recusas conservam estado, histórico e seleção anteriores.

## Formato e compatibilidade

P2-A mantém `user_version=1` e snapshot `INDE-OBJ01-COMPOSITE-P2A-1`, com os mesmos campos persistidos. P2-B usa `user_version=2` e `INDE-OBJ01-COMPOSITE-P2B-1`, acrescentando tabelas, biblioteca inativa e seleção de célula. O DDL do laboratório permanece `saved` + `blobs`; não há migrador canônico nem escrita em schema 37.

O mesmo executável escolhe o perfil explicitamente. Um perfil recusa abrir o formato do outro; não existe atualização automática do banco anterior. `--seed-p2a` só funciona para uma pasta P2-B nova: abre a origem SQLite com `READONLY`, lê uma transação consistente e confere origem, snapshot e blobs. A cópia conserva IDs/bytes, cria outro banco e não copia trabalho ainda não salvo. Os testes verificam os bytes da origem antes/depois, inclusive com P2-A vivo.

Save conserva CAS de revisão/hash, transação SQLite e readback do agregado completo. Versão desconhecida, célula inválida, origem alterada, recurso incompleto ou conflito recusam a operação. Strings possuem orçamento agregado de 8 MiB; célula tem até 4.096 escalares Unicode; snapshot serializado tem até 24 MiB. Não se aplica truncamento.

## Interface

A interface continua local, sem GTK, delegando o modelo ao C++. Textareas são editores de células identificadas, e não outra autoridade persistente. Pedidos são serializados; foco/edição de célula não reconstruem toda a árvore DOM. Tab/Shift+Tab aplicam a célula e navegam pela sequência declarada de IDs de linha/coluna; Ctrl+Enter aplica sem sair. O último Tab sai da grade, sem criar linha automaticamente.

Save e operações estruturais aplicam campos em edição antes de prosseguir; falha de validação impede o save e conserva o rascunho na interface. Durante operação estrutural os campos ficam temporariamente em leitura. Reabrir salvo permite descartar explicitamente trabalho pendente. Seleção de tabela inteira fica distinta da edição da célula.

O teste visual confirmou edição Unicode, Tab/Shift+Tab, save sem Tab prévio, undo/redo, movimento da tabela, remoção/undo e reabertura por processo novo com as seis células e seleção conservadas. O destino inicial de inserção usa um limite válido ou o fim do documento, evitando menu sem rótulo quando um parágrafo pertence a Row.

## Estado do plano

P06 e P07 têm prova técnica no perfil descrito. P08 tem provas de figura/tabela e agregado em sessão viva, com as limitações de histórico acima. P09 agora inclui células, recursos, ordens, perfis e seleção por ID em processo novo. O Documento de origem continua somente leitura e sintético; não se alega abertura de projeto SQLite schema 37 completo.

P10 (reordenação textual autoral), P13 (trabalho não salvo recuperável) e o contexto/publicação completos de P14 permanecem pendentes para o percurso seguinte. O corte não entrega recuperação de trabalho não salvo ou histórico entre reinícios, GTK, Comic compartilhado, migração de obras ou arquivo único `.inde`. O aceite humano da demonstração permanece separado da aprovação técnica.

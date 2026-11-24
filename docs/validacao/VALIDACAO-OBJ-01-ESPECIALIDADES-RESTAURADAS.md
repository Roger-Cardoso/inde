# OBJ-01 — resgate dos menus, ferramentas e reordenação

Data: 24/11/2026. Branch `feature/inde-obj-01-foundation`. Validações local e CI concluídas; aceite humano aberto.

## Problema

A folha comum passou a exibir os mesmos parágrafos e objetos em todas as especialidades, mas os menus especializados e a margem de reordenação continuaram presos ao campo antigo. O novo painel tinha grupos apenas para Composição, Tabelas, Fórmulas e Matemática. Trocar de aba e conservar o texto não comprovava que uma especialidade permitia realizar seu trabalho.

## Referência recuperada

O corte anterior à folha comum, `0a2631ce1d43732c3a5309d0750ee0c9dbaf0d29`, e as fontes integradas fixadas preservam os comandos e layouts anteriores. O registro de `ContextMenuAction` de `writing_workspace_clipboard.cpp` contém ações comuns, Estrutura, Linguística, Semântica e Vínculos, incluindo disponibilidade da seleção, filtro por especialidade, rótulos, ícones e separadores.

As fontes fixadas não foram editadas. O overlay determinístico conserva esse registro em `WritingWorkspace`, tanto para o editor legado quanto para os parágrafos da folha comum. As operações continuam chamando os mesmos caminhos de aplicação. As verificações de disponibilidade e visibilidade são refeitas na abertura do menu.

## Restauração

| Especialidade | Ferramentas e comportamento recuperados |
|---|---|
| Texto | Estatísticas, metas, formatação existente, ações comuns e sugestão de alteração |
| Semântica | Marcações, realce, comentário, marcação da seleção e inspetor original |
| Linguística | Recorrências, parágrafos longos, dicionário e construções de significado |
| Estrutura | Identidades, folhas, alternativas, lentes, comandos de mover e círculos de reordenação na folha comum |
| Vínculos | Trechos marcados, elementos da história, referências, relações e planejamento do texto |
| Composição, Tabelas, Fórmulas e Matemática | Ferramentas existentes mantidas no mesmo painel e menus próprios dos objetos |

Ao trocar de especialidade, a rolagem começa no topo. Os painéis clássicos preservam uma largura útil de 390 pixels; os novos mantêm 280 pixels. O painel recebe os controles C++ anteriores, com seus callbacks originais. Não são cópias decorativas. A montagem é desfeita ao sair do Documento, restituindo os controles à superfície legada quando necessária. As seções de análise e consulta são filtradas por especialidade. F4 e o botão Ferramentas operam o mesmo painel. Abrir uma seção traz seu conteúdo para a área visível após a alocação GTK.

O menu dos parágrafos combina as ações especializadas anteriores com as ações pertinentes do objeto. Os demais objetos mantêm seus menus tipados; ações que exigem seleção textual não são oferecidas sobre uma imagem ou tabela usando silenciosamente outra seleção. Botão direito e Shift+F10 são suportados. As listas conservam ícones e separadores, mantêm largura legível e usam rolagem para caber na janela. As ações herdadas de cada parágrafo são montadas quando seu menu é aberto, evitando criar antecipadamente toda a lista para cada parágrafo.

O clique direito é traduzido das coordenadas da vista de parágrafo para o buffer autoritativo. Uma seleção que contém o ponto clicado é preservada, inclusive atravessando parágrafos. O foco não trunca essa seleção. A restauração automática de foco não substitui a tabela ou outro objeto escolhido; o contorno visual acompanha a seleção atual. O menu sobre outro trecho atualiza o cursor para aquele alvo.

## Reordenação e invariantes

Estrutura mostra círculos para arrastar e permite Ctrl+seta acima/abaixo, os comandos do painel e do menu. O foco acompanha o círculo após a reconstrução, permitindo movimentos sucessivos. A borda do vizinho indica antes/depois; a rolagem na borda do painel acompanha o arrasto. Escape cancela.

A operação usa `move` com intenção autoral no backend existente. Texto e novos objetos pertencem à mesma sequência. Um grupo textual indivisível é movido inteiro e recebe um círculo no primeiro parágrafo visível. As identidades, marcações e vínculos seguem o movimento do bloco pelo motor existente. A atualização da folha também é solicitada para alterações de metadados feitas pelas ferramentas antigas.

Destinos são resolvidos pela identidade. Uma linha lado a lado é tratada como destino inteiro; não se insere entre seus filhos. Mudança de revisão durante o arrasto recusa a ação; desaparecimento do alvo não vira movimento silencioso para o fim. Apresentação que divergiu da ordem autoral continua exigindo a decisão explícita de restaurar a apresentação pela leitura antes do movimento autoral. Não há realinhamento automático.

Os históricos existentes de texto e objetos permanecem distintos. Este corte não declara um histórico global novo. Os testes exercitam o Desfazer de movimentação da composição e sua correspondência com o Documento autoral.

## Verificação

O percurso X11 em projeto descartável foi ampliado para abrir as ferramentas das nove especialidades, conferir menus diferentes nas cinco anteriores, abrir comentário e dicionário com a seleção correta, usar o inspetor semântico, estatísticas, meta e identidades, criar um trecho marcado em Vínculos, mover texto pelo teclado e pelo arrasto e mover imagem na sequência. Movimentos consecutivos por teclado verificam retenção de foco; Desfazer restaura o conteúdo e a ordem. Após salvar e encerrar normalmente, o banco é lido para verificar o trecho marcado, offsets, texto, objetos e integridade.

A prova isolada de projeção também verifica que preparar um menu não trunca a seleção que atravessa parágrafos. As verificações nativas anteriores, publicação, recuperação e compatibilidade continuam obrigatórias.

Código final: `bc636c18aa288972b8badfde38973ff6abc8c045`. Suíte Python completa: **98/98**, **101.557 s**, incluindo quatro verificações nativas e o percurso X11 ampliado. As evidências ficam em [evidencias-obj01-especialidades](evidencias-obj01-especialidades/).

| Conferência | Resultado |
|---|---|
| Arquivos de código fixados | 47 hashes e hash do executável local |
| Fontes integradas | 758 arquivos conferidos |
| Compactados originais | 3/3 hashes e inventários conferidos |
| Arquivos anteriormente protegidos | 83/83 inalterados |
| Projeto de demonstração fechado | Três bancos íntegros, sem violações de chaves estrangeiras |

A [CI final 45408449213](https://github.com/Roger-Cardoso/inde/actions/runs/45408449213), sobre o mesmo commit `bc636c18aa288972b8badfde38973ff6abc8c045`, concluiu com **26/26 trabalhos aprovados**, incluindo a compilação do aplicativo, os testes nativos vinculados ao projeto e o percurso obrigatório com teclado e mouse até o encerramento normal. O resultado completo está em `ci.json`; o trecho do trabalho nativo está em `ci-nativo-testes.txt`. Execute `python3 docs/validacao/evidencias-obj01-especialidades/verificar.py` para conferir os hashes, o código validado e a correspondência com a CI.

As falhas intermediárias foram investigadas em processos descartáveis, incluindo o ciclo de vida da área rolável dos menus e o posicionamento do inspetor semântico; não se experimentou em projetos autorais.

## Demonstração

As alterações estão no editor nativo normal. O projeto de exemplo é `dados/obj01-especialidades-r1/Especialidades.inde`, com perfil próprio, na janela **Objetos e matemática — INDE**. O Documento é **Matemática na narrativa**. O código do aplicativo é o mesmo usado ao abrir projetos normais.

```sh
bash scripts/demos/Abrir-demo-especialidades.sh
```

Use Ferramentas ou F4 nas especialidades; selecione texto e clique direito. Em Estrutura, arraste um círculo até a borda do vizinho ou use Ctrl+seta acima/abaixo. Os controles de mover anteriores operam essa mesma sequência. O projeto de exemplo pode ser alterado para o passe humano. Suas imagens e capturas são sintéticas; os projetos autorais não foram usados como laboratório.

As capturas de menu linguístico, inspetor semântico e círculos de Estrutura vêm do percurso X11 real, em display privado. Não são montagens.

## Limites

Não há aceite humano concluído, certificação de todos os métodos de entrada, uso prolongado com manuscrito grande ou teste de toda combinação de ferramentas. A rolagem de borda do arrasto e todos os arranjos possíveis ainda exigem cobertura mais ampla. A exportação dos objetos permanece fora deste corte. Não houve migração de schema, alteração da main ou publicação de conteúdo autoral.

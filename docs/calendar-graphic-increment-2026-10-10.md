# Calendário gráfico adaptativo — primeiro incremento

Revisão aceita pelo usuário em 10/10/2026: `outputs/revisao-interacao-calendarios-graficos-2026-10-10.md`.
Aceite da revisão autoriza a execução; não aprova o candidato anterior `a0d2849` nem o novo binário.

## Escopo implementado

- Projeção pura de calendário em `project/calendar_view`, sem banco ou escolha implícita.
- Componente GTK `CalendarPicker`, reutilizado em referências, intervalos, associação inicial,
  corte/alinhamento de adoções, criação de marcos, consulta mensal e prévia de definição.
- Meses efetivos por ano, intercalares, dias a partir de zero, períodos epagomenais,
  semanas próprias e disposição compacta para semanas muito curtas/longas ou ausentes.
- Mês visível independente dos limites escolhidos; navegar não modifica a data.
- Escolhas anuais/mensais preservam campos desconhecidos. Hora desconhecida não é 00:00.
- Dois limites de intervalo no mesmo componente, com extremidades e interior destacados.
- Horário sob expansão, nomes e limites do relógio autoral; sequência de unidades validada.
- Salto de ano por Enter e entrada direta explícita `ano; período; dia`, também parcial.
  Não há parser de linguagem natural nem conversão automática para gregoriano.
- Estados claros/escuros de seleção, faixa, foco e registros; células leves, não cartões.
- Um ponto de entrada de teclado na grade; setas, Home/End e PageUp/PageDown.
- Associação por marco/data com ordem numérica sob aprofundamento; rota legada da
  Cronologia direciona ao mesmo fluxo, sem pedir “ordinal do ano zero”.
- Consulta mensal permite lentes de origem/adoções; mostra alcance declarado e informa
  que conversão para consulta não autoriza criar um marco fora da vigência.
- Dia com marcador/contagem revela seus registros sob demanda. Limites de consulta explícitos.
- Busca por nome/data na biblioteca e no diálogo de referências existentes do texto.

Sem alteração de schema (25). Datas existentes não são migradas nem reescritas.
Persistência continua somente por confirmação; proteções de uso compartilhado e concorrência
permanecem. Eixos relativos não se transformam em eixos de dias por mero clique na grade.

## O que este incremento não conclui

1. Criar e vincular uma **nova** referência sem sair da escrita: requer operação composta
   atômica e integração com histórico/autosave/recuperação. O diálogo textual ganhou busca,
   mas continua reutilizando referências existentes. Não deixar cadastro órfão ao cancelar.
2. Atalhos gráficos completos nos detalhes de entidades, relações, unidades e linhas;
   seleção visual complementar de identidades de marcos em eventos/presenças.
3. Superfície compacta de resumo que abre/fecha o seletor na escrita e nos detalhes;
   nas telas de cadastro substituídas, a grade está diretamente no corpo existente.
4. Marcas gráficas de cada corte dentro das células, filtros de uso vigente e referências
   autorais/intervalos na consulta mensal. Neste corte a consulta mostra marcos e acontecimentos
   convertidos, seleciona a lente e explica sua vigência; não é agenda completa de todos os vínculos.
5. Memória da posição de navegação por sistema e ordenação contextual em todos os caminhos.
   Sem contexto, a visualização começa no primeiro ano positivo suportado, sem seleção de fato.
6. Regras próprias de dias fora da semana, pôr do sol, fusos/mapas, eras/correlações alternativas,
   recorrências e identificação de entidades. Reconhecimento continua por último.

As frentes pendentes permanecem autorizadas; não constituem pedido de um novo aceite para
retomar o trabalho. Não marcar a revisão inteira implementada pela entrega deste primeiro incremento.

## Verificação

O manifesto do candidato deve apontar para os logs efetivamente concluídos: CTest,
matrizes GTK em Projeto vazio/Atlas e claro/escuro com reabertura, sanitizers focais,
inspeção visual selecionada e verificação da preservação. Resultados não substituem
aceite humano, teste com escritores, certificação de acessibilidade ou publicação.

Núcleo focal ASan/UBSan: projeção, modelo temporal e motores; outras dependências Release.
UI focal: seletor, biblioteca e integração de calendário na Cronologia; demais unidades Release.
LSan desligado; não declarar toda a aplicação instrumentada.

Não alegar ganho medido de tempo de tarefa: a substituição dos campos existe, mas não
houve pesquisa cronometrada com escritores nesta entrega.

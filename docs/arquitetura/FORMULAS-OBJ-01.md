# OBJ-01 — Objeto Fórmula, contrato e primeiro perfil executável

Data: 2026-11-21. Aprovação: o usuário aprovou expressamente a proposta de fórmulas com variáveis, resultados dependentes, alterações explícitas de estado e uso tanto no documento quanto nas células. Implementação isolada P2-D: `bb19da0`, com verificação adicional de identidade das referências em `53b534f`. Base: `347be9d` (P2-C). Branch `feature/inde-obj-01-foundation`.

Ver [demonstração e linguagem](../../tools/obj01-pilot/P2D.md), [validação](../validacao/VALIDACAO-OBJ-01-P2D.md) e [contrato composto](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md). Este corte incorpora a ideia aprovada ao piloto; não inicia integração GTK, alteração de schemas reais ou arquivo único `.inde`.

## Decisão de produto e experiência

Fórmula é objeto de conteúdo calculável. Uma ocorrência pode estar na sequência do documento ou numa célula de tabela. O local de criação não muda o motor, a capacidade de declarar variáveis, as referências ou o resultado. Uma célula pode alimentar um objeto no documento e vice-versa.

Durante a edição, o próprio lugar do resultado apresenta uma caixa de texto simples com várias linhas. Aplicar ou Ctrl+Enter conserva a fonte e recolhe a caixa; fora da edição, a ocorrência mostra somente o resultado. Erros aparecem como diagnóstico editável. A biblioteca exibe nomes, resultados e variáveis iniciais/atuais, inclusive para objetos sem uso visível.

Equação e Fórmula têm contratos distintos: a primeira representa fonte/apresentação matemática; a segunda avalia conteúdo e dependências. Acrescentar Fórmula não declara implementado o renderizador de equações anteriormente reservado para outro corte.

## Conteúdo e identidade

- Registro lógico: ID persistente de objeto, nome único imutável neste perfil, fonte UTF-8, revisão, bindings de nomes externos para IDs e substituições explícitas de valores atuais.
- Uso no documento: ID próprio de ocorrência apontando para o ID do objeto; participa das ordens autoral/visual, seleção, movimento e remoção de uso.
- Uso em célula: associação entre ID estável da célula e ID do objeto. Não é uma fórmula escondida dentro do texto da célula nem uma ligação por índice visual. O texto anterior é conservado para retirada explícita do uso.
- Referência: `Combate.HP` ou `Combate.resultado`. A fórmula externa é ligada por ID; a variável é identificada pelo nome declarado neste primeiro perfil. Renomeação com reescrita de referências não foi implementada.
- Biblioteca conservada: retirar uma ocorrência/célula não exclui o objeto ou invalida seus dependentes. O mesmo objeto pode ter um uso no documento e em células. Edição de fonte é compartilhada; criar outra fórmula com novo nome cria objeto independente.

`ObjectKind::Formula` foi acrescentado ao fim do enum isolado, sem mudar valores dos tipos anteriores. O grafo registra o tipo e suas ocorrências; o payload e o motor ficam no agregado do piloto e em `object_formula.*`. Não se usa Equation como substituto semântico nem se escreve conteúdo calculado em offsets do Documento proprietário.

## Linguagem admitida

O perfil aceita expressão simples, `= expressão`, ou bloco de linhas com `declara nome = expressão`, `resultado expressão` e `alterar Formula.variavel = expressão`. Admite chaves externas opcionais, comentários `#` e `resultado final calcula ...`. Há um único resultado por fonte e até 32 declarações/32 atribuições.

Operadores numéricos: `+`, `-`, `*`/`×`, `/`/`÷`, `^`, parênteses, sinais unários e comparações `==`, `!=`, `<`, `>`, `<=`, `>=`. Funções: `SOMA`, `MEDIA`, `MIN`, `MAX`, `ABS`, `SE`, com `;` ou `,` entre argumentos. Ponto decimal; nomes e funções distinguem maiúsculas/minúsculas. `SE` avalia somente o ramo escolhido. Não se promete equivalência completa de gramática, precedência ou biblioteca com Excel.

Nomes são identificadores ASCII de até 64 caracteres: letras, números e `_`, começando por letra ou `_`, sem palavras reservadas. Referências desconhecidas ficam diagnosticadas e podem ser ligadas quando o objeto correspondente for criado; bindings já existentes não são desviados por uma nova criação. A leitura de um binding persistido exige que ID e nome do alvo correspondam. Divergência estrutural recusa abertura, mesmo com hash externo recalculado, sem substituir o alvo por outro número válido.

Fontes incompletas ou cálculos inválidos podem ser aplicados e recuperados para correção. O resultado é apresentado como erro, sem número antigo válido. Erros estruturais de identidade, tamanho, formato, associação ou estado persistido continuam recusando a operação/abertura.

## Valores iniciais, estado e execução

`declara HP = 100` define uma expressão inicial. Sem substituição explícita, o valor atual deriva dela e de suas dependências. Uma execução pode gravar HP atual 80; o inicial permanece 100. Editar outra fórmula, mostrar a página, mover uma ocorrência, salvar ou reiniciar não executa novamente a atribuição nem restaura o inicial.

O comando de restauração remove a substituição atual da variável e recalcula a cadeia. Alterar uma declaração inicial recalcula o atual somente se ele não estiver substituído. Remover a declaração numa fonte válida remove a substituição correspondente; desfazer restaura fonte e estado. Fonte sintaticamente incompleta conserva os bindings/valores anteriores para reparo. A interface pode mostrar inicial indisponível caso sua expressão não possa ser calculada sob o estado atual; não inventa um número.

Uma execução lê todos os lados direitos no mesmo estado anterior. Os valores são aplicados simultaneamente num candidato privado, sem dependência da ordem das linhas de atribuição ou das posições dos objetos. Duas atribuições à mesma variável na execução são recusadas. Alvo inexistente, erro de expressão ou novo resultado inválido entre os que antes eram válidos recusam o candidato inteiro. Rascunhos já inválidos em outros objetos permanecem diagnosticados; não são exigidos como condição geral para editar o documento.

Depois da aplicação, os resultados e expressões iniciais dependentes recalculam automaticamente. Outros comandos não são disparados por esse recálculo. Regras de disparo automático por evento, iteração e repetição programada ficam como possibilidades posteriores, sem execução implícita neste corte.

## Dependências e limites

O avaliador resolve referências por objeto/variável, memoriza valores dentro da avaliação e detecta ciclos nos caminhos avaliados. Não ordena cálculo por coordenadas, ordem de leitura, ordem de células ou armazenamento. Referências condicionais de `SE` só são visitadas no ramo escolhido. Não há resolvedor iterativo para ciclos.

Limites: até 64 objetos Fórmula conservados, 256 usos no documento, 2.048 associações de célula, fonte de 16 KiB/objeto, 32 variáveis/32 atribuições/objeto, 2.048 tokens, 512 nós de expressão, profundidade de parsing 48 e de avaliação 64, orçamento de 200.000 visitas por avaliação do conjunto. Funções agregadas aceitam até 32 argumentos. Os limites globais de strings/snapshot/histórico do piloto continuam aplicados, agora contando também a cópia textual de trabalho e o contexto no orçamento agregado de strings.

Números são `double` finitos com magnitude até `10^12`; divisão por zero e resultado não finito/excedido recebem erro. A apresentação usa até 12 algarismos significativos; persistência de valores usa codec de precisão suficiente para roundtrip. Não é aritmética decimal exata. Não há chamadas de sistema, acesso a arquivos, rede, funções fornecidas pelo usuário ou execução de código da linguagem hospedeira a partir da fonte.

## Histórico, persistência e compatibilidade

O perfil P2-D usa `user_version=4` e snapshot `INDE-OBJ01-COMPOSITE-P2D-1` no SQLite auxiliar. Perfis 1/2/3 são preservados e recusam abertura incompatível. A cópia do salvo de P2-C exige destino novo e conexão READONLY, preserva a cópia textual reordenada e começa com instrumento Estrutura. Não migra schema 37 nem escreve a origem congelada. A contagem agregada de strings foi corrigida para incluir a cópia textual de trabalho e o contexto em memória: registros anteriores junto ao limite podem agora ser recusados quando a contagem completa exceder 8 MiB; não há truncamento ou regravação automática. As fixtures anteriores foram reabertas na regressão e preservadas por hash.

O registro de trabalho inclui fontes, IDs, usos, bindings e valores atuais. Resultados são derivados novamente na abertura, sem reexecutar atribuições. Criação, aplicação de fonte, execução, restauração, associação/retirada de célula e movimento/remoção entram no histórico do agregado e no mesmo checkpoint transacional de P2-C. Uma execução é uma única operação reversível e recuperável, abrangendo a cadeia.

Salvar promove o agregado sem executar comandos; reinício recupera estado e seleção, com histórico novo. Erro antes do commit faz rollback; confirmação incerta bloqueia escrita e exige reabertura. O driver de falhas continua separado do executável normal.

Fonte apenas digitada no editor ainda é rascunho do navegador. Aplicar/Ctrl+Enter/Salvar confirma-a no núcleo antes de proteção. A interface avisa ao sair com rascunho. O formulário de criação também só produz objeto ao criar; não se promete recuperação de texto que ainda não foi enviado ao núcleo.

## Invariantes e casos de prova

| Risco | Regra | Prova |
| --- | --- | --- |
| Planilha e documento divergirem semanticamente | Um registro/motor para todos os usos | Fórmula nasce em célula, alimenta documento e outra célula; uso compartilhado |
| Movimento quebrar referência ou executar dano | Binding por ID, avaliação independente de layout, execução separada | Movimento/remoção/undo e reabertura sem repetir atribuição |
| Recalcular apagar estado autoral atual | Inicial e substituição atual separados | HP 100/80, cadeia, reset, undo/redo e SIGKILL |
| Uma atribuição falhar depois de outra já aplicada | Valores simultâneos em candidato único | Troca de valores, alvo inválido, atribuição duplicada e dependente dividido por zero |
| Fonte incompleta desaparecer | Fonte com diagnóstico também é conteúdo persistível | Aplicar fonte inválida, salvar, reabrir e reparar |
| Referência persistida apontar silenciosamente para outro objeto | Conferir ID e nome imutável | Corrupção com hash externo válido recusa abertura |
| Ciclo travar a aplicação | Detecção + profundidade/operações limitadas | Ciclo entre objetos e reparo; limites de profundidade e magnitude |
| Confirmação parcial deixar estados diferentes | Checkpoint do agregado antes da resposta | Falha antes do commit, confirmação perdida e reabertura da cadeia |
| Prontidão expor JSON parcial ao cliente | Publicação atômica do aviso completo | Falha real sob carga identificada e suíte HTTP corrigida nas quatro variantes |

## Limites de entrega

Implementados os cálculos numéricos, variáveis/referências, mudanças explícitas, usos no documento/células, fonte recolhível, diagnósticos, histórico e recuperação. Tabelas continuam no perfil inicial 2 × 3 da demonstração, sem edição de linhas/colunas.

Permanecem fora: compatibilidade XLSX, biblioteca completa Excel, referências A1/intervalos, resultados textuais/datas, renomear símbolos, funções próprias, gatilhos automáticos, iteração, cálculo distribuído e motor financeiro decimal. O publicador canônico ainda não recebe esse agregado; a fronteira do piloto recusa publicação. Equações, GTK, schemas reais e `.inde` único não são promovidos a implementados. A demonstração técnica não substitui aceite humano.

## Atualização posterior aprovada — P2-D2

O [refinamento de texto misto e comando consumível](FORMULAS-TEXTO-OBJ-01.md) implementa a proposta posterior do usuário em linguagem/perfil próprios. Valores atuais ficam na fonte; a frase tem trechos autorais e resultados protegidos; condições retornam texto; alterar fica sozinho e é retirado após execução confirmada. O perfil numérico P2-D descrito acima conserva seu comportamento e formato. [Validação P2-D2](../validacao/VALIDACAO-OBJ-01-P2D2.md).

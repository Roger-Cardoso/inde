# OBJ-01 — P2-D2: texto misto, estado na fonte e comando consumível

Data: 2026-11-21. Base `1560cfe` (P2-D). Implementação `70b809d`. Branch `feature/inde-obj-01-foundation`. O usuário pediu e autorizou este refinamento: valor atual na própria fonte, edição das palavras ao redor do resultado, termos escolhidos por condições e `alterar` solitário que desaparece após agir.

Ver [roteiro utilizável](../../tools/obj01-pilot/P2D2.md), [validação](../validacao/VALIDACAO-OBJ-01-P2D2.md) e [contrato anterior](FORMULAS-OBJ-01.md). P2-D permanece como perfil numérico anterior; este corte cria perfil auxiliar próprio, sem integração GTK ou mudança de projetos reais.

## Autoridade do estado

Na linguagem 2, a fonte distingue expressão inicial e atual:

```text
declara HP = 100
HP = 80
resultado HP
```

A declaração conserva o inicial. A atribuição atual participa da avaliação no lugar da expressão inicial até ser editada/retirada. Não existe substituição numérica escondida no mapa `overrides`: no novo perfil esse mapa deve estar vazio, inclusive na leitura. Registro que tenta introduzir valor oculto é recusado.

Uma atribuição atual escrita manualmente pode ser expressão, sujeita a dependências e ciclos; `HP = HP - 20` é circular e recebe diagnóstico. O comando externo calcula uma vez e grava um **literal concreto**, com roundtrip numérico, aspas/escapes para texto ou booleano. Não grava o lado direito do comando como expressão repetível.

Ao alterar um valor, a implementação substitui a atribuição atual existente ou insere uma linha após a declaração da variável. Preserva a definição inicial, outras instruções, comentários externos à expressão substituída e chaves. Novo comando substitui a mesma linha; não acumula estados antigos no corpo. Reset remove a atribuição atual; desfazer restaura fonte e estado.

## Texto escrito e resultados

Cada uso normal no documento ou célula possui uma apresentação própria: ID do alvo, ID do objeto principal e até 32 trechos ordenados. Um trecho é texto UTF-8 escrito pelo autor ou referência ao resultado de uma fórmula por ID. Pelo menos um resultado do objeto principal deve continuar presente. Outros resultados podem participar do mesmo parágrafo.

O texto ao redor é editado diretamente, no instrumento Texto, sem abrir a fonte da conta. Resultados são spans protegidos. Recálculo atualiza os resultados e conserva as palavras, espaços e referências. Os trechos não são reconstruídos por substituição de strings dentro da frase. Edição de fonte é compartilhada entre os usos do objeto; edição de frase pertence somente à ocorrência/célula escolhida.

O compositor experimental usa a mesma tipografia e entrelinha dos parágrafos de origem para o texto misto. Células usam a tipografia das outras células. Hover/foco revela acesso à fonte/frase por controles sobrepostos, sem espaço permanente no fluxo. O resultado tem acesso por teclado; toque mantém controles disponíveis. A fonte abre no lugar do uso e se recolhe ao aplicar. O editor de frase permite adicionar/reordenar texto e outros resultados.

Esses são parágrafos mistos do agregado auxiliar. Não há edição de trechos nos offsets de `project::Document.content`, nem integração do editor GTK. Os parágrafos da origem continuam somente leitura. Uma string devolvida pela própria expressão é um resultado protegido; palavras que devem continuar editáveis ficam nos trechos escritos da apresentação.

## Tipos, lógica e limites de linguagem

A linguagem 2 admite números, strings UTF-8 e booleanos. A API expõe tipo do resultado e valores atuais/iniciais de variáveis com tipos correspondentes. Condições aceitam booleanos ou números; strings não são convertidas silenciosamente em condições/números. `+` é soma; `&` concatena representações textuais.

Funções: `SE`/`IF`, `E`/`AND`, `OU`/`OR`, `NÃO`/`NAO`/`NOT`, `SOMA`/`SUM`, `MEDIA`/`AVERAGE`, `MIN`, `MAX`, `ABS`, `TEXTO`/`TEXT`. Funções ASCII ignoram caixa; as grafias portuguesas acentuadas admitidas para negação são `NÃO`, `Não`, `não`. Nomes de objetos/variáveis continuam ASCII, únicos no escopo definido, com palavras da linguagem/booleanos reservados. Não há renomeação automática.

`SE`, `E` e `OU` fazem avaliação curta: um ramo não necessário não é visitado. Comparações de igualdade requerem tipos iguais; operações aritméticas e ordenação requerem números. Textos usam aspas duplas e escapes de nova linha, tabulação, aspas e barra. Um comando pode fixar números, textos ou booleanos.

Instruções são separadas por linhas; cada expressão fica numa linha, com quebra visual no campo. Não há continuação livre de expressão por nova linha dentro de parênteses. Não é compatibilidade completa com Excel, nem linguagem de scripts de sistema.

Limites herdados: 64 objetos conservados, 32 variáveis por fórmula, fonte de 16 KiB, parsing 48 níveis/512 nós/2.048 tokens, avaliação 64 níveis/200.000 visitas, magnitude numérica `10^12` com double e apresentação de 12 algarismos significativos. Novo limite de string calculada: 4.096 bytes; orçamento de resultados e variáveis expostos: 512 KiB. Literal de apresentação: até 4.096 escalares/16 KiB. Há até 256 usos de documento, contando usos retirados, e 2.048 associações de célula. Histórico, metadados e blobs mantêm seus orçamentos anteriores. Não são limites de RSS nem medições de latência.

## Comando de uso único

```text
alterar Combate.HP = Combate.HP - 20
```

O comando possui uma única atribuição externa, sem declarações, atribuições locais, resultado ou frase. Misturas e múltiplos alterar são recusados antes de gravar; uma fonte incompleta comum continua podendo ser conservada com diagnóstico. Um comando não oferece valor para outra fórmula nem pode ser interpolado ou ter vários usos simultâneos.

A execução produz candidato privado que reúne: fonte do alvo com valor fixo; resultados dependentes verificados; marca de comando consumido; retirada de todos os seus usos ativos. No documento, a ocorrência sai das ordens autoral/visual; na célula, a associação sai e o texto simples anterior volta a aparecer. Não fica bloco vazio.

Se a expressão/alvo falhar, o limite for excedido ou um resultado antes válido se tornar inválido, o candidato inteiro é recusado. O comando fica disponível e o alvo não muda. Cálculos não disparam outros comandos. Observar, mover, salvar ou reiniciar não executa atribuição.

A publicação em memória e a retirada visual só ocorrem após checkpoint conhecido como confirmado. Exceção antes do commit faz rollback. Resultado incerto retém candidato e bloqueia escrita até reabrir. Não se confirma consumo quando somente a fonte do alvo foi tentada ou vice-versa.

Desfazer restaura valor, fonte, frase, ocorrência e comando não consumido; refazer restaura o conjunto consumido. Após reinício, o trabalho confirmado reaparece com o comando já consumido e histórico vazio. Registros privados/ocorrências retiradas são conservados para integridade e histórico limitado; consumo não é exclusão física definitiva. A biblioteca ativa oculta consumidos e recusa reexecução por ID. A contagem conservada de 64 inclui esses comandos; GC e histórico persistente não foram implementados.

## Formato e compatibilidade

Perfil `p2d2`, `user_version=5`, snapshot `INDE-OBJ01-COMPOSITE-P2D2-1`, linguagem 2 por registro. Versão desconhecida, consumo forjado em fórmula comum ou consumo ainda com uso ativo recusam abertura mesmo com hash externo válido.

A nova linguagem tem avaliador versionado em `object_formula_rich.cpp`; a linguagem numérica anterior conserva seu parser/avaliador e formato físico. A API compartilhada escolhe pelo perfil explícito. Não mistura linguagens num mesmo agregado. Os perfis 1–4 continuam disponíveis e não são atualizados no lugar.

O iniciador P2-D2 começa numa pasta própria nova e não copia P2-D automaticamente. Seed explícito lê somente o salvo P2-D em transação READONLY, materializa substituições antigas na fonte da nova cópia e exige destino novo. Misturas antigas alterar/resultado são incompatíveis e recusam a cópia sem reescrever a origem. Não há descarte silencioso de comandos antigos para forçar compatibilidade.

A interface continua usando os mesmos arquivos estáticos conhecidos pelos servidores antigos; o novo editor de texto foi integrado em `app.js`, sem exigir endpoint de recurso adicional das demos já abertas. As cinco variantes HTTP dos perfis foram verificadas.

## Riscos e provas

| Risco | Invariante | Prova |
| --- | --- | --- |
| Fonte mostrar inicial e esconder atual | Atual real em atribuição na fonte; mapa oculto vazio | Inspeção da fonte no navegador, reset e recusa de snapshot adulterado |
| Recálculo apagar frase | Texto e resultados são trechos distintos por uso | Edição direta, múltiplos resultados, recálculo/reabertura e dois usos com textos diferentes |
| Alterar reaplicar dano ao abrir | Executar fixa literal e consome comando | SIGKILL/reabertura conservam HP 80 e comando ausente |
| Alteração parcial apagar comando sem efeito | Fonte + consumo + ordens no mesmo checkpoint | Quatro janelas de crash e exceções pré/pós-commit |
| Desfazer só metade | Histórico do agregado completo | Undo/redo da fonte, resultado e ocorrência no núcleo e navegador |
| Condição avaliar ramo inválido | Avaliação curta e valores tipados | IF/SE, AND/E, OR/OU, NOT/NÃO com divisões inválidas em ramos não visitados |
| ID/consumo/versão adulterados parecerem válidos | Validação estrutural antes de abrir | Hash externo recalculado não contorna guardas |
| Ordem dos campos JSON marcar rascunho inexistente | Comparação por significado de cada trecho | Falha visual identificada e correção conferida em página nova/reinício |
| Fonte ou texto se tornarem HTML executável | Render por textContent, edição/paste como texto | Código de apresentação não interpreta HTML; não se promete auditoria de todos os browsers |

Publicação canônica continua recusada na fronteira do piloto. Não há merge, migração de obras, GTK, `.inde` único, edição livre da origem, biblioteca Excel completa, A1/intervalos, funções próprias ou gatilhos automáticos. O aceite humano continua separado da observação técnica. O corte não conclui a Fase 2 inteira nem inicia Fase 3.

## Atualização — cartões preenchíveis, 2026-11-22

Após escolha e autorização do usuário, o [editor por cartões](CARTOES-FORMULAS-OBJ-01.md) foi integrado ao piloto P2-D2, com fonte única, prévia no motor e regra/frase atômicas. [Validação própria](../validacao/VALIDACAO-OBJ-01-CARTOES.md). O vínculo com entidades reais, a integração GTK e a publicação canônica permanecem pendentes.

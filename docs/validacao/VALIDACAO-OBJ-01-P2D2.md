# OBJ-01 — validação P2-D2: texto misto e comando consumível

Data: 2026-11-21. Branch `feature/inde-obj-01-foundation`. Base preservada `1560cfe3c490ab1d4090c0e53857789ff2f648cf`; implementação `70b809d3f8165c42b6e5860aa80113eba4577dfb`. Refinamento autorizado do piloto isolado: [contrato](../arquitetura/FORMULAS-TEXTO-OBJ-01.md), [roteiro da demonstração](../../tools/obj01-pilot/P2D2.md), [resultados estruturados](evidencias-obj01-p2d2/resultados.json) e [manifesto SHA-256](evidencias-obj01-p2d2/SHA256SUMS).

## Resultado entregue

A fonte passa a conservar declaração inicial e atribuição atual, como `declara HP = 100` seguida de `HP = 80`. Comando externo calcula uma vez, escreve literal no alvo e se consome no mesmo checkpoint. Comando deve ser solitário: não contém resultado, declaração, atribuição local ou frase. Desfazer/refazer reúne fonte, valores, texto e presença do comando. Reabrir não executa alteração.

Cada ocorrência/célula pode compor texto editável e resultados protegidos, inclusive resultados de outras fórmulas. A edição das palavras não muda a expressão. Fonte é compartilhada entre usos; frase pertence ao uso. O resultado de uma expressão pode ser texto, número ou booleano. SE/IF, E/AND, OU/OR e NÃO/NOT admitem seleção dinâmica de termos e avaliação curta.

O novo perfil auxiliar `p2d2`, versão privada 5, nasce separado. Os perfis anteriores e projetos autorais não recebem atualização no lugar. O avaliador numérico antigo permanece versionado. A cópia explícita de Saved P2-D materializa valores atuais na fonte de uma pasta nova; recusa comandos antigos misturados. Linguagem 2 possui regras de tipos próprias: não há promessa de equivalência para toda expressão numérica legada, especialmente aritmética aplicada a comparações booleanas.

## Testes executados

| Camada | Resultado | Evidência |
| --- | --- | --- |
| GCC Debug | 11/11 programas, 89,25 s | [log](evidencias-obj01-p2d2/gcc-final.txt) |
| Clang Debug | 11/11 programas, 85,69 s | [log](evidencias-obj01-p2d2/clang-final.txt) |
| GCC ASan + UBSan | 11/11 programas, 248,26 s | [log](evidencias-obj01-p2d2/gcc-asan-final.txt) |
| Clang ASan + UBSan | 11/11 programas, 202,16 s | [log](evidencias-obj01-p2d2/clang-asan-final.txt) |
| Release nativo da fonte integrada | 61/61 programas, 291,85 s | [log](evidencias-obj01-p2d2/native-release.txt) |
| Auditor de fontes, recusas | 5/5 testes | [log](evidencias-obj01-p2d2/auditor-fontes.txt) |
| Três arquivos originais | Hashes, manifestos e bytes extraídos conferem | [inventário](evidencias-obj01-p2d2/fontes.json) |
| CI da implementação | 25/25 trabalhos aprovados | [execução GitHub](https://github.com/Roger-Cardoso/inde/actions/runs/44941896304), [JSON](evidencias-obj01-p2d2/ci.json) |

Os 11 programas de cada variante são cinco núcleos de perfil, cinco baterias HTTP e o adaptador da origem. P2-D2 contém 23 casos de núcleo e sete HTTP (quatro comuns herdados e três próprios). Permanecem A: 19+4, B: 12+6, C: 16+7, D: 19+7; adaptador com nove grupos. Não somar casos herdados como cenários novos. Localmente LeakSanitizer foi desativado; ASan/UBSan passaram. A matriz remota habilita detecção de vazamentos.

Os testes novos cobrem números/textos/booleanos e aliases lógicos, ramos não visitados, tipos atuais/iniciais na API, atribuição fixa na fonte, comentários/chaves, reset, múltiplos resultados, apresentação por uso, entrada inválida, ciclos, limites, contextos/revisões, consumo único, recusa de comandos compartilhados/misturados, restauração do texto simples da célula, undo/redo, seed explícito e preservação da origem. Persistência adulterada com hash externo recalculado não contorna versão, estado consumido ou proibição de valores ocultos.

Quatro janelas de crash do novo perfil (`after_blobs`, `after_snapshot`, `before_commit`, `after_commit`), exceção antes do commit, confirmação perdida após commit e encerramento por SIGKILL verificam atomicidade de fonte + consumo + frase. Isso complementa as dez janelas já testadas no perfil C; não constitui prova de toda falha possível de armazenamento ou sistema operacional.

## Observação na interface real

Foi utilizado o motor C++ real, SQLite auxiliar real e documento sintético novo. A demonstração foi operada no navegador: criação de Combate, edição direta das palavras, condição Estado, montagem de uma frase com dois resultados, fórmula criada em célula, execução de Golpe e desfazer/refazer do conjunto. Fonte foi aberta no uso para verificar a linha efetiva `HP = 80`.

A fonte final de Combate é:

```text
declara HP = 100
HP = 80
declara DanoEspada = 10
resultado HP - 2 × DanoEspada
```

A frase final é **“Após os ataques, restaram 60 pontos de vida; o estado é ferido.”**. O uso Estado mostra **“O personagem permanece ferido.”**; a célula mostra **“HP atual: 80”**. O valor 60 decorre da expressão, que ainda subtrai duas vezes DanoEspada do HP atual. O comando Golpe desaparece do fluxo e da biblioteca ativa.

Depois dessas operações, sem Save, o motor sintético foi encerrado com SIGKILL e o servidor reiniciado. A geração 19 e o hash integral de Working reapareceram iguais; Saved continuou na revisão 0. O comando permaneceu consumido, a fonte continuou com HP 80 e nenhum efeito foi repetido. Histórico em memória reapareceu vazio, como previsto. Integridade SQLite `ok`; nenhuma violação de chave estrangeira.

Evidências: [antes do comando](evidencias-obj01-p2d2/antes-do-comando.txt), [fonte efetiva](evidencias-obj01-p2d2/fonte-hp80.txt), [captura da fonte](evidencias-obj01-p2d2/fonte-hp80.jpg), [antes do reinício](evidencias-obj01-p2d2/antes-reinicio.txt), [reinício estruturado](evidencias-obj01-p2d2/reinicio.json), [texto antes](evidencias-obj01-p2d2/texto-antes-reinicio.json), [texto depois](evidencias-obj01-p2d2/texto-depois-reinicio.json), [estado acessível final](evidencias-obj01-p2d2/navegador-final.txt), [tipografia calculada](evidencias-obj01-p2d2/tipografia.json) e [captura final](evidencias-obj01-p2d2/demonstracao.jpg). Os arquivos JSON de texto antes/depois são idênticos em bytes e preservam os espaços autorais. Somente os dumps legíveis de acessibilidade tiveram espaços finais de linha normalizados para a conferência do diff. A captura final mostra o resultado no fluxo sem rótulo de fórmula permanente.

Uma falha encontrada na revisão visual marcava frase inalterada como rascunho por causa da ordem das propriedades JSON. Foi corrigida a comparação por conteúdo dos trechos; mudança de contexto e edição posterior foram conferidas. A apresentação foi incluída em `app.js`, preservando a lista de recursos aceitos pelos servidores anteriores ainda em execução. Rascunhos de fonte/frase são enviados antes de outra operação; estados de editor são limpos ao trocar de contexto ou consumir seu objeto. A observação de navegador é um roteiro concreto; não cobre todos os dispositivos e métodos de entrada.

## Preservação e proveniência

Os oito arquivos de origem/banco das quatro demonstrações anteriores mantiveram seus hashes entre início/fim: [início](evidencias-obj01-p2d2/preservados-inicio.json), [fim](evidencias-obj01-p2d2/preservados-final.json). Manifestos históricos são conferidos nas revisões em que foram emitidos: A/8ea6c23 (37 entradas), B/b1ae38f (39), C/347be9d (44), D/1560cfe (49), total 169. Alteração posterior de código não reescreve essas provas.

Main permanece em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com a alteração de permissão preexistente em `Preparar-ponte.sh` preservada. Fontes compactadas, metadados, modelo e comandos da fundação anterior, adaptadores, protocolo de escrita e repositórios canônicos não foram alterados neste corte. Não houve merge em main, escrita em obras, publicação autoral ou migração de schema canônico.

## Limites e pendências reais

- O texto misto pertence ao agregado experimental. Os parágrafos originais de `project::Document.content` seguem somente leitura; inserção em offsets do editor GTK ainda não existe.
- Uma expressão ocupa uma linha da fonte. Há várias instruções/linhas, mas não continuação livre de expressão dentro de parênteses. Não há compatibilidade integral Excel, referências A1/intervalos, funções de usuário ou execução de scripts externos.
- Texto gerado pela expressão é resultado protegido. Palavras editáveis ficam nos trechos escritos da apresentação.
- Consumo remove presença ativa, mas conserva registro privado e ocorrência retirada para integridade/histórico. Eles contam no limite de 64 objetos. Não há coleta definitiva desses registros nem histórico de undo persistente.
- Aritmética usa double finito, magnitude limitada a `10^12` e 12 algarismos significativos na apresentação; não é decimal exato. Outros limites estão no contrato.
- Não foram comprovados todos os faults de disco/energia, ampla concorrência, latência/RSS, todos os tamanhos de tela, experiência completa do seletor nativo de PNG e aceite humano. Não há bloqueio conhecido impedindo este roteiro local; essas lacunas permanecem explícitas.
- Publicação canônica continua recusada no piloto; GTK, migração de obras, arquivo único `.inde` e fases posteriores ficam fora deste corte. P2-D2 não declara encerrada toda a fase do piloto nem substitui o aceite autoral.

O manifesto pode ser conferido, da raiz deste worktree, com `sha256sum -c docs/validacao/evidencias-obj01-p2d2/SHA256SUMS`. A captura e os arquivos de evidência não contêm a credencial temporária da demonstração local.

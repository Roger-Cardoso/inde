# OBJ-01 — cartões para escolher palavras e frases

Data: 2026-11-22. Base `8ad48d1`, implementação `972cc6a`. Branch `feature/inde-obj-01-foundation`. O usuário escolheu cartões ordenados, contexto preenchível, adicionar/remover condições, comparação editável por número e por entrada textual, e autorizou a integração ao piloto.

O recurso está no perfil P2-D2 existente. Ver [roteiro utilizável](../../tools/obj01-pilot/CARTOES.md), [validação](../validacao/VALIDACAO-OBJ-01-CARTOES.md) e [contrato de texto misto](FORMULAS-TEXTO-OBJ-01.md). Este corte não integra entidades do Planejamento, GTK ou publicação canônica.

## Percurso autoral

**Criar texto por condições** abre os campos de destino, entrada, contexto, cartões e prévia. Nome da entrada e frases são texto livre; o identificador técnico do objeto é atribuído pelo programa. A entrada pode ser Número ou Texto e vir de valor preenchido ou seleção de resultado/variável existente.

Cartões possuem comparação, valor e texto de saída. A lista aceita até 12 condições simples; a primeira atendida vence, com alternativa explícita se nenhuma servir. Subir/Descer mudam prioridade; Remover/Adicionar mudam opções. Nenhuma palavra de programação é exigida nesse percurso. Contas e comandos livres continuam em uma área avançada recolhida.

Contexto contém texto antes/depois do resultado. A frase inteira pode ser autoral e o resultado pode ser palavra, várias frases ou vazio. Quando a apresentação já intercala outros resultados, o editor de cartões conserva essa montagem e edita somente a regra; o autor usa Editar frase para modificar os trechos completos. Não há redução silenciosa de apresentação rica para três trechos.

A edição entra em Texto antes de selecionar o uso; criação entra em Estrutura. Destino/célula são explícitos no diálogo. O diálogo impede cliques acidentais nas demais ferramentas. Cancelar descarta esse rascunho; aplicar confirma regra e frase no mesmo candidato. Avisos de falha aparecem dentro do diálogo, conservando os campos.

## Uma autoridade de cálculo e persistência

Cartões são uma vista reversível de um subconjunto estrito da linguagem 2. Não há avaliador JavaScript concorrente, objeto narrativo simulado nem cópia permanente do modelo visual em outra tabela.

O compilador C++ valida o pedido e escreve fonte contendo um cabeçalho de autoria `INDE-CARDS-1`, metadados de nome/tipo da entrada, declaração `Entrada` e expressão condicional. O cabeçalho não duplica condições/resultados: eles existem somente na expressão. A frase continua nos trechos da ocorrência já definidos pelo perfil 5. Os cartões que a API devolve são reconstruídos da fonte, sem gravação adicional.

Reconhecimento exige que a recompilação reproduza integralmente a forma admitida. A única normalização de espaços permitida nessa comparação é a remoção de linhas vazias entre instruções, inclusive a linha deixada por Restaurar inicial. Comentários adicionais, expressão arbitrária, operação extra ou formato não reconhecido produzem `cards: null`; a fonte segue disponível no editor avançado. O comando visual de edição também verifica isso no servidor, evitando que um cliente reescreva uma fonte livre por aproximação.

A declaração inicial e uma atribuição atual opcional são reconhecidas separadamente. Alteração externa de `Entrada` continua registrada como literal na própria fonte, com campo atual nos cartões; o autor pode voltar à entrada original. O texto numérico digitado é validado estritamente e conservado, sem transformar `15.10` numa aproximação decimal longa ao reabrir.

Operações:

| Pedido | Efeito |
| --- | --- |
| `preview_formula_cards` | Compila/avalia candidato temporário no motor; não muda revisão, SQLite, histórico, seleção nem valores |
| `create_formula_cards` | Usa o comando existente de criação após compilar a fonte; coloca regra e frase no documento/célula numa operação |
| `edit_formula_cards` | Confirma fonte compartilhada e frase do uso escolhido no mesmo checkpoint; pode editar só a fonte conservando apresentações complexas |

Revisão esperada, instrumento, identidade de alvo, tipos, campos e limites são verificados no servidor. Erro estrutural ou cálculo inválido do objeto visual recusa a operação inteira. Outros rascunhos avançados inválidos permanecem reparáveis. Prévia não executa comandos. Respostas atrasadas de prévia não substituem o rascunho mais recente.

O servidor HTTP anuncia `formula_cards_supported`; a interface só oferece os cartões quando o núcleo confirma suporte. Servidores antigos em execução continuam com suas ferramentas anteriores. É necessário iniciar o novo motor/servidor para usar a API nova.

## Comparações e limites

Número usa `<`, `<=`, `==`, `!=`, `>=`, `>` sobre double finito, magnitude até `10^12`. Texto usa igualdade, diferença, conteúdo e prefixo; não há ordenação quantitativa/coerção de strings numéricas. Comparação é exata por conteúdo UTF-8, sensível a caixa, acentos, espaços e representação Unicode. Não há normalização de nomes, correção ortográfica ou inferência de sinônimos. Contém/prefixo vazios são verdadeiros para qualquer texto.

As funções `CONTEM`/`CONTAINS` e `COMECA_COM`/`STARTSWITH` foram acrescentadas somente ao avaliador de linguagem 2, com dois argumentos textuais e resultado booleano. A linguagem numérica 1 permanece inalterada.

Até 12 cartões, nome da entrada até 256 bytes UTF-8, entrada/texto de alternativa até 4.096 bytes e fonte completa até 16 KiB. Valem os demais limites de parser, avaliação, objetos, usos, texto de apresentação e histórico do perfil 5. No caso numérico, valor vazio/inválido recusa aplicação; texto vazio pode ser uma escolha legítima. Grupos Todas/Alguma, entradas de data e lógica livre em blocos ainda não fazem parte desta interface.

## Compatibilidade e invariantes

Formato auxiliar 5, snapshot P2-D2 e linguagem 2 são conservados: sem nova tabela, coluna, migração ou escrita de projeto real. Novas fontes com funções textuais exigem o motor atualizado; abrir em binário antigo pode gerar diagnóstico de função desconhecida. Não se promete compatibilidade semântica inversa para funções novas.

| Risco | Regra de conservação | Prova |
| --- | --- | --- |
| Prévia gravar valores ou comandos | Avaliação em registros temporários, sem checkpoint | Estado e hash do banco iguais antes/depois |
| Regra/frase se separarem após erro | Candidato único, checkpoint existente | Undo/redo, quatro janelas de crash e SIGKILL |
| Troca de prioridade não afetar resultado | Condicionais em ordem, avaliação curta | Condições sobrepostas e reversão da lista |
| Conteúdo autoral virar código | Campos de texto escapados e tipos estritos | Aspas, barras, nova linha, Unicode e tentativa de instrução dentro de string |
| Converter fonte livre perder conteúdo | Reconhecimento completo antes de editar | Comentário adicional mantém fonte avançada e recusa edição visual |
| Achatar frase com outros resultados | Recusa do alvo complexo, edição só de regra disponível | Dois resultados conservados; uso compartilhado/célula independente |
| Erro ficar escondido atrás do diálogo | Mensagem dentro da caixa | Recusa visual de valor numérico vazio conserva o documento |

Continua pendente ligar variáveis a entidades reais, formas de nome e pseudônimos do INDE. O nome literal Armand no exemplo não é vínculo semântico. A edição por offsets do Documento original, integração GTK, exportação canônica, `.inde` único e fases posteriores não foram iniciadas neste corte. Validação técnica não substitui o aceite humano do produto.

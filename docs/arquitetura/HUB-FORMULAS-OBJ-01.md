# OBJ-01 — hub de fórmulas e variáveis alcançáveis

Data: 2026-11-22. Base `7294040`, implementação `700fb1f`. Branch `feature/inde-obj-01-foundation`. O usuário pediu que uma regra criada por cartões pudesse receber alteração de outra fórmula, com consumo do comando, e aprovou um ponto central para escolher a finalidade da fórmula.

Ver [roteiro utilizável](../../tools/obj01-pilot/HUB.md), [validação](../validacao/VALIDACAO-OBJ-01-HUB.md), [contrato dos cartões anteriores](CARTOES-FORMULAS-OBJ-01.md) e [texto misto/comandos consumíveis](FORMULAS-TEXTO-OBJ-01.md).

## Padrão de criação

**Inserir fórmula** reúne cinco caminhos funcionais:

| Caminho | Conteúdo | Comportamento fora da edição |
| --- | --- | --- |
| Escolher palavras ou frases | Condições ordenadas sobre número/texto, alternativas e contexto | Texto calculado no fluxo |
| Fazer uma conta | Dois operandos numéricos, literais ou referências, e soma/subtração/multiplicação/divisão | Resultado no documento/célula |
| Guardar um valor | Variável numérica/textual, inicial e atual separados quando há alteração | Valor e frase do uso; recurso pode ser reutilizado |
| Alterar um valor | Alvo escolhido, definir/acrescentar/diminuir, literal ou referência | Comando pendente; executar fixa o efeito e remove o uso ativo |
| Escrever livremente | Fonte completa preservada, incluindo comandos solitários | Resultado/diagnóstico ou comando pendente |

Não são motores diferentes nem cópias de documentos. Os cinco caminhos usam os mesmos registros Fórmula, bindings por identidade, trechos de apresentação, revisão, histórico e checkpoint do piloto. Não há avaliador JavaScript paralelo.

O hub entra em Estrutura ao criar e em Texto ao editar. O destino pode ser o documento ou uma célula selecionada. O resultado reabre o cartão correspondente. Fontes livres/antigas sem formato reconhecido permanecem livres, sem aproximação ou redução silenciosa.

## Nome de apresentação e referências estáveis

A fórmula possui nome de apresentação editável. A referência para escrita livre usa o nome imutável do registro, escolhido na criação. Cartões novos de condição permitem escolher o símbolo de sua variável; valor guardado também. Conta visual expõe `Primeiro` e `Segundo`. As referências podem ser selecionadas para copiar e aparecem nos seletores junto do nome de apresentação.

Exemplo: apresentação **Preço da negociação**, referência de fórmula `Negociacao`, variável `Preco` → `Negociacao.Preco`. Alterar o título não altera esse vínculo. O compilador e o servidor recusam renomeação de símbolo por edição estruturada, inclusive quando o cliente ignora o campo desabilitado. Não existe refatoração automática global de nomes. A escrita livre continua sendo edição explícita da fonte, com diagnósticos se o autor remover uma declaração usada.

Cartões antigos conservam `Entrada` e seu nome anterior de fórmula. O hub revela essa referência existente e oferece o seletor; não transforma implicitamente a fonte em cartão nomeado novo.

## Ciclo de alteração remota

Uma regra por cartões pode ser alcançada pelo comando escrito `alterar Negociacao.Preco = 15`. **Preparar alteração** junto à referência também abre o cartão de comando com alvo preenchido. O autor pode começar diretamente em **Alterar um valor** e escolher uma variável real do catálogo do agregado.

Prévia avalia um candidato privado e pode mostrar os valores antes/depois, incluindo variáveis dependentes. Ela não grava valores, executa efeitos no documento ou consome comandos reais. Ao criar uma fórmula, pode usar seu nome escolhido para resolver autorreferências legítimas; revisão e identidade continuam verificadas.

**Inserir comando de uso único** prepara uma ocorrência pendente. Somente **Aplicar alteração**, no documento/célula, executa o efeito. Digitar, observar, mover, salvar, recalcular ou reabrir não executa. O comando é solitário, sem frase ou resultado. Ao executar, escreve literal fixo na fonte de destino e consome/retira o próprio uso no mesmo checkpoint. Undo restaura valor e comando; redo restaura efeito e consumo. Na célula, o texto simples anterior volta a aparecer.

Falha conserva candidato anterior e comando. A proteção preexistente recusa execução que quebraria resultado antes válido. Registros privados e usos retirados continuam conservados para histórico/integridade e contam nos limites; consumo não é exclusão física definitiva.

## Fonte única e formatos de autoria

Formato auxiliar 5, snapshot P2-D2 e linguagem 2 permanecem. Nenhuma tabela, coluna ou schema de projeto real mudou.

- `INDE-CARDS-1` permanece aceito integralmente, sem converter arquivos antigos.
- `INDE-CARDS-2` acrescenta título e símbolo da entrada no próprio cabeçalho da fonte. Condições e resultados continuam somente na expressão. Reconhecimento exige acordo integral entre cabeçalho e corpo.
- `INDE-BLOCK-1` identifica os cartões valor/conta/alteração, com metadados mínimos. Operandos, operação efetiva, alvo e valores atuais são reconstruídos da fonte; não ficam duplicados em outro banco/modelo persistido.
- `INDE-WRITTEN-1` guarda título de fórmulas livres novas, seguido do corpo autoral integral. Comentários, espaços, linhas vazias e CRLF do corpo são preservados em ida/volta; fonte livre antiga sem esse cabeçalho continua sem ele.

A API devolve `hub` e `cards` como vistas derivadas. `formula_hub_supported` impede oferecer o hub a binários antigos ainda em execução. Um binário antigo pode não reconhecer a edição por cartões novos; não se promete interface nova em versão anterior. A fonte usa a linguagem 2 existente. Não há promoção a schema canônico ou importação de obras.

Operações novas `create_formula_hub`, `edit_formula_hub`, `preview_formula_hub` validam campos/tipos/limites e delegam ao motor/registro existente. Edição estruturada admite somente o tipo integralmente reconhecido; a escrita livre é a escolha explícita para conservar/editar fonte arbitrária. Prévia não publica um novo estado. Respostas atrasadas não substituem o rascunho mais recente.

Fonte compartilhada e frase do uso são confirmadas juntas. Apresentações que intercalam outros resultados são conservadas; o hub não as achata para antes/resultado/depois. Na troca de tipo numérico/textual dos cartões de condição/valor, rascunhos de cada tipo são conservados na sessão do diálogo; somente o tipo escolhido é aplicado.

## Invariantes e limites

| Risco | Regra | Evidência |
| --- | --- | --- |
| Variável do cartão não poder ser alcançada | Referência visível, copiável e selecionável | Regra criada na UI recebe comando digitado; texto e fonte atualizam |
| Título novo romper referências | Título separado de símbolo/nome imutável | Edição de título aceita; troca forjada de símbolo recusada |
| Prévia executar o comando | Candidato temporário, sem checkpoint | Estado e revisão não mudam; antes/depois coincide com execução posterior |
| Comando reaplicar ao abrir | Consumo no mesmo checkpoint da fonte | SIGKILL/reabertura preservam três comandos consumidos e valores finais |
| Conta se desconectar do valor | Referências reais pelo motor comum | HP 100→80 muda conta 20→16 |
| Converter fonte livre perder autoria | Reconhecimento conservador e corpo livre literal | Comentários/linhas vazias/CRLF preservados; conversão estruturada aproximada recusada |
| Undo restaurar só metade | Estado completo no histórico existente | UI e quatro janelas de crash verificam valor/presença do comando |

Permanecem: 64 objetos conservados incluindo consumidos, 12 condições simples, conta visual com dois operandos/quatro operações, fontes até 16 KiB, textos calculados até 4.096 bytes e os orçamentos anteriores de avaliação, usos e histórico. Números são double finitos, não decimal exato. Grupos Todas/Alguma, mais operações visuais, GC de consumidos e histórico persistente ficam pendentes.

Também continuam pendentes entidades, aliases/pseudônimos, GTK, edição dos offsets originais, publicação canônica, migração de obras e `.inde` único. Este corte não inicia Fase 3 nem encerra toda a fase do piloto. A validação técnica é distinta do aceite humano do produto.

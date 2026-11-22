# Hub de fórmulas — cinco maneiras de criar

Na branch `feature/inde-obj-01-foundation`:

```sh
bash scripts/demos/Abrir-demo-obj01-hub.sh
```

Abra o endereço completo informado. O iniciador usa o motor C++ e SQLite reais, perfil P2-D2/formato privado 5, numa pasta sintética própria: `dados/obj01-piloto-hub/`. Não copia nem substitui as demonstrações anteriores. Para outro experimento, use `--workspace /tmp/meu-hub`.

## Escolher a finalidade

Em **Inserir fórmula**, escolha:

- **Escolher palavras ou frases**: cartões ordenados com entrada numérica/textual, comparação, frase e alternativa.
- **Fazer uma conta**: dois valores ou referências e uma operação de soma, subtração, multiplicação ou divisão. Contas podem alimentar outras contas.
- **Guardar um valor**: uma variável numérica ou textual, reutilizável no documento e em células.
- **Alterar um valor**: escolher uma variável real e definir, acrescentar ou diminuir seu valor. Texto aceita definir; soma/subtração são numéricas.
- **Escrever livremente**: digitar a fonte completa, incluindo comandos de uso único. Fonte incompleta pode permanecer com diagnóstico para reparo.

O **Nome da fórmula** é de apresentação e aceita texto livre. Em **Referências para outras fórmulas**, você encontra a referência da fórmula e da variável, que podem ser escolhidas na criação e ficam estáveis depois. Essas referências usam letras sem acentos, números e sublinhado; começam por letra/sublinhado. Os campos prontos podem ser selecionados para copiar. O nome de apresentação pode mudar sem alterar as referências.

## Ciclo principal: cartão → comando escrito → desaparecimento

1. Em **Escolher palavras ou frases**, conserve o exemplo Preço da negociação: preço inicial 16; maior que 15 → preço caro; igual a 15 → vou negociar; alternativa → que pechincha.
2. Expanda **Referências para outras fórmulas**: fórmula `Negociacao`, variável `Preco`, referência `Negociacao.Preco`. Insira o trecho.
3. Em **Inserir fórmula → Escrever livremente**, dê o nome Negociar e escreva somente:

```text
alterar Negociacao.Preco = 15
```

4. A prévia mostra `Negociacao.Preco: 16 → 15`, sem alterar o documento. **Inserir comando de uso único** prepara o comando no fluxo.
5. No documento, use **Aplicar alteração**. O preço passa a 15; a regra mostra “vou negociar”; o comando some do fluxo e da biblioteca ativa.
6. Desfazer restaura o preço anterior e o comando pendente; Refazer restaura o efeito e o consumo. Reabrir não repete o comando.

Na fórmula original, a declaração inicial continua e o atual aparece em atribuição separada. O editor mostra ambos e permite voltar à entrada original. Editar o inicial não elimina um atual explicitamente alterado.

## O mesmo ciclo com seletores

Reabra uma fórmula e expanda **Referências para outras fórmulas**. **Preparar alteração** abre um cartão novo com a variável já escolhida. Você também pode começar por **Inserir fórmula → Alterar um valor** e selecionar o alvo na lista. Não precisa conhecer ou digitar o caminho.

Escolha a operação, o valor ou uma referência existente. Confira o antes/depois e insira o comando. Inserir não executa; **Aplicar alteração** é a confirmação do efeito. A prévia pode mostrar também variáveis dependentes que mudarão. Um comando sem diferença de valor ainda é consumido ao executar. Um erro conserva o comando e os valores anteriores.

## Valor e conta ligados

Em **Guardar um valor**, o exemplo Vida do personagem cria `Personagem.HP = 100`. Em **Fazer uma conta**, escolha essa variável como Primeiro valor, Dividir e Segundo valor 5: resultado 20. Uma alteração que diminui `Personagem.HP` em 20 deixa HP 80 e atualiza essa conta para 16.

A entrada pode vir de outra fórmula ou variável; os seletores usam os registros reais do documento. Nome de personagem escrito no exemplo é literal, sem vínculo com entidade do Planejamento neste corte.

## Edição, recuperação e limites

Clique no resultado ou use **Abrir fórmula escolhida** para reabrir o cartão correspondente. A escrita livre continua integral. Fontes antigas não são convertidas por aproximação; continuam acessíveis na escrita livre ou, no caso de cartões de condição anteriores, com a referência antiga `Entrada` visível. A troca de tipo numérico/textual conserva os rascunhos de cada tipo durante a edição dos cartões de condição/valor; só o tipo escolhido é aplicado.

Aplicar confirma fonte e frase do uso no mesmo registro recuperável. Prévia e rascunho não gravam nem executam comandos. Cancelar conserva o estado anterior. Após reiniciar, o trabalho confirmado reaparece e o histórico em memória começa vazio. Salvar confirma o estado autoral.

Frases que intercalam outros resultados não são achatadas: o hub conserva a montagem e edita a regra; use Editar frase para alterar os trechos. Comandos ficam sozinhos, sem resultado/frase, e agem somente após execução explícita. Seus registros privados são conservados para integridade/histórico e contam no limite de 64 objetos; não há exclusão física automática/GC.

Neste corte, conta visual possui dois operandos e quatro operações; condições possuem até 12 cartões simples. A linguagem livre oferece os recursos já existentes. Nomes das referências ficam estáveis; não há renomeação com reescrita automática. Permanecem os limites de 16 KiB por fonte, strings calculadas de 4.096 bytes, avaliação, usos, histórico e armazenamento descritos nos contratos anteriores.

Sem vínculo com entidades/pseudônimos, integração GTK, edição de offsets dos parágrafos originais, migração de obras, publicação canônica, arquivo único `.inde` ou início de Fase 3. Esta é uma implementação real dentro do piloto isolado.

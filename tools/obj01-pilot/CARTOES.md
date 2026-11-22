# Texto por condições — cartões preenchíveis

Na branch `feature/inde-obj-01-foundation`, abra uma demonstração independente:

```sh
bash scripts/demos/Abrir-demo-obj01-cartoes.sh
```

Use o endereço completo informado. O iniciador cria/reabre `dados/obj01-piloto-cartoes/`, com motor C++ e SQLite reais, dados sintéticos e perfil P2-D2. Não copia, converte nem substitui as demonstrações anteriores. Outra pasta pode ser escolhida com `--workspace /tmp/meus-cartoes`.

## Roteiro sem escrever comandos

1. Escolha **Criar texto por condições**. Indique o lugar no documento, ou uma célula previamente selecionada.
2. Em **Entrada para comparar**, escolha Número ou Texto, dê um nome livre à entrada e preencha seu valor. Também pode escolher um resultado ou variável já existente, pela lista.
3. Preencha **Texto antes do resultado** e **Texto depois do resultado**. Espaços, pontuação e frases pertencem ao autor.
4. Em cada cartão, escolha a comparação, preencha o valor de comparação e escreva a palavra/frase desejada. **Adicionar condição**, **Remover**, **Subir** e **Descer** alteram a lista. Vale a primeira condição atendida.
5. Escreva a alternativa para quando nenhuma condição servir. A **Prévia no documento** usa o motor C++ real e não grava o rascunho.
6. Use **Inserir texto por condições** ou **Aplicar cartões e frase**. Regra e frase desse uso são registradas numa operação. Cancelar mantém o estado anterior.
7. No documento, clique no resultado para reabrir os cartões. Também pode usar a lista **Trechos com cartões** e **Editar cartões escolhidos**.

Exemplo numérico: entrada `o preço`, valor `16`; se maior que `15`, mostrar `preço caro`; se igual a `15`, mostrar `vou negociar`; senão `que pechincha`. Exemplo textual: entrada `Atitude`, valor `hostil`; se igual a `hostil`, mostrar `ameaçador`; se igual a `amistosa`, mostrar `receptivo`; senão `indecifrável`.

A frase pode ser preenchida como `Na feira, Armand comentou: ` + resultado + `.` ou `O visitante parece ` + resultado + `.`. Esses nomes escritos são literais; o vínculo com entidades do INDE ainda não faz parte deste corte.

## Comparação, compartilhamento e conservação

Números: menor, menor/igual, igual, diferente, maior/igual e maior. Texto: igual, diferente, contiver e começar com. Comparação textual é exata, sensível a maiúsculas, acentos e espaços; não converte `01` em `1`. Texto vazio pode ser comparado; vazio em contiver/começar com corresponde a qualquer texto. O campo numérico aceita a escrita numérica suportada pelo controle do navegador, incluindo fração e expoente; a fonte conserva sua escrita validada.

A mesma regra pode fornecer resultado para documento e células. Editar a regra afeta seus dependentes; a frase é própria de cada uso. Frases que já intercalam outros resultados não são achatadas: os cartões editam somente a regra, e **Editar frase** mantém a montagem completa.

Contas e comandos escritos continuam em **Contas e comandos avançados**. **Abrir fonte avançada** exige aplicar/cancelar primeiro os rascunhos dos cartões. Somente a forma integralmente reconhecida abre no editor visual. Comentários ou expressões livres fora dessa forma permanecem na edição avançada; não são convertidos por aproximação.

Cada operação confirmada protege o trabalho para recuperação. O rascunho só existe no navegador até ser aplicado; uma prévia não o torna recuperável. Desfazer/refazer inclui regra e frase. Após reinício, o trabalho confirmado reaparece com histórico vazio. Salvar confirma o estado autoral; Reabrir salvo pode descartar o trabalho posterior mediante o aviso normal da interface.

Limites: até 12 condições simples, sem grupos Todas/Alguma neste corte; nome da entrada até 256 bytes UTF-8; entrada e texto de cada alternativa até 4.096 bytes; fonte total de 16 KiB. Permanecem limites de objetos, usos, histórico e avaliação do [perfil P2-D2](P2D2.md). Referências são escolhidas entre fórmulas/variáveis reais do piloto; não há catálogo de personagens simulado.

O formato privado 5 e a linguagem 2 são conservados, sem nova tabela ou coluna. Texto e fórmulas anteriores permanecem. As funções textuais novas requerem este motor; um binário anterior pode diagnosticá-las como função desconhecida. Não há promessa de executar todas as fontes novas em versões antigas.

O piloto continua separado do aplicativo GTK, da escrita nos parágrafos originais, da publicação canônica e dos projetos autorais. Não inicia a Fase 3 nem produz arquivo único `.inde`.

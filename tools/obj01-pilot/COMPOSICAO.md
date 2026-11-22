# Compor e remover objetos

Na branch `feature/inde-obj-01-foundation`, abra:

```sh
bash scripts/demos/Abrir-demo-obj01-composicao.sh
```

Use o endereço completo informado. O iniciador abre o motor C++/SQLite real numa pasta sintética nova, `dados/obj01-piloto-composicao/`. Para outro ensaio, acrescente `--workspace /tmp/minha-composicao`. As demonstrações anteriores não são copiadas nem convertidas.

## Escolher e compor

Selecione um objeto no documento ou na lista **Objeto no documento**. **Compor** no objeto ou **Compor objeto selecionado** abre os controles comuns e passa para a apresentação visual. Texto, figura, tabela e fórmula oferecem as mesmas ações.

1. Em **Colocar lado a lado**, escolha o outro objeto, o lado do selecionado, as larguras das colunas e o espaço entre elas.
2. **Mostrar lado a lado** aproxima os dois na apresentação, inclusive se estavam distantes. Conserva a ordem de leitura. Seus pares anteriores são desfeitos; os outros objetos permanecem.
3. Em **Tamanho e alinhamento**, escolha largura, posição dentro da coluna, alinhamento do texto e alinhamento vertical na linha. Use **Aplicar tamanho e alinhamento**.
4. **Voltar à coluna única** desfaz o par. **Restaurar tamanho e alinhamento** limpa somente a formatação individual do selecionado.
5. Compare **Apresentação** e **Ordem de leitura**. A leitura ignora os pares e as larguras visuais. **Restaurar apresentação pela leitura** recupera a sequência e desfaz todos os pares, conservando estilos individuais.

A largura do objeto vai de 20% a 100% da área disponível; numa linha, é relativa à sua coluna. Há posição esquerda/centro/direita, texto à esquerda/centralizado/direita/justificado e topo/meio/base. O alinhamento textual também vale para o texto nas células da tabela. Figuras conservam proporção e descrição para leitura.

O botão **Mover** continua distinguindo leitura + apresentação de somente apresentação. Para texto de origem, mudar a leitura leva o grupo completo pelo motor proprietário; mover só a apresentação pode reposicionar um parágrafo individual. Arranjar nunca reescreve o texto nem seus offsets.

## Remover e restaurar

Passe o mouse ou use o teclado no objeto: **Selecionar**, **Compor** e **Remover** aparecem junto dele. Também pode usar **Remover uso selecionado** em Estrutura. A ação direta muda o instrumento automaticamente e confirma antes os textos pendentes. Não apaga arquivos ou recursos físicos.

- Figura: retira aquele uso; outros usos da mesma imagem continuam.
- Tabela: retira a tabela com seus textos e vínculos de fórmulas conservados.
- Fórmula: retira o uso no fluxo; a fórmula compartilhada permanece na biblioteca e continua alimentando suas dependências. Dentro de uma célula, retirar apenas a fórmula continua disponível nas ferramentas de fórmulas.
- Texto de origem: retira o grupo protegido completo. A interface informa quantos parágrafos serão afetados. Não corta trechos protegidos nem apaga o snapshot autoral.

**Desfazer** restaura a operação inteira, incluindo os pares desfeitos pela remoção. **Refazer** repete a retirada. Esses históricos valem na sessão.

**Itens removidos** permite escolher o conteúdo e onde restaurá-lo, inclusive depois de fechar/reabrir. Restauração manual conserva o conteúdo e o estilo individual, insere no destino escolhido e não recria os pares antigos. O destino deve respeitar os grupos textuais; a apresentação precisa estar alinhada à leitura. Se houver divergência, restaure a apresentação pela leitura primeiro. A remoção em si é permitida mesmo com apresentação independente.

Comandos `alterar` consumidos não são oferecidos como itens restauráveis: somente Desfazer a execução pode restaurar o comando e seu efeito juntos. Não há exclusão definitiva/GC nem renomeação global de identidades.

## Demonstração e limites

O ensaio documentado contém texto com texto, texto com tabela e figura com fórmula, formatações distintas, célula editada e uma imagem retirada disponível para restaurar. Trabalho confirmado é recuperável antes de Salvar; o histórico da sessão reinicia após reabrir. Rascunhos ainda não aplicados de geometria ficam no navegador.

A primeira operação nova que precisa da camada de composição usa snapshot privado `INDE-OBJ01-COMPOSITE-P2D2-2`. O perfil é P2-D2 e o banco auxiliar permanece versão 5, sem novas tabelas/colunas. Snapshot anterior continua legível; enquanto a camada não é usada, conserva o formato anterior. Binário antigo recusa o novo snapshot sem regravá-lo. Os pares antigos figura/parágrafo continuam válidos.

Este corte trabalha com linhas de dois objetos, largura proporcional e alinhamentos, sem arrasto livre X/Y, rotação, sobreposição, quebra de texto em torno de imagens, linhas aninhadas ou grades arbitrárias. Não torna editável o corpo dos parágrafos de origem. Tabelas continuam com duas linhas/três colunas neste piloto. A exclusão individual de parágrafo dentro de grupo protegido exigirá um corte próprio no motor de texto.

Sem integração GTK, publicação canônica, migração de obras, alteração de schema real, arquivo único `.inde` ou início de Fase 3. [Contrato](../../docs/arquitetura/COMPOSICAO-REMOCAO-OBJ-01.md) e [validação](../../docs/validacao/VALIDACAO-OBJ-01-COMPOSICAO.md).

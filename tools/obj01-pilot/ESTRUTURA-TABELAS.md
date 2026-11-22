# Linhas e colunas recuperáveis

Na branch `feature/inde-obj-01-foundation`, execute:

```sh
bash scripts/demos/Abrir-demo-obj01-estrutura-tabelas.sh
```

Abra o endereço completo informado. Esta demonstração usa o motor C++ e o armazenamento SQLite reais, com dados sintéticos em `dados/obj01-piloto-estrutura-tabelas/`. Para outra pasta, acrescente `--workspace /tmp/outra-tabela`. Nenhuma demonstração anterior é copiada ou convertida.

## Criar e transformar

Em **Estrutura**, escolha de 1 a 64 linhas e de 1 a 32 colunas na criação. As células começam vazias. Título, cabeçalho, textos e fórmulas continuam editáveis pelos instrumentos já existentes.

Selecione a tabela e abra **Linhas e colunas**, junto da tabela. Escolha a linha ou coluna de referência para inserir antes/depois ou retirar. O painel informa quantas células preenchidas e fórmulas serão conservadas. Inserir ou retirar confirma primeiro os textos pendentes e usa o instrumento Estrutura.

**Retirar** deixa a linha ou coluna fora da apresentação e conserva suas identidades, textos, fórmulas, frases e detalhes de aparência. Ela aparece em **Linhas e colunas retiradas**, com posição, uma amostra de texto e contagem de conteúdo. **Restaurar linha ou coluna** devolve o item à posição relativa conservada. A última linha e a última coluna visíveis são protegidas; para retirar tudo, remova a tabela inteira.

Cada inserção, retirada ou restauração é um passo de desfazer/refazer. Salvar e reabrir conserva a estrutura inteira e a lista de retiradas. O trabalho confirmado ainda não salvo também é recuperável. O histórico de desfazer é da sessão e não sobrevive ao reinício.

## Aparência e fórmulas

O editor de [aparência da tabela](TABELAS.md) acompanha a quantidade atual de linhas e colunas. As exceções seguem as identidades das células, não seus números na tela. Uma célula destacada continua destacada depois de inserir uma linha antes dela.

Novas células começam vazias e herdam a aparência geral. Aplicar um detalhe à coluna inteira altera as células existentes e visíveis naquele momento; isso não cria uma regra permanente para células futuras. Reaplique esse detalhe se desejar estendê-lo às novas células.

Linhas/colunas retiradas conservam suas exceções de cor, alinhamento, altura e largura. Editar a parte visível não apaga esses detalhes. As larguras visíveis são redistribuídas proporcionalmente; a parcela de uma coluna retirada fica conservada para a restauração. O cabeçalho acompanha sempre a primeira linha visível.

Fórmulas de células retiradas continuam disponíveis na biblioteca e podem alimentar outras fórmulas. Retirar ou restaurar não executa comandos de alteração nem os consome. Para editar uma célula retirada, restaure primeiro sua linha e sua coluna. É possível retirar ambas e inserir outras linhas/colunas nesse intervalo: as interseções antigas continuam conservadas e as novas são criadas vazias.

## Roteiro curto

1. Crie uma tabela de 3 linhas e 4 colunas: Nome, Papel, Condição e Vida.
2. Preencha duas personagens; numa célula de Vida, insira uma fórmula pelo hub e uma frase como `100 pontos`.
3. Escolha Caderno na aparência. Destaque essa célula e alinhe a coluna Vida à direita.
4. Insira uma linha antes da personagem e uma coluna antes de Papel. Confira que a fórmula e o destaque acompanham a célula original.
5. Retire a linha da personagem e a coluna nova. Insira outra linha enquanto ambas estão retiradas.
6. Salve e reabra. Restaure coluna e linha; confira a frase, o resultado, os detalhes e as interseções vazias da linha nova.
7. Experimente desfazer/refazer durante a sessão. Reaplique o alinhamento da coluna se desejar incluí-lo nas células novas.

## Limites e compatibilidade

O limite de 64 linhas e 32 colunas inclui as retiradas: no máximo 2.048 células conhecidas por tabela. A retirada conserva dados e não libera essa capacidade. Ainda não há descarte definitivo, mesclagem, reordenação interna por arraste, alças de redimensionamento ou formatos numéricos.

O perfil continua P2-D2, linguagem de fórmulas 2 e SQLite auxiliar versão 5. A primeira operação de estrutura usa snapshot privado explícito `INDE-OBJ01-COMPOSITE-P2D2-4`, com `composition.table_structure`. Os formatos 1, 2 e 3 continuam legíveis e mantêm sua forma até usar a capacidade nova. O motor anterior recusa o formato 4 sem regravar origem/banco.

O trabalho permanece no piloto isolado, sem GTK, publicador canônico, migração de obras ou `.inde` único. A aprovação técnica não substitui a revisão de uso pelo autor.

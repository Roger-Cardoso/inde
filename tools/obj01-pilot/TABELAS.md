# Tabelas visuais — aparência, tamanho e células

Na branch `feature/inde-obj-01-foundation`, execute:

```sh
bash scripts/demos/Abrir-demo-obj01-tabelas.sh
```

Abra o endereço completo informado. O motor C++/SQLite é real; os dados são sintéticos e ficam somente em `dados/obj01-piloto-tabelas/`. Outra pasta pode ser escolhida com `--workspace /tmp/minhas-tabelas`. Nenhuma demonstração anterior é copiada ou convertida.

## Abrir a aparência

Selecione uma tabela e use **Aparência da tabela** na lateral ou junto da tabela. O editor confirma primeiro textos pendentes e abre três áreas com prévia:

- **Aparência:** Editorial, Caderno, Grade ou Noite; título visível/oculto e acima/abaixo; primeira linha como cabeçalho; letra Clara/Livro/Monoespaçada, tamanho e entrelinhas; cores de fundo, texto, cabeçalho, faixas e bordas; grade, linhas horizontais, contorno ou sem bordas; traço contínuo/tracejado/pontilhado e espessura.
- **Tamanho e posição:** largura e alinhamento do objeto inteiro, texto geral, posição na linha lado a lado, espaços internos, altura mínima geral/por linha e proporção de cada coluna. Alterar uma coluna redistribui o restante; **Distribuir colunas igualmente** desfaz essa personalização. **Usar altura geral** retira a exceção de uma linha.
- **Células:** escolha a linha/coluna de referência e aplique à célula, linha ou coluna inteira. Alinhamento horizontal/vertical, negrito, itálico, fundo e cor do texto são independentes. Mudar uma propriedade conserva as outras. Herdar ou retirar detalhes devolve o padrão da tabela.

Os modelos de estilo conservam as larguras de coluna, alturas específicas e detalhes de células. **Restaurar aparência padrão** limpa a aparência personalizada, incluindo essas exceções, e conserva título, cabeçalho e posição do objeto. A prévia mostra o conteúdo real atual, inclusive resultados de fórmulas; não recalcula ou executa comandos em outro motor.

**Aplicar à tabela** confirma título/cabeçalho, aparência e posição juntos. **Cancelar** ou Esc conserva a tabela anterior. Rascunhos ficam somente no navegador até aplicar. Você pode desfazer e refazer o conjunto.

## Escrever e compor

As células crescem para mostrar o texto inteiro; altura mínima não é corte de conteúdo. Em Texto, continue escrevendo nas células e usando Tab/Shift+Tab/Ctrl+Enter. Os estilos são herdados também pelo texto com resultados de fórmulas. O resultado continua protegido e sua fonte continua acessível.

Largura é percentual do espaço do objeto: numa composição lado a lado, é relativa à coluna da tabela. Use **Compor objeto selecionado** para pares com texto, figura, tabela ou fórmula. A edição de aparência conserva pares existentes e a ordem de leitura. O alinhamento vertical da tabela na linha é diferente do alinhamento do conteúdo dentro das células.

Remover/restaurar uma tabela conserva sua aparência. Salvar e reabrir conserva o conjunto. Operações confirmadas antes de Salvar continuam recuperáveis neste perfil; undo em memória não sobrevive ao reinício.

## Roteiro curto

1. Crie uma tabela com título Inventário de viagem e preencha uma célula com uma observação longa.
2. Abra Aparência da tabela, escolha Caderno e personalize cabeçalho e bordas.
3. Em Tamanho e posição, largura 90%, centro e coluna de observação 55%. As outras passam a 22,5% cada. Ajuste altura mínima por linha.
4. Em Células, escolha a coluna de quantidades e alinhe à direita; depois destaque só uma célula com fundo e negrito.
5. Aplique. Confira o texto inteiro, desfaça/refaça e reabra o editor.
6. Crie outra tabela com uma fórmula em célula e escolha Noite. Confira contraste, resultado e independência dos estilos entre as tabelas.

## Compatibilidade e limites

O perfil continua P2-D2 e o SQLite auxiliar permanece versão 5, sem nova tabela/coluna. A primeira edição de aparência usa snapshot privado explícito `INDE-OBJ01-COMPOSITE-P2D2-3`; formatos 1/2 continuam legíveis e mantêm sua forma até a nova edição. O motor anterior recusa o formato 3 sem regravar origem/banco. Os oito experimentos anteriores ficam preservados.

Medidas desta interface local usam pixels para letra/espaços/alturas e percentuais para largura/colunas. Fonte entre 10 e 32, entrelinha 1 a 2,2; espaços internos 0 a 32; altura geral 24 a 240 e específica até 480. O texto pode expandir a linha além do mínimo. Sem CSS livre: fontes, traços e alinhamentos são escolhas fechadas e cores usam hexadecimal.

O corte original de aparência criou tabelas 2×3. A evolução seguinte acrescenta criação com dimensões escolhidas, inserção, retirada e restauração de linhas/colunas; consulte [Linhas e colunas recuperáveis](ESTRUTURA-TABELAS.md). Mesclar células, formatar números, bordas individuais e redimensionar com alças seguem pendentes. Não há integração GTK, edição do corpo original, publicação canônica, migração de obras ou `.inde` único. [Contrato original de aparência](../../docs/arquitetura/TABELAS-VISUAIS-OBJ-01.md) e [validação daquele corte](../../docs/validacao/VALIDACAO-OBJ-01-TABELAS-VISUAIS.md).

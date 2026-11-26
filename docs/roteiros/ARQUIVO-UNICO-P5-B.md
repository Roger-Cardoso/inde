# Usar o arquivo único no INDE — P5-B

Execute com Bash [Abrir-demo-arquivo-unico-nativo.sh](../../scripts/demos/Abrir-demo-arquivo-unico-nativo.sh), sem argumentos. Ele cria outro projeto fictício e abre **o arquivo `.inde` diretamente no INDE nativo**. Não precisa registrar a pasta depois que o programa encerra. Não usa projetos autorais.

O projeto de demonstração tem texto, alternativa privada, visibilidade, anotação, Composição textual e rascunho, tabela, imagem, equação, linha lado a lado e planejamento ligado ao documento.

1. Abra o documento “Uma obra transportável” e escreva no final.
2. Use **Salvar / Ctrl+S**. O estado passa a estar confirmado no arquivo único.
3. Abra **Projeto → Estados do arquivo único… / Ctrl+Alt+H**. Escolha um estado anterior e clique “Retomar este estado”. Uma revisão é acrescentada; as posteriores continuam disponíveis.
4. Use **Projeto → Salvar cópia em arquivo único… / Ctrl+Alt+S**. Escolha copiar com o histórico ou continuar numa cópia do estado atual. O original é preservado. As cópias continuam sendo o mesmo projeto, com os mesmos identificadores.
5. Encerre e abra novamente pelo arquivo ou por Recentes. Os caminhos exibidos são os do `.inde`.

Para começar outro projeto nesse formato, use **Projeto → Novo arquivo único `.inde`… / Ctrl+Alt+N**. Para selecionar um arquivo existente, use **Projeto → Abrir arquivo único `.inde`… / Ctrl+Alt+O**. Os controles antigos de projetos em pasta continuam disponíveis.

Se houver trabalho recuperável diferente do arquivo, o programa oferece retomar a área ou abrir o salvo, mantendo a outra cópia. Se a confirmação do arquivo falhar ao encerrar, é possível voltar ou encerrar preservando a área indicada. Essa opção só se aplica depois de os escritores nativos terem gravado o trabalho na área persistente.

A confirmação no arquivo ainda usa cópias de componentes. Cotas e crescimento do histórico estão descritos no [contrato](../arquitetura/ARQUIVO-UNICO-P5-B.md). Use a demonstração e cópias de teste nesta etapa; a Fase 5 completa e a adoção autoral geral ainda não estão certificadas.

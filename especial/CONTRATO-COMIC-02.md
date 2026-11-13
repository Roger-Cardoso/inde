# COMIC-02 · criação e espaço de trabalho

Direção solicitada e aplicada em 02/11/2026. Substitui a localização da autoria comic descrita no COMIC-01. O modelo persistente existente é conservado; esta etapa reorganiza a autoria no aplicativo e acrescenta as bases de interação do espaço próprio.

## Navegação

A barra principal contém, nesta ordem:

```text
Criar ▾   Planejar   Revisar   Manuscrito   Cronologia   Mapas   Obras
```

Criar oferece Escrita e Comic. Manuscrito, Cronologia, Mapas e Obras são botões diretos. Os nomes curtos não renomeiam Obras, entidades ou dados autorais. A barra pode rolar horizontalmente em uma janela estreita, mantendo os acessos diretos. O menu principal mantém os atalhos completos sob Espaços.

Comic tem identidade própria no sistema de espaços do INDE, ação de navegação, região de foco e contexto de interface. Não é uma janela flutuante, formulário de Publicação ou processo do estúdio Python.

## Organização do espaço Comic

- Lateral de biblioteca: comics do Projeto, criação/retirada/restauração e sequência de quadros com seleção e reordenação.
- Centro: quadro, arte e elementos; zoom ajustado ao espaço ou escalas definidas, rolagem da área de trabalho, seleção, arraste, redimensionamento pelo canto e guia de centro.
- Inspetor: abas Elemento, Quadro e Obra, com texto, cores, fontes, geometria, alinhamentos, idioma/direção/tradução, proporção do quadro e metadados.
- Publicação: controles de preparação e prévia usando um Site existente. A configuração de Site/Edição e o catálogo de novels continuam no painel comum de Publicação, que deixa de conter o editor de comics.

O rascunho da comic permanece no espaço ao alternar para Escrita ou outro espaço. As alterações de composição ficam em memória até Salvar comic/Salvar Projeto; recuperação automática de conteúdo pendente após falha do processo pertence à continuidade da migração. Fechar/trocar o Projeto e Salvar cópia exigem resolver alterações pendentes. Salvar Projeto também salva o rascunho comic. Uma operação de geração ou escolha de arquivo tem proteção de ciclo de vida própria.

## Contexto, histórico e texto

O contexto conserva comic, quadro, elemento, zoom, rolagem e aba do inspetor, e pode acompanhar os espaços de trabalho salvos do INDE. Contextos anteriores sem o campo comic continuam aceitos. A troca de Projeto limpa o estado do Projeto anterior.

Desfazer/Refazer da composição é próprio do espaço Comic e preserva o histórico da Escrita. São conservados até 40 estados de composição; o histórico não repete os bytes de cada imagem/fonte em todos os estados. Recursos importados podem ser retirados/repostos pelo histórico sem perder seus bytes. A geração pública continua usando somente os recursos selecionados pelo modelo, conforme o contrato anterior.

Entradas provisórias do método de entrada do GTK não são gravadas no modelo. A navegação e o salvamento aguardam o encerramento da composição de texto. A escrita continua no mesmo Documento/buffer e usa os serviços existentes do INDE.

A cópia explícita de texto de Documento continua sendo uma cópia com origem registrada. Ela não se torna sincronização automática por causa da nova navegação.

## Persistência e publicação

A extensão de comics continua schema 1, a publicação continua schema 3 e a base autoral continua schema 28. A interface nova abre os comics do COMIC-01 sem converter ou descartar seus dados. O estúdio Python 0.4 permanece independente até existir um importador completo e comparado.

O gerador C++ e o mecanismo existente de materialização/ZIP/prévia são reaproveitados. O catálogo público misto ainda exige seleção tipada de mídia, Edições comic, revisão de rotas e preparação conjunta.

## Continuidade da migração integral

A mudança da navegação e do espaço de trabalho é esta entrega. A equivalência integral com o estúdio 0.4 exige os próximos trabalhos de autoria:

1. Caudas, conexões e contornos completos, com seus controles de manipulação.
2. Montagem livre de páginas, camadas, organização dos quadros e snap entre elementos.
3. Texto rico internacional, leituras auxiliares, gestão completa das fontes e diagnóstico de glifos.
4. Medição tipográfica e cálculo de tamanho por cenários de resolução.
5. Importação comparada do formato 0.4, com identidades e recursos conservados e perdas bloqueadas.
6. Edições/episódios e catálogo unificado de novels e comics.

Esses itens não são apresentados como concluídos pela criação do novo espaço. A meta continua sendo o estúdio completo em C++/GTK e integrado ao Projeto e à publicação do INDE.

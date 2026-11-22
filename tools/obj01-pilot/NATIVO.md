# Objetos e matemática no INDE nativo

Na branch `feature/inde-obj-01-foundation`, o aplicativo GTK tem o botão **Objetos** no cabeçalho. Ele usa o documento selecionado na **Escrita** e abre a composição dentro do próprio aplicativo. O motor C++ é compartilhado com os testes e o piloto; a janela não inicia Python, servidor HTTP ou navegador externo.

## Experimentar com um projeto novo

```sh
bash scripts/demos/Abrir-demo-objetos-nativos.sh
```

O iniciador compila o aplicativo, cria apenas a amostra nova `dados/obj01-objetos-nativos/Projeto.inde` se ela ainda não existir, verifica o percurso e abre o INDE. Se a criação anterior tiver falhado, a pasta existente é preservada; o iniciador não apaga nem substitui projetos. Na Escrita, selecione **Matemática na narrativa** e use **Objetos**. As demos anteriores permanecem separadas.

Para usar o aplicativo com outro projeto, abra-o normalmente pelo INDE desta branch, selecione um documento e use o mesmo botão. Se houver escrita sendo salva, aguarde concluir antes de abrir Objetos. Recomenda-se experimentar primeiro numa cópia: esta integração é candidata de desenvolvimento, com os limites abaixo.

## Trabalhar no documento

- **Estrutura:** inserir PNG, tabelas com dimensões escolhidas, fórmulas pelo hub e equações; mover, retirar e restaurar objetos e grupos textuais.
- **Texto:** editar células e frases ao redor dos resultados; abrir a fonte das fórmulas/equações. A edição do corpo textual original continua na Escrita.
- **Composição:** colocar objetos lado a lado, ajustar largura, posição e alinhamento; abrir a aparência da tabela.
- **Salvar documento:** confirma a composição por objetos. Operações aplicadas antes de Salvar já são recuperáveis.
- **Salvar projeto**, na janela principal, também confirma a composição desse projeto depois de fechar a janela de Objetos. “Salvar cópia” leva os arquivos da composição na cópia do projeto.

Uma edição aberta deve ser aplicada ou cancelada antes de fechar a janela. O fechamento consulta o editor para conferir rascunhos e pedidos em andamento. Alterações já aplicadas, mas ainda não salvas, podem ser retomadas; o histórico de desfazer reinicia ao abrir outra sessão.

As capacidades anteriores continuam disponíveis: [hub de fórmulas](HUB.md), [composição e restauração](COMPOSICAO.md), [aparência de tabelas](TABELAS.md) e [linhas/colunas recuperáveis](ESTRUTURA-TABELAS.md).

## Matemática inspirada em LaTeX

Use **Inserir equação**. Escolha um modelo — Fração, Raiz, Somatório, Integral, Matriz ou Sistema — ou escreva a fonte. A prévia acompanha a edição. Exemplos:

```latex
x = \frac{-b \pm \sqrt{b^2-4ac}}{2a}
\sum_{i=1}^{n} i = \frac{n(n+1)}{2}
\int_0^1 x^2\,dx = \frac{1}{3}
\begin{pmatrix} a & b \\ c & d \end{pmatrix}
```

**Em destaque** usa a apresentação matemática de bloco. **Compacta** usa a tipografia inline dentro do objeto; isso não significa inserção livre entre caracteres do parágrafo original. Nome, fonte e modo são conservados. Clique em **Editar equação** para retornar à fonte. A equação participa da composição, tamanho/alinhamento, retirada/restauração e histórico como os outros objetos.

**Equação apresenta notação; Fórmula calcula valores e condições.** Não há conversão automática entre a linguagem de Fórmula e LaTeX. Uma equação não altera variáveis. Fonte incompleta ou comando não suportado gera diagnóstico e continua editável; pode ser guardada sem perder o texto. Não é um compilador de documentos LaTeX nem implementa todos os pacotes/comandos TeX.

KaTeX 0.19.1, CSS e fontes estão incluídos localmente. A edição não exige rede. O renderizador não recebe confiança para comandos que carregam recursos externos, com limites de expansão e tamanho. A saída inclui representação visual e MathML; a fonte original continua sendo o dado autoral. Não são executados TeX, comandos do sistema ou acesso a arquivos a partir da expressão.

Limites atuais: 64 equações conhecidas, incluindo retiradas; fonte UTF-8 até 16 KiB; nome até 256 bytes. As regras de tamanho e histórico do documento composto continuam valendo. As equações são objetos próprios, ainda sem catálogo de usos compartilhados ou numeração automática.

## Armazenamento e fronteiras reais

Cada documento possui sua composição em `documents/composed/<id-do-documento>/` dentro do projeto `.inde`, com origem capturada, SQLite auxiliar, conteúdo dos objetos e recursos. O banco canônico permanece schema 37; o auxiliar permanece versão 5. A matemática usa snapshot privado explícito `INDE-OBJ01-COMPOSITE-P2D2-5`, sem conversão dos formatos anteriores até usar a capacidade nova.

O texto original e suas camadas não são substituídos pela composição. Se o documento original for alterado na Escrita depois de criar essa composição, a reabertura recusa a divergência e conserva os dois estados. A reconciliação/edição unificada dessas versões ainda está pendente. Enquanto isso, termine o texto na Escrita antes de compor os objetos, ou trabalhe com documentos/cópias separados.

O publicador anterior recusa documentos que têm uma composição por objetos, pois ainda não há tradução completa desse conteúdo para Sites/Edições. A publicação dos demais documentos mantém o fluxo anterior. Não se gera uma publicação que omita os objetos silenciosamente.

Continuam pendentes: vínculo das variáveis com entidades/pseudônimos reais, edição livre inline no corpo original, equações em células como conteúdo tipado, numeração/referências de equações, serviço comum com Comic, publicação dos objetos e `.inde` único. Esta entrega integra as capacidades implementadas ao aplicativo; não conclui automaticamente esses marcos futuros.

Use os projetos com composição por objetos no aplicativo desta branch. Executáveis anteriores continuam lendo o corpo textual canônico e não apresentam essa composição; a nova proteção do publicador também pertence a esta branch e não modifica programas antigos já instalados. Preserve uma cópia separada ao experimentar entre versões.

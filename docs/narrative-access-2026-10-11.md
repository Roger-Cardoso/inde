# Acesso inicial e interação narrativa — 2026-10-11

Complemento posterior: [compatibilidade do tema herdado](theme-compatibility-2026-10-11.md).
Os resultados de build/GTK abaixo pertencem ao candidato de acesso original.
A execução humana revelou CSS externo Orchis não coberto pelo Xvfb sem
XSettings; não tratar aquela ausência de avisos como prova para esse ambiente.

Correção autorizada pelo pedido de acesso em Projeto limpo, ações contextuais,
seleção comparativa e layout sólido sem divisórias arrastáveis. Base privada
`995fbb27` + fundação/alternativas já entregues. Schema permanece 27.
Não representa aceite humano da versão anterior nem publicação.

## Contrato de interação

| Situação | Resposta da interface |
| --- | --- |
| Projeto sem Obras | Explicação no Planejar e botão para começar ali mesmo; estruturas narrativas e Manuscrito têm entradas locais para a mesma criação. |
| Criar Obra | Título, universo novo ou existente; nome do universo novo pode herdar o título. Campos editoriais complementares continuam editáveis no Catálogo. |
| Cancelar / título inválido | Cancelar não grava. Validação mantém os campos e explica o problema, sem fechar o formulário. |
| Cadastrar Obra e universo | Uma transação e operação reversível; falha na Obra não deixa um universo órfão. As estruturas iniciais vazias do banco são preservadas; não há prosa, capítulos ou fórmula aplicada. |
| Selecionar uma alternativa | Um clique seleciona; duplo clique ou Enter abre. Instrução curta na biblioteca. |
| Ações da alternativa | Três pontinhos próprios: Comparar com, Duplicar, Derivar, Ativar, Salvar como Modelo, Remover. Ativar fica indisponível quando já está ativa. |
| Comparar com | Estado explícito de escolha de alvos, origem identificada, caixas e clique em linha; ação flutuante Comparar e Cancelar. Até oito alvos por consulta. |
| Resultado de múltiplos alvos | Comparações pareadas na área de trabalho, com seletor de destino; não empilha painéis ou promove uma alternativa. |
| Modelos reutilizáveis | Painel contextual aberto por botão, com rolagem limitada; não permanece ao lado da biblioteca. Menu por item: Aplicar / Remover; modelo do sistema não pode ser removido. |
| Dentro da estrutura | Seções Unidades, Linhas e Vínculos, uma por vez com a largura disponível. Papéis disponíveis e Atribuições seguem a mesma separação. Nenhuma divisória arrastável neste workspace. |
| Criar vínculo na seção própria | Origem e destino declarados no formulário; não depende de selecionar uma unidade numa seção oculta. |
| Retomar | A seção do detalhe é salva no contexto da interface. Contextos anteriores sem essa propriedade continuam aceitos. Comparações são refeitas, nunca salvas como aprovações. |

Documentos, entidades e tempo não passam a exigir uma Obra artificialmente.
O aviso inicial explica essa distinção. Universo / Propriedade Intelectual é uma
organização do Projeto, não um registro jurídico. Nenhuma associação semântica
ou entidade é escolhida silenciosamente.

## Autoridade e segurança preservadas

- Montagem narrativa, cronologia, fila de escrita e apresentação editorial
  continuam independentes. Trocar de seção ou selecionar alvos não muda dados.
- Modelos são esqueletos; duplicação/derivação contextual e os cadastros externos
  compartilhados continuam sob as políticas explícitas da fase anterior.
- Remoção mantém impacto, usos e confirmação do auxiliar de exclusão; abrir
  um uso de modelo agora também abre o painel contextual onde ele está.
- Cada ação captura a identidade de sua própria linha. Não depende de uma
  seleção antiga de outro item. A função da ação sobrevive à reconstrução
  da lista que pode destruir o botão de origem.
- Nomes longos têm apresentação limitada sem alterar a identidade ou o texto
  armazenado; cabeçalhos usam ações que podem quebrar linha. Páginas ocultas
  não impõem a dimensão mínima das páginas visíveis.

## Avisos: causas e correções

| Classe | Correção |
| --- | --- |
| API C obsoleta para sondar combos e renderizar fundo em testes | Métodos do modelo gtkmm; leitura de pixels do widget real por WidgetPaintable. Não há pragma nem opção para desativar avisos. |
| Seletor de cores do editor dependente de GtkColorChooser obsoleto | Controle HSV gráfico em Cairo e escalas GTK, paleta, RGB e hexadecimal sincronizados. Mantém confirmação, cancelamento e proteção contra revisão obsoleta, sem elevar a versão mínima do GTK. A prévia usa a superfície do tema ativo. |
| Resultados nodiscard descartados em testes de recusa | Descarte intencional explícito dentro da asserção de recusa; APIs continuam nodiscard. Resultados úteis de salvamento são mantidos. |
| Campos agregados sem inicialização | Inicialização explícita de variantes e padrão explícito de revisão de calendário, sem alterar valores persistidos. |
| Referência ambígua / controle de fluxo | Valor estável no teste de usos; blocos e formatação inequívocos nas ações da interface. |
| EGL/DRI3 quando Cairo foi escolhido | Configuração antes da inicialização GTK: não sondar GL/Vulkan desnecessários ao renderizador Cairo. Preserva escolha explícita de outro renderizador e não altera o sistema. |

O GTK 4.14 usa `GDK_DEBUG=gl-disable,vulkan-disable`; a partir de 4.16
usa `GDK_DISABLE=gl,vulkan`. O ajuste considera a versão realmente carregada.
Referências primárias: [GTK 4.14.5](https://github.com/GNOME/gtk/blob/4.14.5/gdk/gdk.c)
e [GTK 4.16.0](https://github.com/GNOME/gtk/blob/4.16.0/gdk/gdk.c).
O INDE não usa uma área GL nos fluxos aqui alterados; Cairo permanece a política
restrita de compatibilidade já existente para Intel 8086:0102 ou escolha explícita.

## Verificação e revisão

Verificação técnica concluída: recompilação das 277 unidades habilitadas,
sem avisos nas compilações finais; CTest Release 31/31. Núcleo Debug e ASan/UBSan
focais passaram três drivers, com dez unidades do núcleo. Interface de acesso
normal e instrumentada passou 8/8 em cada matriz; regressões do construtor,
alternativas e temporal passaram 8/8 cada. Cores/laboratório passaram 8/8 e cores
instrumentadas 4/4: 52 verificações de execução, mais dois processos reais de
inicialização sem avisos. Quinze unidades da interface de acesso e quatro do
seletor foram instrumentadas, com demais dependências Release e LSan desligado.
Onze quadros selecionados foram inspecionados pelo agente; a superfície real
da prévia de cor também passou uma asserção dos pixels renderizados.

O manifesto privado desta correção é
`outputs/reforma-acesso-planejamento-entrega-2026-10-11.json` na raiz da tarefa,
com hashes de binários, fonte completa incluindo arquivos não rastreados,
evidências e limites. Fonte não commitada e schema 27; aceite humano pendente.
Candidatos anteriores congelados e candidato original preservados.
Matrizes no Projeto vazio/Atlas e temas claro/escuro incluem reinício.
O driver de acesso injeta eventos reais do mouse em Xvfb para distinguir clique
simples, clique duplo e seleção de múltiplos alvos. Não usa dados autorais vivos.

Revisão humana necessária:

1. Criar Projeto limpo e começar uma Obra pelo Planejar, sem consultar manual.
2. Cancelar e corrigir uma tentativa incompleta; reutilizar um universo existente.
3. Selecionar, abrir e executar ações de duas alternativas pelos menus respectivos.
4. Comparar múltiplos alvos e cancelar uma seleção; consultar origem registrada.
5. Abrir e fechar modelos, aplicar um e verificar a remoção assistida.
6. Usar Unidades, Linhas e Vínculos; criar vínculo sem seleção oculta.
7. Repetir com nomes extensos, janela menor e temas claro/escuro; retomar a seção
   depois de fechar e reabrir o programa.

Automação e inspeção do agente não certificam ergonomia humana, leitor de tela,
HiDPI, IME, contraste WCAG integral ou uma sessão longa de escrita. O restante
da grande proposta do construtor mantém a ordem definida; entidades por último
e expansão cartográfica/fusos permanecem frentes posteriores.

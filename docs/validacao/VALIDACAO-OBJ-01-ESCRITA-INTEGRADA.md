# OBJ-01 — objetos nas especialidades da Escrita

Data: 23/11/2026. Branch `feature/inde-obj-01-foundation`. Base `52739a0ba8519c2662ba0b810701b4f0889c8d29`. Integração `cd61468641e0a81f0c451a8f9d28477299546a9b`; visibilidade/interação `c620df836242d783b9e486947f991a947af31366`; versão final de layout e fila de ações `dc7cce0bc3ec51cdd9e44053a3b5c7fa687c6e27`.

[Arquitetura e fronteiras](../arquitetura/OBJ-01-ESCRITA-INTEGRADA.md), [roteiro de uso](../../tools/obj01-pilot/NATIVO.md), [resultados](evidencias-obj01-escrita/resultados.json), [integridade](evidencias-obj01-escrita/integridade.json), [manifesto de arquivos](evidencias-obj01-escrita/SHA256SUMS).

## Resultado e alcance

A janela separada foi retirada. `WritingWorkspace` incorpora `ObjectDocumentEditor` como uma superfície de suas especialidades. O `TextView` do editor original, suas transações, identidades, formatação, âncoras, alternativas, referências e recuperação continuam responsáveis pelo texto canônico. A composição acompanha o Documento completo vigente; editar o texto não invalida definitivamente a composição.

A barra reúne as especialidades anteriores e Composição, Tabelas, Fórmulas e Matemática. O acesso à lateral é sob demanda, à direita. As ações de cada objeto ficam no menu de clique direito, com Menu/Shift+F10 pelo teclado. As células continuam clicáveis como conteúdo. A escrita contínua e a distribuição visual são layouts de especialidades do mesmo Documento.

O comando Salvar da composição inclui a barreira da Escrita e seus metadados pendentes. Movimentos autorais de texto passam pelo editor original e permanecem reversíveis. Lentes existentes atualizam a composição imediatamente, sem apagar conteúdo; uma seleção ocultada não fica disponível para movimentação.

## Verificações locais

| Grupo | Resultado | Evidência |
|---|---|---|
| Aplicação/núcleo, Release com avisos estritos | 63/63 programas aprovados | [CTest](evidencias-obj01-escrita/ctest-63.txt) |
| Perfis anteriores de objetos, fórmulas, tabelas e recuperação | 22/22 programas aprovados | [Compatibilidade](evidencias-obj01-escrita/compatibilidade-22.txt) |
| Aplicação GTK real, ligação viva e ausência de navegador no executável | 3/3 testes aprovados em janela compacta | [Integração nativa](evidencias-obj01-escrita/nativo-compacto.txt) |
| Publicação existente e recusa de composição não suportada | 4/4 testes aprovados | [Publicação](evidencias-obj01-escrita/publicacao-4.txt) |
| Fontes matemáticas fixadas e ajuste explícito | 245 arquivos conferidos | [Auditoria](evidencias-obj01-escrita/fontes-matematicas.txt) |
| Auditor de arquivos compactados | 5/5 casos aprovados | [Auditor](evidencias-obj01-escrita/auditor-fontes.txt) |
| Base integrada schema 37 | 758 arquivos conferidos | [Base](evidencias-obj01-escrita/base-schema37.json) |
| Arquivos de projetos/demonstrações anteriores protegidos | 74/74 com os mesmos hashes | [Preservação](evidencias-obj01-escrita/preservacao-final.json) |

O teste GTK cria um projeto sintético, usa controles reais e verifica: seis modelos matemáticos; três equações; importação PNG pelo seletor nativo; tabela e cartão em célula; alteração remota de 100 para 80, consumo do comando, Desfazer/Refazer; estilos/eixos de tabela; condições numéricas/textuais; retirada/restauração; posição e pareamento; escrita, Desfazer/Refazer pelo editor original; movimento autoral com retorno ao editor original e seu Desfazer; proteção de rascunho; título pendente incluído no Salvar; lente antiga aplicada e revertida; recusa de movimentar seleção oculta; saída à biblioteca e reabertura real do Documento.

A amostra grande foi criada pelo mesmo fluxo com a versão final, em `dados/obj01-escrita-integrada-r3/Projeto.inde`. O teste compacto solicita janela de 1190×750; a área capturada pode ser menor pelas decorações GTK. As dimensões efetivas do display virtual ficam em [medidas](evidencias-obj01-escrita/headless-medidas.json). A amostra registrada também exercita a tela maior local. As capturas são snapshots de widgets GTK após pintura; não são desenhos de uma proposta nem conteúdo web.

- [Editor integrado reaberto](evidencias-obj01-escrita/escrita-integrada.png).
- [Menu do objeto textual](evidencias-obj01-escrita/menu-objeto.png).
- [Edição matemática](evidencias-obj01-escrita/editor-matematica.png).
- [Janela compacta sob Xvfb](evidencias-obj01-escrita/headless.png) e [medidas efetivas](evidencias-obj01-escrita/headless-medidas.json).

Os testes automáticos acionam os menus pelo controlador de clique direito. O acesso por Menu/Shift+F10 está implementado, mas o passe humano de navegação por teclado continua aberto. Não se declara uma auditoria completa de acessibilidade.

## Integridade e compatibilidade

A amostra final conserva schema canônico 37, `user_version=0`; o banco auxiliar continua `user_version=5`. Ambos passaram por `integrity_check`, sem violações de chave estrangeira. O formato privado `INDE-OBJ01-COMPOSITE-P2D2-6` declara a fonte viva; os formatos anteriores continuam reconhecidos. Não há alteração em lote de bancos autorais nem promoção a arquivo único `.inde`.

O corpo e o título no snapshot auxiliar coincidem com os canônicos. Saved e working coincidem, os hashes internos conferem e a captura original permanece idêntica. A amostra mantém uma âncora, uma faixa de formatação e o perfil de visibilidade; contém três equações, uma figura, uma tabela e quatro registros de fórmula. Os testes de adaptadores/códigos proprietários cobrem as estruturas adicionais de captura, folhas, referências e grupos; a amostra gráfica não substitui esses testes.

A `main` permanece em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com a alteração preexistente de modo de `Preparar-ponte.sh`. Os três arquivos compactados originais foram reconferidos por SHA-256; veja [os valores](evidencias-obj01-escrita/arquivos-originais.json).

## Checagem remota e intercorrências

A primeira execução, [45179873468](https://github.com/Roger-Cardoso/inde/actions/runs/45179873468), aprovou 25/26 jobs. A compilação GCC 13 passou; o gate nativo rejeitou avisos críticos GDK de geometria de popup na tela menor. As asserções funcionais da aplicação haviam passado. O teste não foi afrouxado: menus passaram a abrir e ser acionados em etapas distintas, ações fecham o popup antes da alteração e as capturas aguardam a pintura do GTK. A janela compacta também passou a ser exercitada localmente. [Registro intermediário](evidencias-obj01-escrita/ci-intermediaria.json), [falha original](evidencias-obj01-escrita/ci-intermediaria-falha.txt).

A segunda execução [45182784133](https://github.com/Roger-Cardoso/inde/actions/runs/45182784133) também aprovou a compilação e 25/26 jobs, mas expôs o clique fora da região visível e a falta de espera de uma ação de menu adiada. [Registro](evidencias-obj01-escrita/ci-segunda.json), [falha](evidencias-obj01-escrita/ci-segunda-falha.txt). A versão final passou a serializar essas ações, bloquear fechamento enquanto estão pendentes e clicar na região realmente visível do objeto. O ambiente sem gerenciador de janelas foi reproduzido localmente com Xvfb extraído em pasta temporária; não houve instalação de pacote no Linux do autor.

A inspeção das imagens também encontrou o formulário comprimido por uma lateral desativada. A divisão foi corrigida: área central flexível, formulário centralizado e lateral recolhida durante a edição. Novas asserções verificam a largura útil e o espaço ocupado pela lateral. [Antes](evidencias-obj01-escrita/editor-matematica-antes.png), [depois](evidencias-obj01-escrita/editor-matematica.png).

A execução final é [45188525362](https://github.com/Roger-Cardoso/inde/actions/runs/45188525362), para `dc7cce0bc3ec51cdd9e44053a3b5c7fa687c6e27`. **Concluída: 26/26 jobs aprovados**, incluindo compilação limpa GCC 13, testes nativos sob Xvfb e a compatibilidade do domínio com GCC/Clang e sanitizadores. [Resultados completos](evidencias-obj01-escrita/ci-final.json).

No Linux local, permanecem os avisos de keymap `Can't handle >16bit keyvals` já identificados no construtor da janela antiga, antes de abrir os objetos. Não foram suprimidos. As verificações rejeitam `Gtk-CRITICAL`, `Gdk-CRITICAL` e finalização de widgets com filhos pendentes. A demonstração final não apresentou esses erros críticos.

## Limites que permanecem abertos

- O histórico textual e o de objetos são contextuais; não há uma cronologia global única de Desfazer. Quando uma edição retira identidades de parágrafo ou quebra adjacências, os vínculos de apresentação correspondentes são retirados; reverter somente o corpo não recompõe automaticamente todo arranjo anterior.
- A escrita contínua permanece em Texto; a organização dos objetos, nas especialidades visuais. Não se inseriram widgets arbitrários no buffer textual nem caixas de parágrafo concorrentes.
- Estatísticas, busca e semântica do corpo ainda têm o alcance do texto canônico; não agregam automaticamente todos os resultados de cartões, células ou fontes matemáticas.
- A gravação do proprietário e a auxiliar continua sequencial. Não se afirma atomicidade SQLite conjunta entre os dois bancos. Interrupções exigem retomada com a autoridade do Documento preservada e revalidação da composição.
- Rascunhos não aplicados continuam em memória e não recebem promessa de recuperação após queda.
- Variáveis ligadas a entidades/pseudônimos, equivalência completa com Comic, matemática tipada em células, fidelidade tipográfica completa da composição e arquivo `.inde` único não estão concluídos.
- Exportação dos novos objetos fica adiada por decisão do usuário; documento HTML será o foco futuro. O publicador conserva a recusa que evita omitir objetos.

Não há release, merge em main ou aceite humano implícito. O iniciador final usa perfil próprio também para dados/cache GTK, preservando os ambientes do autor.

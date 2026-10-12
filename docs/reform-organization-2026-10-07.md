# Reforma — organização de Planejar e Manuscrito, 2026-10-07

Segundo bloco funcional sobre `6c3bc27858fb8f7237d269390232ac921288b92b`.
O usuário autorizou expressamente continuar a implementação antes do aceite
humano. Isso não aprova o candidato, não autoriza publicação e não transforma
a reforma inteira em concluída.

## Decisões e alterações

### Planejar: materiais primeiro, gestão por solicitação

A entrada padrão passa a ser o explorador de personagens e mundo. Estrutura
narrativa, relações e tempo ficcional continuam acessíveis por um seletor de
área, sem quatro botões e duas ações de histórico competindo lado a lado.
Voltar/avançar permanecem explícitos e limitados ao histórico da sessão.

Busca e criação ficam na área principal do explorador. Tipos de entidade e
de relação são geridos em popovers, sem as antigas seções permanentes de
ações. Os filtros especializados continuam sob demanda. Não houve remoção
de tipos, relações, qualificadores, presenças ou contratos temporais.

A restauração do histórico agora recoloca busca, tipos, contexto e offset.
A volta simples reaproveita os cartões/rolagem quando a consulta é a mesma;
não reutiliza resultados de uma consulta diferente. O detalhe da entidade
aberta é atualizado sem reconsultar e perder o contexto ao voltar de outro
workspace. Se a entidade não existir mais, o explorador volta a ser a saída.

### Manuscrito: conteúdo da obra antes da gestão de modelos

Manuscrito abre na estrutura da obra, não na biblioteca de modelos. A busca
fica disponível junto das ações essenciais de criar e acessar elementos.
Tipos, templates e recuperação ficam em Organizar, num popover rolável e
com largura limitada. As operações continuam usando os serviços anteriores.

No detalhe da unidade, documentos aparecem antes das referências de
entidades. Referências diretas e herdadas têm expansores separados,
inicialmente fechados. As distinções entre documento, unidade editorial e
referência canônica continuam explícitas.
O espaço de capa desaparece quando a obra não tem imagem. Quando existe,
a imagem fica recolhida abaixo dos documentos, sem disputar seu espaço.

Modelos e alternativas abre a gestão em outra página. Alternativas da obra e
modelos reutilizáveis ocupam abas, não dois painéis permanentes. Ativar,
duplicar, derivar, capturar modelo e remover ficam nas ações da alternativa.
Não aparecem como comandos operacionais de outra aba. Abrir uma alternativa
inativa continua exigindo ativação explícita; navegar nunca muda a ativa.

A atualização do seletor de obra não emite mais uma troca operacional para
nenhuma obra e depois para a obra anterior. Isso evita apagar o elemento
aberto ao retornar da Escrita ou salvar/atualizar o projeto. A seleção de
modelo/alternativa também é preservada durante uma atualização. Trocar
deliberadamente de obra limpa filtros que poderiam esconder seu conteúdo.

Rótulos longos em seletores de obra/tipo são limitados apenas na apresentação;
nomes e identidades autorais permanecem íntegros no modelo.

## Preservação e entrega independente

A primeira entrega, seus executáveis, manifesto, launcher e projetos de
revisão não são sobrescritos. A branch deste bloco é
`codex/reforma-organizacao-2026-10-07`.

O novo launcher é `outputs/inde-organizacao-abrir.sh`, com os modos atlas,
vazio, laboratorio-atlas e laboratorio-vazio. Confere o manifesto, a marca
da cópia e o hash do executável. Usa cópias em `review-organizacao` e
preferências independentes em `review-organizacao-config`.

As novas cópias de revisão partem dos snapshots SQLite comprometidos das
cópias da primeira entrega. Não capturam texto ainda não salvo numa sessão
aberta. A QA usa outras cópias descartáveis dos fixtures de referência, sem
semear dados sintéticos nos projetos que serão entregues para revisão.

Schema 18, payload semântico 2, checkpoint 4 e clipboard 2 permanecem
inalterados. Não houve mudanças de autoridade de persistência, automação
semântica ou conversão destrutiva do manuscrito.

## Verificação

A compilação Release e a verificação técnica passaram: 16 CTest e 20 cenários
gráficos completos — reforma,
organização, explorador, relações, edição, shell, interação do editor, barra,
cor e laboratório, em Atlas e Projeto vazio. Passaram ainda seis cenários
suplementares no tema escuro (organização, formulários e laboratório).

Após a queda de energia, o estado salvo foi reconferido: fonte, branch,
binários, primeira entrega e candidato original. A compilação de produção
estava atualizada; o driver de organização foi recompilado e todos os 16 +
20 + 6 ensaios foram repetidos com sucesso. As capturas foram conferidas,
incluindo documentos na área visível e rótulos na janela compacta. Os dados
sintéticos pertencem somente às cópias de QA, não às cópias de revisão humana.

O novo ensaio de organização exerce a janela real, duas obras, capítulo/cena,
documento colocado na unidade e 108 personagens sintéticos na cópia de QA.
Verifica abertura direta, navegação pelo documento, retorno ao mesmo detalhe,
seleção de modelo/alternativa, menus progressivos, busca/página 2, histórico
voltar/avançar, troca de obra, temas, Unicode e fechamento/reabertura.
Também verifica janela de 940 × 660 com título longo e a posição real do
documento dentro da área visível, não apenas sua existência como widget.

Os ensaios anteriores de CRUD, facetas, tipos protegidos, relações e contexto
continuam ativos. Resultados, duração, capturas e hashes estão no relatório
`outputs/reforma-organizacao-verificacao-2026-10-07.json`; o manifesto final
identifica o commit e os binários efetivamente entregues.

Aceite humano continua pendente. Ainda não é validação integral de IME,
leitores de tela, HiDPI, sessões longas ou desempenho percebido no desktop.

## O que este bloco não promete

- Não há novas visões salvas/workspaces personalizados ou retomada completa
  de todos os filtros e cursores após fechar a aplicação.
- A estrutura narrativa e os papéis narrativos conservam seus fluxos internos;
  este bloco reduz a competição na entrada, não redesenha todo esse subsistema.
- A organização por obra não se torna exportação, paginação de impressão ou
  fluxo editorial completo. Produção/exportação continua sendo outro bloco.
- Composição persistente, aliases, calendário ficcional e reversão global de
  operações continuam pendentes, conforme o relatório de reforma geral.

Nenhuma dessas pendências é mascarada como botão sem implementação.

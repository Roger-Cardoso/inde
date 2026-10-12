# Modularização preparatória do editor — 2 de outubro de 2026

O usuário aprovou a Etapa 0 e solicitou a divisão dos arquivos com várias
responsabilidades antes de iniciar a Etapa 1. Os quatro primeiros lotes preservam
o comportamento existente; o quinto separa a extração de relações das correções
da primeira ativação e da compatibilidade gráfica; o sexto preserva o
comportamento ao separar os comandos e seletores temporais. Schema v15 e contratos
públicos são preservados. O trabalho está
na branch local `codex/pre-editor-modularization-2026-10-02`, baseada em
`f155bd866d32c9326e5853a2882e6adbfca6ba23`, com o diff linguístico pré-existente.

## Organização atual

| Área | Arquivos e responsabilidade |
|---|---|
| Planejamento | `planning_workspace.cpp`: ciclo de vida, navegação e ordem de montagem; sufixos `_ui_explorer`, `_ui_relations`, `_ui_detail`, `_ui_time`, `_ui_actions`: montagem e sinais; `_explorer`: cards/paginação; `_explorer_filters`: facetas/contexto; `_entity_types`, `_entity_commands`, `_entity_detail`: tipos, comandos e detalhe; `_time_projection`: projeções/eixos; `_time`: CRUD de eixos/pontos; `_temporal_selectors`: seletores compartilhados; `_event_commands`, `_presence_commands`: ocorrências/participações e presenças; `_relations`: seleção/contraparte; `_relation_explorer`, `_relation_types`, `_relation_commands`: consulta/projeção, tipos e comandos de relação; `_explorer_context`: limpeza do contexto; `_context`: operações de contexto narrativo |
| Cartografia | `cartography_workspace.cpp`: ciclo de vida, reset/refresh e disponibilidade de ações; sufixos `_ui`, `_generation`, `_view`, `_locals`, `_terrain`: montagem, geração assíncrona, cena/desenho/exportação, posicionamento e relevo/bloqueios |
| Estrutura narrativa | `narrative_structure_workspace.cpp`: ciclo de vida, contexto de Obra e navegação; sufixos `_ui`, `_library`, `_detail`, `_roles`: montagem, modelos/alternativas, unidades/vínculos e papéis/atribuições |
| Escrita | `writing_workspace.cpp`: UI inicial; sufixos `_library`, `_document`, `_tools`, `_references`, `_linguistic`, `_editor`: biblioteca, texto/formatação, ferramentas, referências, consulta linguística, estado/undo/autosave |
| Editorial estrutural | `structural_editor.cpp`: UI inicial; sufixos `_navigation`, `_model`, `_inspector`, `_catalog`: navegação, árvore, detalhe/referências e comandos de catálogo |
| Migrações | `schema_migrator.cpp`: transação e ordem de versões; headers privados `schema_sql_v01_v04.hpp`, `schema_sql_v05_v11.hpp`, `schema_sql_v12_v15.hpp`: literais SQL preservados |
| Persistência narrativa | `sqlite_narrative_repository.cpp`: inicialização; sufixos `_entities`, `_relations`, `_references`, `_commands`: leituras por agregado e mutações; `narrative_repository_support`, `narrative_query_support`, `narrative_row_codec`: conexão/identidade, filtros e conversão de linhas |
| Regressão do núcleo | `tests/core_tests.cpp`: despacho na ordem anterior; `tests/core/`: dez grupos de testes, suporte comum e fixture SQLite |
| Atlas demonstrativo | `tools/demo_project_generator.cpp`: CLI; `tools/demo_project/demo_builder.hpp`: declaração; sete módulos para ciclo/auditoria, catálogo/estruturas, entidades, planejamento, documentos, manuscrito e textos complementares |

Os headers de suporte da persistência vivem em `src/persistence/`; os do
gerador em `tools/demo_project/`. Eles são detalhes internos, sem acréscimo à
API pública em `include/inde/`. Cada novo `.cpp` é uma unidade de compilação
real declarada no CMake.

## Evidência do segundo lote

- As 40 funções de teste e sua ordem de execução foram preservadas; asserts
  continuam ativos em Release por `-UNDEBUG` no alvo `inde_core_tests`.
- As 30 definições do repositório narrativo e seus helpers foram copiadas sem
  alterar SQL, limites, binds, validações ou fronteiras de transação.
- Os corpos dos 39 métodos do gerador foram preservados. A declaração da
  classe mantém os argumentos padrão; as definições passam a ser externas.
- Build Release completo e 3/3 CTest passaram depois dos cortes. O teste da
  geração completa executou também as contagens esperadas e a integridade.
- Os Atlas gerados antes/depois têm schema igual e passaram na comparação
  normalizada de 47 tabelas, 2.299 registros, vínculos, manifesto e assets.
  UUIDs/datas e chaves de modelo geradas são variáveis. Relações simétricas
  são comparadas como pares sem direção, pois a persistência ordena seus
  extremos por UUID. A comparação percorre as referências por 32 rodadas;
  ela é evidência sobre esta fixture, não um provador geral de isomorfismo.
- Smoke de oito segundos em cópias de Projeto vazio e Atlas passou, com os
  avisos ambientais de tema GTK, ícone e `ibus` já observados. Inspeção visual
  e medição de digitação não foram realizadas neste lote.

Relatórios e scripts de reprodução estão em
`/home/roger/Documents/Codex/2026-10-02/analise-o-chat/outputs/` e
`/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/modularizacao/`.

## Dívida de fixture identificada

`DemoBuilder::create_narrative_structures` passa `entities_.at(spec.event)`
para o argumento `summary` de `StructureModelService::create_unit`. A ligação
ao Acontecimento é criada separadamente por `add_entity_to_unit`. Assim, o
resumo de seis unidades (e de suas cópias) contém UUIDs. Esse comportamento
existe na linha de base e foi preservado neste corte. Uma revisão de conteúdo
da fixture deverá substituir esses resumos por texto autoral apropriado e
revalidar as cópias/modelos capturados.

## Evidência do terceiro lote

Os arquivos-base passaram de 1.365 para 461 linhas no Planejamento, de 1.198
para 133 na Cartografia e de 1.143 para 99 na estrutura narrativa. Foram
acrescentadas 14 unidades de compilação reais e dois headers privados de
apresentação, `cartography_ui_helpers.hpp` e
`narrative_structure_ui_helpers.hpp`, sem alterar os campos das classes.

A montagem do Planejamento é repartida em nove métodos privados. Ao expandir
suas chamadas na ordem do coordenador, a sequência original de instruções é
idêntica, ignorando somente linhas vazias. A paginação do explorador continua
depois da montagem de relações. Os outros 18 métodos do arquivo-base, os 30
métodos cartográficos e os 33 narrativos foram preservados byte a byte. Os
helpers preservam seus corpos; os compartilhados são `inline` em headers
privados, e os labels de origem/vínculo ficam locais aos seus módulos.

Build Release e 3/3 CTest passaram após cada área. O roteiro opt-in
`tests/ui_workspace_smoke.cpp` foi ligado aos objetos reais do aplicativo pelo
script `tools/build_ui_workspace_smoke.py`. Em cópias isoladas de Projeto vazio
e Atlas, exercitou montagem/refresh, filtros, histórico, páginas narrativas,
prévia/descarte/cancelamento cartográfico e fechamento/reabertura. O detalhe de
estrutura e de entidade é exercitado quando há registros; o Projeto vazio não
é anunciado como cobertura desses caminhos. Os bancos conservaram seus
checksums. Este smoke exige display e não faz parte do CTest headless.
O aplicativo completo também permaneceu ativo por oito segundos nas duas
cópias, até o timeout controlado (124), sem erro crítico; isso valida startup,
não a passagem interativa integral.

Nenhum método de domínio, contrato linguístico, schema ou fluxo de persistência
foi alterado. O cancelamento/join da thread e o desligamento do temporizador
continuam no coordenador cartográfico. Os avisos do tema GTK e do módulo `ibus`
permanecem uma condição ambiental conhecida. A passagem visual humana, a
latência percebida e uma nova rodada Debug/ASan não foram validadas neste lote.

Reprodução, logs e limites estão no relatório
`outputs/modularizacao-lote3-2026-10-02.md`. O patch cumulativo para revisão e
recuperação é `outputs/modularizacao-lotes1-3-f155bd8.patch`, incluindo o diff
linguístico pré-existente; não foi aplicado ao checkout original nem publicado.

## Continuação da operação

O quarto lote executou o corte de `planning_workspace_explorer.cpp`: de 1.085
para 145 linhas de projeção de cards e paginação. Cinco unidades reais separam
filtros/facetas (211), tipos de entidade (185), comandos de entidade (140),
detalhe/seleção (264) e projeções de tempo ficcional (185). As 29 definições de
métodos foram preservadas byte a byte. O header completo da classe e os outros
nove módulos de Planejamento existentes no início do lote não mudaram. No
CMake, a diferença deste lote é somente o registro das cinco novas unidades.

Build Release e 3/3 CTest passaram na linha de base e após cada uma das três
extrações: filtros, comandos e projeções. O novo roteiro opt-in
`tests/ui_explorer_smoke.cpp`, ligado aos objetos reais do aplicativo, cobre
paginação com 102 entidades sintéticas, facetas OR, faceta selecionada sem
resultados, cards/retorno, página fora do limite, CRUD Unicode, identidade,
proteção de tipos em uso/internos e reabertura. Origem temporal é exercitada no
Atlas; a fixture vazia não é anunciada como cobertura dessa origem. Os três
cenários passaram antes/depois em cópias de ambas as fixtures. O roteiro de 11
passos do terceiro lote foi reexecutado nas duas fixtures, após extrair seus
helpers compartilhados para `tests/ui/widget_probe.hpp`.

Este roteiro escreve somente em cópias descartáveis, com autorização explícita
na linha de comando. Entidades/tipos sintéticos são removidos ao final; o banco
da cópia conserva o histórico de alterações e o avanço dos contadores. Portanto
não se anuncia identidade byte a byte do SQLite modificado. A evidência compara
schema, registros autorais, manifesto/assets, integridade e chaves estrangeiras
com as fixtures originais, cujos checksums permanecem intactos. Duas tentativas
iniciais do roteiro falharam porque a busca do botão "Remover" atravessava outra
página; as cópias e logs foram preservados. A correção restringiu o teste ao
rodapé do modal, sem modificar o produto.

Relatório, mapa dos métodos, logs e limites:
`outputs/modularizacao-lote4-2026-10-02.md`. O patch cumulativo atualizado é
`outputs/modularizacao-lotes1-4-f155bd8.patch`; os artefatos dos lotes anteriores
foram preservados. Não houve aplicação ao checkout original, commit ou push.

O quinto corte foi executado após o aceite parcial relatado abaixo. O usuário
autorizou em seguida o sexto corte temporal, sem declarar novo aceite das
correções da primeira abertura/menu. O sexto foi concluído com regressão nas
duas fixtures. O próximo portão é a revisão consolidada dos lotes 5–6, incluindo
as duas ressalvas anteriores e os formulários temporais, no executável atual.
Depois, avaliar fronteiras dos agregados da aplicação/persistência por
responsabilidade, não por teto arbitrário de linhas. Nenhum sétimo corte foi
iniciado; a Etapa 1 continua sem autorização.

## Aceite humano e correções do quinto lote

O usuário informou aceite humano dos demais fluxos da revisão dos lotes 3–4,
com duas ressalvas: primeira entrada no explorador sem entidades/explicação e
travamento/abort ao abrir o Menu do Aplicativo. A aceitação parcial é mantida;
as correções desses dois problemas precisam de confirmação humana específica.
Esse aceite não libera a Etapa 1.

A primeira falha foi reproduzida na janela real: a sessão começa na página
narrativa, enquanto `show_explorer_page` mudava a página sem carregar resultados.
O flag privado `explorer_loaded_` agora exige `refresh` na primeira ativação,
é marcado depois da atualização completa e invalidado em `reset`. O retorno
do detalhe continua sem reconstruir os cards já carregados. A correção é
distinta da extração mecânica.

O teste de relações também fixa 100 entidades auxiliares para colocar a origem
fora da primeira página. Uma variante isolada com somente `reveal_entity` antigo
reproduziu o erro de navegação; a função corrigida sincroniza pesquisa e facetas
dos controles com o contexto antes da consulta. A seção de relações é expandida
explicitamente no teste, como na interação do usuário. O checkout corrigido não
foi revertido para construir essa variante de diagnóstico.

O journal do kernel registrou três GPU HANG e resets do driver `i915`, no
processo `inde[67237]`, entre 20:51:58 e 20:52:12 de 02/10/2026. A GPU local é
Intel PCI `8086:0102`. O abort não foi reproduzido de forma determinística pelo
roteiro; esses registros são evidência do travamento gráfico, não um backtrace
do abort. A política privada `src/ui/rendering_policy.*`, executada antes do
GTK, seleciona Cairo somente quando detecta esse dispositivo em `/sys/class/drm`
e não existe escolha explícita de `GSK_RENDERER`. Outras GPUs/ambiente sem sysfs
mantêm a seleção padrão. Nenhuma configuração do sistema é modificada. Há
tradeoff de CPU/pintura; não se anuncia melhora de desempenho. Os testes
conferem o renderer efetivo da janela, além do resultado dos cliques no menu.

O mecanismo de seleção/fallback é documentado pelo GTK em
[Renderer.new_for_surface](https://docs.gtk.org/gsk4/ctor.Renderer.new_for_surface.html).
A variável é uma
opção de compatibilidade/diagnóstico, não uma preferência persistente de usuário;
é preciso revalidar a política quando GTK/driver/dispositivo mudarem.

A regressão de fechamento/reabertura revelou também uma ação idle de
maximização que capturava `this` sem vínculo de vida/cancelamento. O backtrace
controlado, com `G_DEBUG=fatal-criticals`, aponta para a lambda de
`MainWindow::show_workspace`, após destruição da janela GTK. A conexão agora
fica armazenada, é rastreada com `sigc::track_object` e cancelada ao reagendar,
ao fechar com sucesso e no destrutor. Esse defeito foi reproduzido; não se
afirma que seja a causa do abort específico relatado ao clicar no menu.

O quinto lote preserva byte a byte as 21 definições extraídas do arquivo de
relações (729 linhas): seleção/contraparte (131), projeção/filtros (226), tipos
(160), comandos (204) e limpeza de contexto (42). São quatro novas unidades
reais de compilação. As correções comportamentais são registradas separadamente
e não são anunciadas como mera movimentação de código.

Build Release, 4/4 CTest e os roteiros GTK finais cobrem primeira ativação,
reabertura, encerramento imediato com idle pendente, seis cliques físicos no
menu em cada fixture, CRUD/proteções/qualificadores de relações, origem fora da
primeira página e contraparte/contexto. O projeto novo recebe uma entidade
persistida antes de construir a janela; a entidade de QA é removida depois.
Os roteiros de três cenários do explorador e de 11 passos dos workspaces são
reexecutados. A rodada final usa `G_DEBUG=fatal-criticals`.

Relatório e evidências: `outputs/modularizacao-lote5-2026-10-02.md`,
`outputs/modularizacao-lote5-execucao-2026-10-02.json` e
`outputs/modularizacao-lote5-verificacao-2026-10-02.json`. O patch cumulativo é
`outputs/modularizacao-lotes1-5-f155bd8.patch`. O launcher atual é
`outputs/modularizacao-lote5-abrir-revisao.sh`, com `atlas` ou `vazio`: usa o
binário modularizado e cópias novas, não o executável histórico da Etapa 0.
Os dados originais, catálogos e artefatos anteriores permanecem preservados.
Não houve commit, push, publicação nem liberação da Etapa 1.

## Evidência do sexto lote — formulários temporais

O usuário autorizou o corte seguinte. A organização temporal passou de um
arquivo de 649 linhas para quatro módulos: `planning_workspace_time.cpp` (259)
mantém parsing de ordem e CRUD de eixos/pontos; `_temporal_selectors.cpp` (60)
mantém os provedores limitados de pontos/entidades; `_event_commands.cpp` (208)
mantém ocorrência e participação; `_presence_commands.cpp` (149) mantém presença
aberta/fechada. São três novas unidades de compilação reais. O módulo existente
`_time_projection.cpp` não mudou; exibição/refresh/origem temporal continuam
separados dos comandos. O header completo da classe não mudou.

As 20 definições extraídas são byte a byte idênticas, sem redefinições. O CMake
só registrou as três unidades. O inventário anterior verifica 246 arquivos
existentes fora do corte sem alteração, incluindo as correções do quinto lote,
domínio/aplicação/persistência e contratos linguísticos. A soma de linhas cresce
pelos includes e namespaces necessários a cada unidade; não há funcionalidade
nova ou mudança de schema.

O novo roteiro opt-in `tests/ui_temporal_smoke.cpp`, ligado aos objetos reais do
aplicativo, passou antes/depois em cópias novas do Projeto vazio e do Atlas.
Cria seus próprios pontos/entidades, portanto ambos exercitam tempo de fato,
não apenas montagem vazia. Cobre CRUD Unicode e UUID estável; cancelamento;
ordem inválida/overflow; proteção do eixo principal/eixo com pontos e de pontos
com ocorrência/presença; pesquisa e seleção pelos provedores reais; exclusão de
Acontecimento como participante e de não-Local como Local; ocorrência,
participação, presença aberta/fechada e rejeição de intervalo invertido; remoção
em cascata, limpeza e reabertura. Não se anuncia cobertura de mudança de eixo de
um ponto, todas as falhas possíveis, digitação física ou latência de debounce.

Build Release e 4/4 CTest passaram na linha de base e depois do corte. A rodada
GTK final reexecuta primeira entrada/reabertura, idle pendente e seis cliques
físicos do menu em cada fixture, relações/contraparte, três cenários do explorador
e 11 passos de workspaces. Os harnesses foram religados aos 58 objetos reais da
UI. Todos os roteiros finais tratam erros críticos GTK como fatais. Startup
controlado é verificado separadamente; não substitui passagem humana.

As escritas de QA ficam apenas nas novas cópias `lote6-smoke`; os registros
sintéticos são removidos e os eventos de auditoria preservados. Schema, registros
originais, manifesto/assets e integridade são comparados às fixtures originais.
As cópias de revisão `lote6-review` são independentes e inicialmente idênticas.
Não houve Debug/ASan novo, medição de latência, sessão longa nem AppImage.

Relatório e recuperação: `outputs/modularizacao-lote6-2026-10-02.md`,
`outputs/modularizacao-lote6-verificacao-2026-10-02.json`,
`outputs/modularizacao-lote6-execucao-2026-10-02.json`,
`outputs/modularizacao-lotes1-6-f155bd8.patch` e
`outputs/modularizacao-lote6-checksums.sha256`. O launcher atual é
`outputs/modularizacao-lote6-abrir-revisao.sh`, com `atlas` ou `vazio`.
Os artefatos/checksums anteriores não foram substituídos. A versão anterior dos
binários foi preservada em `work/modularizacao/lote6-baseline`.

Dois lotes (5–6) estão tecnicamente concluídos desde o último aceite parcial.
A próxima operação recomendada é a revisão consolidada humana/técnica, não
outro corte automático ou a Etapa 1. Configuração para analisar/corrigir os
achados: GPT-6.1 Sol / high, mesma tarefa, custo relativo alto, sem escalada.

A divisão física já realizada preserva as classes coordenadoras;
novas fronteiras de serviços/controladores exigirão cortes próprios com
invariantes explícitos. A Etapa 1 permanece pendente da conclusão da operação
de modularização e da direção posterior do usuário.

## Decisão posterior — 3 de outubro de 2026

O usuário declarou “aprovado, iniciar etapa 1”. Fica registrado o aceite
consolidado da operação anterior e a autorização para iniciar a Etapa 1 do
plano do editor. Essa decisão substitui o portão pendente acima; os registros
históricos não são reescritos nem convertidos em medições humanas detalhadas.
Não foi fornecida uma lista individual de cenários ou tempos observados pelo
usuário. O contador de incrementos desde a revisão consolidada reinicia em zero.

A execução passa ao brief `docs/editor-stage1-e1.1-2026-10-03.md`: primeiro
incremento de análise assíncrona e instrumentação. Não é autorização para a
Etapa 2/3, bindings semânticos automáticos, promoção de catálogos ou publicação.
A Etapa 1 completa permanece em execução, sujeita aos critérios do plano.

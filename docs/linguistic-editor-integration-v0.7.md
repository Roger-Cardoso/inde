# Integração do Sistema Linguístico com o Editor — v0.7 + Onto.PT v2.2/v2.4 + promoção v2.5

## Decisão humana

Em 22 de setembro de 2026, o usuário autorizou a integração com o Editor/UI e,
em seguida, autorizou a primeira fatia vertical proposta para o checkout real
do INDE. Esta decisão remove somente a exclusão de integração com a interface.
L-07, L-09, fontes adicionais, ampliação de lote e binding automático não são
autorizados por ela.

Em 23 de setembro de 2026, o usuário autorizou separadamente um lote massivo
em que todas as acepções catalogadas contenham definição. Essa decisão autoriza
somente compilar e instalar o recorte definido do snapshot OpenWordNet-PT já
fixado. Não autoriza nova fonte, rede, texto gerado, L-07, L-09 ou binding.

Em 23 de setembro de 2026, o usuário esclareceu que todas as aquisições
anteriores integradas tiveram aceite humano. Esse aceite incidiu sobre a
aquisição e a integração; não afirma que cada registro esteja livre de
pendências, como repetição de definição dentro da mesma acepção.

Na mesma data, o usuário aceitou uma nova aquisição seletiva do Onto.PT v0.6,
com definições, exclusivamente para teste isolado de qualidade no INDE. A
decisão autoriza manter registros que passem por critérios mensuráveis e
consultá-los visualmente em quarentena. Não autoriza promoção automática,
aquisição integral ou equivalência entre critério automático e revisão
linguística humana.

Após revisar muitos casos no INDE, o usuário aprovou em 23 de setembro de 2026
uma nova política: uma definição não é suficiente apenas por estar presente e
relacionada à acepção; ela precisa introduzir o sentido para quem não conhece a
palavra. A decisão autoriza uma camada editorial paralela, preservação integral
do original, rubrica de utilidade, abstenção quando a evidência for insuficiente
e novo gate humano antes de qualquer promoção.

Depois de verificar todas as palavras da calibração, o usuário aprovou os 32
rascunhos editoriais e não aprovou as 8 abstenções. Para estas, registrou a
orientação de priorizar a acepção e uma definição útil, validar as relações em
segundo lugar e, no extremo em que não houver definição desejável, conservar o
original raso com `motivo` visível. O fallback não é aceite de qualidade nem
autoriza promoção.

Após a aquisição integral permanecer isolada e a revisão humana confirmar a
qualidade geral, o usuário autorizou corrigir as definições que agregavam mais
de uma acepção. A v2.2 separa a segmentação por synset do vínculo entre
fragmento e forma, expõe somente vínculos sustentados por critérios
mensuráveis, conserva fragmentos sem vínculo para revisão e mantém todos os
novos sentidos como `needs_human_confirmation`. Underscore técnico vira espaço
apenas na exibição; hífen incerto nunca é removido em massa.

Em 29 de setembro de 2026, o usuário autorizou aplicar uma arquitetura que
separa ocorrência de fonte, alinhamento, cluster congelado, redação isolada,
mapa de evidências e QA cruzado. A autorização permite implementar e testar a
camada v2.3, mas não equivale a aceitar seus 130.057 clusters provisórios,
aprovar novas definições ou promover o Onto.PT para o catálogo canônico.

Ainda em 29 de setembro, a revisão de `aba`, `ab-rogação` e `abade` revelou
que um registro-fonte podia conter múltiplas unidades semânticas e que o gate
de duplicação era assimétrico. O usuário autorizou a etapa sucessora v2.4:
atomizar evidências, bloquear registros compostos, alinhar famílias inteiras e
expor classes distintas da mesma grafia sem misturá-las. Essa autorização não
equivale a adjudicar os átomos nem a promover clusters.

Depois da revisão visual, o usuário autorizou explicitamente prosseguir e
promover os melhores candidatos, exigindo cuidado com candidatos repetidos em
acepções diferentes da mesma palavra. A v2.5 interpreta a autorização como um
lote conservador: duplicatas exatas são colapsadas com toda a proveniência;
colisões aproximadas, redações distintas oriundas do mesmo synset e
sobreposições com L-06 bloqueiam os envolvidos. Fora dos três casos humanos,
uma promoção exige qualidade mínima 95/100 e a mesma candidata em pelo menos
dois synsets. O lote resultante possui 86 sentidos e permanece num quinto banco
somente leitura; não altera L-06, v2.2 ou v2.4.

## Fatia implementada

O workspace de Escrita oferece duas operações deliberadamente explícitas:

1. consulta exata de uma palavra/expressão selecionada ou digitada no catálogo
   L-06;
2. exploração nominal de Construction, Frame, SyntacticFrame e MWE no catálogo
   promovido L-08.
3. consulta opcional e explicitamente rotulada ao lote Onto.PT em quarentena.
4. apresentação separada de definição aprovada, definição original, flags,
   pontuação, decisão humana e motivo do fallback.
5. consulta conjunta, no escopo aceito, das definições promovidas v2.5,
   identificadas por rótulo, proveniência e estado de colisão.

O fluxo é `UI -> LinguisticService -> SqliteLinguisticCatalog`. A UI não recebe
SQL, não abre o banco diretamente e não contém regras de identidade lexical.
Os catálogos são globais e externos ao projeto `.inde`; nenhum schema do
projeto foi alterado.

## Invariantes

- abertura SQLite com `SQLITE_OPEN_READONLY` e `PRAGMA query_only=ON`;
- statements fixos e parametrizados; limite entre 1 e 50;
- chave lexical normalizada em NFC + Unicode casefold antes da consulta;
- validação dos metadados dos contratos `l06-query-1` e `l08-promotion-1`;
- L-06 precisa declarar runtime somente leitura e rede ausente;
- L-08 precisa declarar revisão aceita, origem `synthetic_fixture`, estado
  `derived`, escopo `internal_productive` e zero bindings automáticos;
- nenhuma consulta contém vencedor, seleção ou botão de aplicar acepção;
- navegação L-08 não executa match no Documento;
- o conteúdo e a seleção do Documento não são persistidos no catálogo;
- ausência ou incompatibilidade do catálogo desabilita o resultado sem criar
  banco vazio, buscar na rede ou degradar silenciosamente o contrato.
- a quarentena usa arquivo, caminho, método e contrato próprios;
- o contrato de quarentena exige `quarantine_ready`,
  `accepted_for_isolated_testing`, `criteria_passed_needs_review`,
  `internal_quarantine`, `promoted=0`, `isolated=1`, rede ausente e zero
  bindings automáticos;
- a UI mostra a procedência Onto.PT e a classificação de sobreposição com
  OpenWordNet-PT, mas não mescla, deduplica ou promove registros em tempo de
  consulta.
- o contrato editorial v1.1 exige `editorial_review_ready`, política aceita,
  conteúdo `drafts_pending_human`, 960 avaliações, 667 candidatos a manter a
  fonte, 293 intervenções, 32 rascunhos, 8 abstenções e zero aceites humanos;
- o rascunho é `derived`, nunca substitui `definition_original` e não pode ser
  confundido com texto da fonte;
- conflitos semânticos e evidência insuficiente produzem abstenção, não uma
  definição inventada.
- a decisão sucessora v1.2 aceita exatamente 32 definições editoriais e mantém
  exatamente 8 abstenções como `deferred_not_approved`;
- as abstenções usam o original como fallback somente com `motivo` visível,
  relações `pending` e indicação explícita de que o fallback não é aceite;
- prioridade de resolução: estabelecer a acepção, produzir definição útil,
  validar as relações contra a acepção e só então manter o fallback, se ainda
  necessário.
- a extensão v2.2 exige contrato `ontopt-priority-sense-decomposition-1`,
  `isolated=1`, `promoted=0` e as identidades SHA-256 fixadas das bases pai e
  de referência;
- fragmentos sem vínculo mensurável não entram na consulta padrão, e sentidos
  decompostos aparecem explicitamente como pendentes de confirmação humana;
- a interface apresenta as contagens reais do v2.2 e não confunde sua política
  aprovada com os 32 aceites individuais herdados da calibração v1.2.
- o overlay derivado v2.3 permanece num arquivo separado, exige a identidade do
  pai v2.2 e declara `isolated=1`, `promoted=0`, ausência de coleta externa e
  ausência de geração de definições;
- a v2.3 introduz um cluster provisório por definição consultável, distingue
  evidência autorizada de mero contexto e mostra o mapa de cada segmento da
  definição para sua evidência;
- os códigos `LEX-DEF-001` a `LEX-DEF-004` não reescrevem nem promovem
  conteúdo: vazamento, adição sem evidência e colapso bloqueiam a redação e
  retornam ao alinhamento; redundância exige revisão cruzada humana.
- o overlay v2.4 preserva a v2.3 e introduz `evidence_atom`,
  `surface_form_group` e `lexical_form_key=(pt, normalizada, UPOS)`;
- `LEX-DEF-005` bloqueia evidência documental com múltiplas unidades
  definicionais até classificação humana de atomicidade;
- `LEX-DEF-006` cria uma família simétrica para duplicação, contenção ou
  absorção provável; nenhum membro fica liberado isoladamente;
- grafias iguais em UPOS distintos aparecem relacionadas na interface, mas
  nunca são comparadas como acepções irmãs.
- o contrato v2.5 exige autorização humana identificada, linhagem SHA-256 fixa,
  `isolated=1`, `promoted=1`, predecessores não modificados, ausência de rede e
  zero bindings automáticos;
- duplicata normalizada na mesma `lexical_form_key` vira uma única promoção;
  quase duplicata, contenção, família v2.4, mesmo synset com textos distintos e
  sobreposição com L-06 são bloqueios simétricos;
- a mesma grafia em UPOS diferente continua fora do domínio de colisão;
- fonte única só pode ser promovida quando pertence aos casos humanos
  localizados; o lote geral exige pelo menos dois synsets de origem;
- uma definição promovida é uma seleção de conteúdo fonte, não uma escolha
  contextual no Documento e não cria `SemanticBinding`.

## Configuração local

O aplicativo lê:

- `INDE_L06_CATALOG` para `dictionary-l06-query.sqlite`;
- `INDE_L08_CATALOG` para `dictionary-l08-promoted.sqlite`.
- `INDE_QUARANTINE_CATALOG` para
  `dictionary-ontopt-v0.6-decomposition-quarantine-v2.2.sqlite`.
- `INDE_DEFINITION_EVIDENCE_REVIEW` para
  `definition-evidence-review-v2.4.sqlite` (v2.3 permanece fallback legado).
- `INDE_DEFINITION_PROMOTION` para
  `definition-candidate-promotion-v2.5.sqlite`.

Sem essas variáveis, procura ambos em
`$XDG_DATA_HOME/inde/linguistic/` ou
`$HOME/.local/share/inde/linguistic/`. Os bancos não são distribuídos
automaticamente pelo repositório.

A quarentena usa, por padrão, o subdiretório `quarantine/` sob essa mesma raiz.
O arquivo instalado permanece fisicamente separado do catálogo L-06 aceito. O
INDE procura a v2.2 primeiro e recorre à v1.2 apenas como compatibilidade
legada.

O INDE procura primeiro o overlay v2.4 e recorre à v2.3 no mesmo subdiretório
`quarantine/`. A ausência de ambos não torna a consulta v2.2 indisponível, mas
remove da interface átomos, famílias, mapa de evidências e gates de QA. Um
overlay presente e incompatível falha fechado.

O banco v2.5 fica na raiz linguística, separado tanto do L-06 quanto do
subdiretório `quarantine/`. Quando válido, seus resultados são anexados à
consulta do catálogo aceito e marcados como promoção v2.5. Quando ausente ou
incompatível, o L-06 continua consultável sem essa extensão. O adaptador abre
todos os bancos com `SQLITE_OPEN_READONLY` e `PRAGMA query_only=ON`.

O catálogo L-06 v0.9 atualmente instalado possui 9.855 formas, 9.855 lexemas
e 13.776 acepções. Todas têm `definition_state=known` e definição não vazia.
Os registros permanecem `source_asserted` e `needs_review`: a autorização do
lote não equivale a validar linguisticamente cada acepção.

A v0.9 corrige o parser da v0.8, que limitava a ASCII os nomes prefixados do
RDF e encerrava objetos prematuramente quando havia ponto no nome. A correção
recuperou 2.422 formas e 2.784 acepções, preservou integralmente o conjunto
anterior e elevou para 2.414 a quantidade de formas não ASCII. As 83.762
ligações WordSense -> Word foram reavaliadas e nenhuma permanece sem alvo.

## Fora do escopo

- tokenização ou persistência de ocorrências do texto real;
- análise incremental contínua;
- destaque automático de Construction/MWE;
- `SemanticCandidateSet` persistido no projeto;
- `OPERATION_RESOLVED` ou `USER_BOUND`;
- alteração do texto, substituição lexical ou refatoração semântica;
- telemetria, rede ou serviço remoto;
- L-07 e L-09.

Esses limites impedem que a autorização de UI seja confundida com autorização
para construir o corpus local da Obra ou tomar decisões semânticas pelo autor.

## Verificação

`inde_linguistic_integration_tests` cria catálogos efêmeros, mede os bytes
antes/depois e cobre: consulta com múltiplas acepções, ausência de seleção e
binding, statements parametrizados, limites, catálogo ausente, contrato
incompatível, Unicode composto/decomposto e rejeição de build L-08 que declare
binding automático. Quando as variáveis de ambiente estão definidas, o mesmo
executável exige que todas as acepções retornadas pelo catálogo L-06 tenham
definição conhecida e não vazia e faz smoke do catálogo L-08 aceito.

Para a quarentena, o teste mede os bytes antes/depois, verifica definição,
registro e synset de origem, sobreposição de referência, versão dos critérios,
ausência de seleção/binding e rejeita um arquivo que declare promoção. O smoke
com o banco real confirmou consulta somente leitura do lote de 960 sentidos.

A extensão v1.1 exige que a view editorial cubra as 960 entradas,
preserve os 960 originais, mantenha rascunho e original em campos distintos,
rejeite contagens ou estados incompatíveis e devolva ao domínio o gate, os
flags, a pontuação, o estado editorial e a justificativa. O teste real confirma
que `a-par` apresenta o rascunho sem perder “em igual proporção.”.

A extensão v1.2 exige adicionalmente 32 decisões `accepted`, 8 decisões
`deferred_not_approved`, 8 relações pendentes e 8 fallbacks originais com
motivo. Os testes cobrem uma definição editorial aceita e um original
sinalizado, além de rejeitarem metadados que ampliem essas contagens ou indiquem
promoção.

A extensão v2.2 testa separadamente a identidade da decomposição, seu estado
isolado e não promovido, as contagens de fragmentos, sentidos vinculados,
fragmentos sem vínculo e hífens pendentes. O smoke com o banco real consulta
`entusiasta` e exige ao menos um resultado
`policy_approved_decomposition_v22` com
`needs_human_confirmation`. O catálogo é aberto somente para leitura e seus
bytes são preservados antes e depois da consulta.

A extensão v2.3 testa o contrato do overlay, a identidade do pai, isolamento,
ausência de promoção/coleta/geração, cobertura consistente de clusters,
definições e anotações e o vocabulário fechado de códigos QA. A consulta lê o
overlay separadamente, associa a anotação por `source_record_id + sense_id` e
preserva os bytes tanto da v2.2 quanto da v2.3. O smoke real exige 130.057
clusters e evidencia, para cada resultado, somente o contexto autorizado da
acepção atual.

A extensão v2.4 mantém os testes v2.3 e acrescenta linhagem fixa do predecessor,
atomização exaustiva, bloqueio de evidência composta, famílias simétricas e
isolamento por `(forma normalizada, UPOS)`. Fixtures reais exigem que `aba`
exponha seus átomos, que todos os membros de `ab-rogação` e `abade` recebam o
mesmo gate e que grafias multiclasse nunca sejam comparadas entre UPOS. O smoke
real cobre 130.057 clusters e o overlay é preservado byte a byte durante a
consulta.

A extensão v2.5 testa o contrato e as três identidades SHA-256, a autorização
humana, a ausência de mutação/rede/binding, a bijeção entre disposição
`promoted` e `promoted_sense_v25`, a inexistência de colisão bloqueante e a
ausência de sobreposição com L-06. O teste efêmero confirma leitura sem escrita
e falha fechada quando `automatic_bindings_created=1`; o smoke real exige 86
promoções e três resultados promovidos para `abade`.

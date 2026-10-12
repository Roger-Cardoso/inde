# Estado atual — vocabulário autoral / 2026-10-12

Propostas aprovadas explicitamente pelo usuário; aplicação ao candidato
privado aceito, schema 28 sem migração. [Contrato](author-vocabulary-2026-10-12.md).
Interface, laboratório, acessibilidade e ajuda usam vocabulário orientado à
tarefa. Nomes pessoais e contratos internos não são traduzidos. Identificadores
opcionais, formulário de marcações, alternativa para não ficção e glossário
entram neste corte. Resultados finais e limites ficam no manifesto da tarefa
`outputs/reforma-vocabulario-autoral-entrega-2026-10-12.json`.
Aceite humano desta nova entrega pendente; não commitado nem publicado.

# Estado histórico — realização textual e continuidade / 2026-10-11

Aceite humano do corte de acesso/tema confirmado neste chat; novo bloco
autorizado. Implementação privada em schema 28: alvos por âncora/ocorrência,
conferência e reconciliação, separação explícita de prosa, navegação de ida e
volta, cadeias/ciclos por autoridade e presença planejada declarada.
[Contrato, limites e revisão](narrative-realization-continuity-2026-10-11.md).
Verificação técnica concluída: CTest 33/33; 291 unidades recompiladas sem avisos;
núcleo Debug e ASan/UBSan focais (14 unidades, quatro drivers); GTK normal 8/8
e focal 8/8 (20 unidades); quatro regressões 8/8 cada; dois startups e seis
quadros inspecionados. Demais dependências Release, sem LSan ou certificação
integral de acessibilidade. Coletores selecionados encerraram com código zero.
Manifesto: `outputs/reforma-realizacao-continuidade-entrega-2026-10-11.json`
na raiz da tarefa. Não reutilizar hashes históricos como evidência deste corte.
Aceite humano deste bloco ainda pendente. As seções anteriores registram o
estado na emissão; o aceite posterior de acesso/tema está reconhecido acima.
Não commitado, sem publicação e sem antecipação das próximas frentes.

# Estado histórico — compatibilidade de tema herdado / 2026-10-11

Correção complementar tecnicamente verificada. Base GTK compatível antes da
inicialização, paleta INDE e preferência Sistema independentes do CSS externo.
Dark reconhecido sem distinção de caixa. Schema 27; CTest 32/32, auditoria de
286 unidades sem avisos finais, Debug/ASan da política e driver focal de tema.
Matrizes normal/focal têm sete verificações positivas cada, com controles
negativos intencionais; quatro startups de cada matriz são Release, não ASan.
Sonda real do desktop sem janela, dois quadros do driver inspecionados.
[Causa, evidências e limites](theme-compatibility-2026-10-11.md).
Aceite humano pendente. Nenhum CSS/configuração global alterado ou release feito.

# Estado anterior — acesso inicial e interação narrativa / 2026-10-11

Correção tecnicamente verificada, schema 27 mantido. Acesso inicial em contexto,
menus próprios por alternativa/modelo, comparação multialvo pareada e seções
Unidades/Linhas/Vínculos sem divisórias arrastáveis. Avisos tratados por causa,
sem desativar diagnósticos. [Contrato e revisão](narrative-access-2026-10-11.md).
Não reutilizar os hashes nem a aprovação técnica do candidato anterior como
evidência desta fonte modificada. Aceite humano pendente.

CTest Release 31/31; 277 unidades recompiladas sem avisos finais. Matrizes GTK:
acesso 8/8 e focal instrumentada 8/8; construtor, alternativas e temporal 8/8
cada; cores/laboratório 8/8 e cores instrumentadas 4/4. Total 52 verificações
de execução, mais dois startups limpos. Núcleo Debug e sanitizers focais:
três drivers, dez unidades; interface focal: quinze unidades de acesso e quatro
de cores, demais dependências Release, sem LSan. Onze quadros inspecionados
pelo agente, sem certificar acessibilidade integral ou ergonomia humana.

# Estado anterior — Alternativas, linhas e impacto / 2026-10-11

Schema 27, implementação privada tecnicamente verificada. Derivação completa do contexto
de planejamento existente, Documentos/cadastros compartilhados explicitamente,
proveniência registrada e comparação por linhagem; ordem própria das linhas;
autoridades dos vínculos e prévia com avisos não bloqueantes. Não sincroniza
montagem, cronologia, fila e editorial. Não gera prosa, faz merge ou publica.
Release/CTest 30/30. Núcleo Debug e ASan/UBSan focais passaram os dois drivers;
GTK alternativas Release 8/8 e instrumentado 8/8; regressões construtor e temporal
8/8 cada; startup principal/laboratório e inspeção visual selecionada.
Foco: oito unidades do núcleo e nove da interface, dependências Release, sem LSan.
Hashes/relatórios no manifesto final deste corte. Aceite humano pendente, continuação
autorizada sem inferir aceite anterior. [Limites e revisão](narrative-alternatives-2026-10-11.md).

# Estado anterior — Construtor narrativo inicial / 2026-10-11

Primeiro corte implementado e tecnicamente verificado sobre a base humana aceita `995fbb27`.
Schema 26: contextos de unidade, fila de escrita opcional, realizações textuais
muitos-para-muitos e relações em foco. Migração aditiva e transacional;
Salvar cópia reidentifica os novos vínculos. Nova página de unidade dentro de
Planejar, com detalhes recolhíveis e escrita/retorno. Sem novo painel permanente.
Ordens narrativa, cronológica, de trabalho e editorial conservam autoridades
independentes. Não conclui derivação contextual, impactos/conhecimento ou toda a
reforma. Ver [estado detalhado e revisão](narrative-builder-increment-2026-10-11.md).
Aceite humano deste corte pendente. Registros abaixo são históricos.
Release: 29/29 CTest; interface do construtor 8/8 e ASan/UBSan focal 8/8;
regressão temporal 8/8. Núcleo Debug e ASan/UBSan focais passaram.
Inicialização dos binários principal/laboratório e inspeção visual selecionada
passaram; não são aceite humano nem certificação integral de acessibilidade.

# Estado anterior preservado — Reforma de interface / 2026-10-08

Quinto bloco implementado: Produção derivada da Obra, seleção explícita dos
Documentos, ordem da estrutura ativa, perfis Manuscrito para avaliação/Leitura,
DOCX/EPUB/PDF/HTML/TXT e relatório de fontes, omissões, perdas e hashes.
Seleção e prévia usam páginas alternativas dentro de Manuscrito, sem painel
permanente ou nova tarefa fixa do shell. Contexto guarda IDs/opções; retomada
exige nova prévia e reconhecimento das perdas. Schema 19 e payloads 2/4/2
permanecem. Release, 19/19 CTest, 32 cenários GTK completos, 16 suplementares
no tema escuro e oito focais finais de continuidade/produção passaram nos mesmos
binários. Dois perfis passaram por verificação independente dos formatos e
EPUBCheck; todas as 25 páginas DOCX/PDF das amostras finais e saídas da interface
foram inspecionadas visualmente. Três conjuntos ASan/UBSan focais passaram,
com dependências Release e LSan desligado; não a aplicação GTK inteira.
Os quatro cortes anteriores receberam aceite humano explícito; o quinto exige
aceite próprio e não foi publicado. Escopo/limites/ordem em
[reform-work-production-2026-10-08.md](reform-work-production-2026-10-08.md).
Próximo: impacto/reversão das operações do Projeto, antes de Composição
persistente, aliases e calendário ficcional. Reforma inteira não concluída.

Quarto bloco implementado: revisão editorial com fila/detalhe/comparação exclusivos,
comentários decidíveis, sugestões explícitas ancoradas, estados vinculados por
fingerprint ao Documento e revisão independente. Contexto de Revisar integra
os espaços de trabalho. Schema 19, com migração transacional e preservação das
projeções semânticas/temporais; payloads semântico 2/checkpoint 4/clipboard 2.
Release, 18/18 CTest, 28 cenários GTK, 12 suplementares no tema escuro,
parser independente de exportação e dois conjuntos ASan/UBSan focais passaram
nos mesmos binários finais; dependências Release e LSan desligado, não a aplicação
GTK inteira instrumentada. A disputa intermitente de rolagem na reabertura foi
corrigida e passou com reprodução deliberada no ensaio de continuidade.
O quarto corte recebeu aceite humano posteriormente, registrado separadamente
em reforma-producao-aceite-anterior-2026-10-08.json. Seu manifesto congelado
conserva o estado histórico da entrega; não foi publicado. Escopo e limites em
[reform-editorial-review-2026-10-07.md](reform-editorial-review-2026-10-07.md).

Terceiro bloco implementado e tecnicamente verificado: continuidade, espaços de trabalho nomeados e
exportação inicial de Documento HTML/TXT, com prévia/perdas/readback.
Release, 17 CTest, 24 cenários GTK, oito suplementares no tema escuro,
parser independente de HTML/TXT e ASan/UBSan focal passaram no código final.
O terceiro recebeu aceite humano explícito posteriormente, registrado em
reforma-revisao-aceite-anterior-2026-10-07.json; seu manifesto congelado conserva
o estado histórico de entrega. Publicação não autorizada. Estado e limites em
[reform-continuity-production-2026-10-07.md](reform-continuity-production-2026-10-07.md).

Segundo bloco: organização de Planejar e Manuscrito, autorizado antes do
aceite humano. Planejar abre nos materiais, com seletor compacto de área,
busca direta e gestão de tipos sob demanda. Manuscrito abre na obra,
conserva seu detalhe ao retornar da Escrita e separa modelos/alternativas
em abas. Documentos têm prioridade sobre referências recolhidas.
A primeira entrega permanece congelada e disponível, sem sobrescrita.
Release atualizado, 16 CTest, 20 cenários GTK em Atlas/Projeto vazio e seis
suplementares no tema escuro passaram novamente após a queda de energia.
Estado e limites em [reform-organization-2026-10-07.md](reform-organization-2026-10-07.md).
O aceite humano de ambos os cortes foi confirmado posteriormente pelo usuário.

## Primeiro corte preservado — Shell, temas e escrita / 2026-10-07

Primeiro corte da reforma geral implementado: shell mínimo de três tarefas,
editor com uma barra e menus progressivos, modo Foco, Claro/Escuro/Sistema,
tokens compartilhados, retomada por Projeto/Documento, fila real de revisão,
formulários progressivos e laboratório com uma edição visível por vez.
O candidato anterior foi preservado integralmente e permanece intocado.
Schema/codecs e autoridades de persistência permanecem inalterados.
16 CTest passaram; verificações GTK/capturas e hashes devem ser consultados
na entrega de 2026-10-07, não nos manifestos anteriores. Aceite humano confirmado
posteriormente; manifestos congelados conservam o estado histórico.
Revisão/produção completa e Composição persistente continuam pendentes.
Detalhamento, limites e sequência em [reform-2026-10-07.md](reform-2026-10-07.md).

## Predecessor preservado — Editor conectado ao Projeto / 2026-10-04

Implementado neste corte: ações contextuais da seleção; criação atômica de
entidade/tipo/ocorrência; seleção canônica sem match por nome; criação de relação
e tipo com prévia/direção; materialização a qualquer tipo (incluindo Acontecimento
e Informação); distribuição derivada manual; sugestões inline de nome/resumo e
relações do Projeto, com confirmação e operação composta reversível.

Novos vínculos textuais têm evidência de âncora e entram em Camadas/Entidades,
busca persistida e consultas Escrita↔Planejamento. Não duplicam referências.
Criar cadastros modifica o Projeto deliberadamente; undo textual não os apaga.
Falha de persistência reverte a transação inteira, incluindo tipos e change_log.
Relações canônicas, relações de marcações e contexto temporal continuam distintos.
Schema 18/codecs preservados; definições antigas permanecem na versão original.

Validação técnica concluída: 16 CTest e 17 verificações GTK em cópias privadas
(novo fluxo em vazio, Atlas e vazio limpo; escrita, semântica, barra, cor,
escala e recuperação em vazio/Atlas), 18 capturas e preservação de todas as
linhas SQLite anteriores. ASan/UBSan focal passou em 18 unidades e dois drivers,
com núcleo e dois cenários GTK; restante Release, LSan desligado. Build final e
manifestos reconferidos no fechamento. Conferir
outputs/editor-interacao-verificacao-2026-10-04.json para hashes e entrega selada.
Correção real adicional: criação síncrona adota novo baseline de autosave após
transação atômica bem-sucedida. Ensaios antigos de barra/recuperação foram
adequados a contadores e checkpoints preexistentes, sem consumir dados anteriores.
Aceite humano desta entrega ainda não obtido. E5-S1R aceito pelo pedido atual;
E5-S2 não iniciado, prioridade alterada explicitamente pelo usuário.

## Registro histórico E5-S1R (mantido, não é o estado do novo corte)

Este registro cobre a reforma do laboratório sobre E5-S1;
o predecessor imediato é editor-etapa5-spike-2026-10-04. Não substitui
o histórico global mantido no repositório externo inde-docs. Base Git f155bd8,
alterações locais sem commit/publicação. Versão CMake 0.1.0; schema principal
18, journal auxiliar 1, payload de checkpoint 4 e exportação 1; clipboard 2.
Payload semântico 2, leitor de 1/2. Leitor de checkpoints 1/2/3/4.

- Consolidação anterior: aceite humano geral com pedido de remover Busca
  semântica. Ensaios humanos individuais não inferidos desse aceite.
- Botão redundante removido; pesquisa persistida permanece dentro de Camadas.
- Perspectiva situada v1: pessoa, focalização, focalizadores canônicos opcionais,
  nota; incompletude e aconselhamento explícitos, sem conversão da voz livre.
- Relações entre Documentos: consultas paginadas por direção, escolha de alvo
  local/persistido, prévia, confirmação guardada, notas, backlinks, navegação,
  proteção de alvos e desvinculação no Documento de origem.
- Barra em três fileiras na ordem pedida, nomes e acesso direto aos painéis.
- Fonte, tamanho 6–96, cor RGB, seis níveis de título, Subtítulo/Citação/Corpo e
  quatro alinhamentos persistentes, com migração transacional preservando v16.
- Seletor de cor visual nativo: tonalidade/saturação, paleta, RGB 0–255,
  hexadecimal sincronizado e prévia no fundo do editor. Confirmar aplica no
  alvo capturado; cancelamento e hexadecimal inválido não alteram o texto.
- Perspectiva global tipada sem âncora artificial, incluindo Documento vazio.
  Uma declaração corrente por Documento; local/global, incompletude e múltiplos
  intervalos ficam explícitos. A perspectiva livre não é convertida.
- Vínculos manuais de contexto ficcional por anotação: eixo e ponto/período
  fechado existentes. Prévia, confirmação, desfazer/refazer, proteção de FKs e
  de inversão por reordenação. Não cria acontecimento, presença ou calendário.
- Busca persistida inclui declarações globais e filtros por eixo/ponto; período
  inclui seus dois limites. Identidades e proveniência ficam no agregado.
- Comentários editoriais ancorados e modelos inline literais do Documento,
  sugestão por prefixo, confirmação explícita e gestão reversível, sem scripts.
- Mesma infraestrutura de transações, âncoras Unicode, undo/redo, autosave e CAS;
  leitores mantêm compatibilidade dos checkpoints 1/2/3 e clipboard 1.

Validação técnica e hashes devem ser conferidos nos relatórios
`outputs/editor-semantico-3-*.json` do workspace de entrega. Em 2026-10-04 o
usuário aceitou o restante e as pendências anteriores, solicitando melhorar a
cor e avançar. Não se infere certificação medida de IBus/sessão de 45 minutos.
O incremento semântico 3 foi revisado e aprovado por pedido direto posterior.
E5-S1 recebeu revisão humana crítica quanto à clareza/utilidade, não aceite do
laboratório. E5-S1R implementa seleção/cursor em vez de terços, identificação
explícita, múltiplos painéis/versões/origens, usos/impacto, famílias de alternativas,
promoção independente, ocultação/camadas e criação efetiva de novo Documento na
cópia preparada. Conserva camadas representáveis e avisa cortes/globais/conflitos.
Blocos, versões e montagem continuam voláteis; o Documento salvo é snapshot
independente, não composição viva persistida. Editor principal/schema/codecs
não mudam. Buffers físicos continuam no CLI, fora da interface autoral.
Gate técnico próprio: 15 CTest, 3 cenários GTK, preservação de linhas autorais,
criação/readback e ASan/UBSan focal; conferir resultados finais e hashes em
outputs/editor-montagem-*. Novo aceite humano pendente. Nenhuma etapa inteira
encerrada. Sem binding lexical ou inferência de realidade. Importação gen7 e HTML
derivado dispensados pelo usuário; calendário profundo programado após Etapa 5.

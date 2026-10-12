# Decisões do editor: interação com Projeto e histórico

## EI-01 — Transações insert-only e intenção contextual

Entidade e novo tipo opcional são inseridos junto ao Documento/vínculo, após
CAS da identidade/estado esperado, na mesma transação SQLite. Relação e tipo
opcional são inseridos juntos, validando snapshots de entidades/tipo. Sem upsert
de entidades existentes e sem compensação por apagar dados já compartilhados.
UI não escreve SQL. Desfazer textual não é apagar entidades/relações do Projeto.
Criação síncrona drena autosave, persiste a transação composta e só após sucesso
limpa o baseline interno antigo do salvador. Próximo job recebe o Documento
recém-confirmado; rejeição não perde a base anterior ou o rascunho.

## EI-02 — Uma fonte por associação

Ocorrência nova é inde.semantic.entity-mention/v1; materialização é
inde.semantic.materialization/v1. Campo Entity tipado é a identidade canônica;
âncora tem evidência/hash e resolução. Referências legadas são conservadas, não
convertidas silenciosamente. Projeções de usos unem as duas fontes, deduplicam
entidades e excluem marcações arquivadas. Não criar referência nativa redundante
que sobreviva ao retarget de uma marcação. Não inferir fatos ou tempos.

## EI-03 — Sugestões explicáveis e seleção capturada

Popup consulta até seis entidades após debounce existente; prefixos @nome,
@entidade:, @personagem:, @local:, @acontecimento:, @informacao:.
Resumo e relações são dados canônicos do Projeto, não scripts/geração/NLP.
Escolha abre prévia; inserir texto e criar associação opcional é uma transação
editorial reversível. Snapshot de dados e revisão do texto são revalidados.
Mudança de foco/PRIMARY não redefine o intervalo confirmado; IME incompleto
bloqueia mutação. Identidade do Projeto impede aplicar em outro contexto.

## EI-04 — Distribuição é projeção, não diagnóstico automático

Cada categoria mede união de intervalos resolvidos, em caracteres Unicode com
espaços. Sobreposição de categorias é contada separadamente; não somar
percentuais como categorias exclusivas. Arquivadas não participam. Globais
não ganham cobertura artificial; pendências não resolvidas ficam fora do
percentual. Relações entre marcações e relações canônicas incidentes são
contagens diferentes. Consulta canônica limitada a 32 entidades/200 relações
por entidade declara piso em vez de inventar um total. Não salvar estatística
como fato ou classificação autoritativa.

## Histórico: E5-S1R e incrementos anteriores

## E5S1R-01 — Separar rascunho, versão aplicada e leitura

Reforma autoral substitui UI física. Bloco/versão/ocorrência são nomeados e
identificados; leitura não recebe rascunhos antes da aplicação confirmada.
Trocar origem ou escolha de leitura não descarta edição. Impacto enumera usos
da montagem, inclusive ocultos e versões fixas. Biblioteca e alvo operacional
ficam sincronizados. Histórico de montagem não é histórico durável de escrita.
Divisão usa seleção/cursor do autor, nunca terços arbitrários.

## E5S1R-02 — Alternativas locais e promoção independente

Cada família tem Base e escolhas próprias; combinar opções não suprime bases de
outras famílias. Promover cria identidade nova, guarda proveniência e torna uso
comum. Outras opções ficam explicitamente ocultas, não apagadas. Não inferir
equivalência semântica, fusão de versões ou vínculo de propagação com origens.

## E5S1R-03 — Documento derivado salvável não é montagem viva persistida

Atualiza a fronteira E5S1-01: materialização explícita agora cria Document novo
somente na nova cópia marcada, após prévia, avisos e confirmação. A origem não
recebe edições. Blocos/ocorrências continuam em memória, não em schema dedicado.
Estado novo prevalidado antes de aplicar comando; guarda esperada impede
sobrescrever uma base antiga e mantém rascunho em erro. Repositório canônico
faz save/readback; não afirmar certificado insert-only concorrente.

## E5S1R-04 — Camadas conserváveis, conflitos explícitos

Atualiza E5S1-03: recortes guardam formatos e anotações contidas. IDs locais são
recriados por ocorrência; entidades/eixo/pontos canônicos do mesmo Projeto são
preservados. Cortes atravessados e contexto global em montagem composta exigem
avisos, sem vencedor automático. Não gravar relações de origem como novos links
persistentes. Casos incompatíveis falham fechado; criação não ignora rascunhos.

As decisões E5S1 abaixo registram o estágio anterior, preservado como histórico;
suas limitações de UI e de materialização não são o estado do E5-S1R.

## E5S1-01 — Experimento não substitui autoridade

Laboratório separado e desativado por padrão no build. Não é instalado com
inde, não modifica o editor principal nem schema/codecs. Documento legado
permanece autoritativo. Projeção experimental não é um Document salvável.
Conversão futura exige autoridade única por modo e política de camadas antes
de schema dedicado; nunca duas cópias editáveis do mesmo corpo.

## E5S1-02 — Identidade, revisão e colocação são dimensões distintas

Composição, bloco, revisão e colocação possuem identidades diferentes. Revisão
é imutável e exige base esperada para alterar. Live/forked acompanham identidade
corrente, pinned resolve revisão fixa, referência não incorpora caracteres.
Bifurcação cria identidade com proveniência; não significa sincronização futura.
Ordem/variante/camada pertencem à colocação. Perfil resolve escolha explícita;
alterar a leitura não apaga revisões nem modifica cronologia/editorial.

## E5S1-03 — Fonte e fronteiras não são propagação semântica

Extração volátil conserva origem, SHA256 do corpo e offsets Unicode da revisão
inicial. Editar invalida equivalência dos offsets originais. Mapas de projeção
têm offsets próprios; globais da origem não se tornam globais da composição.
Sem copiar/reidentificar âncoras, entidades, relações ou anotações neste spike.
Referências/colocações invisíveis e proveniência também protegem exclusão.
Blocos aninhados e tombstones persistentes ficam para contrato posterior.

## E5S1-04 — Desempenho é medido na ponte, não só no buffer

Comparar string, chunks imutáveis de 4096 bytes e piece table volátil, sem
prometer rope balanceado, armazenamento incremental ou O(log n). Medir também
materialização, histórico/metadados atuais, checkpoint completo e GTK até
after-paint. Ganho isolado não autoriza substituir GtkTextView nem escolher
formato autoral. Corpus sintético, amostras pequenas, Xvfb e ausência de IBus
são limites explícitos. Decisão definitiva aguarda gate deste spike.

## ES3-01 — Declaração global não é intervalo artificial

AnnotationScope é explícito. Documento global não possui anchor_id; escopo
intervalar mantém evidência e resolução Unicode. Este contrato habilita somente
inde.semantic.perspective/v1 global, sem alterar suas definições de campo.
Uma declaração não arquivada por Documento, inclusive incompleta. Arquivadas
permanecem identificáveis; reancoragem global é recusada. Perspectiva livre é
independente. Leitura no cursor mostra contexto local/global, incompletude e
ambiguidade; não escolhe entre intervalos sobrepostos nem atribui focalizador.

## ES3-02 — Tempo do texto é contexto, não fato ficcional

SemanticTemporalBinding identifica eixo ficcional, ponto inicial e fim opcional.
Fim ausente é um ponto, não período ilimitado; dois pontos delimitam período
fechado inclusivo. Limite de 32 vínculos por anotação. Origem, fonte, nota e data
de criação pertencem ao payload autoritativo. A projeção indexa apenas IDs.
Pontos devem existir no mesmo eixo do Projeto; inversão e duplicação recusadas.
FKs protegem exclusão/mudança de eixo; trigger protege inversão por reordenação.
Notas e intervalos arquivados também preservam suas referências. Nenhum ordinal
se converte em data, duração, calendário ou unidade física.

## ES3-03 — Compatibilidade e migração transacional

Schema 18 refaz semantic_annotations e ambas as tabelas referenciantes na mesma
transação, por cópias temporárias sem FKs; preserva linhas e relações. Não
desabilita foreign_keys nem reescreve payloads v1 na abertura. Payload semântico
v2 e checkpoint v4; leitores de v1 e checkpoints 1–3 mantidos. Futuros ou campos
incompatíveis falham fechado, sem descarte. Save-as remapeia project_id também
na projeção temporal; histórico contabiliza strings/vetores novos.

## ES3-04 — Seletor de cor é independente da lente

GTK fornece paleta e editor de tonalidade/saturação por API pública. RGB/hex
sincronizados e prévia sobre o fundo do editor; alfa não faz parte do contrato
Foreground. A seleção autoral é capturada antes de campos adquirirem PRIMARY,
mas alteração de epoch/revisão recusa o comando. Composto possui dono C++ para
desconectar callbacks antes de GTK destruir filhos. Não reapresentar um
OverlayDialog dentro de response, que destrói o próprio adaptador.
Testes usam display/perfil/clipboard isolados e GSettings em memória.

## ES2-01 — Perspectiva situada é uma anotação, não a verdade do Documento

Nova chave protegida inde.semantic.perspective/v1, sem alterar as cinco
definições v1 anteriores. Intervalo com identidade/evidência existente;
persona e focalização obrigatórias, focalizadores canônicos opcionais. Interna
sem focalizador recebe conselho não bloqueante. O autor pode declarar sem
atribuir personagem; nada é inferido. Document.perspective permanece textual.

## ES2-02 — Relações pertencem à origem, não aos dois Documentos

Schema 16 já permite o alvo no mesmo Projeto. A nova consulta retorna ambas
as identidades, estado e título, com paginação limitada e SQL parametrizado.
A UI combina saídas locais em rascunho com entradas externas persistidas:
não ressuscita saídas desfeitas, não força save por pesquisa, não altera outro
Documento por uma ação no alvo. Desvinculação de entrada externa exige navegar
à origem. FK continua a impedir remover alvo utilizado.

## ES2-03 — Escolha exige prévia e verificação otimista

Antes da confirmação, mostrar os endpoints, âncora, trecho, papel e estado.
Capturar epoch/revisão da origem e snapshot do alvo externo. Alvo ou origem
alterados recusam o comando e pedem nova prévia. A relação referencia identidade,
não congela o texto futuro do alvo. Uma mudança posterior à confirmação pode
alterar o trecho; não se promete snapshot/transclusão/pinning.

## ES2-04 — Modularidade e compatibilidade

Consulta de relações, navegação e escolha/prévia têm módulos próprios.
Nenhuma UI contém SQL. Novos campos usam os codecs genéricos já existentes;
payload desconhecido e dados antigos continuam conservados. A mudança não
autoriza match lexical, criação silenciosa de entidades ou novas fontes.

## ES2-05 — Transição GTK e encerramento seguro

Mudanças rápidas no seletor são coalescidas no idle lendo a última combinação
de escopo/papel. Cancelamento invalida o callback pendente. Testes esperam o
estado apresentado, não presumem uma barreira a partir de um temporizador.
O buffer usa guarda fraca de vida invalidada no início da destruição do
workspace: notificações de seleção/clipboard posteriores não acessam botões
já destruídos. Essa dependência foi identificada por backtrace na regressão.

## TB-01 — Formatação é modelo persistente, não apenas aparência GTK

Schema 17 acrescenta valor validado aos intervalos de formatação e modelos
inline por Documento. Migração transacional conserva os intervalos legados.
Undo/redo compara os valores e os modelos, incluindo seu custo na retenção.
Clipboard v2 e checkpoint v3 preservam os parâmetros; leitores mantêm v1 e
v1/v2, respectivamente. Não aplicar migração aos projetos originais na revisão.

## TB-02 — Comandos inline são escolha literal sob autoridade do autor

NovelWriter.html foi lido como referência, não executado nem convertido em
requisito de segurança. Modelos são prefixo, palavra-chave, nome e texto.
Escolha explícita insere texto numa transação; ferramentas nativas apenas
abrem os fluxos já existentes. Não há eval, macros executáveis, localização
fixa, fontes novas, binding automático ou conversão de tokens em fatos.
Modelos pertencem ao Documento, não a uma biblioteca global ainda inexistente.
Ctrl+Alt+Espaço evita tomar Ctrl+Espaço de métodos de entrada.

## TB-03 — Comentário é anotação editorial ancorada

inde.editor.comment/v1 reutiliza identidade, evidência e estados de âncora.
Nota e aberto/resolvido são dados editoriais, não texto ficcional nem corpo.
Comentar sem seleção consulta essas camadas; com seleção abre o formulário.

## TB-04 — Seletores não restauram seleção dentro de changed

No X11, mudar a seleção do TextView enquanto um seletor GTK emite changed
pode bloquear a atualização de PRIMARY em uma solicitação de tempo do servidor.
Backtrace identificou XIfEvent/GDK na restauração. Fonte, tamanho e estilo
agendam a transação no idle e verificam vida, epoch, revisão e seleção.
Refletir o formato calcula primeiro o estado desejado, evitando alternar
default e valor atual e emitir alterações redundantes. Testes aguardam o
estado efetivamente persistido. O campo numérico não duplica os botões −/+.

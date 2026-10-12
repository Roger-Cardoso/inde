# E5-S1 — laboratório de blocos e spike físico do texto

Base editor-semantico-3 aceita por pedido direto em 2026-10-04. Este corte
entrega código experimental separado, não a Etapa 5 inteira nem uma migração.
Schema 18, semântico 2, checkpoint 4, clipboard 2 e envelope/journal 1 não mudam.
O executável principal reconstruído permanece bit a bit igual ao aprovado.

## Autoridade e identidade

- Documento legado continua fonte de verdade: corpo e camadas existentes.
- TextBlocksLab tem identidade de composição, blocos e colocações em memória.
- Um bloco não é automaticamente parágrafo, cena, entidade ou unidade editorial.
- Revisões são imutáveis e identificadas; editar cria sucessora e confere a
  revisão esperada, sem último timestamp vencer.
- Live acompanha a revisão corrente da mesma identidade; pinned preserva a
  revisão escolhida; forked usa nova identidade com origem bloco/revisão;
  reference-only não incorpora caracteres.
- Ordem pertence à composição, não ao bloco. Reordenar exige permutação exata.
- Variante e camada pertencem à colocação; o perfil escolhe uma variante e um
  conjunto explícito de camadas. Comum não significa selecionar toda alternativa.
- Impacto inclui colocações invisíveis: vivas afetadas, fixadas preservadas e
  referências de navegação. Exclusão protege todas e a proveniência de bifurcações.
- Não existem blocos aninhados neste experimento; ciclos de transclusão não
  são suportados. Isso não constitui contrato de aninhamento pronto.

## Fonte, intervalos e camadas

Importar para memória cria nova identidade com origem documental e revisão
literal. A revisão inicial registra intervalo Unicode e SHA256 do corpo da
origem. Conteúdo alterado deixa de afirmar equivalência dos offsets originais;
bifurcar conteúdo idêntico preserva essa evidência, sem compartilhar identidade.

A projeção fornece colocação, bloco, revisão e offsets próprios. Uma referência
sem incorporação é segmento de comprimento zero e não cria âncora artificial.
Essa projeção não é Document, não tem método de salvar/promover e não copia
âncoras, referências, semântica, perspectiva global ou comandos inline.
Os dados completos da origem ficam intactos. Contratos de transferência de
camadas, âncoras cruzando cortes e edição composta ainda precisam de decisão
antes de materializar a composição como Documento autoral.

## Três representações físicas

1. Contiguous: string UTF-8, cópia na edição, snapshots com alocações completas.
2. ImmutableChunks: fragments imutáveis de até 4096 bytes em limites UTF-8,
   compartilhamento de segmentos e compactação local. Vetor, não rope balanceado.
3. PieceTable: representação híbrida volátil, peças de texto inicial/inserções,
   compartilhamento entre snapshots. Também usa vetor, sem promessa O(log n).

As três usam offsets de bytes internos validados; a API autoral existente e
mapas de projeção usam caracteres Unicode. Não normalizam NFC/NFD. Cortes são
de codepoints, não garantia de fronteira de palavra ou cluster de grafema.
Buffer/revisões/projeção: teto de 32 MiB; 256 blocos, 2048 colocações,
4096 revisões, 32 variantes e camadas. Fragmentos têm teto de 100 mil.

O benchmark usa corpus sintético de 100 mil e 500 mil palavras. Mede
início/meio/fim, colagem de 100 mil palavras, materialização, busca com
materialização, snapshots, ponte para transações/histórico semântico existentes
e checkpoint SQLite completo em bancos experimentais temporários.
Não é persistência incremental de produção. Os bancos benchmark_body/chunks
nunca entram no .inde. Memória retida é estimativa de backing allocations e
descritores deduplicados; não é RSS nem contagem de todo o allocator.

## Laboratório gráfico

Executável separado inde_text_blocks_lab, não instalado com inde nem acessível
por uma nova barra no editor principal. INDE_BUILD_TEXT_SPIKE é OFF por padrão;
o candidato de avaliação configura ON explicitamente.
O lançador só oferece cópias preparadas, com marcador e schema já corrente;
a fachada de leitura não oferece salvar/atualizar/migrar. Leituras reutilizam
o repositório existente; não são uma nova conexão SQLite read-only certificada.

Exemplo sintético é puramente em memória. Origens reais da cópia têm paginação
de 50 e carregamento sob demanda. Dividir em três usa cortes por tamanho;
repetir/fixar/bifurcar/referenciar/alternativa/camada/ordem são explícitos.
Rascunho do bloco à esquerda e leitura derivada à direita. Confirmar revisão
afeta apenas memória; não autoriza escrita no Projeto. O impacto aparece antes
da confirmação. Experimento tem undo/redo limitado a oito comandos; digitação
do rascunho não é revisão confirmada. Trocar origem/reiniciar fecha esse histórico.
Limite gráfico inicial de origem 2 MiB; o CLI exercita escala maior.

Testes GTK exercitam vazio sem Documentos, cópia vazia com conteúdo de revisão
e Atlas. Com 100 mil palavras, medem inserção até after-paint no Xvfb privado.
São 12 amostras por backend/fixture; p99 nessa amostra é próximo do máximo,
não certificação estatística de sessão longa, IME real ou editor principal.
O backend não remove o custo de obter/materializar todo o buffer GTK.

## Gate e próximo corte

Relatórios outputs/editor-etapa5-spike-*.json no workspace de entrega e manifesto
contêm evidências finais. Aceite anterior registrado; este spike tem gate próprio.
ASan/UBSan instrumenta as quatro unidades novas e drivers, não núcleo anterior,
GTK ou dependências; LSan desligado. Tentativas falhas não contam como certificado.

Próximo corte E5-S2: fundação persistente de blocos/revisões/colocações, schema
dedicado e conversões explícitas com prévia/impacto; fechar antes a autoridade
por modo documental e política de transferência das camadas. Sem dupla escrita
editável Document.content/blocos. Não substituir GtkTextView com base apenas
em ganho de microbenchmark. Calendários configuráveis continuam após Etapa 5.
Sem importação gen7/HTML, novas fontes ou binding lexical automático.

Recomendação GPT-6.1 Sol/high para E5-S2 (migração/autoridade), mesma tarefa,
custo relativo alto. Medium basta para revisão humana/ajuste UX do laboratório.

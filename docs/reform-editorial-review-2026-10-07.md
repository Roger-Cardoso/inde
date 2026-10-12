# Reforma — revisão editorial, 2026-10-07

Base aceita: `1c5286fe12edd91014aef41d4c07352f31ddddce`. Aceite explícito:
“Aceite humano confirmado. Prossiga com a próxima grande implementação dessa
reforma na ordem que foi definida”. Registro adicional: no workspace de entrega,
`outputs/reforma-revisao-aceite-anterior-2026-10-07.json`. Os três cortes aceitos
e seus registros congelados não são sobrescritos; publicação não foi autorizada.

## Resultado e sequência

A próxima frente da ordem definida era revisão editorial, após a exportação
inicial de Documento. Este corte implementa fila, detalhe, comentários decidíveis,
sugestões ancoradas, decisões vinculadas ao conteúdo e comparação somente leitura.

Produção ordenada da Obra e formatos profissionais permanecem a frente seguinte.
Depois: impacto/reversão de operações do Projeto; Composição persistente, com
contrato de autoridade/recuperação antes de schema; aliases e calendário ficcional.
Este corte não declara a reforma inteira concluída.

## Superfície mínima e temas

Revisar tem três páginas mutuamente exclusivas: fila, detalhe e comparação.
Escrita não ganha painel permanente. A fila é paginada (50 Documentos), pesquisável
e cancelável, com filtros de pendências/editorial/todos/comentários/marcações.
Estados em revisão, ajustes solicitados e decisões desatualizadas entram na fila.
Um Documento incompatível exibe erro local, sem inferir aprovação nem interromper
a avaliação dos demais.

O detalhe pagina comentários/sugestões (30 itens). Identidade, histórico e
diagnósticos ficam recolhidos. Criar revisão e registrar estado estão no menu
Documento. A comparação usa uma única área de texto; identidades/datas das fontes
são consultáveis em “Identidade das fontes”, não uma segunda coluna de editor.

Temas usam os tokens compartilhados. Diferenças têm sinais −/+ e números de linha,
além de cores. − refere-se à base; + ao Documento examinado. Alterar o tema só
atualiza as cores das tags, sem reconstruir o texto ou perder a posição de leitura.
Janela compacta de 1000 px e contraste de texto das tags >= 4,5:1 entram no ensaio GTK.

## Fluxo operacional

1. Na Escrita, selecione um trecho e use Sugerir alteração no menu da seleção
   ou no menu Editar. Informe o texto proposto e, opcionalmente, o motivo.
   Texto proposto vazio significa remoção do trecho. Guardar não altera o corpo.
2. Em Revisar, abra o Documento. Resolva/reabra comentários ou examine uma
   sugestão. A prévia mostra observado/proposto. Cancelar não aplica nem decide.
   Recusar conserva a proposta; aplicar exige que a âncora esteja resolvida
   e que o trecho corresponda exatamente ao texto observado.
3. Voltar ao mesmo texto preserva a sessão e o undo. Aplicar/desfazer/refazer
   acompanha texto e estado da sugestão em uma transação de edição.
4. Documento → Registrar estado editorial permite Em escrita, Em revisão,
   Ajustes solicitados e Revisão aprovada. Responsável é obrigatório;
   aprovar exige decidir comentários e sugestões pendentes.
5. Mudanças autorais posteriores tornam a decisão desatualizada. O histórico
   conserva os registros anteriores; a interface não transforma isso em nova
   aprovação. O estado não publica, não bloqueia a escrita e não decide fatos ficcionais.
6. Documento → Criar revisão independente cria outro Documento. A fonte não
   é alterada. Copia texto, formatação, modelos inline, perspectiva livre,
   meta e organização. Não copia âncoras, referências de entidades, comentários,
   sugestões, decisões ou camadas semânticas — a prévia declara essa diferença.
7. Comparar versões exige escolher/confirmar a base. A origem de uma revisão
   pode ser pré-selecionada, mas não se inicia comparação nem merge automaticamente.
   Somente diferenças e navegação anterior/próxima são opcionais.
8. Espaços de trabalho guardam também página, Documento, pesquisa, filtro
   e par da comparação de Revisar. Ao reiniciar, a comparação é recalculada
   sobre as fontes atuais; uma identidade ausente leva à fila/detalhe disponíveis.
   Contextos v1 anteriores sem a chave de revisão continuam aceitos.

## Autoridade e confiabilidade

Decisões/sugestões são camadas editoriais autorais do Documento, não preferências.
Tipos nativos `inde.editor.suggestion@1` e `inde.editor.review-state@1`;
payload semântico 2, checkpoint 4 e clipboard 2 permanecem. Uma decisão global
não cria âncora artificial nem substitui a perspectiva tipada do Documento.

Cada decisão tem sequência, responsável, parecer, data e SHA-256 do estado
autoral revisado. O fingerprint exclui o próprio histórico de decisões e as
datas operacionais do Documento; inclui texto, formato, organização e camadas.
Sequências duplicadas, versões/escopos incompatíveis e campos ausentes não
produzem aprovação válida. Campos editoriais não podem ser alterados, apagados
ou arquivados pelo formulário semântico genérico; decisões explícitas ficam
em Revisar. O undo da sessão continua sendo uma operação autoral deliberada.

Uma gravação exige a fonte esperada ainda vigente. Se o Documento está aberto
na Escrita, a mudança passa pela mesma transação/undo, baseline e barreira de
autosave. Outro Documento usa a conferência de estado do repositório.
Falha de salvamento não é apresentada como sucesso; edição local permanece
na Escrita, com aviso e undo, sem sobrescrever a fonte concorrente.

A comparação calcula em worker sobre cópias observadas. Antes de exibir o
resultado, confere se as duas fontes ainda correspondem ao observado. Não
rotula diferenças antigas com metadados de uma fonte que mudou durante o cálculo.

O laboratório conserva sua fronteira experimental: não migra projetos e não
sobrescreve Documentos. Materialização não transmite aprovação ou sugestão
editorial da origem à montagem; avisa a omissão. Composição continua volátil.

## Migração necessária: schema 18 → 19

O schema 18 reservava o escopo global exclusivamente à perspectiva v1 e mantinha
índice único para qualquer anotação global. O 19 admite também eventos editoriais
v1 sem âncora e conserva a unicidade exclusivamente da perspectiva.

A migração reconstrói a projeção de anotações e suas três tabelas dependentes
(entidades, relações e tempo ficcional) na mesma transação, com cópias temporárias
sem FKs antes de remover as projeções antigas. Não desliga FKs. Restaura índices
e os três gatilhos de ordem temporal. Payloads autorais permanecem intactos.
Não há conversão de decisões antigas inferidas.

Projetos abertos por este novo editor migram para 19. Binários dos cortes
anteriores não abrem bancos 19: use as cópias independentes fornecidas para
avaliação. O lançador e o preparo da entrega usam novas cópias marcadas,
verificadas e no schema 19, com configuração própria. As cópias e binários
aceitos anteriormente permanecem disponíveis no estado original.

## Limites explícitos

- Comparação é textual e somente leitura, não merge, paginação editorial,
  diff visual de estilos ou história imutável do Documento. Avisa quando
  metadados/camadas diferem.
- Até 2 MiB de texto combinado na comparação visual; exceder recusa, não trunca.
  LCS tem orçamento de 2 milhões de células/20 mil linhas; acima usa blocos
  delimitados por prefixo/sufixo comuns e declara o modo.
- Campos de proposta/parecer têm limite de 4096 bytes UTF-8 conforme o payload
  semântico existente. Sugestão não é substituição global nem automação.
- Revisão independente é outro Documento com origem opcional, não snapshot
  imutável. Comparar observa os estados atuais. A remoção de sua fonte segue o
  contrato anterior de origem opcional; proteção/impacto global ainda é frente posterior.
- Resolver comentário não reancora a marcação. Aplicar sugestão pode deixar
  marcações afetadas ambíguas/órfãs; nenhuma reassociação silenciosa acontece.
  Aprovação editorial não certifica toda a modelagem semântica do Projeto.
- Sem colaboração multiusuário, importação de comentários externos, DOCX/EPUB/PDF
  da Obra completa ou Composição persistente neste corte.

## Verificação

Verificação final passou nos mesmos binários Release do editor e laboratório:

- 18/18 CTest; execução final de 40,35 s, sem falhas.
- 28 cenários GTK em Atlas/Projeto vazio, incluindo quatro reinícios reais.
- 12 cenários suplementares iniciados no tema escuro, incluindo quatro reinícios.
- Comparação em janela de 1000 px, leitura das tags de diferenças e contraste
  >= 4,5:1 em claro/escuro; capturas visuais conferidas.
- Parser independente de HTML/TXT; readback da exportação inicial preservado.
- ASan/UBSan focal de revisão e de continuidade/exportação; dependências do
  núcleo em Release, LSan desligado. Não certifica toda a aplicação GTK,
  sessão de escrita de 45 minutos, todos os IMEs ou todo hardware gráfico.
- Integridade/FKs e preservação das linhas autorais anteriores das fixtures;
  migração, rollback tardio e convivência entre perspectiva/histórico editorial.

Evidências no workspace de entrega: `outputs/reforma-revisao-verificacao-2026-10-07.json`,
`outputs/reforma-revisao-tema-escuro-2026-10-07.json` e
`outputs/reforma-revisao-testes-nucleo-2026-10-07.log`.
O manifesto registra SHA-256 dos binários e os cortes aceitos anteriores.
Aceite humano deste novo bloco permanece pendente; não houve publicação.

Correções encontradas nos ensaios: o laboratório ainda tinha uma conferência
fixa de schema 18 ao materializar Documentos; passou a usar a versão corrente,
sem ganhar autorização para migrar projetos. A regressão de continuidade
também revelou uma disputa intermitente entre restauração de rolagem e
scroll-to-insert tardio do GTK. A restauração agora observa o valor e a geometria
do ajuste durante vários ticks, com prazo limitado e cancelamento por teclado
ou clique. O ensaio de reabertura provoca deliberadamente um scroll-to-insert
tardio para reproduzir essa disputa. Os logs das tentativas anteriores ficam
preservados em `work/reforma-2026-10-07/logs/`, além da verificação final.

## Passe humano deste corte

Use `outputs/inde-revisao-abrir.sh` no workspace de entrega (atlas/vazio ou
laboratorio-atlas/laboratorio-vazio). Na nova cópia, crie uma proposta, examine
a prévia, cancele, aplique e volte ao texto; confirme undo/redo e legibilidade.
Registre uma decisão, altere o texto e confirme “decisão desatualizada”.
Crie uma revisão independente, modifique-a, compare e retome um espaço salvo.
Confira os avisos de camadas não copiadas e teste os dois temas com seu texto real.

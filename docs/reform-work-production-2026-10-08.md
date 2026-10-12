# Reforma — Produção da Obra, 2026-10-08

Quinto bloco, sobre a revisão editorial aceita no commit
`ace99635cb84db45cb401758664f6c17ff83bf29`. O aceite explícito do usuário está
registrado separadamente em `outputs/reforma-producao-aceite-anterior-2026-10-08.json`.
Os quatro manifestos e binários anteriores permanecem congelados, com o estado
histórico da entrega. Aceite anterior não aprova automaticamente este corte.

## Resultado e ordem

Produção deriva uma saída da Obra a partir da estrutura editorial ativa e dos
Documentos expressamente escolhidos. Entrega DOCX, EPUB refluível, PDF, HTML,
TXT e relatório de fontes, omissões, perdas e hashes. Dois perfis: Manuscrito
para avaliação e Leitura. Não publica, não cria Composição persistente, não
altera a ordem canônica e não escolhe silenciosamente qual revisão deve entrar.

A próxima frente da ordem aceita é impacto e reversão das operações do Projeto,
distinta do undo textual. Depois, Composição persistente, precedida por contrato
de autoridade/recuperação antes de schema; depois aliases e calendário ficcional.
A reforma inteira ainda não está concluída. Esta produção básica não representa
diagramação comercial, todas as exigências de editoras ou todas as formas de livro.

## Superfície mínima e temas

Manuscrito oferece “Produzir obra…”, sem nova tarefa fixa no shell. Seleção e
prévia ocupam páginas alternativas, nunca duas áreas competindo com a Escrita.
Escolher textos abre diálogo temporário; cancelar conserva a seleção anterior.
Fontes, omissões e perdas ficam recolhidas e, quando abertas, têm altura limitada.
Não acrescenta painel permanente, dashboard ou editor paralelo.

Utiliza os mesmos tokens de Claro/Escuro/Sistema. Texto de prévia é somente
leitura e herda as cores do editor; escolha/omissão também têm rótulos explícitos,
não apenas cores. A lista de unidades pagina em 30 itens; a busca de candidatos
de uma unidade pagina em 50, com ordenação dos escolhidos por subir/descer/retirar.
Prévia e saída são coisas diferentes: a prévia textual não finge mostrar a
paginação final de DOCX/PDF nem a refluência do leitor EPUB.

## Fluxo operacional

1. Abra uma Obra em Manuscrito e use Produzir obra. Escolha o perfil, nome
   autoral opcional e formatos DOCX/EPUB/PDF. HTML, TXT e relatório sempre entram.
2. Em cada unidade, Escolher textos permite incluir ou omitir seu título e
   adicionar os Documentos na ordem desejada. Mesmo um único candidato exige
   escolha explícita. Esboços, referências e revisões independentes não entram
   por nome, propósito ou decisão editorial inferida. Sem título e sem textos
   significa omitir a unidade; só títulos não bastam para produzir uma Obra.
3. A ordem das unidades vem da árvore editorial ativa: pais antes dos filhos,
   posição entre irmãos. Posições empatadas, ciclos, pais ausentes, unidades
   incompatíveis ou fontes ausentes são recusados; não se desempata por UUID.
   Para mudar essa ordem, volte à organização da Obra, não à lista de saída.
4. Preparar prévia copia as fontes atuais. Confira os textos, sua ordem, nomes,
   datas, omissões e estilos substituídos. Uma decisão editorial ausente ou
   desatualizada gera aviso; Produção não impõe aprovação como requisito e não
   registra uma aprovação nova. Todas as limitações precisam ser reconhecidas.
5. Escolher destino abre a janela de pasta. O destino deve ser externo ao
   Projeto; será criada uma pasta nova `INDE-obra-<UUID>`, nunca sobrescrita de
   produção anterior. Cancelar o seletor não grava; cancelar a geração solicita
   interrupção. Pasta parcial não é entrega concluída.
6. Os formatos são gerados em worker a partir da cópia observada, sem serviço
   de Projeto no worker. O relatório identifica o momento e as fontes desse
   snapshot; alterações posteriores não mudam a saída já preparada. Voltar à
   seleção e preparar novamente observa as fontes atuais.
7. A interface confirma sucesso somente depois da escrita exclusiva, releitura
   exata e sincronização. Os arquivos de conteúdo têm SHA-256, tamanho e fontes
   no relatório; o próprio relatório também passa por releitura e sincronização.
   Falhas de escrita/sincronização são avisadas; arquivos parciais são preservados
   para diagnóstico, não rotulados pela interface como conclusão.
8. Espaços de trabalho guardam IDs, ordem escolhida e opções, não corpos ou
   arquivos produzidos. Reabertura exige nova prévia e novo reconhecimento das
   perdas. Uma estrutura ativa diferente exige confirmar nova seleção; nada
   restaura uma aprovação ou saída antiga silenciosamente.

## Formatos e perdas

DOCX usa WordprocessingML com estilos Normal/Title/Subtitle/Heading1–6,
parágrafos, tabulações, cabeçalhos hierárquicos, página Letter e margens de uma
polegada. Fonte declarada Liberation Serif 12 pt, preto; entrelinha dupla para
avaliação, 1,5 para leitura. Numeração de página usa campo PAGE no rodapé,
dependente da atualização de campos pelo aplicativo leitor. Existe página inicial
de identificação, não capa comercial. Fontes podem ser substituídas pelo leitor.

EPUB tem pacote, navegação, ordem de leitura, identificação e idioma; mimetype
primeiro e sem compressão, XHTML e CSS locais. Perfil declara dupla/1,5, mas o
leitor pode mudar fonte, espaçamento e quebras. Sem JavaScript, recursos remotos,
imagens, DRM ou incorporação de fontes. Pacote `version="3.0"`; validação pelo
EPUBCheck oficial, não promessa de acessibilidade universal de todos os leitores.
Referências técnicas: [EPUB](https://www.w3.org/TR/epub-33/),
[EPUBCheck](https://github.com/w3c/epubcheck) e
[WordprocessingML](https://learn.microsoft.com/en-us/office/open-xml/word/structure-of-a-wordprocessingml-document).

PDF usa Pango/Cairo local, Letter, margens de uma polegada, corpo serifado
12 pt e páginas numeradas. Caracteres sem fonte representável recusam PDF,
com aviso para instalar a fonte ou escolher outros formatos; não gera quadrados
silenciosamente. A inspeção visual encontrou uma escala 96 DPI inadequada para
coordenadas PDF em pontos, corrigida para 72 DPI antes da entrega. Parágrafos
curtos são mantidos juntos; parágrafos maiores que a área da página podem
continuar na seguinte. Não há controle tipográfico completo de viúvas/órfãs
para esses parágrafos longos. Não é PDF/A,
PDF/UA, PDF etiquetado, paginação de livro ou preparação para impressão comercial.
Extração/cópia do PDF depende das heurísticas do leitor, principalmente com fontes
de fallback para emoji; não promete texto extraído idêntico ao corpo autoral.
O ensaio de Unicode usa ordem do fluxo de conteúdo e normaliza espaços, além
da conferência visual. TXT continua sendo o formato com corpos exatos.

DOCX/EPUB/PDF/HTML conservam quatro estilos inline: negrito, itálico, sublinhado
e tachado, inclusive sobrepostos. Todo outro estilo autoral é substituído pelo
perfil e declarado por Documento. Formatos profissionais normalizam CRLF/CR
para LF e conservam parágrafos vazios. TXT conserva os corpos UTF-8 exatos,
mas acrescenta identificação, títulos escolhidos e separadores: não é backup.
Camadas semânticas, comentários, sugestões, decisões, âncoras, vínculos e comandos
não passam ao texto produzido. Nada transmite aprovação ou autoridade canônica.

## Limites e confiabilidade

- Preparação gráfica: até 1000 unidades, 100 textos escolhidos por unidade,
  500 candidatos por unidade e 10000 Documentos observados. Seleção salva tem
  orçamento de 24 KiB dentro do contexto de interface de 64 KiB. Excessos são
  recusados com mensagem; não existe truncamento silencioso.
- Texto escolhido: até 8 MiB somados. Núcleo admite árvore de até 10000 unidades
  e profundidade 64; essas capacidades não ampliam os limites da interface.
- Pacotes ZIP32 sem compressão, limitados a 128 MiB; PDF também limitado a
  128 MiB. Não há importação DOCX/EPUB, notas de rodapé, imagens, tabelas,
  estilos profissionais configuráveis ou geração de livro sob medida neste corte.
- Gravação exige pasta absoluta nova, rejeita componentes simbólicos e usa
  criação exclusiva dos arquivos. Pastas são privadas e arquivos têm permissão
  somente do usuário. Não é serviço multiusuário resistente a ataques de corrida
  de diretórios, transação do sistema de arquivos ou garantia contra hardware
  defeituoso. Releitura exata e fsync são medidas locais, não garantia absoluta.
- Captura de fontes é síncrona e limitada; a geração é assíncrona/cancelável.
  Não se afirma benchmark universal de obras enormes, todas as fontes Unicode
  ou todas as versões de Word/LibreOffice/leitores EPUB. Fechar o Projeto solicita
  parada e aguarda encerrar o worker para não deixar acesso a objetos destruídos.
- Plano de Produção é preferência de workspace. Não ocupa tabelas autorais ou
  substitui o futuro contrato de Composição. Schema permanece 19, payloads
  semântico 2/checkpoint 4/clipboard 2; nenhum original é migrado pela preparação
  das cópias de revisão.

## Verificação e aceite

Validação técnica no código final:

- Release: 19/19 CTest, 33,99 segundos no registro final do núcleo.
- Bateria completa: 32 cenários GTK em Atlas e Projeto vazio, incluindo seis
  reinicializações. Suplemento do tema escuro: 16 cenários, incluindo seis
  reinicializações, nos mesmos hashes dos binários principal e laboratório.
- Oito cenários focais finais de continuidade/produção, incluindo quatro
  reinicializações, passaram com o ensaio de rolagem sincronizado descrito abaixo.
  A repetição geral adicional foi interrompida pelo encerramento do terminal
  (código 143) perto do fim e não é contada como uma bateria concluída. A bateria
  completa anterior de 32 casos e os dois suplementos concluídos são evidências
  separadas; não há promoção de resultado parcial a aprovação.
- Três conjuntos ASan/UBSan focais: continuidade, revisão e produção. Dependências
  Release, LSan desligado; não é instrumentação de toda a aplicação GTK.
- Dois perfis passaram por leitura independente de HTML/TXT, estrutura e texto
  de DOCX/EPUB, limites tipográficos/texto do PDF e readback com hashes. Ambos
  os EPUBs passaram pelo EPUBCheck oficial 5.4.0 sem erros ou avisos. Não se
  confunde validação de pacote com aprovação universal de leitores.
- Todas as 21 páginas finais das amostras DOCX/PDF foram inspecionadas visualmente
  pelo agente. Quatro páginas adicionais de PDF gerado pela interface real também
  foram inspecionadas; Unicode, emoji e ordem dos corpos foram reconferidos nos
  arquivos de Atlas e Projeto vazio contra suas fontes SQLite.
- Preservação: os 402 arquivos não ignorados do candidato original foram
  comparados com o arquivo de resgate; binário original e quatro entregas
  anteriores mantêm seus hashes. Esta comparação não afirma cobertura de
  arquivos ignorados, dados externos ou autoridade de backup de produção.

Evidências, documentação, binários e código serão congelados no manifesto desta
entrega. Aceite humano deste novo corte permanece pendente e separado do aceite
dos quatro anteriores. Publicação não autorizada.

O ensaio ampliado também identificou uma disputa intermitente de retomada da
rolagem do bloco anterior: a seleção podia consumir um quadro inteiro e adiar
a aplicação da posição disponível. A correção aplica a posição imediatamente
após selecionar e continua conferindo os ajustes de foco/layout posteriores.
Eventos de teclado, clique e agora roda/gesto de rolagem interrompem a retomada
automática. O ensaio mantém a perturbação deliberada de scroll-to-insert e
acrescenta a verificação de que a rolagem manual não é sobrescrita. A janela
nativa de destino também libera sua referência após responder, sem reter
cada seletor fechado em produções sucessivas.
Uma abertura síncrona longa também podia deixar o timer periódico do ensaio já
vencido: a cobrança às vezes acontecia antes da perturbação deliberada. O ensaio
final exige a execução desse evento e sua acomodação, com limite de quatro
segundos medido após a reabertura. Continua conferindo o ajuste real, não apenas
uma preferência salva. Retomada tem prioridade sobre leituras/gravações de contexto
que ficaram prontas no mesmo ciclo do GTK, com callbacks limitados e intervalos.

Passe humano sugerido: no Projeto vazio, criar Obra e duas unidades, escrever
dois Documentos numa unidade, escolher só um e produzir; no Atlas, selecionar
fontes conscientemente e abrir DOCX/EPUB/PDF; repetir no tema escuro; guardar e
retomar o workspace; testar cancelar antes de gravar. Confirmar que a superfície
permanece simples, as omissões são claras, o texto é legível e as perdas aceitáveis.

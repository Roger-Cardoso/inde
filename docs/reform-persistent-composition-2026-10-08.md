# Reforma INDE — sétimo bloco: Composições persistentes e ajustes do editor

## Continuidade e escopo

Base aceita pelo usuário: `bf05f632c318f3d481bb41f382c7df41ad2905c5`, sexto
bloco. A autorização registrada em `reforma-composicao-aceite-anterior-2026-10-08.json`
inclui os três reparos e a próxima frente da ordem: Composição persistente.
Esta entrega exige seu próprio aceite humano; não publica nem encerra a reforma.
As seis entregas anteriores e o candidato original permanecem separados.

O contrato de autoridade foi registrado antes da migração, em
`composition-authority-contract-2026-10-08.md`. Documento não se transforma
silenciosamente em bloco. Composição é um agregado novo; importar copia texto
e camadas, mantém a origem inteira congelada e não modifica o Documento.
`live` acompanha o bloco, não futuras edições da origem. O laboratório volátil
continua separado; apenas seu motor puro foi reutilizado.

## Os três reparos solicitados

- Clique direito sobre uma seleção oferece “Consultar seleção no dicionário…”.
  A escolha abre a consulta com o termo selecionado e executa a busca local.
  O item fica desabilitado sem seleção, para seleção multilinha ou acima de
  256 bytes UTF-8. A consulta permanece somente leitura, sem binding ou acepção
  escolhida automaticamente. Consulta indisponível não vira resultado inventado.
- O menu de contexto pinta somente seu conteúdo. O contêiner externo e a
  viewport ficam transparentes, sem segunda borda/sombra; a lista rola dentro
  de um limite de altura. A seleção mantém destaque mesmo sem foco no editor.
  O menu é ancorado após a linha selecionada, inclusive pelo teclado, em vez
  de nascer sobre a própria palavra. A consulta corrige também o limite de
  texto do SearchEntry nesta combinação GTK/gtkmm, que antes gerava erro GTK.
- “Comandos inline” contém modelos literais e ajuda específica, não uma cópia
  de todas as ferramentas do editor. Rótulos extensos abreviam em uma linha na tela,
  mantendo nome acessível completo. A área rolável reserva o espaço medido do
  cabeçalho e rodapé, para o botão Fechar não cobrir os últimos itens.

## Como trabalhar com uma Composição

1. Em **Escrever → Biblioteca de Documentos → Composições…**, dê um nome e crie.
2. Importe um Documento inteiro ou selecione o trecho na prévia da origem.
   Texto e camadas dessa origem são congelados juntos, inclusive a organização.
3. Abra uma ocorrência. Digitar salva um rascunho após a pausa e na barreira
   de navegação/fechamento; a leitura continua usando a versão aplicada.
4. Escolha **Aplicar versão do bloco** ou **Descartar rascunho**. Reiniciar
   recupera rascunhos sem aplicá-los automaticamente.
5. No menu da Composição/ocorrência, reorganize, reutilize, fixe uma versão,
   torne independente, oculte ou retire uma ocorrência. Referência sem texto
   guarda presença na montagem sem inserir corpo na leitura.
6. Divida no cursor ou separe uma seleção Unicode na versão editável, depois
   de aplicar/descartar o rascunho. Crie alternativas e camadas de leitura;
   “Alternativas e camadas” escolhe quais entram na projeção.
7. “Ver origem congelada” mostra a fonte histórica integral, mesmo se o
   Documento original for alterado ou removido posteriormente.
8. “Ler Composição” é uma projeção textual somente leitura. “Preparar saída
   independente…” apresenta texto, avisos e proveniência. Só após conferir
   ficam disponíveis criar Documento independente ou exportar a leitura.

Lista, ordem, edição, leitura, importação, origem e saída usam a mesma área,
uma página por vez. Não acrescenta workspace principal, inspetor fixo ou
vários editores lado a lado. A navegação salva apenas IDs, página e seleção;
texto e aprovação nunca são guardados em preferências. Importação e aprovação
de saída precisam ser preparadas de novo ao restaurar um contexto.

## Conservação e conflitos

Revisões aplicadas e origens congeladas são imutáveis na persistência.
Rascunhos ficam fora da leitura e da produção. Aplique/Descarte é explícito;
preparar saída com qualquer rascunho é recusado. Falha ao salvar não libera a
barreira de saída nem alega que texto ainda em memória está seguro no disco.
Recarregar exige confirmação para descartar apenas alterações locais não salvas;
rascunhos já persistidos continuam no Projeto.

Revisão e SHA-256 são conferidos juntos, incluindo o caso de um número de
revisão reaparecer depois de reversão de operação (ABA). A saída é derivada
novamente da receita validada, sem confiar em um corpo forjado pelo chamador.
Na materialização, Documento e proveniência são gravados na mesma transação:
uma falha no registro da receita desfaz a criação inteira. A confirmação da
saída não sobrevive a mudança, recarga ou reinício.

Cortes Unicode recortam estilos por interseção e trasladam âncoras contidas.
Camadas que atravessam cortes permanecem na origem congelada, acompanhadas de
avisos; não são reassociadas em silêncio. Edições usam o motor de conservação
de camadas, sem prometer que referências históricas ainda existem no Projeto.
Se uma entidade canônica conservada no bloco deixar de existir, a validação
da materialização recusa o vínculo inválido. Ainda não há editor dedicado de
camadas do bloco para resolver esses casos; a origem e a leitura continuam
consultáveis. Não se deve prometer remediação completa nesta primeira frente.

HTML, TXT e relatório de conservação usam o fluxo de exportação existente:
pasta nova, sem sobrescrever, leitura de volta e sincronização. A receita é
conferida antes da exportação; o arquivo produzido é um snapshot independente,
não um vínculo que acompanha alterações concorrentes posteriores. Estilos e
camadas são preservados no agregado/materialização; as prévias são textuais,
não uma implementação nova de WYSIWYG ou paginação de livro.

## Migração e limites

Schema **21** acrescenta `compositions` e `composition_outputs`. Não muda
payload semântico 2, checkpoint 4 ou clipboard 2. Migração aditiva e
transacional, com chaves estrangeiras ligadas. Salvar como copia agregado e
receitas para o novo dono, preservando os snapshots históricos. Operações
novas de Composição e produção entram no histórico do Projeto; debounce de
rascunho não polui esse histórico. Não é um backup nem histórico universal.

O agregado inteiro admite **2 MiB**, incluindo camadas, revisões, origens e
rascunhos: até 256 blocos, 2048 ocorrências, 4096 revisões, 32 alternativas e
32 camadas. Falha fechado, sem truncar ou podar revisões automaticamente.
Não há remoção definitiva de bloco/Composição nesta UI; retirar uma ocorrência
não destrói sua história. Ainda não há importação de estado volátil do laboratório,
mesclagem automática, perfis nomeados múltiplos nem adoção automática de origens.

As cópias de avaliação desta entrega são novas e migram para schema 21.
Executáveis anteriores devem usar suas próprias cópias; não abrir uma cópia
migrada com executável antigo. Os Projetos autorais e candidatos anteriores
não são migrados durante a validação.

## Verificação e frentes restantes

Os artefatos de verificação da entrega registram a compilação exata, testes de
núcleo, cenários GTK de Atlas e Projeto vazio, reinício real, temas, capturas
visuais, sanitizadores focais e comparação de preservação. Evidência técnica
não substitui seu aceite de uso, principalmente a impressão de minimalismo.

IME físico, tecnologias assistivas, HiDPI, bases grandes e sessões longas
continuam gates próprios. Sanitizadores focais não certificam todas as
dependências GTK. Próxima frente da ordem: aliases/calendários e demais itens
do plano; esta entrega não começa essa etapa nem considera a reforma concluída.

# Reforma INDE — sexto bloco: impacto e reversão de operações do Projeto

## Continuidade e autorização

Base aceita: quinto bloco, commit local
`0d642709703f860ef135559409dbedfbf15c5185`. O usuário confirmou o aceite
humano e autorizou o próximo bloco da ordem registrada. O registro separado
`outputs/reforma-operacoes-aceite-anterior-2026-10-08.json` conserva essa
autorização sem reescrever os manifestos históricos. Esta entrega tem aceite
humano próprio pendente; não é publicação nem conclusão da reforma inteira.

## O problema corrigido

Desfazer a digitação não deve fingir que desfez uma entidade ou relação
canônica criada a partir do texto. Excluir uma estrutura tampouco deve
prometer restaurar tudo quando só consegue devolver os nomes das unidades.
Esta frente acrescenta um histórico de transações do Projeto e uma prévia
dos efeitos reais, com confirmação explícita e recusa de conflitos.

Não acrescenta workspace principal, inspetor fixo, docking ou barra contínua.
Menu do aplicativo → Arquivo → Histórico de operações do Projeto abre uma
janela temporária. Lista e detalhe ocupam a mesma área, nunca duas colunas
concorrentes. O atalho de restauração do Manuscrito abre esse mesmo fluxo,
passando pela barreira de salvamento e pela atualização das áreas afetadas.
Usa as cores e os estilos compartilhados dos temas claro/escuro, os controles
acessíveis e a navegação modal existentes.
O ciclo de Tab ignora páginas inativas da pilha, para não prender o foco
numa lista oculta ao examinar o detalhe de uma operação.

## Uso

1. Uma remoção coberta abre sua prévia: identificação, categorias, quantidades
   e até três exemplos por categoria. Atualizações mostram campos afetados em
   linguagem de trabalho, sem expor o corpo completo do Documento.
2. Cancelar/fechar não confirma a remoção. Digitação pendente pode ter sido
   salva pela barreira anterior; isso é salvamento normal, não uma exclusão.
3. Conferir os efeitos habilita a confirmação. Atualizar a prévia limpa esse
   reconhecimento. O programa confere novamente os dados na transação real.
4. O histórico mostra operações registradas, revertidas e reversões
   compensatórias. Há 25 registros por página, sem carregar toda a fila.
5. Abrir um registro prepara a reversão. Confirmar produz uma nova transação
   compensatória, marca a original como revertida e retoma a navegação usando
   fontes atuais. Não recoloca um buffer antigo por cima do texto persistido.

Uma operação já revertida não pode ser revertida de novo. Esta frente não
implementa refazer automático do Projeto. Reconhecimento e prévia não são
persistidos como autorização; cada confirmação exige observação atual.

## Cobertura, sem prometer um histórico universal

O registro começa nas operações novas desta versão. Os pontos de captura
estão nas transações de catálogo de Obras/universos, entidades/tipos/relações,
vínculos narrativos, unidades editoriais, modelos/alternativas/ativação,
tempo ficcional/ocorrências e criação/organização/remoção de Documentos e
grupos. Criação de entidade a partir do texto e criação de relação com
contexto são registradas como transações compostas, não operações soltas.

Prévia de remoção integrada diretamente à interface: Documento, grupo,
entidade, relação, Obra, universo e ramificação editorial. Os demais comandos
cobertos mantêm seus formulários atuais e podem ser examinados/revertidos
depois pelo histórico. Não foi uniformizada a confirmação de todos os
subsistemas neste bloco.

Autosave de texto/formatação/camadas sem mudança organizacional não gera
registro de Projeto. A interface salva a digitação antes de abrir a edição
de organização. Se uma chamada composta da API alterar texto e organização
na mesma transação, o histórico registra os efeitos efetivos dessa transação,
inclusive texto; a prévia declara o campo afetado. Não se deve descrevê-la
como um undo exclusivamente de metadados.

No Documento existente, os gatilhos organizacionais deste corte são colocação
editorial, grupo, finalidade, origem e rótulo de revisão. Título, perspectiva
e meta de palavras isolados continuam fora do histórico de Projeto. Não se
deve interpretar a cobertura inicial como reversão de todo campo da interface.

Operações cartográficas autônomas continuam com seu mecanismo próprio;
efeitos cartográficos decorrentes de uma transação coberta são conferidos.
Arquivos externos, imagens, exportações, manifestos, preferências, recuperação
textual e dados fora dessas transações não são restaurados por este histórico.
Não há retroconversão de change_log nem botão novo de arquivamento.

## Proteções de fonte e integridade

Documento usado como origem de revisão ou referência textual não pode ser
removido pelo fluxo direto nem pela API correspondente. Entidades em uso
também são protegidas: a mensagem identifica a categoria do uso a desvincular.
Nenhum vínculo é apagado silenciosamente por essas remoções diretas.

Reverter a criação de um Documento vazio depois que ele ganhou texto é
bloqueado. Reverter uma criação com dependentes novos também é bloqueado.
Alterações posteriores em campos afetados impedem a reversão; alterações em
outros campos podem permanecer. Exemplo testado: devolver um grupo removido
restaura a associação sem eliminar uma edição textual posterior.

Remover e restaurar uma ramificação nova devolve unidades, hierarquia e
colocação dos Documentos, além dos demais efeitos capturados. O caminho
compatível do repositório também usa o registro novo quando disponível.
Lixeiras anteriores ao schema 20 não contêm os vínculos que já perderam:
a API legada continua existindo, mas não há restauração completa retroativa
nem sua execução automática pelo atalho novo da interface.

Salvar como preserva o histórico como proveniência, mas a identidade da
captura continua sendo a do Projeto original. Registros herdados são visíveis
e não reversíveis na cópia; operações feitas na cópia podem ser revertidas
normalmente. Copiar bytes para uma avaliação, sem criar outro Projeto, não
troca a identidade autoral.

## Contrato técnico e migração

Schema 20 acrescenta `project_operations`, índices e a identificação da
exclusão estrutural associada. Não muda os formatos semântico 2, checkpoint 4
ou clipboard 2. A migração é transacional, com chaves estrangeiras ligadas.
Executáveis antigos não devem editar uma cópia já migrada: usar as cópias
separadas das entregas anteriores para comparação.

A captura usa SQLite Session Extension. A compilação exige essa extensão e
PREUPDATE_HOOK; não degrada silenciosamente para um histórico sem reversão.
O conjunto de alterações guarda valores anteriores/novos, identidade e
SHA-256. É informação autoral local, não criptografada e não um backup.
Somente o rótulo visual é abreviado em Unicode, sem cortar os efeitos nem
os valores canônicos. Nomes extensos de participantes não impedem a reversão;
o teste do núcleo exerce esse limite com nomes Unicode longos.

A prévia roda numa simulação privada em memória, dentro de uma observação
consistente do banco. Antes da aplicação, o inverso verifica schema, chaves,
presença e valores dos campos afetados. Chaves estrangeiras nunca são
desligadas. Cascatas locais previstas são toleradas somente após essa
conferência; o efeito final normalizado deve coincidir exatamente com o
inverso esperado. Efeitos extras, conflito de identidade, hash incompatível,
schema divergente ou falha ao registrar o histórico cancelam a transação.

Se a transação já confirmou e a recarga de caches falhar, o erro distingue
“confirmado; reabra o Projeto” de “não confirmado”. Evita repetir uma
operação que já persistiu.

Limites fechados, sem truncar efeitos: 8 MiB por changeset; 20 mil registros
por operação; 128 MiB de memória de captura; banco de até 256 MiB para a
simulação privada; até 1 milhão de deslocamento na paginação. O histórico
acumulado não tem poda automática nesta frente. Simular banco grande é
síncrono: tarefas extensas e latência percebida continuam um gate de produto.
Falhar pelo limite conserva os dados, mas não resolve escalabilidade.
A memória da captura é conferida ao extrair os efeitos, não é um teto rígido
de alocação durante a operação. Esse orçamento não substitui ensaio de pico
de memória nem trabalho posterior de redução do custo de prévias grandes.

Referências primárias do mecanismo:
[SQLite Session Extension](https://www.sqlite.org/sessionintro.html),
[aplicação e conflitos](https://www.sqlite.org/session/sqlite3changeset_apply.html),
[inversão](https://www.sqlite.org/session/sqlite3changeset_invert.html).

## Verificação e revisão humana

A entrega selada identifica o commit, os binários, o patch/bundle, as cópias
mutáveis de revisão e os hashes das evidências. O núcleo exerce cancelamento
sem mutação, conflito, novos usos, transações compostas, restauração completa,
ativação alternativa, falha no próprio journal, paginação, identidade de cópia,
reinício, hash e rollback da migração.

O ensaio GTK usa objetos da interface real, Projeto vazio e Atlas em cópias
descartáveis, configuração isolada e display privado. Confere a remoção,
cancelamento, atualização de prévia, rejeição de conflito, reversão, temas,
paginações, retomada do editor e proteção de fonte; inclui reinício de processo.
Sanitizadores focais não equivalem a certificação de toda a aplicação GTK.
IME físico, tecnologias assistivas, HiDPI e sessão longa exigem avaliação
humana própria. Contagens e resultados finais constam nas evidências, não
devem ser inferidos a partir de compilação ou de um piloto.

Resultado do candidato final congelado:

- Release: 20/20 testes CTest passaram, em 46,28 s.
- Interface: 36 cenários GTK passaram nas duas bases, incluindo os 14 fluxos
  e quatro reinícios por base, com `G_DEBUG=fatal-warnings`.
- Tema escuro: mais dez cenários passaram, separados da cobertura completa.
- ASan/UBSan focal do núcleo: testes e três unidades de persistência
  instrumentadas; demais dependências Release, LSan desativado.
- ASan/UBSan focal da interface: quatro cenários passaram, com o driver e
  três unidades de UI instrumentados; demais objetos Release, LSan desativado.
- Leitura de exportações: passou separadamente; o teste negativo de
  preferências corrompidas emite aviso esperado e usa `fatal-criticals`.
- Inspeção visual: 12 capturas finais de remoção, reversão, lista, proteção
  de origem e reinício foram examinadas, nas duas bases e nos dois temas.
  Display privado 1280×900, janela 1000×720; sem recorte ou edição das imagens.

O manifesto vincula essas evidências aos hashes do executável principal e do
laboratório, sem misturar resultados de pilotos anteriores. A inspeção não
certifica contraste numérico de todos os controles. Dois refinamentos de
apresentação permanecem explícitos: a paginação fica no corpo rolável da lista,
e alguns rótulos compensatórios repetem o nome do alvo. Não impedem a reversão,
mas podem ser simplificados numa rodada de acabamento.

Para aceitar: criar grupo e Documento; remover o grupo e reverter, mantendo
uma edição textual posterior; remover/restaurar um Documento com formatação;
remover/restaurar uma ramificação com textos colocados; criar entidade de um
trecho e conferir a reversão; provocar um uso novo e verificar o bloqueio;
reiniciar e conferir que não há reconhecimento herdado. Repetir claro/escuro.

## Próxima frente da ordem

Composição persistente, com contrato de autoridade, conversão Documento↔bloco,
âncoras/cortes, camadas, recuperação, origem preservada e exportação antes de
qualquer migração. A seleção de produção entregue no quinto bloco não deve
ser renomeada como Composição persistente. Aliases e calendário continuam
depois dessa frente, não implementados antecipadamente aqui.

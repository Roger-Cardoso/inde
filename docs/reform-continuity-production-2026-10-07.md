# Reforma — continuidade e saída inicial, 2026-10-07

Base: `f8c0375aa0c5d83ac5f1ea636f2800d01dd73192`.
O usuário confirmou o aceite humano dos dois blocos anteriores e autorizou
iniciar as frentes posteriores. O registro adicional da entrega é
`outputs/reforma-aceite-humano-2026-10-07.json`. Os manifestos anteriores
permanecem congelados, com seu estado histórico. Este novo bloco não herda
aceite humano antecipado nem autorização de publicação.

## Continuidade e espaços de trabalho

O contexto da interface é versionado e vinculado à identidade do Projeto,
fora do SQLite autoral. A cada mudança de tarefa, salvamento e encerramento,
e periodicamente se houver mudança de estado, o contexto é guardado nas
preferências locais. Escrita também guarda a posição por Documento.

- Escrita: Documento/Biblioteca, seleção e cursor em offsets Unicode,
  rolagem vertical, pesquisa, grupo e filtros de colocação, finalidade,
  perspectiva, revisões e entidade.
- Planejar: área, entidade/ponto aberto, busca, escopos/facetas, página,
  consultas de relações e rolagem do explorador/relações.
- Manuscrito: obra, unidade aberta, biblioteca/aba de modelos/alternativas,
  seleção, filtros e expansão/rolagem da árvore.
- Modo Foco e tarefa principal são conservados. Mapas, gráficos, catálogo
  e fila de revisão conservam a tarefa, não todo seu estado interno.

Restauração por solicitação: áreas ocultas não são reconstruídas apenas
para reabrir o Projeto. Seu contexto aguarda a primeira visita. Na volta à
Escrita, não se restaura texto antigo por meio de preferências. Documento
ausente leva à Biblioteca; offsets fora do texto atual são limitados ao seu
tamanho. Contexto inválido/incompatível é recusado sem conversão de conteúdo.
Isso não transporta undo entre sessões nem substitui recuperação de texto.
Seleção/foco e rolagem são restaurados em momentos distintos do layout GTK,
para que a rolagem automática até o cursor não desfaça a posição de leitura.
O encerramento não grava o recolhimento de seleção causado pelo desmonte da
janela. Digitação/clique no editor interrompem a restauração pendente.

Menu do aplicativo → Exibir → Espaços de trabalho permite guardar, retomar,
substituir explicitamente pelo contexto atual e remover uma entrada. Até
20 espaços por Projeto. Nomes duplicados são permitidos, identidades são
distintas. Remover uma entrada não remove Documento, entidade ou Obra.
Preferências são locais, não acompanham automaticamente outro computador.
Não há docking nem novos painéis permanentes: layout mínimo e modo Foco,
com contexto de tarefa. A serialização de cada contexto é limitada a 64 KiB.
O conjunto serializado tem limite de 1,5 MB; uma gravação que ultrapassaria
esse teto é recusada antes de substituir a lista anterior.

## Saída inicial de Documento

Ações do Documento → Exportar Documento, ou Arquivo → Exportar Documento.
O editor termina a barreira de salvamento; o exportador prepara um snapshot
imutável do Documento persistido. A prévia contém o corpo e os limites da
saída antes da escolha de destino. Cancelar não cria arquivos exportados.

O pacote novo contém `documento.html`, `documento.txt` e `relatorio.json`.
TXT conserva os bytes UTF-8 do corpo. HTML escapa caracteres especiais e
conserva estilos inline suportados: negrito, itálico, sublinhado, tachado,
tamanho e cor. Sobreposições são segmentadas por offsets Unicode. Cabeçalhos,
alinhamento, citação/subtítulo e família de fonte não são representados como
formatação estrutural: são declarados no relatório quando utilizados.

Comentários/camadas, âncoras, vínculos, metadados, comandos inline e organização
não entram na leitura exportada. Nenhum código, script ou recurso remoto é
injetado. HTML possui política restritiva de conteúdo e CSS local de leitura.
Prévia é textual, não promessa de paginação idêntica a um navegador/impressora.

Destino novo e exclusivo, fora da pasta do Projeto pelo fluxo de UI. Arquivos
existentes nunca são substituídos. Caminhos simbólicos são recusados. Os
arquivos são sincronizados e relidos; o relatório é escrito por último,
com identidades, data da fonte, hashes, perdas e confirmação de readback.
Uma falha preserva a pasta parcial para diagnóstico, sem alterar a fonte.

É exportação de leitura de UM Documento, não montagem ordenada da Obra,
backup, formato de reimportação, DOCX/PDF/EPUB ou diagramação final. Exportar
não arquiva fontes, não ativa alternativa nem destrói a composição.

## Verificação e entrega

Passaram no código final: Release, 17 CTest (incluindo o novo teste de
preferências/exportação), 24 cenários GTK (22 fluxos em Atlas/Projeto vazio
mais dois reinícios efetivos de processo), e oito verificações suplementares
no tema escuro. QA usa cópias independentes e configurações isoladas.
Readback adicional de HTML usa parser independente e compara o conteúdo
textual exportado com TXT. As entregas aceitas não são sobrescritas.
ASan/UBSan focal passou nas novas unidades de exportação, preferências,
espaços salvos e parser inline de contexto, com o teste do núcleo. Demais
dependências permanecem Release; LSan foi desligado. Não é certificação
de sanitizadores de toda a aplicação GTK.

Evidência final: `outputs/reforma-continuidade-verificacao-2026-10-07.json`,
`outputs/reforma-continuidade-tema-escuro-2026-10-07.json` e
`outputs/reforma-continuidade-testes-nucleo-2026-10-07.log` no workspace
de entrega. O manifesto do novo corte identifica commit, binários, hashes,
bundle/patch e cópias independentes para revisão humana. Aceite humano deste
bloco permanece pendente; implementação e verificação não o substituem.

O ensaio de continuidade exerce espaços nomeados por menus reais, seleção
Unicode, rolagem, busca na página 2, detalhe, reabertura, reinício do processo,
fallback de Documento ausente e prévias de exportação claro/escuro. O teste
do núcleo cobre sobreposições de estilos, caracteres HTML, CR/LF, UTF-8
inválido, pasta existente, symlink, preferências corrompidas, isolamento por
Projeto, substituição/remoção e limites de quantidade/profundidade.

Schema 18, semântica 2, checkpoint 4 e clipboard 2 permanecem inalterados.
Verificação automática não certifica IME real, sessão longa, acessibilidade
assistiva, HiDPI ou desempenho de exportação de manuscritos extensos.
O ensaio GTK verifica prévia/cancelamento, mas não controla o seletor nativo
de destino. A gravação/readback é exercitada pelo núcleo e pelo parser
independente; escolher destino, abrir o HTML no navegador e imprimir ainda
exigem conferência humana do fluxo completo.

Para a conferência humana: guardar um espaço em Escrita com seleção e
rolagem, outro em Planejar com consulta/detalhe, voltar a ambos e reiniciar;
conferir também a unidade do Manuscrito. Exportar um Documento pela UI para
pasta externa, conferir os três arquivos e as perdas do relatório. Usar as
novas cópias `review-continuidade`, sem consumir as entregas anteriores.

## Frentes posteriores ainda autorizadas, não concluídas aqui

1. Revisão: comparação de versões, sugestões/decisões e revisão editorial
   completa; a fila existente continua sendo comentários/diagnósticos.
2. Produção: ordenação da Obra, perfis e formatos profissionais, com perdas
   e readback próprios. HTML/TXT de Documento não fecha essa frente inteira.
3. Impacto e reversão de operações do Projeto, distintos do undo textual.
4. Composição persistente: contrato de autoridade antes de schema,
   recuperação, acompanhamento/fixação, origem arquivada e materialização.
5. Aliases e calendário ficcional; continuidade situada e visões adicionais.

Não foram adicionados comandos vazios para representar essas pendências.

# E5-S1R — montagem de texto orientada à escrita

## Pedido e fronteira da entrega

Reforma solicitada após revisão humana do E5-S1: divisão por terços rejeitada,
edição/leitura e impacto pouco claros, alternativas sem promoção, ausência de
montagem real entre Documentos. O aceite do editor semântico 3 continua válido;
não se infere aceite do laboratório anterior nem desta reforma.

Candidato isolado `codex/editor-workshop-2026-10-04`, HEAD-base
`f155bd866d32c9326e5853a2882e6adbfca6ba23`. Sem commit, push, release ou migração.
Editor principal permanece inalterado. `INDE_BUILD_TEXT_SPIKE=OFF` por padrão;
o laboratório separado exige cópia preparada e schema 18. Arquivos congelados,
projetos originais e perfil real não são destinos desta implementação.

## Contrato autoral

- **Origem:** Documento congelado em leitura na sessão; não recebe edições do
  bloco. Há dois exemplos em memória e Documentos reais da cópia, em páginas de 50.
- **Bloco:** trecho identificado e nomeado, com origem e versões próprias.
- **Versão:** snapshot imutável do texto e das camadas conserváveis. Alteração
  exige a revisão esperada; não sobrescreve histórico ou origem.
- **Ocorrência:** um uso ordenado de um bloco na montagem. Pode acompanhar a
  versão atual, manter versão fixa, usar um bloco independente ou ser referência
  sem texto. Ocultação é diferente de retirar a ocorrência.
- **Leitura:** concatenação literal das ocorrências escolhidas, sem rascunhos,
  separadores inventados ou edição direta do resultado. Mapas têm offsets Unicode
  próprios. A escolha de alternativa/camada altera a leitura, não os blocos.
- **Rascunho:** edição em painel independente, fora da leitura até Aplicar versão.
  Trocar origem, ocorrência, perfil ou desfazer não o descarta silenciosamente.
- **Documento criado:** materialização independente da leitura, salva somente
  na cópia nova após prévia, avisos e dupla confirmação. Não é montagem viva salva.

## Operações e interface

Origens, Blocos, Ocorrências e Usos/impacto substituem comandos físicos na UI.
Indicador EM FOCO mostra número da ocorrência, bloco, versão, origem, modo e
visibilidade. Seleção na biblioteca sincroniza o alvo dos comandos e o impacto;
bloco sem ocorrência não mantém um alvo antigo escondido. Abertura de versão,
reutilização e nomeação são distintas. Botões inaplicáveis ficam desabilitados.

Painéis de edição simultâneos (até 16) ficam acima de Leitura/Mapa/Documento a
criar. Versões históricas são somente leitura. É possível fechar um painel sem
perder a montagem, com confirmação para eventual rascunho. Aplicar versão mostra
contagens, até seis ocorrências identificadas, ocultas afetadas e fixas preservadas;
Usos/impacto contém a lista integral **desta montagem**, não um índice global.

Dividir no cursor ou separar a seleção usa limites escolhidos pelo autor. O
texto concatenado e outras ocorrências permanecem iguais. Novos blocos conservam
proveniência e recebem nomes; a fonte e o bloco anterior não são apagados.
O laboratório não tenta adivinhar fronteiras narrativas, frases ou parágrafos.

Alternativas nomeadas pertencem a famílias independentes, cada uma com Base.
Escolher uma opção de uma família preserva as escolhas das demais. A leitura
expõe as escolhas atuais. Promover a bloco cria identidade independente e torna
a ocorrência comum; Base e outras opções da família ficam ocultas, não apagadas.
Reutilização permite novas ocorrências vinculadas; versão fixa não acompanha
edições posteriores. Tornar independente rompe apenas o vínculo escolhido.

Mover, ocultar, retirar, converter em referência, consultar origem, camadas
nomeadas e desfazer/refazer da montagem continuam separados. Retirar não apaga
bloco ou versões. O mapa também identifica blocos sem ocorrência, ainda na
biblioteca. Undo de montagem retém no máximo 16 estados; não é histórico durável.

## Conservação e criação efetiva

`slice_layers` conserva formatos recortados e âncoras/anotações totalmente
contidas. Marcação que atravessa o corte, declaração global de recorte parcial,
relação com endpoint externo e contexto não representável geram aviso, sem
escolha semântica automática. Texto de origem permanece intacto.

`edit_layers` usa transação textual mínima existente, gravidades e estados das
âncoras. Deslocamento/revisão não implica resolução forçada. Camadas incompatíveis
ou limites excedidos recusam a operação antes de trocar o estado da montagem;
rascunho e base do painel só mudam depois da aplicação bem-sucedida.

`materialize` usa a ordem/visibilidade atual e gera Document com identidade nova.
Formatos, âncoras, evidências, anotações, relações internas e vínculos temporais
conserváveis ganham IDs locais novos por ocorrência. IDs de entidades e eixos/
pontos ficcionais continuam canônicos no mesmo Projeto. Perspectivas globais de
múltiplas ocorrências não viram uma perspectiva vencedora da composição. Modelos
inline conflitantes são avisados e omitidos; modelos iguais são deduplicados.
Referências sem texto não viram vínculo permanente entre Documentos.

Não copiar colocação editorial, organização, meta ou revisão documental por
inferência. Prévia lista texto, contagens e avisos; rascunho pendente bloqueia a
criação. Mudança na montagem/título/rascunho invalida a prévia. Aceitar avisos e
confirmar a criação salva novo Documento através do repositório canônico, com
readback; não expõe atualização de origem. Exige marcadores novos, identidade do
manifesto e schema iguais, sem symlinks nos caminhos protegidos. É guarda de cópia
e de não sobrescrita normal, não certificação de corrida adversarial/concurrente.

## Módulos e limites

`text_blocks.cpp` mantém identidades/revisões/projeção; `text_blocks_operations.cpp`
contém recortes, promoção e opções. `block_layers.cpp` conserva recortes/edições;
`block_materialization.cpp` cria o agregado derivado. `lab_source.cpp` limita
leitura/criação à cópia. GTK dividido entre layout, comandos, editores e render.
Buffers físicos anteriores ficam no CLI técnico, sem nova escolha de backend.

Limites: 256 blocos, 2048 ocorrências, 4096 revisões, 32 variantes/camadas,
32 MiB de texto retido/projeção, 2 MiB por origem gráfica, 16 painéis/estados de
undo. Não há paginação física, busca global de usos, blocos aninhados, merge,
autosave da montagem, formatos na edição do painel ou persistência dos vínculos.
A leitura apresenta formatação preservada; seis níveis de título têm prévia
visual simplificada, sem alterar os metadados do Documento criado.

## Validação e continuação

Gate: 15 CTest; regressão GTK em vazio limpo/vazio/Atlas, múltiplas origens,
rascunhos/versões simultâneos, edição vinculada e fixa, seleção, alternativa,
promoção, camadas/ocultação/referência, ordem/undo/fechamento de painel, rascunho
100 mil palavras e criação/readback de Documento. Auditar todas as linhas
anteriores, integridade/FKs, bytes de sementes/origens e atualização derivada do
contador change_log. Capturas e ASan/UBSan focal pertencem ao novo SHA.
Passagem técnica não substitui revisão humana, IBus real ou sessão de 45 minutos.

Fonte/evidência detalhadas em `outputs/editor-montagem-*` do workspace de entrega.
Próximo gate humano é clareza e utilidade desta montagem. E5-S2 persistente exige
contrato de autoridade única, conversão explícita, conservação/recuperação e
projeto de schema antes de integrar vínculos vivos ao editor principal.
Calendários e tempo configurável continuam após Etapa 5; gen7/HTML dispensados.

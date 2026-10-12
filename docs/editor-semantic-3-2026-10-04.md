# Editor: seletor RGB e contextos semânticos — 2026-10-04

Candidato isolado sobre editor-barra, base f155bd8, branch
codex/editor-semantic-3-2026-10-04. O pedido autorizou corrigir o seletor e depois
iniciar os cortes grandes seguintes; essa ordem foi respeitada. Aceite anterior
registrado separadamente. Não foi realizado commit, push ou release.

## Uso

- Botão A: paleta visual e ajuste de tonalidade/saturação, R/G/B de 0 a 255,
  hexadecimal e amostra sobre o fundo do editor. Aplicar confirma; cancelar não
  altera o Documento. Hex incompleto não aplica e informa o motivo.
- Camadas > Perspectiva tipada do Documento: criar/editar declaração global,
  sem selecionar texto. Estados e campos são os da perspectiva situada v1.
  Detalhes da declaração global não oferecem reancorar/ir a um trecho fictício.
- Detalhe de anotação > Vincular tempo ficcional: selecionar eixo, ponto ou
  período fechado, examinar a prévia e confirmar. Não modifica Planejamento.
- Camadas > Projeto / Obra: filtros de abrangência global/intervalar e contexto
  temporal opcional por eixo e ponto; resultados paginados e navegáveis.

## Contrato de contexto

Escopo global não é cobertura do corpo. Texto apagado continua tendo sua
declaração global, enquanto intervalos perdidos permanecem pendentes de revisão.
No cursor, uma perspectiva local resolvida fornece o contexto local; múltiplas
declarações locais ficam ambíguas, sem escolha silenciosa. A informação é uma
projeção de declarações humanas, não uma inferência da voz textual.

Tempo ficcional permanece separado dos tempos narrativo, editorial e do sistema.
Não interpreta datas no texto. Não calcula duração por diferença de ordinais,
não cria evento/presença e não corresponde ao calendário configurável futuro.
O autor pode remover vínculo explicitamente com undo/redo; apagar pontos
referenciados ou inverter limites é recusado até desfazer a referência.

## Persistência e validação

Schema principal 18; schema do journal 1; payload de checkpoint 4, semântico 2,
clipboard 2 e envelope de exportação 1. Compatibilidade: semântico 1/2,
checkpoints 1/2/3/4, clipboard 1/2. Não usar executável anterior em cópia migrada;
as revisões predecessoras e backups permanecem intactos.

Relatórios técnicos reproduzíveis e SHA do binário estão em
outputs/editor-semantico-3-*.json do workspace de entrega. Build/CTest/GTK e
sanitização focal não equivalem a aceite humano. Dependências e GTK não são
certificados integralmente por ASan; LSan desligado. IME real/45 minutos não
foram inferidos do aceite anterior ou da curta regressão atual.

## Limites e próximo corte

Sem importação gen7/HTML, coleta de fonte ou binding lexical automático. Não
encerra integralmente Etapa 2/4. Blocos e variantes exigem spike físico e decisão
de contrato; calendário profundo de worldbuilding permanece após Etapa 5.
Revisão humana das funções novas em cópias independentes antes de promover o
candidato. Recomenda-se GPT-6.1 Sol/high no spike, mesma tarefa para preservar
contexto, custo relativo alto; UX delimitada pode usar medium.

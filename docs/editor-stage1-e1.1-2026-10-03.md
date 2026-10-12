# Etapa 1 do editor — E1.1: análise assíncrona e medição

Data: 2026-10-03. Autorização: “aprovado, iniciar etapa 1”.
Plano de referência: `outputs/plano-editor-processador-semantico-inde.md`,
seções 7–8, no workspace de entrega. Base Git: `f155bd866d32c9326e5853a2882e6adbfca6ba23`,
mais os seis cortes e o diff linguístico preexistente preservado.

## Escopo e fronteiras

Este incremento inicia a Etapa 1; não conclui toda a etapa nem todo o primeiro
lote originalmente proposto na seção 8. Implementa coordenador de análise,
estatísticas e proofreading multilinha em background, invalidação e diagnóstico
local. Localizar/substituir e suas transações ficam no próximo incremento.
Sem schema novo, catálogo alterado, decisão semântica, migração de histórico,
recuperação por journal, commit, push ou publicação.

## Contrato implementado

- Snapshots possuem texto/identidade por valor e vocabulário imutável compartilhado.
- Um worker por coordenador; somente um trabalho ativo e um snapshot pendente,
  substituível. Debounces existentes de 260/550 ms são mantidos.
- Uma edição invalida imediatamente a geração, antes do debounce. Troca,
  restauração, reset e destruição cancelam trabalho que perdeu validade.
- Cancelamento cooperativo nos percursos de texto e descarte por geração mesmo
  quando um analisador não coopera. Uma chamada externa Enchant em execução não
  pode ser interrompida no meio; destruição aguarda o término seguro do worker.
- Resultados e erros são valores; callbacks GTK e aplicação de tags/estatísticas
  ocorrem exclusivamente na thread da interface, por polling de 30 ms.
- `busy()` inclui resultado pronto ainda não consumido, evitando encerrar polling
  se um resultado chegar entre a tentativa de leitura e a consulta de estado.
- Cada TextView editável observado tem corretor/broker/cache próprios; o serviço
  mutável de preferências e o broker da interface não são usados pelo worker.
  Widgets descartados liberam timers e workers. Campos Entry continuam síncronos.
- Edição limpa problemas e tags obsoletos; F7 distingue revisão pendente de revisão
  concluída. Uma correção verifica limites e o trecho atual antes de substituir.
- Offsets de parágrafos longos ficam não acionáveis até uma análise válida.
- Histórico por snapshots e persistência síncrona permanecem: não são substituídos
  por este incremento, nem apresentados como objetivos já resolvidos.

## Observabilidade e testes

`INDE_EDITOR_PROFILE=1` habilita tempos locais no terminal, sem conteúdo, trechos,
nomes ou caminhos de Documentos. Registra callbacks de alteração, snapshot/envio,
execução no worker, aplicação GTK, undo, autosave e abertura/restauração.
Alterações de restauração são separadas das de digitação. O tick seguinte do frame
clock é um indicador parcial: não mede apresentação final, pixels ou teclado físico.

`inde_text_analysis_tests` testa Unicode/paridade, fila substituível, descarte de
analisador não cooperativo, invalidação durante debounce, exceções/retomada,
destruição/join e snapshots independentes de vocabulário. Faz parte do CTest.
`tests/ui_writing_smoke.cpp` usa objetos de produção e fixtures descartáveis:
~1 MiB, 40 inserções rápidas + 16 pausadas, contagem, tags/F7, autosave real,
desfazer/refazer, formatação/âncora, reabertura, troca e reset pendente.
É opt-in; não faz parte de um CTest headless.

Evidências consolidadas, hashes, percentis, limites e launcher ficam em
`outputs/editor-etapa1-e1.1-2026-10-03.md` e seus anexos no workspace de entrega.
O aceite humano desse incremento é distinto do aceite já dado aos cortes 5–6.
Após E1.2 será obrigatória nova revisão consolidada antes de um terceiro incremento.

## Pendências da Etapa 1

`DocumentEditSession`, `EditCommand`/`EditTransaction`, deltas e fallback,
localizar/substituir (Ctrl+F/H, palavra inteira, replace-all em um undo), clipboard
conservador, journal/recuperação, otimização do caminho de salvamento/restauração,
IME e teclado físico, sessão prolongada, medição de pintura real e validação humana.
AppImage e outros backends de correção ainda exigem validação própria.

Próximo corte recomendado: E1.2, localizar/substituir com transação reversível e
testes de offsets/camadas; o gargalo medido de salvamento/restauração deve orientar
a revisão da sessão de edição. GPT-6.1 Sol / high, mesma tarefa, custo relativo alto
pela integridade de texto, formatação e âncoras; sem escalada automática de esforço.

## Continuidade autorizada — 2026-10-03

O usuário autorizou combinar o corte recomendado com mais um ou dois cortes.
O sucessor integra E1.2, DocumentEditSession e transações reversíveis:
[brief atual](editor-stage1-large-cut-2026-10-03.md). Este arquivo mantém o
registro histórico de E1.1; suas pendências de busca/sessão não descrevem o
candidato sucessor. O feedback positivo da análise não foi convertido em
aceite humano específico de cenários GTK. A revisão consolidada continua devida.

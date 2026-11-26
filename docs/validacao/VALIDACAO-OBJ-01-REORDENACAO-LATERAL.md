# OBJ-01 — reordenação e arrasto lateral

Data: 26/11/2026. Branch `feature/inde-obj-01-foundation`. Continuidade do [plano de maturidade](../arquitetura/PLANO-MATURIDADE-EDITOR-OBJ-01.md) e do primeiro corte de [fluidez](VALIDACAO-OBJ-01-FLUIDEZ-E1A.md).

## Comportamento

Na especialidade Estrutura, os círculos passam a reconhecer quatro bordas. A borda superior/inferior move a unidade estrutural antes/depois do destino. As bordas laterais formam uma linha com o bloco vizinho. O mesmo traço verde de inserção orienta as quatro possibilidades, com sombra interna que não muda o tamanho do bloco nem desloca o destino durante o arrasto. Escape cancela a operação e limpa o indicador.

O destino lateral exige também um deslocamento horizontal de pelo menos 8 px. Um arrasto reto pela faixa dos círculos continua vertical; estar perto da margem esquerda de outro bloco, sozinho, não transforma o movimento em composição lateral. O percurso real verifica esse caso antes de formar um par.

Organizar lado a lado modifica a apresentação, mantendo a ordem autoral e o texto. Um movimento vertical altera separadamente a posição da unidade em cada ordem, sem promover o restante da apresentação a ordem de leitura. Funciona também depois de uma composição visual que tornou as duas ordens diferentes.

A unidade vertical inclui a linha inteira e o fecho dos grupos de leitura indivisíveis que ela toca. Conserva os IDs, proporções e espaçamento das linhas. Os movimentos de texto continuam usando o comando canônico do Documento, responsável por remapear formatação, âncoras e alternativas. A operação só publica seu candidato após as validações; recusa, referência obsoleta e conflito de revisão não devem modificar o estado anterior.

O arrasto lateral usa o perfil já existente de duas colunas. Não substitui uma linha ocupada de outro par, nem separa um grupo de texto indivisível: esses destinos não anunciam uma soltura válida. Um membro pode mudar de lado dentro do próprio par, conservando identidade, proporções e espaçamento. Não há implantação de colunas arbitrárias, grade geral, schema novo ou arquivo único `.inde`.

## Redução do trabalho

O movimento deixa de reconstruir a folha inteira. As vistas compatíveis são reordenadas no contêiner existente; a adoção do Documento ajusta seus intervalos canônicos. Quando um pareamento muda, apenas as unidades afetadas são reconstruídas. Conteúdo ou apresentação incompatível ainda usa a reconstrução protegida.

As geometrias de destino são capturadas no início do arrasto e renovadas se a dimensão da folha mudar. O movimento usa comparações numéricas dessas caixas; o indicador anterior só é removido quando o destino muda. A rolagem automática mantém o ponteiro e as caixas no mesmo sistema de coordenadas.

O ponteiro é obtido nas [coordenadas da superfície do evento GDK](https://docs.gtk.org/gdk4/method.Event.get_position.html), convertido pela [transformação do GtkNative](https://docs.gtk.org/gtk4/method.Native.get_surface_transform.html) e então para a folha. Isso evita tratar a posição interna do botão, com seus estilos e deslocamentos durante a rolagem, como se fosse uma posição estável no documento.

Os checkpoints, conferência de leitura após gravação, orçamento do histórico e salvamento canônico permanecem. Não há atualização otimista do texto antes da aceitação, redução de durabilidade ou migração de projeto autoral.

## Contratos e provas

O domínio acrescenta `move_unit(id, reference, after)` e `place_beside(id, other, side)`. Ambos exigem o contexto estrutural e o perfil P2-D2. A operação anterior `move` mantém seu contrato para compatibilidade. `place_beside` cria linhas 50/50 com intervalo de 16 px; ao trocar os membros de um par existente, conserva as propriedades desse par.

Os círculos, Ctrl+↑/↓, ações relativas e a opção autoral do formulário de posição usam a nova unidade estrutural. A opção visual explícita desse formulário conserva o comportamento anterior de mover o item individual.

`inde_object_structural_move_tests` entra na matriz GCC/Clang, com e sem ASan/UBSan, do domínio isolado. Cobre ordens divergentes, linha com proporções personalizadas, movimentação da linha inteira, âncora remapeada, grupo indivisível com alternativa privada, Desfazer/Refazer, destino ocupado, destino na própria unidade, revisão antiga e reabertura do estado salvo.

O novo percurso de entrada real usa colagem, círculos, teclas e botões normais. Mede movimentos consecutivos em 20/200 linhas, confere a conservação das vistas, exercita os indicadores antes de soltar, cancelamento, destino ocupado, composição de texto, duas tabelas, movimentação do par, Desfazer/Refazer, salvamento, integridade e saída normal. O observador somente lê o estado e a geometria.

As medidas de interface incluem o envio X11 e a confirmação periódica do observador, não sendo latência pura de pintura. Ensaios intermediários e falhas do driver não constituem aprovação final. A meta de 100 ms e sessões prolongadas continuam dependentes de prova própria.

## Resultado local final e velocidade

A suíte completa passou **100/100 em 202,907 s**, sem compilação concorrente. Inclui os três percursos reais: uso geral, vazios/fluidez e reordenação/arrasto lateral. A comparação usa uma cópia do binário anterior à mudança, com SHA-256 `8a0462ea1ede2a686d03dd81005012e5dd1841a24e8b6ae381272e6dfa2b7322`, e o binário Release final. A referência anterior também concluiu seu percurso, salvamento e saída normal; não é apenas uma amostra extraída de teste interrompido.

Cada perfil contém três movimentos consecutivos de Ctrl+↓ do mesmo primeiro bloco, intercalados somente com a observação da conclusão. O texto esperado, foco para continuar e conservação das vistas são conferidos. A referência final foi executada antes da suíte nova, sem compilação concorrente em nenhuma das duas execuções.

| Linhas de prosa | Anterior: mínimo–máximo | Anterior: mediana | Final: mínimo–máximo | Final: mediana |
|---|---|---|---|---|
| 20 | 369,473–452,921 ms | 373,504 ms | 286,694–290,066 ms | 287,055 ms |
| 200 | 3.389,055–4.797,822 ms | 3.459,160 ms | 789,082–2.397,394 ms | 990,424 ms |

Nos seis movimentos finais, as 20/200 vistas mantiveram as mesmas identidades observadas. A referência anterior reconstrói a folha; coincidências isoladas de endereço depois de desalocar/recriar uma vista não são tratadas como conservação.

No perfil pequeno de arrasto, a confirmação após soltar levou 254,809–257,346 ms nos três destinos laterais exercitados, 256,808 ms no movimento acima e 420,224 ms no movimento reto para baixo. Esses números pertencem à fixture pequena, não ao perfil de 200 linhas.

A instrumentação localiza o pico remanescente: no terceiro movimento de 200 linhas, `save_barrier_gtk` levou 1.645,73 ms, `object_owner_commit` 1.720,07 ms e a aplicação completa do resultado 1.921,43 ms. A reconciliação visual desse movimento levou 34,394 ms. O trabalho do comando no trabalhador ficou em 131,275 ms. Há sobreposição de fases; não se somam esses valores nem se atribui toda a barreira apenas à escrita de bytes no disco.

Assim, a redução da reconstrução está demonstrada, mas a pausa de gravação não está resolvida. Não há promessa de ausência de atrasos, percentis 95/99, 2.000 parágrafos ou uso prolongado. A incorporação canônica ainda espera a confirmação de recuperação/salvamento; tornar essa etapa responsiva exige conservar suas garantias e a coerência entre bancos. A inserção após arranjos divergentes (A02/M10) também permanece fora deste corte.

O novo teste de domínio passou localmente com ASan/UBSan e detecção de vazamentos, saída 0. A CI da implementação e o pacote de hashes serão fixados após sua conclusão remota.

## Reprodução

```sh
cmake --build build-nativo --target inde inde_object_native_tests --parallel 2
INDE_REAL_INPUT_CASE=structural_reorder python3 tests/run_obj01_real_input.py
INDE_REAL_INPUT_FULL_SUITE=1 python3 tests/run_obj01_real_input.py
```

O runner exige suas dependências X11 e cria display, gerenciador de janelas, perfil e projeto privados. O filtro é apenas diagnóstico; a CI normal executa os três casos reais. `INDE_REORDER_COUNTS` permite perfis diagnósticos de pelo menos quatro linhas; somente 20/200 são declarados nesta validação final. `INDE_REORDER_BASELINE=1` pula os recursos novos e mede somente a sequência anterior, usando `INDE_TEST_BINARY` para indicar um executável preservado. Esse modo não valida o arrasto lateral.

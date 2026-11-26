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

A conversão desconta o deslocamento da superfície antes de transformar para o widget, seguindo a [implementação do próprio GTK](https://mail.gnome.org/archives/commits-list/2020-June/msg03018.html). O observador faz a conversão inversa para informar coordenadas físicas corretas ao driver X11 e publica o XID da superfície nativa. Assim o teste não confunde a janela nativa com a moldura do gerenciador, nem mistura posições de widgets e da superfície.

Os checkpoints, conferência de leitura após gravação, orçamento do histórico e salvamento canônico permanecem. Não há atualização otimista do texto antes da aceitação, redução de durabilidade ou migração de projeto autoral.

## Contratos e provas

O domínio acrescenta `move_unit(id, reference, after)` e `place_beside(id, other, side)`. Ambos exigem o contexto estrutural e o perfil P2-D2. A operação anterior `move` mantém seu contrato para compatibilidade. `place_beside` cria linhas 50/50 com intervalo de 16 px; ao trocar os membros de um par existente, conserva as propriedades desse par.

Os círculos, Ctrl+↑/↓, ações relativas e a opção autoral do formulário de posição usam a nova unidade estrutural. A opção visual explícita desse formulário conserva o comportamento anterior de mover o item individual.

`inde_object_structural_move_tests` entra na matriz GCC/Clang, com e sem ASan/UBSan, do domínio isolado. Cobre ordens divergentes, linha com proporções personalizadas, movimentação da linha inteira, âncora remapeada, grupo indivisível com alternativa privada, Desfazer/Refazer, destino ocupado, destino na própria unidade, revisão antiga e reabertura do estado salvo.

O novo percurso de entrada real usa colagem, círculos, teclas e botões normais. Mede movimentos consecutivos em 20/200 linhas, confere a conservação das vistas, exercita os indicadores antes de soltar, cancelamento, destino ocupado, composição de texto, duas tabelas, movimentação do par, Desfazer/Refazer, salvamento, integridade e saída normal. O observador somente lê o estado e a geometria.

As medidas de interface incluem o envio X11 e a confirmação periódica do observador, não sendo latência pura de pintura. Ensaios intermediários e falhas do driver não constituem aprovação final. A meta de 100 ms e sessões prolongadas continuam dependentes de prova própria.

## Resultado local final e velocidade

A suíte completa final passou **100/100 em 210,169 s**, com `GTK_THEME=Adwaita`, sem compilação concorrente. A primeira iteração local passou 100/100 em 202,907 s, mas sua CI falhou; ela é preservada separadamente e não é a aprovação final. Inclui os três percursos reais: uso geral, vazios/fluidez e reordenação/arrasto lateral. A comparação usa uma cópia do binário anterior à mudança, com SHA-256 `8a0462ea1ede2a686d03dd81005012e5dd1841a24e8b6ae381272e6dfa2b7322`, e o binário Release final. A referência anterior também concluiu seu percurso, salvamento e saída normal; não é apenas uma amostra extraída de teste interrompido.

Cada perfil contém três movimentos consecutivos de Ctrl+↓ do mesmo primeiro bloco, intercalados somente com a observação da conclusão. O texto esperado, foco para continuar e conservação das vistas são conferidos. A referência final foi executada antes da suíte nova, sem compilação concorrente em nenhuma das duas execuções.

| Linhas de prosa | Anterior: mínimo–máximo | Anterior: mediana | Final: mínimo–máximo | Final: mediana |
|---|---|---|---|---|
| 20 | 369,473–452,921 ms | 373,504 ms | 204,865–290,491 ms | 287,466 ms |
| 200 | 3.389,055–4.797,822 ms | 3.459,160 ms | 787,880–892,146 ms | 883,455 ms |

Nos seis movimentos finais, as 20/200 vistas mantiveram as mesmas identidades observadas. A referência anterior reconstrói a folha; coincidências isoladas de endereço depois de desalocar/recriar uma vista não são tratadas como conservação.

No perfil pequeno de arrasto, a confirmação após soltar levou 172,472–341,209 ms nos três destinos laterais exercitados, 255,889 ms no movimento acima e 339,049 ms no movimento reto para baixo. Esses números pertencem à fixture pequena, não ao perfil de 200 linhas.

A primeira iteração local registrou um pico de 2.397,394 ms, que não foi apagado do histórico da validação. Sua instrumentação localiza a espera: no terceiro movimento de 200 linhas, `save_barrier_gtk` levou 1.645,73 ms, `object_owner_commit` 1.720,07 ms e a aplicação completa do resultado 1.921,43 ms. A reconciliação visual desse movimento levou 34,394 ms. O trabalho do comando no trabalhador ficou em 131,275 ms. Há sobreposição de fases; não se somam esses valores nem se atribui toda a barreira apenas à escrita de bytes no disco.

A execução final não repetiu esse pico em suas três amostras de 200 linhas. Isso não certifica sua eliminação: a redução da reconstrução está demonstrada, mas a barreira de gravação permanece. Não há promessa de ausência de atrasos, percentis 95/99, 2.000 parágrafos ou uso prolongado. A incorporação canônica ainda espera a confirmação de recuperação/salvamento; tornar essa etapa responsiva exige conservar suas garantias e a coerência entre bancos. A inserção após arranjos divergentes (A02/M10) também permanece fora deste corte.

O novo teste de domínio passou localmente com ASan/UBSan e detecção de vazamentos, saída 0. A correção posterior de coordenadas alterou somente interface, observador, driver e workflow; os hashes do domínio instrumentado permaneceram iguais e estão registrados.

A [CI final 45854500399](https://github.com/Roger-Cardoso/inde/actions/runs/45854500399), no commit `49b9313a4a81a8a5147b954f11683bf3cee4e105`, aprovou **26/26 trabalhos**. A prova estrutural passou dentro dos 23 testes de cada uma das quatro configurações de domínio: GCC/Clang, com e sem sanitizers. O trabalho nativo compilou o aplicativo real, passou matemática, quatro testes de vínculo/projeção/GTK, quatro de publicação e os **três percursos X11 em 107,049 s**, incluindo os dois que haviam falhado na primeira iteração.

O [artefato sintético da CI](https://github.com/Roger-Cardoso/inde/actions/runs/45854500399/artifacts/15134504213) foi publicado pelo próprio workflow: ZIP de 196.726 bytes, SHA-256 `54aaaf09c1bed998fc0bff07b0acb1568f430610454a057926d50725a34ad664`, sujeito à retenção de sete dias configurada no workflow. O pacote no Git conserva os logs, imagens e hashes necessários independentemente dessa retenção temporária.

A primeira [CI 45851260025](https://github.com/Roger-Cardoso/inde/actions/runs/45851260025), no commit `ef5fc7eec9987f0e5e12d00ba0028b93e62aa083`, falhou nos percursos de foco de vazios e de reordenação. Os outros 25 trabalhos passaram, inclusive a prova estrutural em todas as configurações, mas a execução não é aceita. O observador misturava coordenadas do widget e da superfície e o driver procurava a janela pelo gerenciador. A correção final uniformiza as duas direções da conversão, usa o XID nativo explícito, evita alvos parcialmente cortados quando cabem na área visível e exige foco no círculo antes de enviar a tecla. Não retira as asserções de conteúdo, foco ou arranjo. A CI também passa a preservar imagens, estados e projetos sintéticos para diagnóstico, com a ação de upload fixada em revisão imutável.

## Reprodução

```sh
cmake --build build-nativo --target inde inde_object_native_tests --parallel 2
GTK_THEME=Adwaita INDE_REAL_INPUT_CASE=structural_reorder python3 tests/run_obj01_real_input.py
GTK_THEME=Adwaita INDE_REAL_INPUT_FULL_SUITE=1 python3 tests/run_obj01_real_input.py
```

O runner exige suas dependências X11 e cria display, gerenciador de janelas, perfil e projeto privados. O filtro é apenas diagnóstico; a CI normal executa os três casos reais. `INDE_REORDER_COUNTS` permite perfis diagnósticos de pelo menos quatro linhas; somente 20/200 são declarados nesta validação final. `INDE_REORDER_BASELINE=1` pula os recursos novos e mede somente a sequência anterior, usando `INDE_TEST_BINARY` para indicar um executável preservado. Esse modo não valida o arrasto lateral.

## Evidências e demonstração

O [pacote verificável](evidencias-obj01-reordenacao-lateral/README.md) reúne os logs completos locais, referência anterior, amostras brutas, fases medidas, integridade, prova de memória, CI e hashes de 51 arquivos de implementação/integração/testes/instruções e dos três compactados fixados. O verificador lê o pacote e o checkout sem abrir projeto ou gravar no banco.

A demonstração usa outra cópia do projeto sintético fechado: `dados/obj01-reordenacao-lateral/Demo-final.inde`. A janela **Demonstração final — reordenação lateral — INDE** está aberta no documento **Documento de uso real**, especialidade **Estrutura**, com seis parágrafos e duas tabelas pareadas. As janelas anteriores foram preservadas. O arquivo [demo-folha.png](evidencias-obj01-reordenacao-lateral/demo-folha.png) registra esse estado; os quatro `drop-*.png` registram os indicadores durante arrastos reais.

Roteiro curto: arraste um círculo à borda superior/inferior de outro bloco; arraste com deslocamento horizontal até a borda lateral para formar um par; use Escape para cancelar; mova o par de tabelas acima/abaixo de um parágrafo; confira Desfazer/Refazer. Não solte sobre uma linha já ocupada de outro par. A leitura não muda só porque dois itens foram colocados lado a lado.

Para reabrir a cópia existente:

```sh
scripts/demos/Abrir-demo-folha-comum.sh "$PWD/dados/obj01-reordenacao-lateral/Demo-final.inde"
```

Branch `feature/inde-obj-01-foundation`, implementação `49b9313a4a81a8a5147b954f11683bf3cee4e105`. Main, projetos autorais, arquivos originais fixados e schemas não foram alterados por este corte. A demonstração é a aplicação normal C++/GTK, não uma interface web. Validação técnica não encerra E1/E1a/E2 nem substitui aceite humano.

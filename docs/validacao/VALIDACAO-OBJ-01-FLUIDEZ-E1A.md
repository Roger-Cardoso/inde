# OBJ-01 — retomada e primeiro corte de fluidez

Data: 26/11/2026. Branch `feature/inde-obj-01-foundation`. Base `ad9189c6ca7a256175adf8d87d2984eecf21d8c6`. Continuidade do [plano de maturidade](../arquitetura/PLANO-MATURIDADE-EDITOR-OBJ-01.md), com prioridade ao relato de pausas ao apagar parágrafos vazios.

## Problema e resultado do corte

Apagar uma quebra de parágrafo podia retirar uma identidade da apresentação e provocar a reconstrução da folha inteira. As vistas nativas, menus e círculos eram recriados, mesmo quando quase todo o conteúdo continuava válido. A lista de objetos também era reconstruída integralmente. A atualização de seleção podia reaplicar tags sem necessidade.

O corte conserva as unidades visuais existentes quando apenas a estrutura textual muda e os objetos, arranjos e propriedades de apresentação permanecem compatíveis. Retira as unidades textuais que desapareceram, cria as novas e reordena as sobreviventes. Os intervalos de cada vista são conferidos e atualizados a partir dos limites canônicos atuais. Os outros objetos e linhas compostas compatíveis conservam seus widgets.

A lista de alvos retira somente os IDs ausentes, altera os rótulos que mudaram e insere/reordena os demais quando necessário. A seleção vazia é limpa explicitamente. A formatação local só é reconstruída quando o conteúdo, intervalo ou tags exigem atualização. Mudanças de seleção continuam projetadas.

Há caminho de reconstrução completa para alterações incompatíveis. Na primeira montagem, os rótulos temporários são substituídos por vistas editáveis reais; não podem ser conservados como se fossem campos de escrita. Um arrasto é cancelado antes de reconciliar uma mudança estrutural. Contextos de entrada provisória e substituição entre parágrafos continuam protegidos.

O motor de persistência do agregado, seus checkpoints, readback e reconciliação de histórico não foram alterados. Não há redução de durabilidade para aparentar resposta mais rápida. Essa parte ainda contribui para o custo e permanece como alvo de investigação posterior.

## Medições e seus limites

O percurso usa X11 em display, gerenciador de janelas, D-Bus e projeto sintético privados. Cria o projeto pela interface, cola o texto pelo clipboard e apaga vazios com teclas reais. O observador registra o estado, a geometria dos controles, a seleção de combos e a identidade das vistas; não preenche campos nem executa comandos.

As amostras `observed_ms` incluem o envio de eventos e a confirmação pelo observador de baixa prioridade, que consulta a interface a cada 100 ms. Não são medidas puras de pintura ou latência percebida; a observação pode acrescentar custo. Elas não certificam o objetivo de resposta de até 100 ms do plano.

A referência anterior à reconstrução incremental registrou três exclusões com aproximadamente 3,06–3,16 s no perfil com 200 linhas de prosa e nove vazios iniciais. Esse ensaio foi interrompido no passo posterior de navegação/Backspace, antes de o teste aguardar a confirmação do novo foco. As amostras de Delete são evidência de lentidão; o ensaio anterior não é declarado aprovado como percurso completo. Compilação concorrente e a ausência de uma medição de pintura independente também limitam a comparação a uma orientação local, sem fator universal de aceleração.

A suíte local final aprovou **99/99 testes em 179,242 s**, incluindo a substituição recusada e os dois percursos reais. A execução final ocorreu sem compilação concorrente. CI e commit exato serão fixados nas evidências após a conclusão remota.

| Perfil | Delete individual, mínimo–máximo observado | Mediana das três exclusões |
|---|---|---|
| Referência anterior, 20 linhas de prosa | 356,332–442,838 ms | 358,802 ms |
| Referência anterior, 200 linhas de prosa | 3.057,502–3.159,410 ms | 3.067,105 ms |
| Código final, 20 linhas de prosa | 186,999–187,585 ms | 187,129 ms |
| Código final, 200 linhas de prosa | 389,163–482,148 ms | 479,452 ms |
| Código final, 200 linhas, tabela e equação pareadas | 664,526–754,664 ms | 696,730 ms |

Essa redução local não encerra a necessidade de melhorar a fluidez. A meta de 100 ms não está comprovada e o perfil misto continua perceptivelmente mais caro. A colagem inicial do perfil de 200 linhas levou cerca de 2,63 s sem os objetos e 3,31 s com o par; ela ainda cria muitas vistas e tem custo próprio. As exclusões consecutivas, digitação seguinte e reversões conservaram o conteúdo esperado.

O perfil opcional `INDE_EDITOR_PROFILE=1` acrescenta tempos sem conteúdo autoral para adoção do Documento, atualização de alvos, reconciliação, reconstrução completa e atualização das vistas. A rotina de adoção medida inclui a conferência do motor e a obtenção de sua projeção para a interface; não se atribui todo esse tempo ao disco.

## Percurso novo e regressões

O teste de entrada real foi dividido em dois casos que compartilham a preparação normal: o percurso anterior de especialidades/objetos e o novo de vazios/fluidez. A CI executa ambos. O teste novo exercita perfis de 20 e 200 linhas, depois repete com tabela e equação pareadas lado a lado. O número efetivo de parágrafos inclui a abertura e os vazios.

O percurso confere:

- Colagem exata, sem retirar vazios por normalização automática.
- Delete, navegação confirmada até o vazio, Backspace e três exclusões consecutivas.
- Continuação da digitação no ponto lógico correto.
- Conservação da instância de uma vista de parágrafo não afetada.
- Desfazer até recuperar a prosa e os vazios; Refazer até recuperar o estado revisado.
- Escolha do vizinho no combo por teclas realmente suportadas e confirmação do ID antes de formar a linha.
- Conservação da linha composta durante edição, Desfazer e Refazer.
- Salvamento pelo controle normal e encerramento por Ctrl+Q.
- Conteúdo canônico igual ao snapshot salvo no auxiliar, recursos/arranjo presentes, integridade SQLite e ausência de violações de chaves estrangeiras.

A prova nativa de projeção acrescenta exclusão de vazio, ajuste explícito dos intervalos após divisão e nova edição através da vista conservada. As provas anteriores de Unicode, formatação, entrada recusada, seleção entre parágrafos e visibilidade permanecem.

A otimização também cobre uma recusa de substituição entre parágrafos: o buffer local já pode ter sido modificado pelo GTK, enquanto a origem permaneceu intacta. Essa vista é restaurada explicitamente a partir da origem, mesmo sem um sinal de alteração do buffer autoritativo. A nova prova negativa confere origem e vista, evitando que uma rejeição deixe texto não aceito aparente.

O driver aguarda o formulário antes de rolar e usa a margem para não alterar combos sob o ponteiro. End não escolhe o último item de um `GtkTreePopover` aberto: o teste percorre opções com Down e confirma com Return. Esses ajustes do driver não são apresentados como implementação de uma nova interface de composição.

## Reprodução

```sh
cmake --build build-nativo --target inde inde_object_native_tests inde_object_text_projection_tests --parallel 2
INDE_REAL_INPUT_CASE=fluency python3 tests/run_obj01_real_input.py
INDE_REAL_INPUT_FULL_SUITE=1 python3 tests/run_obj01_real_input.py
```

O filtro opcional `INDE_REAL_INPUT_CASE` seleciona um nome de teste para diagnóstico; a execução normal e a CI não aplicam esse filtro. `INDE_FLUENCY_PARAGRAPHS` permite escolher o perfil sintético. Perfis acima do conjunto final validado não são declarados suportados apenas porque o parâmetro os aceita.

## Estado do plano e fronteiras

Este é o primeiro corte de retomada de E1a. A reconstrução visual foi reduzida e ganhou prova de conservação das vistas. Não se declara concluído o bloco E1/E1a/E2, uma transação entre bancos, histórico global, todas as falhas de sessão, a meta de 100 ms ou todos os 45 percursos de maturidade.

Continuam pendentes a redução do trabalho síncrono de adoção/checkpoint com garantias adequadas, perfis maiores, comportamento prolongado e uma medição de pintura com menos interferência do observador. Depois vêm as correções de inserção após divergência de ordens e de movimentação da linha inteira, conforme E2. Ctrl+Home/End entre vistas e outras equivalências de navegação continuam no alcance de E3.

Os projetos autorais, main, fontes fixadas, schema canônico e formato do auxiliar permanecem fora das alterações. A demonstração deste corte usa somente o projeto sintético produzido pelo teste, no mesmo executável nativo do editor normal. Aceite humano permanece aberto.

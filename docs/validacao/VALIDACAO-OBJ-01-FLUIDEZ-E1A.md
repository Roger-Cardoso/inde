# OBJ-01 — retomada e primeiro corte de fluidez

Data: 26/11/2026. Branch `feature/inde-obj-01-foundation`. Base `ad9189c6ca7a256175adf8d87d2984eecf21d8c6`. Continuidade do [plano de maturidade](../arquitetura/PLANO-MATURIDADE-EDITOR-OBJ-01.md), com prioridade ao relato de pausas ao apagar parágrafos vazios.

## Problema e resultado do corte

Apagar uma quebra de parágrafo podia retirar uma identidade da apresentação e provocar a reconstrução da folha inteira. As vistas nativas, menus e círculos eram recriados, mesmo quando quase todo o conteúdo continuava válido. A lista de objetos também era reconstruída integralmente. A atualização de seleção podia reaplicar tags sem necessidade.

O corte conserva as unidades visuais existentes quando apenas a estrutura textual muda e os objetos, arranjos e propriedades de apresentação permanecem compatíveis. Retira as unidades textuais que desapareceram, cria as novas e reordena as sobreviventes. Os intervalos de cada vista são conferidos e atualizados a partir dos limites canônicos atuais. Os outros objetos e linhas compostas compatíveis conservam seus widgets.

A lista de alvos retira somente os IDs ausentes, altera os rótulos que mudaram e insere/reordena os demais quando necessário. A seleção vazia é limpa explicitamente. A formatação local só é reconstruída quando o conteúdo, intervalo ou tags exigem atualização. Mudanças de seleção continuam projetadas.

Há caminho de reconstrução completa para alterações incompatíveis. Na primeira montagem, os rótulos temporários são substituídos por vistas editáveis reais; não podem ser conservados como se fossem campos de escrita. Um arrasto é cancelado antes de reconciliar uma mudança estrutural. Contextos de entrada provisória e substituição entre parágrafos continuam protegidos.

Um segundo gargalo estava na adoção de uma alteração textual pelo agregado: cada estado do histórico voltava a decodificar e validar o mesmo Documento. A operação agora constrói uma projeção verificada do Documento recebido e a reutiliza na validação de cada estado. Uma captura anterior também pode ser reaproveitada dentro dessa única chamada quando projeto e snapshot coincidem exatamente. Ela é invalidada diante de qualquer diferença; não há cache entre operações.

Cada estado continua passando pelas regras de objetos, ordem, arranjo, referências e limites. O contexto de validação exige coincidência de projeto, snapshot original, identidade do Documento e hash do texto corrente. A leitura de disco continua pelo caminho completo; quando ele acaba de produzir uma projeção verificada, aproveita essa mesma projeção em vez de decodificá-la outra vez. Checkpoints, readback, orçamento do histórico e momento de confirmação permanecem. Nenhum debounce de salvamento ou redução de durabilidade foi introduzido.

## Medições e seus limites

O percurso usa X11 em display, gerenciador de janelas, D-Bus e projeto sintético privados. Cria o projeto pela interface, cola o texto pelo clipboard e apaga vazios com teclas reais. O observador registra o estado, a geometria dos controles, a seleção de combos e a identidade das vistas; não preenche campos nem executa comandos.

As amostras `observed_ms` incluem o envio de eventos e a confirmação pelo observador de baixa prioridade, que consulta a interface a cada 100 ms. Não são medidas puras de pintura ou latência percebida; a observação pode acrescentar custo. Elas não certificam o objetivo de resposta de até 100 ms do plano.

A referência anterior à reconstrução incremental registrou três exclusões com aproximadamente 3,06–3,16 s no perfil com 200 linhas de prosa e nove vazios iniciais. Esse ensaio foi interrompido no passo posterior de navegação/Backspace, antes de o teste aguardar a confirmação do novo foco. As amostras de Delete são evidência de lentidão; o ensaio anterior não é declarado aprovado como percurso completo. Compilação concorrente e a ausência de uma medição de pintura independente também limitam a comparação a uma orientação local, sem fator universal de aceleração.

A suíte local final aprovou **99/99 testes em 163,356 s**, incluindo a substituição recusada, histórico de 32 passos e os dois percursos reais. A execução final ocorreu sem compilação concorrente. Código final: `a3d8159cb21076c0b39ad8279acd17d1d4ba810a`. A aprovação local anterior de 99/99 em 179,242 s pertence à primeira iteração; seu log e manifesto são preservados como tal.

| Perfil | Delete individual, mínimo–máximo observado | Mediana das três exclusões |
|---|---|---|
| Referência anterior, 20 linhas de prosa | 356,332–442,838 ms | 358,802 ms |
| Referência anterior, 200 linhas de prosa | 3.057,502–3.159,410 ms | 3.067,105 ms |
| Código final, 20 linhas de prosa | 108,227–189,258 ms | 189,209 ms |
| Código final, 200 linhas de prosa | 391,279–669,533 ms | 391,325 ms |
| Código final, 200 linhas, tabela e equação pareadas | 485,761–496,464 ms | 486,368 ms |

Essa redução local não encerra a necessidade de melhorar a fluidez. A meta de 100 ms não está comprovada e o perfil misto continua perceptivelmente mais caro. A colagem inicial do perfil de 200 linhas levou cerca de 2,55 s sem os objetos e 3,15 s com o par; ela ainda cria muitas vistas e tem custo próprio. As exclusões consecutivas, digitação seguinte e reversões conservaram o conteúdo esperado.

Um ensaio separado, sem GTK, mede a edição canônica, a adoção pelo agregado e a obtenção de sua vista. Usa 200 linhas, vazios iniciais e 0, 8 ou 32 alterações de apresentação no histórico. A tabela mostra a mediana de três adoções consecutivas por perfil; não inclui pintura, envio de teclas nem o observador. Os binários anterior e final são independentes e têm hashes registrados; a fonte do ensaio e as nove amostras de cada execução estão no pacote de evidências.

| Alterações de apresentação preparadas | Adoção antes de reaproveitar projeções | Adoção no código final |
|---|---|---|
| 0 | 125,317 ms | 84,414 ms |
| 8 | 411,382 ms | 139,779 ms |
| 32 | 1.382,590 ms | 260,757 ms |

Esse ensaio localiza um custo que cresce com o histórico. Ele não substitui os tempos do percurso completo nem certifica latência universal. Não foi medido percentil 95/99 ou sessão prolongada nesta retomada.

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

A prova nativa do vínculo com o Documento também cria 32 alterações de apresentação, adota uma exclusão textual, desfaz e refaz todos os 32 passos e salva/reabre. Cada passo deve manter o texto novo e recuperar a largura correta; a primeira configuração deve reaparecer no fim do Desfazer. As provas anteriores de histórico com movimentação autoral, alteração de timestamp, divisão e recusa de outro Documento continuam presentes.

Uma prova local separada instrumentou o domínio e suas dependências de Documento com ASan/UBSan, incluindo detecção de vazamentos. Ela usa `Session` em modo vivo, 32 passos de apresentação, edição canônica de parágrafos, Desfazer/Refazer, recusa de outro Documento e reabertura do estado salvo. **Passou com saída 0 e sem diagnóstico de memória.** Não instrumenta toda a aplicação GTK. A tentativa inicial no ambiente restrito terminou por impedimento do LeakSanitizer à inspeção do processo; esse log foi preservado, e o mesmo binário passou fora dessa restrição.

A otimização também cobre uma recusa de substituição entre parágrafos: o buffer local já pode ter sido modificado pelo GTK, enquanto a origem permaneceu intacta. Essa vista é restaurada explicitamente a partir da origem, mesmo sem um sinal de alteração do buffer autoritativo. A nova prova negativa confere origem e vista, evitando que uma rejeição deixe texto não aceito aparente.

O driver aguarda o formulário antes de rolar e usa a margem para não alterar combos sob o ponteiro. End não escolhe o último item de um `GtkTreePopover` aberto: o teste percorre opções com Down e confirma com Return. Esses ajustes do driver não são apresentados como implementação de uma nova interface de composição.

A primeira CI desta retomada, no commit `e21b1955ec38ab5fbdfa1c421b0798d35a02cb3f`, **falhou**: o novo percurso enviou Delete enquanto o cursor ainda estava no fim da colagem. Os outros 25 trabalhos passaram, mas isso não aprova a execução. O driver passou a aguardar foco real na vista inicial e cursor/seleção canônicos exatamente no fim da abertura antes de enviar a exclusão. O registro da falha é conservado junto das evidências, separado da execução final.

A [CI final 45838699855](https://github.com/Roger-Cardoso/inde/actions/runs/45838699855), no commit `a3d8159cb21076c0b39ad8279acd17d1d4ba810a`, **aprovou 26/26 trabalhos**. Inclui fontes fixadas, matrizes GCC/Clang com/sem sanitizers e aplicação nativa. O trabalho nativo compilou o aplicativo real, aprovou matemática, os quatro testes de vínculo/projeção/GTK, quatro de publicação e os **dois percursos X11 em 79,035 s**. Os tempos de interface locais acima não são substituídos pelos tempos do equipamento remoto.

## Reprodução

```sh
cmake --build build-nativo --target inde inde_object_native_tests inde_object_text_projection_tests --parallel 2
INDE_REAL_INPUT_CASE=fluency python3 tests/run_obj01_real_input.py
INDE_REAL_INPUT_FULL_SUITE=1 python3 tests/run_obj01_real_input.py
```

O filtro opcional `INDE_REAL_INPUT_CASE` seleciona um nome de teste para diagnóstico; a execução normal e a CI não aplicam esse filtro. `INDE_FLUENCY_PARAGRAPHS` permite escolher o perfil sintético. Perfis acima do conjunto final validado não são declarados suportados apenas porque o parâmetro os aceita.

## Estado do plano e fronteiras

Este é o primeiro corte de retomada de E1a. A reconstrução visual foi reduzida e ganhou prova de conservação das vistas. Não se declara concluído o bloco E1/E1a/E2, uma transação entre bancos, histórico global, todas as falhas de sessão, a meta de 100 ms ou todos os 45 percursos de maturidade.

Continuam pendentes a redução adicional do trabalho síncrono de adoção/checkpoint com garantias adequadas, perfis maiores, comportamento prolongado e uma medição de pintura com menos interferência do observador. O reaproveitamento de projeções reduz a causa medida no histórico, mas não elimina a dependência do tamanho do Documento e do número de estados. Depois vêm as correções de inserção após divergência de ordens e de movimentação da linha inteira, conforme E2. Ctrl+Home/End entre vistas e outras equivalências de navegação continuam no alcance de E3.

Os projetos autorais, main, fontes fixadas, schema canônico e formato do auxiliar permanecem fora das alterações. A demonstração deste corte usa somente o projeto sintético produzido pelo teste, no mesmo executável nativo do editor normal. Aceite humano permanece aberto.

## Evidências e demonstração

O [pacote verificável](evidencias-obj01-fluidez-e1a/README.md) separa código final, primeira iteração, CI recusada, referência parcial e ensaio sem GTK. `verificar.py` confere fontes, compactados, evidências e vínculo da CI sem abrir projeto nem gravar dados. O relatório de integridade usa o projeto de teste fechado; a demonstração abre outra cópia, com nome distinto.

A cópia local está em `dados/obj01-retomada-e1a/Demo-fluidez-final.inde`, na janela **Demonstração — fluidez de parágrafos — INDE**, documento **Documento de uso real**. O texto já inclui tabela e equação na mesma linha. Para rever a falha original, pode-se colar um trecho com vazios nesse documento sintético, apagar as quebras com Delete/Backspace, escrever logo depois e usar Desfazer/Refazer. O controle normal de salvar e Ctrl+Q exercitam salvamento e saída. A execução automática já percorreu essas ações com conteúdo esperado e encerramento normal.

Para reabrir a cópia pela preparação existente:

```sh
scripts/demos/Abrir-demo-folha-comum.sh "$PWD/dados/obj01-retomada-e1a/Demo-fluidez-final.inde"
```

O projeto é uma pasta no formato atual do INDE. Não representa adoção de arquivo único `.inde`. A janela de demonstração da primeira iteração, caso ainda aberta, foi preservada; a janela identificada acima corresponde ao código final.

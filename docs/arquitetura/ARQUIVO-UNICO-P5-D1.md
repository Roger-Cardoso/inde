# P5-D1 — confirmação em segundo plano

Continuidade autorizada após o usuário informar “teste aprovado” para P5-C e pedir prosseguimento. Base `21ae72c9718659d0bddd5472f7203209d7396f40`, branch `feature/inde-p5-single-file`. O [contrato C](ARQUIVO-UNICO-P5-C.md) e os perfis de armazenamento permanecem. Não há migração de schemas, projetos autorais, alterações em main ou implementação web.

## Percursos deste corte

Salvar/Ctrl+S, fechar o projeto ativo em arquivo único e encerrar o programa passam a confirmar o arquivo por um trabalhador exclusivo. A janela continua processando eventos e exibindo a fase. A edição fica temporariamente pausada, preservando o conteúdo que está sendo conferido.

O formulário oferece Cancelar salvamento e Sair após salvar. Um pedido de saída pode ser retirado por Continuar no editor. Ctrl+Q e o fechamento da janela durante a operação registram o pedido; não destroem a sessão usada pelo trabalhador. Uma falha ao encerrar conserva a escolha anterior de voltar ou encerrar preservando a área de recuperação.

O corte é **D1**: a confirmação de abertura/troca, criação, cópias e consulta/restauração do histórico continua síncrona. O esvaziamento dos escritores nativos e a preparação do Documento antes da confirmação também permanecem no fluxo da interface. Este corte não permite continuar escrevendo sobre a mesma área durante a captura. Uma futura edição concorrente exige outra estratégia de snapshot e revisão; não basta retirar a pausa.

## Propriedade e ciclo

`ConfirmationTask` recebe uma sessão já preparada. O trabalhador acessa somente `FileSession`, armazenamento, transporte e controle de operação. Nenhum widget ou `ProjectService` atravessa essa fronteira. O proprietário deve manter a sessão viva e sem outras chamadas até a conclusão. A interface permite uma única tarefa e bloqueia comandos de projeto enquanto ela existe.

A conclusão publica o resultado por atômico com release/acquire. A interface recolhe o trabalhador antes de reutilizar a sessão, ler a sequência ou executar a continuação de fechamento. A destruição pede cancelamento quando permitido e aguarda o trabalhador antes de liberar a sessão; nenhum trabalhador fica destacado usando um proprietário destruído.

A área de trabalho é drenada pelos serviços nativos antes de iniciar. O trabalhador executa a captura/publicação e a conferência final de recuperação. O sucesso aparece depois dessas etapas. Falhas de leitura, integridade, quota, gravação ou recibo conservam seus significados anteriores; não são transformadas em sucesso ou cancelamento genérico.

## Cancelamento e confirmação final

`OperationControl` usa um estado atômico para decidir a disputa entre cancelar e iniciar a confirmação final. Se o cancelamento vence, os checkpoints lançam `operation_cancelled`; a transação é revertida e o novo estado não é publicado. A área editável permanece com o trabalho do autor.

A barreira final fica depois da preparação e validação do candidato, imediatamente antes de COMMIT. Se ela vence, novas solicitações de cancelamento são recusadas. O programa aguarda a confirmação e o recibo; não declara que desfez uma gravação já iniciada. O mesmo fechamento do controle cobre a confirmação sem mudança, que não acrescenta uma revisão.

Leitura e hash de origem têm checkpoints por blocos de até 64 KiB. Também há checkpoints na montagem e publicação. Chamadas já iniciadas ao sistema de arquivos e ao SQLite precisam retornar antes de observar cancelamento; não há interrupção forçada de uma conexão nem promessa de cancelamento instantâneo de qualquer I/O.

A conferência final e a marcação da recuperação continuam depois do salvamento. Se um recibo falha após a publicação, o resultado propaga a incerteza e a sessão exige conferência na reabertura, conforme P5-B. Cancelar não apaga o trabalho nem as versões anteriores.

## Interface, foco e saída

A pausa usa a camada modal nativa existente. São mostradas fases reais — leitura, conferência, preparação, confirmação e recuperação — sem percentual inventado. O cancelamento fica indisponível na confirmação final. O foco muda para uma indicação sem ação quando necessário, evitando que Enter passe automaticamente de Cancelar para Sair.

Ao terminar, a superfície anterior é reativada. Para parágrafos da folha comum, a retomada usa a rotina protegida de foco do editor existente. Foco GTK direto podia selecionar o objeto que contém o parágrafo, reconstruí-lo e perder o ponto de escrita. Outros controles usam uma referência fraca, conferida antes da restauração. Não se recria o Documento inteiro para devolver o foco.

A autorização de saída confirmada é consumida uma vez. Fechar um projeto continua pela rotina existente somente após sucesso; cancelar mantém o projeto aberto. Pedir saída durante a confirmação e escolher continuar não encerra o programa posteriormente. Uma confirmação de fechamento não atualiza o manifesto só por estar fechando, conservando o contrato anterior.

## Validação e fronteiras

Os testes disputam a fronteira atômica, cancelam um candidato já preparado, recusam cancelamento tardio, destroem o controlador com tarefa ativa e conferem recibo incerto nos dois perfis. ASan/UBSan/LeakSanitizer e ThreadSanitizer são registrados separadamente.

O teste GTK usa teclado e mouse reais. Somente o host explícito de observação dispõe de gates por arquivo para manter o trabalhador numa fase enquanto verifica a janela, cancela, pede/retira saída ou interrompe o processo. O aplicativo normal não lê esses arquivos nem recebe atrasos artificiais. Mutações e confirmações continuam nos serviços reais; não se emitem sinais de botões para simular execução.

A Fase 5 e a maturidade global continuam abertas. O total de trabalho de I/O não desaparece, a edição é pausada e operações fora do escopo D1 ainda podem bloquear a interface. As pendências de fluidez e de E3–E7 continuam no plano de maturidade. Não há coleta automática de áreas ou histórico, aumento de quotas ou certificação de obras reais.

[Demonstração](../roteiros/ARQUIVO-UNICO-P5-D1.md), [validação](../validacao/VALIDACAO-P5-D1.md), [controle](../../cpp/project_file_operation.hpp), [trabalhador](../../cpp/project_file_task.hpp), [integração](../../cpp/main_window_project_file_task.cpp).

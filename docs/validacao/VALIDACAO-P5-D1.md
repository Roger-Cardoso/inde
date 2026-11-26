# Validação P5-D1 — confirmação, cancelamento e saída

Data: 2026-11-26. Base `21ae72c9718659d0bddd5472f7203209d7396f40`, branch `feature/inde-p5-single-file`. Continuidade após o usuário informar aprovação do teste P5-C. Esse aceite se refere ao corte anterior; a validação técnica de D1 é separada do seu futuro aceite humano.

O [contrato](../arquitetura/ARQUIVO-UNICO-P5-D1.md) delimita a confirmação assíncrona de Salvar, fechar projeto e encerrar. O manifesto em `evidencias-p5-d1` identifica fontes, resultados locais e CI. Os manifestos A/B/C continuam evidências históricas das revisões indicadas neles.

## Casos novos

| Camada | Verificação |
| --- | --- |
| Controle | 100 disputas entre cancelar e selar a confirmação, exigindo um único vencedor |
| Transação | Cancelamento após escrever e validar o candidato, antes de COMMIT, conserva estado e histórico anteriores |
| Trabalhador | Cancelar antes de capturar, tentar novamente, recusar cancelamento tardio, confirmação sem mudança e destruição com tarefa ativa |
| Recuperação | Recibo falha após publicação: erro de incerteza, reabertura sem duplicar o estado, área preservada |
| Perfis | Sessões anteriores e por blocos exercitadas pelo trabalhador |
| GTK | Eventos continuam chegando, edição pausada, cancelar preserva arquivo e texto, tentativa seguinte funciona e foco volta ao texto |
| Saída | Pedido pendente, retirada do pedido, fechamento da janela aguarda, falha permite sair preservando e reabrir |
| Interrupção real | Processo encerrado enquanto o candidato está preparado: reabertura nativa reverte a transação incompleta e oferece o trabalho preservado |

O teste gráfico controla somente o agendamento do trabalhador por gates do host de observação. Usa X11 real para as ações e lê o conteúdo efetivamente confirmado no SQLite. O aplicativo normal não contém pausas de demonstração. A prova de retomada inclui continuar digitando depois de cancelar/confirmar, sem procurar novamente o parágrafo.

## Resultados locais

| Verificação | Resultado |
| --- | --- |
| GCC Release | 5/5, 12,73 s |
| Clang + ASan/UBSan/LeakSanitizer | 5/5, 27,82 s |
| Clang + ThreadSanitizer, coordenador | 1/1, 1,58 s |
| Suíte nativa completa | 72/72, 184,85 s |
| Interface e integrações existentes | 103/103, 268,35 s |
| Percurso GTK de confirmação D1 | 14 verificações aprovadas |
| Percursos GTK anteriores | 14 verificações em cada perfil, anterior e por blocos |
| Demonstrador normal com recurso de 16 MiB | Salvar, Sair por teclado e integridade aprovados |
| Preservação | 758 fontes integradas e três compactados conferidos |

Os sanitizers instrumentam as unidades da biblioteca; não significam instrumentação completa do GTK e dos binários de sistema. ThreadSanitizer cobre o coordenador e seus casos de concorrência. A prova gráfica usa aplicação real com críticos GTK tratados como falha. Os passes de 72 e 103 casos antecedem somente as guardas finais contra acesso síncrono à sessão durante o trabalhador. O percurso gráfico D1 foi repetido depois desse reforço, incluindo tentativas de Salvar, histórico e cópia durante a operação. Código validado: `416aeb33e6098396e01b4e9fc5d7621ef75dcdd1`. A [CI 45904238149](https://github.com/Roger-Cardoso/inde/actions/runs/45904238149) terminou em 26/11/2026, às 22:34:48 UTC, com **6/6 jobs aprovados**: quatro combinações GCC/Clang com e sem ASan/UBSan/LeakSanitizer (cinco testes em cada), ThreadSanitizer (um teste de concorrência) e integração nativa. Esta última compilou a aplicação do zero, passou sete testes P5, o demonstrador anterior, os percursos gráficos nos dois perfis, o novo percurso D1 e a medição de crescimento do histórico. A [execução inicial 45903203724](https://github.com/Roger-Cardoso/inde/actions/runs/45903203724), anterior ao reforço final das guardas, também passou 6/6; as evidências de CI anexadas aqui correspondem à revisão final.

## Correções durante o corte

A primeira integração retomava o foco diretamente pelo GTK. Na folha comum, esse foco podia ativar a seleção do objeto que contém o parágrafo e reconstruir a vista, deixando o trecho fora da área visível. A integração passou a usar a rotina protegida do editor para esse caso e conserva referência fraca para os demais controles. Também reativa a superfície antes de devolver o foco.

O verificador inicial procurava um botão desabilitado pela mesma regra usada para clicar. Widgets desabilitados não precisam ser destinos de clique; o teste passou a conferir seu estado visível/sensível diretamente, conservando eventos reais para as ações.

O driver anterior agora espera uma observação posterior ao salvamento e à conclusão do trabalhador antes da próxima ação. A publicação do banco pode preceder a finalização do recibo e da conferência; ler somente o conteúdo salvo não prova que a interface já liberou a próxima operação.

## Limitações

A drenagem e preparação dos escritores nativos permanece na interface. Operações de abertura/troca, criação, cópia e histórico continuam com trechos síncronos. Edição e reordenação continuam com as pendências já registradas. Este corte comprova resultados funcionais e continuidade do processamento de eventos; não certifica a latência global do programa ou todo cenário físico de falha.

Main, projetos autorais, fontes congeladas e compactados são preservados. Os testes e a demonstração usam dados fictícios. Áreas reservadas e versões anteriores não são apagadas automaticamente. Os limites de transporte e armazenamento permanecem os mesmos.

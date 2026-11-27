# Validação P5-D2 — operações do arquivo

Data: 27/11/2026. Branch `feature/inde-p5-single-file`, base `4da5e09e8ac53848752bbe6f7d084ea2128f0593`. D1 foi aprovado pelo usuário. O aceite humano deste novo lote permanece separado dos testes.

O [contrato D2](../arquitetura/ARQUIVO-UNICO-P5-D2.md) reúne abertura/troca, criação, duas cópias, consulta paginada e restauração. Todos os projetos de teste são fictícios e novos. A cópia congelada e os três arquivos compactados foram novamente conferidos; main e projetos autorais não receberam alterações deste corte.

## Resultados locais já conferidos

- Núcleo independente: 5/5, com os dois perfis, publicação sem substituição, cancelamento e recibo incerto depois da restauração.
- Instrumentação do núcleo P5 com Clang ASan/UBSan/LeakSanitizer: 5/5; ThreadSanitizer: 1/1.
- Percurso gráfico D2: 18 verificações, com teclado e mouse reais, histórico de mais de cem estados, cancelamento, preservação do vínculo anterior, adoção e saída pendente.
- Percursos anteriores: 14 verificações no formato integral, 14 por blocos e as 14 verificações gráficas D1.
- Compatibilidade com pastas: 6 controles gráficos complementares de abertura/criação, cancelamento, confirmação da origem e encerramento; roteiro local conservado em `evidencias-p5-d2/gtk-legacy-driver.py`, separado dos percursos de CI.
- Lançador entregue: abriu o aplicativo normal com um arquivo fictício, recebeu Salvar/Encerrar por teclado e teve sua integridade conferida.
- Suíte nativa: 71 testes passaram na rodada completa. O novo teste de conflito foi ajustado para respeitar a trava de sessão e passou em reexecução específica (1/1). Nenhum teste ficou com falha pendente.

O novo teste de conflito inicialmente tentava abrir duas sessões protegidas no mesmo workspace, operação corretamente impedida. Ele agora injeta uma revisão externa coerente diretamente no banco fictício e comprova a recusa, mesmo com snapshot local limpo.

A primeira rodada gráfica encontrou a regravação redundante dos recibos da composição depois de confirmar a origem. A correção remove a drenagem repetida e verifica o salvamento idêntico sem regravar o banco. O teste de interrupção também passou a aguardar o estado nativo persistido, em vez de supor que um segundo fixo bastava sob carga. A cópia de recuperação pendente do ensaio inicial continuou preservada.

## Regressões do editor

A execução geral cobriu 103 testes em 283,871 s. Passaram 102; a única falha foi o mesmo novo teste de conflito, compilado antes da correção da preparação descrita acima. Sua reexecução Python passou (1/1, 1,823 s), assim como a reexecução CTest (1/1, 2,30 s). Os logs da primeira rodada e das reexecuções são conservados separadamente: não são apresentados como uma execução única com 103/103 e 72/72.

Os percursos de parágrafos vazios, entrada de texto, tabelas, imagens, especialidades, fórmulas, arrasto estrutural e reordenação lateral continuaram passando. As observações de latência da suíte incluem entrada X11 e o observador, sob carga de outros testes. Continuam superiores à meta editorial de 100 ms em vários casos; este lote não é apresentado como correção global dessa latência.

## CI final e proveniência

Revisão validada: `2fb977c6ade45afdf461b246adfbfe88d68ea92b`. [Execução P5-D2 — seis trabalhos aprovados](https://github.com/Roger-Cardoso/inde/actions/runs/46017286321), encerrada com `success`.

| Trabalho remoto | Resultado |
| --- | --- |
| GCC, compilação estrita | 5/5 testes P5 |
| GCC, ASan/UBSan/LeakSanitizer | 5/5 testes P5 |
| Clang, compilação estrita | 5/5 testes P5 |
| Clang, ASan/UBSan/LeakSanitizer | 5/5 testes P5 |
| Clang, ThreadSanitizer | 1/1 teste do trabalhador e disputas |
| Editor nativo | Compilação limpa; 8/8 testes de integração/objeto; demonstração, ciclo integral, ciclo por blocos, D1, D2 e regressão do crescimento do histórico aprovados |

O trabalho nativo remoto recompilou e passou pelo teste de objeto já com a preparação corrigida. Seus oito testes não são apresentados como reexecução remota dos 72 testes locais ou dos 103 da suíte geral. Da mesma forma, as quatro combinações de compilador/instrumentação exercitam variantes dos mesmos cinco testes P5.

O [manifesto verificável](evidencias-p5-d2/manifesto.json) conserva hashes das fontes e das evidências. As fontes listadas foram comparadas byte a byte com a revisão de CI. `ci-execucao.json` registra o resultado e a revisão retornados pelo GitHub; `ci.json` e `ci-resultados.txt` conservam trabalhos, etapas e trechos de resultados. O roteiro complementar de pastas foi validado localmente e está identificado como evidência posterior, sem modificar o código da revisão aprovada pela CI.

Os manifestos A–D1 permanecem históricos, vinculados às suas próprias revisões. Esta entrega não os reescreve para aparentar que aqueles resultados foram executados sobre D2.

## Regressão do armazenamento

O ensaio sintético de 4 MiB conservou a restauração exata e o reaproveitamento de recursos: 26.099.712 bytes novos no perfil integral e 393.216 no perfil por blocos para a mesma sequência de alterações. Esses números medem crescimento de recursos no histórico, não velocidade de edição ou ganho introduzido pelo D2. Ambos os resultados são conservados como regressão de P5-C.

## Reproduzir e conferir

O [roteiro nativo](../roteiros/ARQUIVO-UNICO-P5-D2.md) descreve a demonstração entregue. Na raiz desta cópia de trabalho, `python3 tests/project_file/verify_operations_evidence.py` confere fontes, relatórios, evidências e o resultado de seis trabalhos da CI registrados no manifesto. A execução remota foi concluída antes de fechar o manifesto.

Os testes gráficos são `tests/project_file/test_native_operations.py`, `test_native_session.py` e `test_native_confirmation.py`, cada qual com uma pasta de saída nova e sessão X11/D-Bus privada. `P5_TRANSPORT_BLOCKS=1` escolhe o perfil por blocos na fixture de sessão. Os dois roteiros complementares em `evidencias-p5-d2` verificam o lançador normal e as transições para pastas; não são apresentados como etapas da CI.

## Limites preservados

A edição fica pausada durante as operações. Drenagem dos escritores, adoção do projeto, abertura do domínio nativo e reconstrução das especialidades ainda têm trabalho na interface. Não há garantia global de 100 ms, interrupção instantânea de qualquer chamada ao sistema, retenção automática, mudança de quotas, migração de obras reais ou conclusão da Fase 5/E3–E7.

[Demonstração nativa](../roteiros/ARQUIVO-UNICO-P5-D2.md).

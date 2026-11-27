# Validação P5-D3 — preparação nativa e recuperação

Data: 27/11/2026. Base `2fad9a3`, branch `feature/inde-p5-single-file`. D2 foi aprovado pelo usuário; esse aceite não é estendido automaticamente a D3. O [contrato D3](../arquitetura/ARQUIVO-UNICO-P5-D3.md) delimita preparação, escritores, confirmação de objetos e passagem para a folha.

## Resultado conferido localmente

- Núcleo P5: 5/5, incluindo transmissão da preparação, cancelamento, falha, entrega duplicada e destruição enquanto aguarda.
- Núcleo P5 com Clang ASan/UBSan/LeakSanitizer: 5/5; Clang ThreadSanitizer: 1/1.
- Novo teste nativo de recuperação: journals quente/frio interrompidos por processo, igualdade dos campos, preservação do formato, ausência de banco novo, recusa de journal órfão, links, WAL e versão desconhecida.
- Novo roteiro gráfico: 23 verificações aprovadas, incluindo reserva SQLite real durante a espera, escrita recente, cancelamento, nova tentativa, journal frio, interrupção entre escritores e retomada.
- Regressões gráficas: D2, 18 verificações; sessão integral, 14; sessão por blocos, 14; D1, 14. Todos aprovados na revisão final.
- Suíte geral do editor: 103/103 em 269,317 s, incluindo entrada real, especialidades, objetos, parágrafos vazios e reordenação lateral.

- Conjunto nativo final: 73/73 em 170,29 s, incluindo o novo teste da recuperação privada.
- Lançador entregue: abriu o aplicativo normal, recebeu Salvar/Encerrar por teclado e teve o arquivo conferido.
- Transições complementares para pastas: 6 controles gráficos aprovados, em roteiro local separado da CI.

## CI final

Revisão validada: `3add1e58765f218d97ee5ffca5528aa2168d7ec7`. [Execução D3 — seis trabalhos aprovados](https://github.com/Roger-Cardoso/inde/actions/runs/46026854904), concluída com `success`.

| Trabalho remoto | Resultado |
| --- | --- |
| GCC, compilação estrita | 5/5 testes P5 |
| GCC, ASan/UBSan/LeakSanitizer | 5/5 testes P5 |
| Clang, compilação estrita | 5/5 testes P5 |
| Clang, ASan/UBSan/LeakSanitizer | 5/5 testes P5 |
| Clang, ThreadSanitizer | 1/1 teste do trabalhador e transmissão da preparação |
| Editor nativo | Compilação; 9/9 testes de integração/objeto/recuperação; demonstração; sessões integral e por blocos; D1; D2; D3 com 23 verificações; regressão do crescimento do histórico aprovados |

A primeira tentativa dessa revisão recebeu HTTP 403 do repositório Microsoft pré-configurado no runner durante `apt-get update`, antes de compilar a variante Clang sem instrumentação. O fail-fast cancelou as outras três variantes da matriz. Depois de o trabalho nativo terminar com sucesso, foram repetidos somente os quatro trabalhos afetados; todos passaram. Os resultados do editor e de concorrência foram conservados. A ocorrência está em `ci-infraestrutura.txt`, com os IDs dos logs originais/reutilizados em `ci.json` e `ci-resultados.txt`.

A primeira revisão D3 (`ba6b446`) também concluiu seis trabalhos com sucesso, conforme `ci-primeira-revisao.json`. Ela não substitui a execução final da proteção de saída. Os nove testes remotos nativos não são apresentados como reexecução de todos os 73 testes locais ou dos 103 gerais; as quatro variantes de compilador/instrumentação exercitam combinações dos mesmos cinco testes P5.

## Achado e correção da recuperação

Uma regressão após interrupção deixou o texto principal persistido e um journal privado com cabeçalho ainda não confirmado. Sem texto sujo, os escritores não precisavam gravar novamente. O conversor recusava corretamente o auxiliar que restava. O primeiro ensaio foi preservado em `dados/p5d3-validacao-20261127/gtk-integral`.

O caminho novo passa esse banco ao proprietário e ao SQLite, sem apagar o auxiliar manualmente. Um journal quente é recuperado normalmente. Para o auxiliar frio restante, uma transação reconfirma o mesmo marcador privado de formato 1, conferido sob reserva. Todos os campos dos registros são comparados antes/depois. Não há migração do schema canônico 37, resolução automática de pendências ou retenção/descarte de dados.

Os processos de teste são interrompidos antes/depois da sincronização do journal. Isso não é prova de perda elétrica, hardware defeituoso ou todos os sistemas de arquivos.

## Falhas antes e depois dos recibos

Uma falha ainda na preparação nativa mantém a janela aberta e retira o pedido de saída. Não é oferecida uma opção que prometa preservar trabalho possivelmente só em memória. O teste retira temporariamente a permissão de escrita da pasta de dados, tenta encerrar, confere arquivo/texto preservados e volta a salvar depois de restabelecer a permissão.

Falhas posteriores na publicação do `.inde`, com o trabalho privado já confirmado, conservam a opção anterior de encerrar preservando essa área. O roteiro D1 foi reexecutado para verificar essa distinção.

| Fronteira da falha | Comportamento verificado |
| --- | --- |
| Escritores nativos ainda sem recibo confirmado | Janela permanece aberta, pedido de saída retirado, texto disponível para nova tentativa |
| Cancelamento antes da confirmação do arquivo | Arquivo anterior preservado; escritas privadas já iniciadas podem terminar |
| Gravação do arquivo falha após preparar o trabalho privado | Pode voltar ou encerrar explicitamente preservando a área de recuperação |
| Interrupção depois do texto e antes dos objetos | Retoma a área preservada e confirma a escrita recuperada |
| Journal privado quente ou frio interrompido | SQLite trata o auxiliar, com campos dos registros preservados |

## Escopo das provas

A reserva SQLite do roteiro gráfico retém uma escrita real enquanto o observador continua recebendo eventos. Os gates adicionais só existem no host de teste explícito; o aplicativo normal não lê esses arquivos. Os resultados são produzidos pelos serviços reais, com entrada de teclado/mouse, e não por emissão de sinais de widgets.

Os testes P5 instrumentados cobrem o núcleo e a transmissão entre threads; não são apresentados como instrumentação de todo GTK ou de todos os serviços do aplicativo. O teste da recuperação privada e os percursos gráficos são registrados separadamente.

As observações de fluidez da suíte geral incluem entrada X11 e polling, sob carga de outros testes. A meta global de até 100 ms continua aberta. Drenagem síncrona dos trabalhadores foi retirada deste percurso de preparação, mas captura de valores GTK, Comic alterado, manifesto, recibo raro de desfazer até a base, reabertura excepcional de sessão incerta, adoção e renderização ainda têm trabalho na interface.

## Proveniência e reprodução

As três bases compactadas e os 758 arquivos congelados foram novamente conferidos. Main e projetos autorais são preservados; todos os ensaios usam pastas fictícias novas. Os manifestos A–D2 permanecem históricos nas respectivas revisões.

O roteiro entregue está em [Demonstração P5-D3](../roteiros/ARQUIVO-UNICO-P5-D3.md). O [manifesto de fontes e evidências](evidencias-p5-d3/manifesto.json) identifica a revisão de código e o resultado da CI. As 58 fontes listadas foram comparadas byte a byte com esse commit. `ci-execucao.json` registra a identidade e a conclusão obtidas do GitHub. Na raiz desta cópia, `python3 tests/project_file/verify_preparation_evidence.py` confere os hashes e os seis resultados efetivos.

Os roteiros complementares de lançador normal e transições para pastas foram validados localmente e conservados em `evidencias-p5-d3`, separados dos percursos de CI. A aprovação humana de D3 e uma eventual promoção/release continuam distintas desta validação técnica.

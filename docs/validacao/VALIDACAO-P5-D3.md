# Validação P5-D3 — preparação nativa e recuperação

Data: 27/11/2026. Base `2fad9a3`, branch `feature/inde-p5-single-file`. D2 foi aprovado pelo usuário; esse aceite não é estendido automaticamente a D3. O [contrato D3](../arquitetura/ARQUIVO-UNICO-P5-D3.md) delimita preparação, escritores, confirmação de objetos e passagem para a folha.

## Resultado conferido localmente

- Núcleo P5: 5/5, incluindo transmissão da preparação, cancelamento, falha, entrega duplicada e destruição enquanto aguarda.
- Núcleo P5 com Clang ASan/UBSan/LeakSanitizer: 5/5; Clang ThreadSanitizer: 1/1.
- Novo teste nativo de recuperação: journals quente/frio interrompidos por processo, igualdade dos campos, preservação do formato, ausência de banco novo, recusa de journal órfão, links, WAL e versão desconhecida.
- Novo roteiro gráfico: 21 verificações aprovadas, incluindo reserva SQLite real durante a espera, escrita recente, cancelamento, nova tentativa, journal frio, interrupção entre escritores e retomada.
- Regressões gráficas: D2, 18 verificações; sessão integral, 14; sessão por blocos, 14; D1, 14. Todos aprovados na revisão final.
- Suíte geral do editor: 103/103 em 269,317 s, incluindo entrada real, especialidades, objetos, parágrafos vazios e reordenação lateral.

O conjunto nativo final, a demonstração entregue, as transições complementares para pastas e o resultado remoto serão acrescentados ao fechamento das evidências.

## Achado e correção da recuperação

Uma regressão após interrupção deixou o texto principal persistido e um journal privado com cabeçalho ainda não confirmado. Sem texto sujo, os escritores não precisavam gravar novamente. O conversor recusava corretamente o auxiliar que restava. O primeiro ensaio foi preservado em `dados/p5d3-validacao-20261127/gtk-integral`.

O caminho novo passa esse banco ao proprietário e ao SQLite, sem apagar o auxiliar manualmente. Um journal quente é recuperado normalmente. Para o auxiliar frio restante, uma transação reconfirma o mesmo marcador privado de formato 1, conferido sob reserva. Todos os campos dos registros são comparados antes/depois. Não há migração do schema canônico 37, resolução automática de pendências ou retenção/descarte de dados.

Os processos de teste são interrompidos antes/depois da sincronização do journal. Isso não é prova de perda elétrica, hardware defeituoso ou todos os sistemas de arquivos.

## Escopo das provas

A reserva SQLite do roteiro gráfico retém uma escrita real enquanto o observador continua recebendo eventos. Os gates adicionais só existem no host de teste explícito; o aplicativo normal não lê esses arquivos. Os resultados são produzidos pelos serviços reais, com entrada de teclado/mouse, e não por emissão de sinais de widgets.

Os testes P5 instrumentados cobrem o núcleo e a transmissão entre threads; não são apresentados como instrumentação de todo GTK ou de todos os serviços do aplicativo. O teste da recuperação privada e os percursos gráficos são registrados separadamente.

As observações de fluidez da suíte geral incluem entrada X11 e polling, sob carga de outros testes. A meta global de até 100 ms continua aberta. Drenagem síncrona dos trabalhadores foi retirada deste percurso de preparação, mas captura de valores GTK, Comic alterado, manifesto, recibo raro de desfazer até a base, reabertura excepcional de sessão incerta, adoção e renderização ainda têm trabalho na interface.

## Proveniência e reprodução

As três bases compactadas e os 758 arquivos congelados foram novamente conferidos. Main e projetos autorais são preservados; todos os ensaios usam pastas fictícias novas. Os manifestos A–D2 permanecem históricos nas respectivas revisões.

O roteiro entregue está em [Demonstração P5-D3](../roteiros/ARQUIVO-UNICO-P5-D3.md). O fechamento terá manifesto de fontes/evidências e resultado da CI na revisão de código indicada, com verificador `tests/project_file/verify_preparation_evidence.py`.

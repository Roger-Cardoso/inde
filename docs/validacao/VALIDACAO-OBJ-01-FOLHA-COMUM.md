# OBJ-01 — folha comum e escrita direta

Data: 23/11/2026. Branch `feature/inde-obj-01-foundation`. Base anterior: `0a2631ce1d43732c3a5309d0750ee0c9dbaf0d29`. Implementação inicial: `84a277c814ec6164a26c8b0f2beddbe6e738de6a`; correção de ciclo GTK e versão validada: `e285f667e24ed682bced62fe2cb8c1eefc842310`. Estado: validação local e CI concluídas com aprovação. Este registro cobre o corte de folha comum, sem declarar encerrada toda a Fase 1.

> Atualização de 23/11/2026: o relato posterior de bloqueios mostrou que os testes deste corte não bastavam para declarar usabilidade. Consulte a [correção e validação por entrada real](VALIDACAO-OBJ-01-USO-REAL.md). Os resultados abaixo são históricos e não representam aceite humano.

## Correção de direção

A integração anterior usava uma superfície textual e outra de composição sincronizadas. Ela não cumpria a exigência de trabalhar a mesma folha em todas as especialidades. Este corte faz Texto, Semântica, Linguística, Estrutura, Vínculos, Composição, Tabelas, Fórmulas e Matemática operarem a folha mista, com parágrafos editáveis e objetos presentes.

[Contrato e fronteiras](../arquitetura/OBJ-01-FOLHA-COMUM.md), [roteiro de demonstração](../../tools/obj01-pilot/NATIVO.md), [evidências](evidencias-obj01-folha-comum/).

## O que foi implementado

- A troca de especialidade conserva a mesma instância da folha e os campos de texto quando a estrutura permanece estável. A mudança fica nos instrumentos e realces.
- A digitação em qualquer especialidade atravessa os guardas e o histórico da Escrita existente; não se criou outro Documento autoritativo ou um histórico próprio por caixa.
- Vistas de parágrafo usam coordenadas Unicode e as mesmas tags de formatação, semântica, busca e visibilidade do buffer canônico. Uma vista desatualizada recusa sobrescrever texto novo.
- Enter reconcilia as identidades e devolve o foco ao novo parágrafo. Seleções que atravessam parágrafos são substituídas depois da entrega completa do GTK, em uma transação da Escrita, com barreira de fechamento durante essa entrega.
- A seleção primária do sistema pertence ao buffer canônico. As vistas deixam de disputar sua propriedade; a inscrição do GTK é equilibrada na realização/desrealização dos widgets. Esse comportamento foi investigado no [código oficial GTK 4.8.3](https://github.com/GNOME/gtk/blob/4.8.3/gtk/gtktextview.c) e utiliza a [API pública de seleção](https://docs.gtk.org/gtk4/method.TextBuffer.remove_selection_clipboard.html).
- Busca, referências, alternativas e inspetores devolvem o foco à folha comum. Os comandos de histórico de objetos são nomeados como tal; não se declara um histórico global único.
- Visibilidade e Documento são adotados juntos, inclusive para continuar digitando em um novo parágrafo vazio sob lente. A atualização visual não salva bancos.
- Abrir a folha em um Documento puramente textual não impede sua exportação/publicação anterior. Uma conferência somente de leitura verifica origem, integridade, identidade e ausência de objetos/arranjos; casos incompatíveis recusam gerar uma saída parcial. Equações e demais objetos continuam sem exportação nova.
- Documentos anteriores sem parágrafos canônicos conservam o editor linear e recebem indicação da ação explícita de identificação. Não há conversão automática em lote.

## Verificações

A versão final registra os resultados em `evidencias-obj01-folha-comum/`. Os testes não se limitam a screenshots: criam projetos sintéticos, usam a janela GTK real e conferem o Documento persistido e o banco auxiliar.

O percurso gráfico verifica as nove especialidades, identidade do campo de parágrafo, permanência de tabela/equação/arranjo lado a lado, escrita Unicode em Composição, Desfazer, Enter e continuação de escrita, sinal de composição IME e bloqueio de troca, substituição de seleção entre parágrafos e bloqueio de fechamento, retorno ao texto em uma operação de Desfazer, fórmulas remotas, tabela, menus, lentes, salvamento, biblioteca e reabertura.

O teste isolado de projeção verifica Unicode, offsets do parágrafo vizinho, guarda de edição recusada, vista desatualizada, compartilhamento e retirada de tags, invisibilidade sem remoção, bloqueio de edição e substituição completa de seleção entre parágrafos.

| Conjunto | Resultado local | Evidência |
|---|---|---|
| CI do código final | 26/26 trabalhos; conclusão `success` | `ci.json`, `ci-nativo-testes.txt` |
| Núcleo integrado CTest | 63/63; 101,66 s | `ctest-63.txt` |
| Domínio e compatibilidade, incluindo HTTP local | 22/22; 58,38 s | `compatibilidade-22.txt` |
| Suíte Python completa sobre o commit congelado | 97/97; 47,65 s | `python-97.txt` |
| Quatro verificações nativas em tela compacta | 4/4; 24,75 s | `gtk-4.txt` |
| Percurso GTK real maximizado | Aprovado, 1920 × 1032 | `demo-r6-stdout.txt`, `demo-r6-stderr.txt`, `gtk-smoke.json` |
| Fontes integradas fixadas | 758 arquivos conferidos | `base-schema37.json` |
| Fontes matemáticas fixadas | 245 arquivos conferidos | Saída da suíte Python |
| Arquivos anteriores protegidos | 83/83 inalterados | `preservados-inicio.json`, `preservacao.json` |
| Compactados originais | 3/3 hashes conferidos e inventários internos válidos | `arquivos-originais.json`, `auditor-fontes.txt` |
| SQLite da demo, auxiliar e recuperação | 3/3 `integrity_check=ok`, sem falhas de FK | `integridade-demo.json` |

CI deste código: [execução 45283287790](https://github.com/Roger-Cardoso/inde/actions/runs/45283287790). Resultado: **26/26 trabalhos aprovados**, incluindo as matrizes GCC/Clang com e sem sanitização e o trabalho nativo completo. O commit remoto é exatamente `e285f667e24ed682bced62fe2cb8c1eefc842310`; os metadados estão em `ci.json` e os testes GTK/publicação em `ci-nativo-testes.txt`.

Os testes de HTTP local e display precisaram de acesso a sockets locais. Uma tentativa restrita de CTest recusou esse acesso; os mesmos 22 testes passaram após executar no ambiente com sockets disponíveis. O Xvfb foi extraído de pacotes oficiais em diretório temporário, sem instalação no sistema. Há avisos de keymap de 16 bits no Xvfb, mas as verificações recusam `Gtk-CRITICAL`, `Gdk-CRITICAL` e vazamentos de filhos GTK nos percursos exercitados.

No ensaio de publicação, o serviço real preserva texto simples, rejeita outro projeto e outra versão do Documento, recusa equação tanto antes quanto depois de salvar e permite texto novamente após Desfazer/salvar. Os hashes do banco autoral e do auxiliar permanecem iguais durante a conferência.

## Demonstração e reprodução

Iniciador: `bash scripts/demos/Abrir-demo-folha-comum.sh`. A amostra final é `dados/obj01-folha-comum-r6/Projeto.inde`, com perfil próprio. As amostras intermediárias não são a entrega final e não foram apagadas. No Documento **Matemática na narrativa**, alterne Texto, Semântica e Composição; escreva em um parágrafo e confira os objetos presentes. Use o clique direito para editar ou organizar objetos e **Instrumentos** para recolher a lateral.

As capturas `gtk-folha-text.png`, `gtk-folha-semantic.png`, `gtk-folha-composition.png` e `gtk-reaberto.png` vêm da janela nativa real. Algumas registram uma transição do painel ou uma posição intermediária de rolagem; não são montagens nem pretendem mostrar todo o Documento de uma vez. A reabertura mostra os instrumentos recolhidos e os parágrafos legíveis. Presença de tabela/equações/arranjo em cada especialidade também é conferida por widgets e estado, além das imagens.

Para conferir a entrega sem abrir ou alterar projetos:

```sh
python3 docs/validacao/evidencias-obj01-folha-comum/verificar.py
# Neste Work, incluindo os 83 arquivos anteriores e a demo final:
python3 docs/validacao/evidencias-obj01-folha-comum/verificar.py --locais
# Repetir a inspeção SQLite da amostra fechada, em modo somente de leitura:
python3 docs/validacao/evidencias-obj01-folha-comum/inspecionar_demo.py dados/obj01-folha-comum-r6/Projeto.inde
```

`codigo-validado.json` fixa os 34 arquivos do commit e o hash do executável local. `SHA256SUMS` cobre os artefatos desta pasta. As amostras em `dados/` são locais e ignoradas pelo Git; um clone limpo pode recriar a demo pelo iniciador, mas não reproduz os UUIDs e hashes específicos desta execução. A conferência `--locais` exige os arquivos originais desta máquina e falha claramente se estiverem ausentes ou diferentes.

Comandos usados: `cmake --build build-nativo --target inde inde_object_text_projection_tests --parallel 2`; `ctest --test-dir build-nativo --output-on-failure -j2`; `cmake --build build-obj01-p2a --parallel 2`; `ctest --test-dir build-obj01-p2a --output-on-failure -j2`; `python3 -m unittest discover -s tests -v` sob Xvfb; `python3 verificar_base.py --integrado`; `python3 tests/audit_obj01_sources.py`. A CI recria o ambiente e executa suas matrizes independentes, incluindo compiladores e sanitizers.

## Invariantes e riscos cobertos

| Invariante | Verificação concreta | Fronteira restante |
|---|---|---|
| Uma folha em todas as especialidades | Mesma instância do parágrafo, objetos mapeados e arranjo preservado nas nove especialidades | Gestos antigos exclusivos do widget linear ainda precisam de adaptação |
| Texto com uma autoridade | Inserção/remoção atravessam guardas canônicos; fonte desatualizada não é sobrescrita | Não se declara equivalência de todos os gestos entre vistas |
| Uma entrada aceita chega inteira | Substituição entre parágrafos, proteção de fechamento e Desfazer completo | Históricos de objetos e texto continuam separados |
| Conteúdo legível antes de receber foco | Altura mínima ao reabrir e altura integral de um parágrafo com várias linhas | Não substitui ensaio de todos os tamanhos de fonte e métodos de entrada |
| Cursor utilizável em escrita longa | Posição do cursor conferida dentro da área visível da folha | Desempenho de manuscritos grandes e navegação vertical integral continuam abertos |
| Lentes conservam a origem | Aplicar e retirar perfil mantém corpo canônico e restaura os parágrafos | Escopo editorial de publicação é independente da visibilidade |
| Persistência respeita o proprietário | Texto, identidades, âncora e formatação conferidos após salvar e reabrir | Não existe transação SQLite única entre proprietário e auxiliar |
| Acervo preservado | Arquivos anteriores e compactados reconferidos por SHA-256 | A conferência tem a lista explícita de arquivos registrada nas evidências |

## Intercorrências preservadas

O teste gráfico inicialmente expôs a perda de foco após inserir um parágrafo. A restauração passou a procurar o cursor canônico na nova estrutura, sem ficar presa ao identificador da vista antiga. Uma nova asserção exige continuar digitando no novo parágrafo.

Outra execução expôs que Mostrar tudo conservava a projeção visual da lente anterior. A atualização passou a recalcular também a retirada de uma lente, sem salvar os bancos durante a atualização visual. O teste que encontrou a falha permanece ativo.

A substituição de seleção entre parágrafos expôs a disputa de seleção primária entre buffers GTK realizados, ausente no teste isolado sem janela. A correção mantém a autoridade da seleção no buffer canônico e acrescenta cobertura de fechamento e Desfazer na janela real.

A inspeção das capturas encontrou vistas textuais com altura insuficiente antes do foco. A hospedagem dos parágrafos passou a propagar a altura natural do GTK; o teste exige altura legível ao reabrir, exibição integral de texto com várias linhas e cursor dentro da área visível. A imagem anterior foi preservada para comparação.

As etapas intermediárias permanecem nas evidências, separadas dos resultados finais. Não foram removidas as asserções que detectaram essas falhas.

O ensaio sob carga revelou que a observação periódica do teste podia antecipar os idles de entrada e layout. A reconciliação aceita passou a executar antes da pintura; o roteiro observa o estado depois dos idles GTK, mantendo os mesmos critérios e limite global de tempo. A notificação de altura acompanha apenas mudanças reais de tamanho da vista focada. A instrumentação de revelar um objeto posiciona a rolagem após obter o foco, cancelando a animação anterior. O percurso final passou em tela compacta e maximizada.

A [primeira CI deste corte](https://github.com/Roger-Cardoso/inde/actions/runs/45280909484), em `84a277c`, aprovou 25/26 trabalhos. O trabalho nativo compilou e completou as verificações funcionais, mas o teste recusou erros críticos `g_signal_handler_disconnect` e `gdk_frame_clock_idle_end_updating`; o log está em `ci-primeira-falha.txt`. O código ainda cancelava animações de ajuste a partir da notificação de layout e iniciava uma rolagem interna desnecessária ao focar o `TextView`. A correção `e285f66` transfere toda a alteração de rolagem para o próximo ciclo do loop e remove essa animação interna. O teste passa a executar com `G_DEBUG=fatal-criticals` e também recusa explicitamente `GLib-GObject-CRITICAL`. As verificações locais finais, inclusive a demo maximizada, usam esse modo estrito; não se considerou suficiente simplesmente repetir a CI antiga até passar.

## Preservação e limites

A main permanece separada em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com sua alteração preexistente de modo em `Preparar-ponte.sh`. O schema canônico continua 37 (`schema_migrations`; `PRAGMA user_version` do proprietário é 0), SQLite auxiliar versão 5 e formato privado versão 6. Os arquivos compactados originais, a extração fixada e as demonstrações anteriores são conferidos por hash. A checagem final de integridade é somente de leitura e também comprova que não alterou os três bancos da demo; os snapshots salvo/de trabalho coincidem e o título/corpo conferem com o proprietário.

Continuam abertos: seleção por arrasto entre vistas; navegação vertical completa entre parágrafos; adaptação de gestos e tooltips antigos ligados ao widget linear; acessibilidade integral; IME físico; sessão longa e desempenho com manuscritos grandes; cronologia única de Desfazer; restauração integral de arranjos após retirada e retorno de identidades. A digitação no novo campo usa o histórico textual existente e não encerra essas frentes.

Busca, estatísticas e semântica ainda têm o alcance do corpo canônico. Não se afirma agregação automática de células, resultados de cartões e fontes matemáticas. A persistência entre proprietário e auxiliar continua sequencial. Formulários não aplicados permanecem em memória.

Publicação dos novos objetos, estados narrativos de variáveis, vínculos de variáveis a entidades/pseudônimos, equivalência completa com Comic e arquivo único `.inde` não são promovidos a funcionalidades concluídas por este corte. Não há release, merge em main ou aceite humano implícito.

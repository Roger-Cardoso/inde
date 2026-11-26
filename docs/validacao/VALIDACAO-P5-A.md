# Validação P5-A — estados e transporte do projeto

Data: 2026-11-26. Branch: `feature/inde-p5-single-file`, derivada de `1b77951e7b4f5e9cf8f19161675fdd64d835f506`. O [contrato](../arquitetura/ARQUIVO-UNICO-P5-A.md) delimita o primeiro corte. Este relatório distingue biblioteca, transporte, serviços nativos, demonstração GTK e experiência humana.

## Resultado local

| Camada | Resultado | Evidência |
| --- | --- | --- |
| Núcleo nativo existente e P5 | 68/68 CTest passaram | [log](evidencias-p5-a/native-ctest.txt) |
| Biblioteca independente e transporte, GCC 15 Release | 2/2 alvos, incluindo 12 casos Python | [log](evidencias-p5-a/gcc-release-ctest.txt) |
| Mesmos alvos, Clang 18 Debug, ASan/UBSan e vazamentos | 2/2 alvos; sem diagnóstico de memória no passe concluído | [log completo](evidencias-p5-a/clang-asan-ctest.txt) |
| Serviços reais, projeto schema 37 novo | Comparações e restauração passaram | [relatório](evidencias-p5-a/verificacao.json) |
| Demonstrador real, editor GTK e teclado | Salvar, sair, capturar e conferir passaram em display privado | [resultado](evidencias-p5-a/gtk-demonstracao.json) |
| Regressão completa de interface/integrações | 103/103 testes passaram, incluindo entrada real e encerramento | [log](evidencias-p5-a/editor-regressoes.txt) |
| Fontes congeladas | 758 arquivos conferidos; bytes originais preservados | [conferência](evidencias-p5-a/fontes-fixadas.json) |
| Três arquivos compactados | Hashes e inventários conferidos | [auditoria](evidencias-p5-a/arquivos-fontes.json) |

O primeiro passe do detector de vazamentos foi impedido pela restrição de observação de processos do ambiente (`LeakSanitizer ... does not work under ptrace`). O passe foi repetido fora dessa restrição, ainda apenas sobre projetos fictícios, e passou com detecção de vazamentos ativa. A primeira compilação Clang também apontou conversões com sinal no parser JSON congelado: a configuração foi corrigida para aplicar aos arquivos herdados as advertências do build proprietário e manter `-Wconversion -Werror` em todos os arquivos novos, sem editar a fonte congelada.

Na revisão final foi reproduzida outra borda do parser herdado: `std::stod` interpreta o ponto decimal segundo o locale, podendo truncar o valor silenciosamente em ambiente brasileiro. O invólucro P5 foi corrigido, e o teste usa um locale `pt_BR.UTF-8` real, verifica o valor `0.6120081954998241`, zero negativo, gravação/reabertura e restauração do locale do chamador tanto no sucesso quanto no erro. O CI instala e exige esse locale; não aceita omitir esse caso. A [repetição dos três alvos P5 no build nativo](evidencias-p5-a/p5-native-ctest.txt) e os logs independentes incluem a correção final.

## Regras exercitadas

| Caso | Comprovação |
| --- | --- |
| Alteração atômica | Conteúdo, recurso e participação no estado entram juntos; falha não deixa versão/recurso órfão |
| Concorrência | Segunda sessão bloqueada, pedido de estado antigo e versão de elemento antiga recusados, identidade estrangeira recusada |
| Vínculos | Retirar destino isoladamente falha; retirar destino e dependente no mesmo lote funciona; restauração recompõe os dois |
| Tipo/descrição | Tipo da identidade preservado, chaves de edição/JSON duplicadas recusadas, UTF-8 e precisão numérica conservados, profundidade excessiva recusada |
| Recursos | Bytes não textuais e arquivo vazio preservados; bytes ausentes recusados; componente inalterado reutiliza sua versão/recurso |
| Restauração | 1 → 2 → restauração como 3; conteúdo de 3 igual ao de 1 e estado 2 ainda disponível |
| Falha antes/depois de confirmar | Exceção antes de COMMIT desfaz; confirmação perdida depois de COMMIT exige reabrir e encontra o estado confirmado |
| Interrupção real | Processo termina antes/depois de COMMIT com recurso de 4 MiB, maior que o cache padrão de páginas; reabertura confere estado antigo/novo íntegro |
| Falta de capacidade | `SQLITE_FULL` real pelo limite de páginas e limite de histórico recusam a gravação sem apagar o estado anterior |
| Uso indevido da sessão | Handle herdado por fork, substituição/movimentação do arquivo e alteração SQLite externa detectados |
| Corrupção | Alteração de esquema e conteúdo adulterado de versão antiga são detectados, mesmo se o estado atual já voltou a outra versão |
| Formato incompatível | Arquivo alheio, SQLite alheio, WAL e truncamento recusados; arquivos alheios não são regravados |
| Transporte | Origem preservada, outro local, acentos, pastas vazias, recursos binários, destino existente recusado, links/escape de caminho recusados, banco com FK inválida recusado |

Os testes C++ estão em [project_file_tests.cpp](../../cpp/project_file_tests.cpp); os 12 casos de transporte usam fixtures mínimas construídas para cada teste e **não se apresentam como projetos completos do INDE**. O teste [project_file_native_tests.cpp](../../cpp/project_file_native_tests.cpp) usa `ProjectService`, os repositórios nativos e o motor de objetos reais.

A fixture nativa inclui parágrafos identificados, estilo, âncora, perspectiva, comando literal, alternativa privada, lente de visibilidade e anotação semântica. A comparação usa o snapshot completo do Documento. Inclui ainda Composição textual com usos live/pinned/reference-only e rascunho privado, planejamento associado ao texto, PNG, tabela, equação e linha com ordens autoral/visual divergentes. O teste edita a cópia, registra outro estado, restaura o original, reabre o estado posterior e compara todos os arquivos originais por hash.

## Demonstração e evidência retida

O [demonstrador](../../scripts/demos/Abrir-demo-arquivo-unico.sh) produz um projeto novo e o [teste de interface](../../tests/project_file/test_native_demo.py) aciona Ctrl+S e Ctrl+Q com teclado real em X11/D-Bus privados. Não invoca sinais GTK para simular o fechamento. Ao sair, o launcher registra a pasta no arquivo único e confere integridade. [Histórico produzido](evidencias-p5-a/historico-final.json) e [conferência final](evidencias-p5-a/integridade-final.json).

O passe local apresentou advertências GTK sobre keyvals e avisos dos portais/FUSE do ambiente, sem erro crítico GTK e sem impedir o fechamento. Este teste comprova o demonstrador e os atalhos exercitados, não o aceite humano de todas as especialidades. Não há adoção transparente do novo arquivo pelo botão Salvar nesta entrega: o editor grava a pasta, e o launcher só captura depois do encerramento normal.

## Reproduzir e conferir

```bash
python3 verificar_base.py --integrado
python3 tests/audit_obj01_sources.py
python3 tests/project_file/verify_evidence.py
cmake -S tests/project_file -B build-p5 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-p5 --parallel 2
ctest --test-dir build-p5 --output-on-failure
```

Para a camada nativa, compilar `inde_project_file_native_tests` no build `cpp`, executar o alvo e usar o roteiro do demonstrador. O [workflow](../../.github/workflows/inde-p5.yml) compila GCC/Clang com e sem instrumentação, o editor e todos os alvos listados pelo CTest, executa a suíte nativa e a demonstração GTK. A conferência remota será registrada após a execução identificada; a existência do workflow não é, sozinha, aprovação de CI.

## Lacunas e fronteiras

- Abertura/Salvamento/Salvar como/Recentes/recuperação nativos ainda precisam usar diretamente a sessão do arquivo único. O launcher é uma prova de transporte e confirmação ao encerrar.
- A captura do projeto real é por componente: banco canônico modificado implica outra cópia daquele banco no histórico. A biblioteca de elementos não converte automaticamente cada entidade real em descrição textual.
- Não foram convertidos projetos autorais nem promovidas mudanças a main. O checkout principal continua em `d41a5c9`, com a alteração preexistente em `Preparar-ponte.sh` preservada.
- Limites de volume são deliberados; bancos WAL e origens com arquivos de trabalho SQLite são recusados. Não há migração implícita nem reparo automático.
- A preservação binária cobre o que foi capturado; o uso posterior de um projeto hostil no editor não foi certificado como seguro.
- Perda elétrica física, todos os sistemas de arquivos, portas fora de Linux, desempenho de grandes acervos, compactação e união de cópias divergentes não foram certificados.
- O histórico dentro do mesmo arquivo não substitui backup externo. Regressões automáticas, aprovação de CI e aceite humano são evidências distintas.

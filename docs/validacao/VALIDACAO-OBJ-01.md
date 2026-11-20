# INDE-OBJ-01 — Validação da auditoria e correção da Fase 1

Data: 2026-11-20. Branch `feature/inde-obj-01-foundation`. Código da correção: `cb583168c96741f6ebd5a2820266cdbd4e1ec5fa`; commits posteriores deste corte registram evidências/documentação e auxiliares de teste gráfico. Estado: **experimental, fundação isolada em memória**, sem avanço de fase.

## Preservação e identidade do trabalho

A cópia original em `INDE/inde` estava na `main`, com alteração preexistente somente da permissão de execução de `Preparar-ponte.sh`. Foi preservada. O trabalho ocorreu na área separada `INDE/obj01-audit`, na branch solicitada, a partir de `d5e92cc`. `main` e `origin/main` permaneceram em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`. Não houve merge, force-push, alteração de histórico anterior ou uso de projeto autoral como fixture.

Os três pacotes de fontes têm os hashes fixados nos manifestos e foram extraídos e conferidos arquivo a arquivo: schema 28 (641), P03c original (757), integrado (758). [fontes.json](evidencias-obj01/fontes.json) registra hashes e diferenças completas. [base-compilada.json](evidencias-obj01/base-compilada.json) distingue a base arquivada, as 23 transformações preexistentes da cópia de build e os overlays usados pelo Especial.

Nenhum tar.gz, manifesto de fontes, migrador, codec, serviço autoral ou banco real foi alterado. Testes existentes que exercitam migrações fazem isso em projetos sintéticos recém-criados. Isso não constitui migração de projetos reais nem integração de persistência OBJ-01.

## Resultados desta execução

| Verificação | Resultado | Evidência |
| --- | --- | --- |
| Três arquivos de fontes: hashes, manifestos, extração e bytes | Aprovados | [fontes.json](evidencias-obj01/fontes.json) |
| Verificador de arquivos: corrupção, inventário divergente, travessia, caminhos absolutos, links, duplicatas e destino existente | 5 testes aprovados | [archive-verifier-tests.txt](evidencias-obj01/archive-verifier-tests.txt) |
| Fundação com GCC 15.2.0, C++20, avisos como erros | 13 grupos de modelo + 15 grupos de comandos aprovados; 2/2 alvos CTest | [foundation-gcc.txt](evidencias-obj01/foundation-gcc.txt) |
| Fundação com Clang 18.1.8, mesmas regras rigorosas | 28 grupos aprovados | [foundation-clang.txt](evidencias-obj01/foundation-clang.txt) |
| Fundação com GCC + ASan/UBSan locais | 28 grupos aprovados; LSan local desativado | [foundation-asan-ubsan.txt](evidencias-obj01/foundation-asan-ubsan.txt) |
| CI remota: fontes e GCC/Clang sem/com sanitizadores | **5/5 jobs aprovados**, com LSan habilitado nos jobs sanitizados | [execução 44759492358](https://github.com/Roger-Cardoso/inde/actions/runs/44759492358), [ci.json](evidencias-obj01/ci.json) |
| Compilação completa do Especial e executáveis nativos/core | Aprovada: 440 etapas iniciais e atualização dos dois alvos OBJ-01 | [native-build.txt](evidencias-obj01/native-build.txt), [native-configure.txt](evidencias-obj01/native-configure.txt), [native-foundation-build-retry.txt](evidencias-obj01/native-foundation-build-retry.txt) |
| CTest completo do build nativo | **49/49 testes aprovados**, incluindo os dois alvos OBJ-01 | [native-ctest.txt](evidencias-obj01/native-ctest.txt) |
| Integração/publicação/importação Python + serviços C++ reais | **92/92 testes aprovados, zero ignorados** | [integration-python.txt](evidencias-obj01/integration-python.txt) |
| GTK: parágrafos, alternativas, leitura, visibilidade e editor unificado | **16 cenários atuais aprovados**; driver histórico P03b falha em 2 tentativas e não executa 2 reinícios | [gtk-documentos.json](evidencias-obj01/gtk-documentos.json), [logs](evidencias-obj01/gtk-documentos.txt) |
| GTK: Comic e sessão Escrita/Publicação | Comic aprovado; primeira publicação falhou sem diagnóstico, repetição direta e verificação final de ambos aprovadas | [resultado final](evidencias-obj01/gtk-workspaces-final.json), [saída final](evidencias-obj01/gtk-workspaces-final.txt) |

O perfil nativo usa Release, `INDE_BUILD_CORE_TESTS=ON`, `INDE_CODE_OPTIMIZATIONS=ON`, `INDE_STRICT_WARNINGS=ON` e `INDE_ENABLE_LTO=OFF`. Dependências disponíveis: gtkmm 4.10.0, SQLite 3.45.1 com sessão/preupdate, ICU 74.2, GLib/Pango/Cairo. Os testes de núcleo mantêm asserções habilitadas (`-UNDEBUG`). Desligar LTO reduz o custo de recompilação; não é ensaio de desempenho ou certificação da configuração de distribuição com LTO/PGO.

Amostras visuais geradas exclusivamente de fixtures sintéticas: [vizinhos na vista filtrada](evidencias-obj01/gtk-vizinhos-filtrados.png) e [espaço Comic](evidencias-obj01/gtk-comic.png). As imagens documentam a interface existente exercitada pelos testes; não são uma prévia de integração visual OBJ-01.

## Cobertura nova da fundação

Os 15 grupos de comandos verificam reordenação simples/múltipla nos cinco arranjos; reutilização do mesmo objeto sem mover seus outros usos; transferência estrutural; subárvores; ciclos diretos/indiretos; destinos/âncoras ausentes ou selecionados; seleções inválidas; políticas explícitas de geometria; não finitos/overflow; pintura separada da leitura; conversões de arranjo; lotes atômicos; estado/revisão/undo/redo intactos após recusa; conflitos após undo; no-ops; limites de comandos e orçamento de histórico.

Há **4.000 reordenações e 1.500 transferências** comparadas a oráculos independentes de vetores, com seeds fixas; **250 árvores geradas**, perturbadas com referências/colocações inválidas; e operações de transferência/recusa/undo com **7.000 contêineres aninhados**. O teste antigo de validação profunda também permanece. Comparações verificam o agregado completo nos cenários de rollback/undo, não só a quantidade de filhos.

Esses ensaios demonstram integridade nas entradas testadas; não são prova formal, benchmark de latência ou garantia contra todos os grafos/colisões hash. O histórico é limitado por quantidade e bytes de snapshots, com descarte dos estados mais distantes documentado no contrato. Falhas de alocação não foram injetadas; a garantia correspondente é sustentada pela organização do commit e pelos movimentos `noexcept`, não por um teste de esgotamento real da memória.

O [resumo estruturado de ambiente e resultados](evidencias-obj01/ambiente-resultados.json) registra compiladores, dependências, perfil e separação das ocorrências. Os arquivos de evidência possuem [SHA256SUMS](evidencias-obj01/SHA256SUMS), conferíveis a partir dessa pasta com `sha256sum -c SHA256SUMS`. São checksums locais para detectar divergência, sem pretensão de atestação externa.

## Reprodução

Somente fundação, sem dependências GTK/SQLite:

```sh
cmake -S tests/object_model -B build-obj01 -G Ninja
cmake --build build-obj01 --parallel 2
ctest --test-dir build-obj01 --output-on-failure
sh tests/run_object_model_tests.sh
CXX=clang++ sh tests/run_object_model_tests.sh
SANITIZE=1 sh tests/run_object_model_tests.sh
python3 -m unittest discover -s tests -p test_obj01_source_audit.py -v
python3 tests/audit_obj01_sources.py --extract-to /tmp/obj01-fontes-novas
```

Em ambiente que rastreia processos, o LeakSanitizer pode falhar com a mensagem `LeakSanitizer does not work under ptrace`. Foi a limitação efetivamente observada na primeira tentativa local. Para repetir ASan/UBSan nesse ambiente: `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 SANITIZE=1 sh tests/run_object_model_tests.sh`. A CI remota executou ASan/UBSan com `detect_leaks=1`; as duas condições não devem ser confundidas.

Regressões existentes, exclusivamente em fixtures novas:

```sh
python3 verificar_base.py --extrair
python3 verificar_base.py --integrado --extrair
cmake -S cpp -B build-nativo -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DINDE_BUILD_CORE_TESTS=ON -DINDE_ENABLE_LTO=OFF
cmake --build build-nativo --target inde inde_web_bridge inde_comic_tests \
  inde_comic_demo inde_publication_tests inde_catalog_tests inde_import_tests \
  inde_work_import_tests inde_publication_ui_smoke inde_integration_tests \
  inde_object_model_tests inde_object_commands_tests inde_all_native_checks --parallel 2
ctest --test-dir build-nativo --output-on-failure
python3 -m unittest discover -s tests -v
for driver in document_paragraphs document_alternatives document_reading_order \
              document_visibility editor_specializations; do
  python3 tests/preparar_drivers_integrados.py "$driver"
done
python3 tests/run_obj01_legacy_smokes.py /tmp/obj01-gtk-documentos-novos
python3 tests/verificar_workspace_comic.py /tmp/obj01-gtk-workspaces-novos
```

Os dois últimos comandos exigem sessão gráfica acessível e destinos inexistentes. Os drivers foram ligados aos objetos efetivamente compilados do Especial, usando cabeçalhos/overlays correspondentes. Não são mock de interface nem nova implementação GTK. O runner documental padrão executa quatro percursos atuais × dois temas × execução inicial/reinício (16 casos), registrando cada saída e resultado. A visibilidade atual é exercitada no driver `editor_specializations`, junto a clipboard, busca, semântica e movimentação de vizinhos. `--include-historical-visibility` acrescenta o driver P03b antigo para diagnóstico explícito; ele não faz parte do conjunto atual aprovado. A execução forense registrada tentou os cinco drivers: 16 cenários atuais passaram, dois iniciais P03b falharam e seus dois reinícios não foram executados. O verificador Comic agora também grava stdout/stderr antes de recusar um processo com erro, e limita cada subprocesso a 120 segundos.

## Limitações e ocorrências

1. **Driver P03b obsoleto, não uma aprovação de toda a UI histórica.** `tests/ui_document_visibility_smoke.cpp` do pacote exige um `Gtk::TextView` separado chamado `visibility-lens-editor`, na asserção `Original and derived continuous lens`. A implementação P03c usa o mesmo editor/buffer com tags de visibilidade (`writing_workspace_visibility.cpp`, `open_visibility_lens`, `hidden_text_tag_`). As duas tentativas, claro e escuro, terminaram com SIGTRAP (-5) antes do percurso, e os reinícios correspondentes não foram feitos. Os logs individuais estão preservados: [claro](evidencias-obj01/gtk-document_visibility-light.log), [escuro](evidencias-obj01/gtk-document_visibility-dark.log). O driver atual P03c passou nos dois temas/reinícios e verifica explicitamente visibilidade, conteúdo canônico oculto, folhas, formatação, clipboard/PRIMARY, busca, reordenação entre vizinhos filtrados, semântica e preedit simulado. Atualizar a cobertura histórica exige revisão própria; não alteramos a UI nem enfraquecemos a asserção antiga para fazê-la passar.
2. **Falha inicial no smoke PUB-02.** O primeiro `verificar_workspace_comic.py` passou em Comic e recusou o processo de publicação, mas descartou stdout/stderr antes de registrar a causa. Essa ocorrência ficou [preservada](evidencias-obj01/gtk-workspaces-primeira-tentativa.txt). Uma [repetição direta em fixture nova](evidencias-obj01/gtk-pub02-repeticao-direta.txt) passou, assim como a execução final completa de Comic e publicação. A causa inicial permanece indeterminada; não a atribuímos ao ambiente sem prova. O helper foi corrigido para gravar a saída antes de `check_returncode` e impor timeout. Não houve modificação no código de publicação.
3. **Compilação incremental durante a alteração da API.** O build inicial em andamento compilou o teste do modelo enquanto seu cabeçalho era modificado; o objeto ficou com a assinatura anterior de `validate`, causando [erro de ligação](evidencias-obj01/native-foundation-build-incremental-failure.txt) no primeiro rebuild. Refizemos apenas esse objeto gerado; os dois alvos foram ligados com sucesso e a suíte nativa foi executada depois. A construção isolada limpa e a CI já haviam passado; o erro não foi omitido nem tratado como bloqueio permanente.
4. **Limites reais de recuperação e integração.** Não há persistência/recovery OBJ-01, reordenação do Documento a partir do grafo nem exportação de novos objetos. O novo undo só cobre memória. Esses são gates futuros declarados, não operações disponíveis ao autor na interface existente.

Não foram testados dispositivos físicos, teclado ABNT2/IME real, leitor de tela, Wayland, HiDPI, plataformas além dos ambientes registrados, sessões longas, mídia arbitrária hostil ou todos os percursos GTK históricos. ASan/UBSan desta entrega instrumentam a fundação isolada, não o aplicativo integral. Benchmarks longos e LTO/PGO de distribuição não foram reexecutados.

O campo SHA-256 de `Resource` continua sendo metadado sintático, sem bytes de mídia no modelo. Os hashes confrontados com bytes neste corte são os arquivos de fontes e a publicação exercitada pelas suítes existentes. Não há codec, recovery durável, seleção visual, adaptador textual ou exportador OBJ-01. Não foi demonstrada equivalência de migração, nem aceita uma nova UX.

A cópia recursiva de Save As e a adoção separada dos bancos, restauração multibanco e identificação histórica `source_base` permanecem riscos/gates descritos no [inventário](../arquitetura/INVENTARIO-OBJ-01.md), sem ensaio de concorrência/falha entre bancos. O trabalho não corrige silenciosamente esses serviços durante uma fundação isolada.

A execução remota inicial de 13 grupos ([44753323199](https://github.com/Roger-Cardoso/inde/actions/runs/44753323199), código `215a12f`) é evidência histórica. A nova evidência acima cobre a correção atual; não reutilizamos o sucesso anterior como prova das mudanças.

**Resultado de escopo:** fundação reforçada dentro da Fase 1, com auditoria, comandos, invariantes e CI verificáveis. O schema 37, os projetos autorais e a main foram preservados. Nenhum gate de integração, GTK novo ou arquivo único `.inde` foi promovido por esta entrega.

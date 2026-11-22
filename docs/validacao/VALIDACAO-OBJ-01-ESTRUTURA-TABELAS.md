# OBJ-01 — validação de linhas e colunas recuperáveis

Data: 2026-11-22. Base `26adaa22995def08b5ea2e269e9ef3a193d398c8`; implementação `90939bb4652d1ba3452ccefb62174c8dea68ccbd`; branch `feature/inde-obj-01-foundation`. [Contrato](../arquitetura/ESTRUTURA-TABELAS-OBJ-01.md), [roteiro](../../tools/obj01-pilot/ESTRUTURA-TABELAS.md), [resultados](evidencias-obj01-estrutura-tabelas/resultados.json), [manifesto](evidencias-obj01-estrutura-tabelas/SHA256SUMS).

## Resultado e testes locais

Criação de tabelas com dimensões escolhidas; inserção antes/depois, retirada e restauração de linhas/colunas com células estáveis. A parte retirada conserva textos, fórmulas, frases, detalhes de aparência e proporções. O editor visual acompanha a grade visível; alterações são atômicas, recuperáveis e podem ser desfeitas/refeitas durante a sessão.

| Configuração | Programas | Tempo | Log |
| --- | --- | --- | --- |
| GCC Debug | 21/21 | 149,08 s | [GCC](evidencias-obj01-estrutura-tabelas/gcc-final.txt) |
| Clang Debug | 21/21 | 145,87 s | [Clang](evidencias-obj01-estrutura-tabelas/clang-final.txt) |
| GCC ASan + UBSan | 21/21 | 447,84 s | [GCC sanitizers](evidencias-obj01-estrutura-tabelas/asan-final.txt) |
| Clang ASan + UBSan | 21/21 | 347,08 s | [Clang sanitizers](evidencias-obj01-estrutura-tabelas/clang-asan-final.txt) |

Os 19 programas anteriores foram repetidos. Os dois novos incluem **18 casos de núcleo** e **sete HTTP**: três próprios e quatro comuns herdados. Não são 84 casos individuais: são 21 programas CTest executados em quatro configurações. Compilação estrita, sintaxe JS/Python/iniciador e `git diff --check` aprovados. LeakSanitizer desativado localmente e habilitado na matriz remota.

Cobertura nova:

- Criação configurável e API 2×3 anterior preservada; tipos, contagens e campos extras inválidos recusados sem alteração parcial.
- Inserções por identidade, Unicode, textos e estilos conservados; retirada/restauração de altura e exceções de célula.
- Linha e coluna retiradas simultaneamente, novas interseções com eixos ocultos e restauração após reabrir.
- Projeção/mesclagem de aparência: detalhes ocultos preservados, IDs ocultos enviados diretamente recusados e pesos restaurados por identidade.
- Fonte, binding, frase e resultado de fórmula conservados; edição/seleção/anexação/texto misto recusados em células retiradas.
- Último eixo visível protegido, aparência de uma única coluna válida, 64×32 aceito e retirados incluídos no limite.
- Alvos inativos/inexistentes, eixo inválido e revisão desatualizada recusados integralmente.
- Comando oculto não executado nem consumido; pares externos e leitura conservados por edição interna.
- Desfazer/refazer, SIGKILL, salvar/reabrir e **12 interrupções reais de processo**: inserir/retirar/restaurar × `after_blobs`, `after_snapshot`, `before_commit`, `after_commit`. O teste de falhas usa eixo linha; isso não é prova de todas as panes nem de cada combinação de coluna/falha.

A auditoria demonstrou uma falha antes da correção: desfazer fórmula numa linha recém-inserida descartava o histórico anterior de inserção. [Reprodução que falhava](evidencias-obj01-estrutura-tabelas/regressao-antes.txt); [18 casos aprovados após a correção](evidencias-obj01-estrutura-tabelas/nucleo-apos-correcao.txt). A regressão também está incluída nas quatro matrizes finais. A retenção de frases agora respeita os endpoints existentes no snapshot de destino.

## Conferência no navegador

Fixture nova em `dados/obj01-piloto-estrutura-tabelas/`, com motor C++, SQLite e HTTP autenticado reais. Nenhum projeto autoral foi usado.

1. Criada tabela **Elenco da viagem**, 3×4, preenchida com Nome/Papel/Condição/Vida. Fórmula de Vida de Armand em célula: `100 pontos`, com resultado protegido e frase editável.
2. Aplicado Caderno, letra 17; coluna Vida à direita e célula da fórmula em negrito. Inserida linha antes de Armand e coluna Grupo antes de Papel. Fórmula, frase e estilo acompanharam a célula original. [Estado antes da retirada](evidencias-obj01-estrutura-tabelas/antes-retirada.json).
3. Preenchidos Breno e os grupos. Retiradas a linha de Armand e a coluna Grupo. O catálogo identificou “Armand · 4 preenchidas / 1 fórmulas” e “Grupo · 4 preenchidas / 0 fórmulas”.
4. Inserida e preenchida a linha Dora enquanto ambos os eixos estavam retirados. Salvo esse estado, restaurada a coluna sem novo Salvar. A linha de Armand permaneceu no catálogo. [Interface antes do reinício](evidencias-obj01-estrutura-tabelas/antes-reinicio.txt).
5. Encerrado com SIGKILL somente o motor dessa fixture e reiniciado o servidor. Antes de qualquer nova ação de interface, snapshot/hash/geração de trabalho e estado salvo foram comparados: **idênticos**. Working geração **76**, Saved revisão **1**, grade visível 4×5, grade conhecida 5×5, linha com fórmula ainda retirada. Integridade `ok`, chaves estrangeiras sem violações, auxiliar versão 5; histórico em memória vazio. [Resumo](evidencias-obj01-estrutura-tabelas/reinicio.json), [estado recuperado](evidencias-obj01-estrutura-tabelas/estado-recuperado.json), [interface recuperada](evidencias-obj01-estrutura-tabelas/depois-reinicio.txt).
6. Restaurada a linha após o reinício: **100 pontos**, frase e negrito reapareceram na posição conservada. A interseção da coluna restaurada com a linha Dora veio vazia e foi preenchida como Leste. [Restauração](evidencias-obj01-estrutura-tabelas/restauracao-final.txt).
7. Reaplicado o alinhamento à direita à coluna Vida para incluir as células novas, conservando o negrito de Armand. Salvo o conteúdo e a aparência finais. A comparação independente confirmou as **20 células anteriores sem mudança**, os mesmos registros/bindings/frases de fórmula, exceções antigas preservadas e resultado 100. A grade final tem 25 células, leitura e corpo de origem conservados. [Comparação](evidencias-obj01-estrutura-tabelas/comparacao-celulas.json), [estado final](evidencias-obj01-estrutura-tabelas/estado-final.json).

[Imagem final da demonstração](evidencias-obj01-estrutura-tabelas/demo-final.jpg), [estado acessível final](evidencias-obj01-estrutura-tabelas/estado-visual-final.txt). Sem avisos/erros capturados no console durante a conferência final. Dumps acessíveis tiveram apenas espaços finais normalizados; conteúdo do banco não foi normalizado. A comparação de reinício usa os snapshots do armazenamento, e não igualdade dos dumps de interface.

## Compatibilidade, fontes e preservação

Snapshot privado V4 explícito (`composition.table_structure`), SQLite auxiliar 5 e linguagem de fórmulas 2. Sem DDL novo. Formatos 1/2/3 continuam legíveis e preservam formato até uso da capacidade nova, com regressões anteriores repetidas. Binário anterior V3 testado contra cópia isolada da fixture V4: recusou com `unsupported_snapshot_version`, sem alterar os bytes de origem/banco. [Compatibilidade](evidencias-obj01-estrutura-tabelas/compatibilidade.json).

Os **18 arquivos** de origem/banco das **nove demos anteriores** mantêm SHA-256: [início](evidencias-obj01-estrutura-tabelas/preservados-inicio.json), [fim](evidencias-obj01-estrutura-tabelas/preservados-final.json). Os nove manifestos históricos foram conferidos nas revisões correspondentes: **397 entradas**. Eles não foram reescritos para ocultar mudanças legítimas de arquivos vivos. O relatório antigo da ponte de Composição e suas evidências continuam intactos.

Três arquivos compactados foram novamente conferidos por hash, inventário/manifesto e bytes extraídos: schema 28 (641 arquivos), schema 37 original (757) e schema 37 integrado (758). [Inventário completo](evidencias-obj01-estrutura-tabelas/fontes.json). Os cinco testes do auditor de fontes passaram: [log](evidencias-obj01-estrutura-tabelas/auditor-fontes.txt). Este corte revalida a proveniência; não alega repetir toda a investigação funcional histórica de GTK/Comic/publicação.

Fontes, metadados, modelo/comandos da fundação, adaptadores, planos/sessões/escrita/checkpoint e código de fórmulas/cartões/hub não mudaram. Main permanece `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com a alteração preexistente apenas de modo em `Preparar-ponte.sh` conservada.

CI de implementação [45068431579](https://github.com/Roger-Cardoso/inde/actions/runs/45068431579): **25/25 trabalhos aprovados**, no commit exato da implementação. [Resultado completo](evidencias-obj01-estrutura-tabelas/ci.json). A matriz inclui núcleo, adaptadores, planos, sessões/checkpoint, arquivos de origem e piloto, com GCC/Clang e diagnósticos de memória.

## Limites e reprodução

O limite conhecido inclui retirados; não há descarte definitivo para liberar quota. Novas células recebem estilo geral, sem inferir regra permanente de coluna a partir de exceções antigas. Cabeçalho acompanha a primeira linha visível. Fórmulas retiradas continuam recursos da biblioteca e podem participar de dependências; não são desligadas do cálculo por estarem fora da apresentação.

Fora: mesclagem, arraste/reordenação interna, alças de redimensionamento, formatos numéricos e bordas individuais. Sem integração GTK/publicação, migração de obras ou `.inde` único, e sem início de Fase 3. Não se comprovaram todos os dispositivos/entradas, concorrência ampla, latência/RSS, todas as falhas de armazenamento ou aceite humano. Os 61 testes nativos históricos não foram repetidos/apresentados como novos.

Para repetir o núcleo do piloto: configurar `tests/object_composite_pilot` com GCC ou Clang, compilar e executar `ctest --test-dir <pasta-de-build> --output-on-failure`. `-DOBJ01_SANITIZE=ON` ativa ASan/UBSan. As quatro pastas usadas localmente foram `build-obj01-p2a`, `build-obj01-p2a-clang`, `build-obj01-p2a-asan` e `build-obj01-p2a-clang-asan`.

Na raiz do worktree: `sha256sum -c docs/validacao/evidencias-obj01-estrutura-tabelas/SHA256SUMS`. O [roteiro de uso](../../tools/obj01-pilot/ESTRUTURA-TABELAS.md) abre uma demonstração nova ou a pasta sintética já existente.

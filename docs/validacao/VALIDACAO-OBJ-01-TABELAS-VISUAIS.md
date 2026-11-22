# OBJ-01 — validação de tabelas visuais

Data: 2026-11-22. Base `08c6e8a1f63aebac77d72316cbe0974c1de78ea0`; implementação `59d551207f326fd2a827c041c61b8edd99a233f6`; branch `feature/inde-obj-01-foundation`. [Contrato](../arquitetura/TABELAS-VISUAIS-OBJ-01.md), [roteiro](../../tools/obj01-pilot/TABELAS.md), [resultados](evidencias-obj01-tabelas-visuais/resultados.json), [manifesto](evidencias-obj01-tabelas-visuais/SHA256SUMS).

## Resultado e matriz local

Editor real de aparência de tabelas com prévia, quatro modelos, cores/bordas/tipografia, título/cabeçalho, largura/posição, proporções de colunas, alturas e detalhes por célula/linha/coluna. Aplicação atômica com histórico/recuperação. Textos crescem sem rolagem interna obrigatória; fórmulas conservam resultado/fonte e herdam estilo.

| Configuração | Programas | Tempo | Log |
| --- | --- | --- | --- |
| GCC Debug | 19/19 | 137,83 s | [GCC](evidencias-obj01-tabelas-visuais/gcc-final.txt) |
| Clang Debug | 19/19 | 134,76 s | [Clang](evidencias-obj01-tabelas-visuais/clang-final.txt) |
| GCC ASan + UBSan | 19/19 | 374,18 s | [GCC sanitizers](evidencias-obj01-tabelas-visuais/asan-final.txt) |
| Clang ASan + UBSan | 19/19 | 299,74 s | [Clang sanitizers](evidencias-obj01-tabelas-visuais/clang-asan-final.txt) |

Inclui os 17 programas anteriores. Bateria nova: **16 casos de núcleo**; HTTP **sete** (três próprios + quatro comuns herdados, sem contar herdados como cenários novos). Compilação estrita com avisos tratados como erros. Sintaxe JS/Python/iniciador e diff conferidos. LeakSanitizer desativado localmente e habilitado na matriz remota.

Cobertura nova: defaults/quatro modelos; título/cabeçalho/aparência/posição no mesmo undo; campos, tipos, cores e CSS inválidos; medidas/colunas sem arredondamento destrutivo; identidade e geometria inválidas; detalhes parciais incluindo negrito falso; referências de célula estrangeira recusadas; fórmula/textos inalterados por aparência; remoção/restauração de estilo; pares/leitura conservados; reset reversível; formatos 1/2 conservados e V3 explícito; contexto/revisão/uso inativo; reaplicação idêntica sem novo histórico/payload; SIGKILL; quatro janelas reais de crash (`after_blobs`, `after_snapshot`, `before_commit`, `after_commit`). Driver de falhas separado. Não se prova toda pane de energia/armazenamento.

## Navegador e demonstração

Nova fixture sintética `dados/obj01-piloto-tabelas/`, motor C++, SQLite e HTTP autenticado reais:

1. Inventário de viagem preenchido com seis células, incluindo observação longa. Prévia Noite/letra 20 cancelada: estado integral real igual, inclusive revisão/geração. [Prova de cancelamento](evidencias-obj01-tabelas-visuais/previa-cancelada.json).
2. Caderno personalizado: cabeçalho verde `#35574f`, texto claro `#fff7e8`, grade e título centralizado. Largura 90%, centro, colunas 22,5/55/22,5; altura geral 48 e segunda linha 110, conteúdo no meio. Campos redistribuídos conferidos na interface. [Prévia de tamanho](evidencias-obj01-tabelas-visuais/tamanho-previa.jpg), [estado](evidencias-obj01-tabelas-visuais/tamanho-previa.txt).
3. Coluna de quantidades alinhada à direita; célula do valor com negrito e fundo `#ecddb6`, conservando o alinhamento herdado. Detalhes aparecem imediatamente na prévia e mudam somente a propriedade escolhida. [Prévia de células](evidencias-obj01-tabelas-visuais/celulas-previa.jpg), [estado](evidencias-obj01-tabelas-visuais/celulas-previa.txt).
4. Aplicar confirma o conjunto. Medição no DOM: seis editores com altura visível igual à altura de conteúdo; observação longa integral, sem scroll interno. Fonte Georgia e coluna à direita verificadas. [Medidas](evidencias-obj01-tabelas-visuais/medidas-celulas.json), [tabela](evidencias-obj01-tabelas-visuais/tabela-aplicada.jpg).
5. Desfazer e Refazer restauram integralmente tabela, apresentação, conteúdo e vínculos: [undo](evidencias-obj01-tabelas-visuais/undo-estilo.txt), [comparação](evidencias-obj01-tabelas-visuais/undo-refazer.json).
6. Outra tabela, Quadro de personagem, recebe fórmula Personagem.HP em célula, resultado “100 pontos”. Noite, largura 80%, centro, texto centralizado e título abaixo. Cores/letra herdadas pelo resultado protegido. [Prévia](evidencias-obj01-tabelas-visuais/noite-formula-previa.jpg), [resultado estilizado](evidencias-obj01-tabelas-visuais/formula-estilizada.json).
7. Salvar e Reabrir salvo conservam as duas tabelas e estilos: [estado](evidencias-obj01-tabelas-visuais/salvo-reaberto.txt). Inventário passa de fonte 17 para 18 sem novo Salvar. Remover a tabela e Desfazer conserva sua aparência: [retirada](evidencias-obj01-tabelas-visuais/tabela-retirada.txt).
8. Interrupções controladas com SIGKILL somente no motor sintético. No reinício final, comparação imediatamente após abrir, antes de novas edições: snapshot/hash/geração e salvo idênticos. Working geração **45**, Saved revisão **1**, fonte salva 17 e recuperada ainda não salva 18, duas tabelas estilizadas e uma fórmula em célula. Resultado 100; integridade `ok`, chaves estrangeiras sem violações, histórico vazio. [Resumo](evidencias-obj01-tabelas-visuais/reinicio.json), [antes](evidencias-obj01-tabelas-visuais/antes-reinicio.txt), [depois](evidencias-obj01-tabelas-visuais/depois-reinicio.txt).

[Editor final](evidencias-obj01-tabelas-visuais/editor-final.jpg), [estado do editor](evidencias-obj01-tabelas-visuais/editor-final.txt) e [documento](evidencias-obj01-tabelas-visuais/documento-final.jpg). As capturas antes/depois ilustram o trabalho; o resumo compara o snapshot do reinício final, sem depender de igualdade dos dumps de interface. Dumps acessíveis tiveram somente espaços finais de linhas normalizados; o conteúdo autoral do banco não foi normalizado.

## Compatibilidade, CI e preservação

SQLite auxiliar continua 5, sem nova tabela/coluna, e linguagem 2. A aparência usa revisão privada de snapshot P2D2-3. Formatos 1/2 continuam legíveis e mantêm formato até a nova edição. O motor anterior recusa V3 sem alterar origem/banco: [teste real](evidencias-obj01-tabelas-visuais/compatibilidade.json). Não houve conversão das oito demonstrações anteriores.

Os 16 arquivos de origem/banco das demos anteriores conservaram hashes: [início](evidencias-obj01-tabelas-visuais/preservados-inicio.json), [fim](evidencias-obj01-tabelas-visuais/preservados-final.json). Manifestos A/B/C/D/D2/Cartões/Hub/Composição-edição foram verificados nas revisões correspondentes: **348 entradas**. Relatório e evidências antigos da ponte de Composição permanecem intactos. São provas históricas, sem reescrever hashes para ocultar mudanças legítimas deste corte.

Três arquivos compactados novamente conferidos por hash/manifestos e bytes extraídos: [fontes](evidencias-obj01-tabelas-visuais/fontes.json). Auditor de recusas, cinco testes: [log](evidencias-obj01-tabelas-visuais/auditor-fontes.txt). Fontes, metadados, modelo/comandos da fundação, adaptadores, escrita/checkpoint e código anterior de fórmulas/cartões/hub não mudaram neste corte.

CI de implementação [45063533150](https://github.com/Roger-Cardoso/inde/actions/runs/45063533150): **25/25 trabalhos aprovados**, no commit exato. [Resultado completo](evidencias-obj01-tabelas-visuais/ci.json). Main permanece `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com o modo preexistente de `Preparar-ponte.sh` conservado.

## Limites

Criação continua 2×3. Fora: adicionar/remover linhas/colunas, mesclar, formatos numéricos, bordas/medidas por célula e alças de resize. Estilos locais não são promessa de tipografia/publicação canônica. Rascunhos não aplicados ficam no navegador. Não foram comprovados todos os dispositivos/entradas, ampla concorrência, latência/RSS, todas as falhas de armazenamento ou aceite humano.

Os 61 testes nativos antigos não foram repetidos/apresentados como novos. Foram executados os 19 programas do piloto/adaptador em quatro configurações e a matriz CI da fundação. Sem projeto autoral, schema real, GTK, publicação, `.inde` único ou início de Fase 3.

Verificação na raiz do worktree: `sha256sum -c docs/validacao/evidencias-obj01-tabelas-visuais/SHA256SUMS`.

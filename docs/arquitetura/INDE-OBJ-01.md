# INDE-OBJ-01 — Contrato de objetos estruturados e composições

Versão **0.1.1 experimental (reforço da fundação 0.1)**, 2026-11-20. Estado: modelo **em memória**, sem persistência ou integração GTK. Código: `cpp/object_model.hpp`, `cpp/object_model.cpp`.

## Modelo e autoridades

O **objeto** tem identidade e tipo lógico (parágrafo, imagem, tabela, equação, contêiner). Uma **ocorrência** é o uso identificado desse objeto em uma composição; o mesmo objeto pode ter várias ocorrências, mas cada ocorrência pertence a um só layout. Um **layout** contém ocorrências em ordem lógica de leitura e regras de disposição. Um **recurso** tem identidade lógica independente de caminho de arquivo, hash SHA-256 e metadados MIME.

Tipos de layout previstos: `Stack` (vertical), `Row` (lado a lado), `Grid`, `Overlay` (camadas) e `Freeform` (superfície livre). Contêineres podem referenciar outros layouts, mas a composição ativa deve formar uma **única árvore enraizada**: a raiz não tem pai, cada outro layout tem exatamente um pai e todos são alcançáveis a partir da raiz. Contêineres diferentes não podem reutilizar a mesma árvore de layout, mesmo quando o objeto contêiner possui várias ocorrências; reutilização de subdocumentos exigirá modelo explícito de instâncias ou cópias. Recursos e objetos sem ocorrências podem existir como itens de biblioteca, mas layouts desconectados pertencem fora da composição ativa. O `Frame` é relativo ao layout, não a pixels de tela. Adaptabilidade, tamanhos físicos, paginação e renderização pertencem a camadas futuras. **Ordem de leitura não pode ser inferida de coordenadas**; posição gráfica não implica cronologia ou causalidade narrativa.

## Invariantes verificadas neste corte

1. Identificadores não vazios e únicos, inclusive entre categorias; layout raiz existente.
2. Toda referência a objeto, ocorrência, layout e recurso deve resolver.
3. Cada ocorrência participa de exatamente um layout, mas objetos podem ser reutilizados em diferentes ocorrências.
4. Contêiner precisa de layout interno; outros objetos não podem declarar layout; imagens precisam de recurso.
5. O campo SHA-256 de recurso deve utilizar 64 dígitos hexadecimais minúsculos; o MIME deve ter forma conservadora `tipo/subtipo` e o tamanho de recurso deve ser positivo. Essa é validação **sintática de metadados**, não conferência dos bytes nem cobertura completa da gramática MIME.
6. Grid exige colunas positivas; outros layouts não aceitam esse campo.
7. Freeform exige retângulo finito, com largura e altura positivas; layouts de fluxo não admitem geometria absoluta.
8. Ciclos, compartilhamento indevido de layout, raiz aninhada e layouts inalcançáveis são diagnosticados com travessias iterativas; nenhum erro é corrigido silenciosamente.
9. Valores desconhecidos dos enums de tipo são recusados; os diagnósticos preservam ordem determinística para entradas idênticas.
10. A validação é somente leitura; não altera a composição, seus objetos ou recursos.

Foram exercitados casos com **7.000 contêineres aninhados**, com compilação rigorosa e execução com ASan/UBSan. Isso demonstra a ausência de recursão nesse percurso testado, mas não estabelece limites de tamanho para documentos não confiáveis nem certifica comportamento em todas as plataformas.

O protótipo **não** representa ainda texto rico, células editáveis, fórmulas, bytes de mídia, alterações transacionais, persistência, objetos semânticos existentes ou numeração de publicação. Não substitui `project::Document` nem `project::Composition`, já existente na fonte integrada.

## Futuro contêiner único `.inde`

Atualmente `.inde` é uma pasta de Projeto contendo, entre outros, `data/project.sqlite3`, `data/publicacao.sqlite3` e `data/comics.sqlite3`. O objetivo posterior é um **arquivo SQLite** com modelos e recursos binários incorporados. A proposta separa identidade lógica (`resource.id`) de integridade/deduplicação (`resource.sha256`); originais incorporados são autoritativos, e miniaturas/caches são derivados.

A migração deverá ser versionada, verificável em cópias sintéticas, reversível por preservação da origem e capaz de preservar revisões, identidades públicas, semântica e histórico. Transferências de bancos em uso devem usar backup SQLite consistente e considerar WAL; jamais mera cópia de arquivo aberto. Credenciais locais não serão incorporadas por padrão.

Auditabilidade exige operações com revisão esperada, histórico, procedência declarada de recursos, hashes e verificações de integridade. Um log dentro do próprio arquivo não é prova de inviolabilidade contra alterações deliberadas. Publicação deve ter seleção explícita: visibilidade do editor e redações alternativas não autorizam divulgar conteúdo privado.

## Sequência prevista

1. Auditar a fonte integrada extraída e os contratos `Document`, `Composition`, recuperação e SQLite.
2. Adaptadores de leitura não destrutivos entre modelos existentes e OBJ-01.
3. Comandos e histórico transacionais sobre ocorrências e recursos.
4. Renderizadores GTK para fluxo/colunas, grades, sobreposição e superfície livre.
5. Recursos binários partilhados entre Escrita, Comic e Publicação, com importação sem perda.
6. Converter percursos das demos em testes do núcleo unificado.
7. Converter o formato físico `.inde` somente após validação comparativa e backups.

O modelo ainda não oferece controle de acesso, cardinalidades complexas de grade, verificação criptográfica de conteúdo ou políticas de limites de ingestão. Nenhuma dessas ausências deve ser confundida com funcionalidade já entregue.

Ver [inventário](INVENTARIO-OBJ-01.md) e [validação](../validacao/VALIDACAO-OBJ-01.md).

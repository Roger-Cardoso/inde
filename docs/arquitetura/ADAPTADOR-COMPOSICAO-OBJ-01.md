# OBJ-01 — Ponte de leitura da Composição persistida

Data: 2026-11-20. Base do corte: `6f4c216`, branch `feature/inde-obj-01-foundation`. Estado: **compatibilidade experimental da Fase 1, somente leitura**. Sem aplicação de rascunhos, escrita de volta, interface nova, alteração de schema ou arquivo único `.inde`.

## Autoridade e captura

`objects::compat::read_composition(StoredComposition, limits)` recebe o agregado e envelope já carregados. Faz preflight sobre os dados emprestados antes de chamar o codec; valida a Composição com `encode_composition`; confronta o SHA-256 calculado com o declarado; decodifica uma cópia profunda e exige recodificação idêntica. Toda a fonte fica privada e independente, inclusive Documentos de camadas/folhas com alocações compartilhadas na entrada.

`CompositionSourceStamp` conserva projeto, ID da Composição, revisão, SHA-256 e `updated_at`. Revisão deve ser positiva; projeto UUID; timestamp não vazio, UTF-8 e até 128 bytes; hash deve coincidir com os bytes codificados. A API não abre SQLite, prova pertencimento, concede permissão ou autentica um envelope fornecido pelo chamador. A integridade do agregado e a proveniência histórica são verificadas pelas regras do codec proprietário existente.

`source_snapshot()` expõe apenas acesso constante à cópia de `StoredComposition`, incluindo blocos de biblioteca sem ocorrências, **todas** as revisões, origens integrais congeladas, faixas exatas de procedência, avisos de conservação, bifurcações, classificações, perfil e rascunhos. Rascunho persistido não vira revisão aplicada. Campos, IDs e conteúdo não são truncados ou reinterpretados para obter um grafo válido.

A ponte não se restringe a Documentos com IDs de parágrafo: um bloco existente pode conter várias linhas ou ter origem sem estrutura de parágrafos. Conserva o que o codec aceita e não cria IDs de parágrafo. Recodificação divergente é recusa explícita; não é migração de origens antigas. A ponte de Documento mantém sua admissão mais restrita de parágrafos canônicos.

## Ocorrências, revisões e tipos

O grafo tem um Stack com **todas as ocorrências da fonte na mesma sequência**, inclusive ocultas e referências. Cada `CompositionOccurrenceBinding` registra ocorrência e bloco originais, revisão efetiva, modo, revisão fixada, variante, família, camada, ocultação declarada e visibilidade efetiva. Essas informações não são deduzidas de texto/coordenadas.

| Modo da fonte | Revisão efetiva | Incorporação |
| --- | --- | --- |
| Live | Revisão corrente do próprio bloco | Texto, se visível no perfil |
| Pinned | Revisão explicitamente fixada | Texto da revisão fixada, se visível |
| Forked | Revisão corrente do bloco bifurcado, com sua procedência conservada | Texto, se visível |
| ReferenceOnly | Alvo resolvido segundo a regra atual do núcleo; permanece navegação | Zero texto, mesmo quando visível |

Live não acompanha automaticamente mudanças posteriores no Documento original. A origem documental é snapshot congelado; bloco e Documento continuam autoridades distintas.

Foram acrescentados ao enum experimental, ao fim dos valores anteriores, **TextBlock** e **TextReference**. Um bloco capturado não recebe falsamente o tipo Paragraph nem inventa divisão em parágrafos. TextReference representa navegação sem incorporação; não é um bloco textual vazio. O validador reconhece os novos tipos, conservando as regras de árvore/frames e todas as invariantes anteriores. Não há enum persistido OBJ-01 nem schema alterado.

IDs de grafo usam namespaces derivados e determinísticos:

```text
composição: obj01/composition/<project>/<composition>
layout:     obj01/composition-layout/<project>/<composition>
ocorrência: obj01/composition-occurrence/<project>/<composition>/<source placement>
texto:      obj01/text-block/<project>/<composition>/<source block>/<effective revision>
referência: obj01/text-reference/<project>/<composition>/<source block>/<effective revision>
```

Os componentes são UUIDs validados pelo núcleo. IDs autorais permanecem intactos no binding e no snapshot. Ocorrências conservam sua identidade quando Live avança; o objeto de captura é qualificado pela revisão e muda nesse avanço. Dois usos da mesma revisão incorporada compartilham objeto; uma revisão fixada diferente e uma referência possuem objetos diferentes. Isso evita atribuir dois conteúdos incompatíveis ao mesmo objeto do grafo congelado.

Essa identidade derivada não substitui a identidade lógica permanente do bloco, nem IDs públicos, URLs ou numeração editorial. O bloco original e sua revisão ficam explícitos no binding. Blocos sem ocorrência e versões históricas não usadas continuam na fonte privada; não viram colocações artificiais.

## Grafo completo, leitura e inspeção privada

`reading()` delega a `TextBlocksLab::project(profile)` da captura. Conserva exatamente texto, segmentos, IDs originais, coordenadas Unicode e faixas de origem do núcleo. Não acrescenta separadores: o núcleo concatena textos conforme suas regras. Referências visíveis têm segmento de largura zero. Ocorrências ocultas, camadas excluídas e variantes não selecionadas permanecem no grafo, mas não entram nessa leitura.

O perfil é o da **Composição**. Perfis de visibilidade do Documento de origem continuam conservados como metadados da origem; não se transformam automaticamente em seleção de saída da montagem.

- `incorporated_text(graph_occurrence_id)` só devolve texto para ocorrência visível e incorporadora. Bloco visível vazio devolve vista vazia presente; referência/oculta/ausente devolve `nullopt`.
- `revision_snapshot(graph_occurrence_id)` oferece inspeção explícita e privada do Documento de camadas de uma ocorrência incorporadora, inclusive oculta. Referência ou ID ausente devolve ponteiro nulo.
- Ponteiros/vistas dependem da vida útil da projeção. `source_snapshot()` contém toda a autoria privada; essas restrições dos resolvers não são controle de acesso sobre os dados já fornecidos pelo chamador.

Nem grafo, nem leitura, nem resolvers são formatos públicos ou autorização de publicar. Não há exportador OBJ-01. Copiar o grafo para `CompositionEditor` permite ensaiar apresentação/undo; a captura, leitura original, fonte persistida, classificações e rascunhos não acompanham a cópia nem são regravados.

## Atualização, limites e recusas

`check_source(current_stored_composition)` compara o envelope fornecido e revalida seus bytes. Outro projeto ou ID é conflito explícito. Revisão diferente invalida a captura mesmo com hash igual. Hash ou timestamp diferente também invalida. Mudar somente rascunho, nome, perfil, estado de ocorrência ou revisão aplicada altera o agregado e é detectado; hash declarado incompatível é recusa por integridade.

O check não consulta banco, sincroniza threads ou torna um save atômico. A entrada precisa permanecer estável durante cada chamada. O futuro protocolo de escrita precisará conferir a autoridade na transação; revisão de `CompositionEditor` continua independente da revisão persistida da Composição.

| Limite máximo/padrão | Valor |
| --- | ---: |
| Blocos | 256 |
| Ocorrências | 2.048 |
| Revisões agregadas | 4.096 |
| Bytes de strings da fonte, incluindo origens/camadas/rascunhos | 8 MiB |
| Snapshot completo serializado | 2 MiB, mantendo o limite do codec existente |
| Entradas de coleções do preflight | 100.000 agregadas |

Podem-se fornecer limites menores; zero ou ampliação acima desses máximos é erro. O preflight conta strings repetidas e verifica UTF-8/NUL, sem copiar antes o snapshot de blocos. A contagem profunda de Documentos é compartilhada com a ponte documental, sem relaxar sua exigência de conteúdo canônico. Origens/camadas desta ponte usam a validação própria do agregado, preservando seus modelos existentes.

Limites não são promessa de RSS: codec e leitura podem alocar temporários. A leitura do núcleo tem seu próprio limite de 32 MiB; não é o mesmo limite do snapshot armazenado. Recusa devolve diagnósticos e nenhuma projeção parcial. Não há truncamento, aplicação implícita de rascunho, normalização silenciosa ou reparo semântico. Falhas de alocação podem propagar, conservando a entrada.

## Verificação e construção

O alvo nativo é separado da interface/publicação. O build mínimo `tests/object_composition` estende o build documental com sete unidades reais: operações/snapshot de blocos, camadas, transação textual/delta e codec de Composição. São **25 unidades proprietárias no total**, sem GTK/SQLite no build mínimo. As rotinas de edição estão presentes para reproduzir/validar estados reais nos testes; a API do adaptador só lê.

A exceção Clang `-Wno-sign-conversion` permanece restrita ao alvo dos fontes antigos, conforme registrado no corte documental. Todo o código novo e seus testes mantêm avisos rigorosos como erros. ASan/UBSan do build mínimo instrumentam os adaptadores e essas unidades reais. O teste nativo de serviço usa o núcleo Release completo e SQLite em fixture nova; as duas coberturas são distintas.

A CI acrescenta quatro combinações GCC/Clang × sem/com sanitizadores, conservando os nove jobs anteriores. Os cenários incluem todos os modos, versões distintas para o mesmo bloco, origem completa, rascunhos, perfis/camadas/famílias, igualdade da leitura com o núcleo, independência das alocações, fronteira de 2.048 ocorrências e ordens geradas. A prova de repositório compara todos os arquivos da fixture antes/depois, guarda versões e detecta outro escritor.

## Próximo requisito

As duas pontes de leitura agora permitem estudar o plano de alterações com fontes e identidades explícitas. O próximo corte deve distinguir alteração visual de mudança autoral, mapear apenas operações demonstravelmente conservadoras e produzir diagnóstico antes de qualquer escrita. Movimento de texto deve delegar ao motor existente e manter suas recusas sobre resmas/âncoras; mudança de ordem de ocorrências da Composição deve preservar modos, versões e classificações. Escrita persistente, GTK, unificação de bancos e `.inde` único continuam fora deste corte.

Ver [fundação](INDE-OBJ-01.md), [ponte documental](ADAPTADOR-DOCUMENTO-OBJ-01.md) e [validação deste corte](../validacao/VALIDACAO-OBJ-01-COMPOSICAO.md).

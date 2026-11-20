# OBJ-01 — Ponte de leitura do Documento, corte de compatibilidade

Data: 2026-11-20. Branch `feature/inde-obj-01-foundation`, a partir de `73dee10`. Estado: **implementação experimental de leitura do núcleo**, sem integração GTK, escrita de volta, schema novo ou arquivo único `.inde`. É o próximo corte de compatibilidade da Fase 1; não promove gates de edição/persistência.

## Decisão e motivo

Após a auditoria e os comandos da fundação, o maior risco é tratar a mesma prosa como duas autoridades concorrentes. O próximo avanço precisa provar a correspondência entre o Documento autoritativo e o grafo de ocorrências antes de aplicar comandos desse grafo ao texto. Esta ponte faz essa prova em memória, reaproveitando os tipos, validação e codecs reais do schema 37.

O corte começa em `project::Document`. `project::Composition` tem revisões de blocos, origens congeladas, modos live/pinned/forked/reference-only, perfis e rascunhos; exige adaptador próprio. Não se representa um bloco de várias linhas ou uma referência como se fosse automaticamente um parágrafo. A integração sintética desta entrega verifica que esses dados de Composição permanecem intactos, mas não os traduz.

## Autoridade, captura e acesso

`objects::compat::read_document(project_id, document, limits)` recebe um Documento já carregado. O adaptador não abre caminhos, bancos, sessões ou projetos; não chama serviços de save, migradores, GTK, filtros de leitura ou publicação. O ID de projeto é declaração do chamador e precisa ser UUID válido; esta API não verifica pertencimento no banco nem concede acesso.

A operação faz preflight, usa `persistence::encode_document_snapshot` para validar e codificar o Documento inteiro, decodifica em uma captura própria e exige igualdade exata da recodificação. A decodificação impede alias até de folhas cobertas cujo chamador ainda retenha um ponteiro mutável. O resultado expõe apenas acesso constante à captura, ao grafo e aos vínculos. Não existe API de gravar/aplicar o grafo na fonte.

A captura é **privada de autoria**. Conserva corpo integral, formatação, âncoras, vínculos com entidades, semântica/ambiguidades já registradas, comandos inline literais, finalidade, perspectiva, metadados, parágrafos/IDs aposentados, resmas/folhas cobertas e perfis de visibilidade. Não faz reancoragem, classificação, reparo semântico, inferência de personagens/POV ou interpretação narrativa.

O grafo contém apenas os parágrafos ativos do corpo integral. Uma folha coberta continua na captura, não vira um parágrafo ativo. Um perfil que oculta um trecho não retira esse trecho do grafo. Presença de alternativas, visibilidade e restrições textuais gera avisos separados de erros. Essa captura não é um formato público ou autorização para publicar conteúdo oculto/privado.

## Identidades e vínculos

O grafo usa um único layout Stack e conserva a ordem autoral capturada. Para cada linha canônica existe um objeto Paragraph e uma ocorrência. Os IDs derivados são determinísticos e têm categorias distintas:

```text
composição: obj01/document/<project UUID>/<document UUID>
layout:     obj01/layout/<project UUID>/<document UUID>
objeto:     obj01/paragraph/<project UUID>/<document UUID>/<paragraph UUID>
ocorrência: obj01/occurrence/<project UUID>/<document UUID>/<paragraph UUID>
```

Os IDs originais não são alterados. Os IDs de grafo qualificam a origem por projeto/documento, evitando colidir quando uma cópia reutiliza IDs locais. São distintos de URLs/IDs públicos; nenhuma reserva de publicação é alterada. A identidade não deriva do texto, do offset ou do hash: parágrafos com prosa idêntica continuam distintos, e editar/reordenar não cria nova identidade para o mesmo parágrafo sobrevivente.

`ParagraphBinding` conserva ID de parágrafo, objeto, ocorrência, SHA-256 do conteúdo e offsets Unicode da captura (sem LF). `paragraph_text(occurrence_id)` resolve esse vínculo sobre a captura própria, retornando vista somente leitura. Ocorrência inexistente retorna `nullopt`, distinguindo-a de um parágrafo vazio. O chamador deve conservar a projeção enquanto usa a vista textual.

`ReadingGroupBinding` reutiliza `application::reading_blocks`, incluindo o agrupamento de parágrafos cobertos por resmas de várias linhas. Esses grupos registram a regra textual preexistente; não inferem unidades narrativas. O grafo permanece plano neste corte. Seus comandos visuais não ganham permissão para dividir resmas ou âncoras na fonte.

## Atualização da fonte e concorrência

`DocumentSourceStamp` guarda projeto, Documento e SHA-256 do snapshot completo. Documento não dispõe de uma revisão numérica única equivalente à da Composição; timestamp ou hash do corpo sozinho não é suficiente.

`check_source(project_id, current_document)` compara a fonte fornecida com a captura. Troca de projeto/documento é conflito explícito. Alterar título, estilos, anotação, perfil, folha coberta, IDs aposentados ou qualquer outro campo serializado invalida o snapshot, mesmo sem alterar o corpo ou o timestamp. Fonte inválida é recusada; nunca é reparada para passar. Restaurar exatamente o mesmo snapshot é considerado compatível com a captura: isto é verificação de igualdade, não histórico monotônico do Documento.

O método compara **dados fornecidos em memória**. O chamador precisa carregar a fonte atual e mantê-la estável durante cada chamada; a API não sincroniza threads. Não é lock, autorização de escrita, prova de estado atual do banco ou transação entre o check e um save. O futuro protocolo de escrita terá de conferir novamente a autoridade dentro da transação persistente. A revisão do `CompositionEditor` continua independente deste stamp.

Copiar `graph()` para `CompositionEditor` permite ensaiar composição visual e undo sem modificar o Documento capturado nem o banco. Mesmo uma reordenação visual que separaria uma resma não é transferida à fonte. Não se declara que os comandos genéricos de objetos já substituem `move_reading_block`/`SemanticAnchorEngine`.

## Admissão, limites e falhas

O corte só aceita identidades de parágrafos habilitadas e conteúdo canônico. Documentos sem IDs retornam `paragraph_identity_required`; partições antigas de offsets sem payloads canônicos retornam `canonical_paragraphs_required`. A ponte não identifica parágrafos automaticamente nem migra a fonte. Validação do domínio continua autoritativa para partição, resmas, semântica, referências e enums.

Os limites padrão também são máximos desta implementação:

| Limite | Valor | Verificação |
| --- | ---: | --- |
| Parágrafos ativos por captura | 10.000 | Antes de materializar objetos |
| Entradas de coleções percorridas no preflight | 100.000 agregadas | Antes de percorrer cada coleção |
| Bytes das strings da fonte | 8 MiB agregados | Inclui metadados, corpo, conteúdo de parágrafos e folhas cobertas; duplicações são contadas |
| Snapshot serializado | 16 MiB | Após codec existente, antes da decodificação própria |
| Grafo resultante | Limites padrão OBJ-01 | Validação integral antes de devolver resultado |

Limites menores podem ser fornecidos; zero ou valores acima desses máximos são recusados. O limite de 10.000 não redefine os 100.000 IDs textuais do domínio nem afirma suporte a todos os tamanhos aceitos pelo editor. É um limite explícito desta ponte experimental; 10.001 não vira uma captura parcial de 10.000.

Preflight valida UTF-8 e recusa NUL em todas as strings percorridas. Conta bytes, não capacidade/RSS. O codec existente pode alocar uma representação temporária maior por escape/serialização, sujeito a seu próprio limite de 64 MiB; a quota de 16 MiB não é promessa de pico total de memória. Fonte já está em memória; isto não é parser de arquivo hostil nem mecanismo de ingestão.

Recusa retorna diagnósticos e **nenhuma projeção parcial**. Diagnósticos usam códigos e IDs limitados, sem incorporar prosa de exceções do domínio. Exceções de alocação podem propagar; a entrada não é modificada. Não há poda, truncamento, geração aleatória de IDs ou descarte de camadas para alcançar limites.

## Construção e dependências

A biblioteca `inde_object_document_adapter` e seus testes são alvos separados no build nativo. O aplicativo `inde-especial` e a biblioteca de publicação não dependem dela. A fundação OBJ-01 isolada continua sem GLib, GTK ou SQLite.

O build específico `tests/object_document` compila o adaptador e 18 unidades reais do domínio/codec do pacote verificado. Usa GLib e UUID para UTF-8, hashes e identidades; não configura nem liga GTK/SQLite. Emprega a transformação de diagnósticos já revisada em uma cópia local do build, sem mudar os fontes originais. Com sanitizadores, essas unidades e o adaptador são instrumentados; no build nativo completo, o núcleo reutilizado é Release não instrumentado.

A CI conserva os cinco jobs anteriores e acrescenta GCC/Clang × sem/com sanitizadores para esta ponte. O teste de repositório SQLite/Composição é executado no build nativo completo; não faz parte do build mínimo sem SQLite. São coberturas distintas.

## Próximos cortes e condições de avanço

1. **Entregue aqui:** leitura do Documento, captura integral privada, mapa de identidades, freshness e prova de ausência de escrita.
2. **Entregue no corte posterior:** [ponte da Composição persistida](ADAPTADOR-COMPOSICAO-OBJ-01.md), preservando revisão/hash do agregado, revisão efetiva de cada ocorrência, pinned/live/forked/reference-only, origens, variantes, camadas e rascunhos sem aplicá-los.
3. **Depois das duas pontes:** contrato de plano de alteração que diferencie apresentação visual e mudança autoral. Um movimento textual deve delegar ao motor existente, manter suas recusas e ser ensaiado com offsets Unicode, âncoras, resmas, semântica, visibilidade, undo e recovery.
4. **Antes de escrita persistente:** guarda dentro da transação, prova de falhas/conflitos e equivalência em fixtures, incluindo preservação de revisões/IDs públicos e separação entre bancos.
5. **Só após esses requisitos:** interação visual e renderização de objetos. Arquivo único `.inde`, mídia incorporada e migrações permanecem cortes posteriores próprios.

Ver [contrato da fundação](INDE-OBJ-01.md), [auditoria](INVENTARIO-OBJ-01.md) e [validação deste corte](../validacao/VALIDACAO-OBJ-01-LEITURA.md).

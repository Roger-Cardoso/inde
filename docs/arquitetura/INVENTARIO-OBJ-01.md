# INDE-OBJ-01 — Auditoria aprofundada das bases e aprendizados

Data: 2026-11-20. Branch `feature/inde-obj-01-foundation`; ponto de partida `d5e92cc`. Referência de produção preservada: `main` em `d41a5c9`. Escopo autorizado: reforço da **Fase 1**, em memória. Nenhum adaptador, GTK novo, alteração de schema ou conversão de projeto autoral integra este corte.

Este relatório substitui a limitação da auditoria inicial: os arquivos compactados foram efetivamente abertos, extraídos e conferidos, e os componentes abaixo foram lidos em suas implementações. A execução de testes é registrada separadamente em [validação](../validacao/VALIDACAO-OBJ-01.md); inspeção de código não equivale a teste de interface nem a aceite humano.

## Bases examinadas e proveniência

| Base | SHA-256 conferido com o manifesto | Arquivos regulares |
| --- | --- | ---: |
| `fontes/inde-schema28-89b47f27.tar.gz` | `3d6bee4c60dbecc602a56881b1957f90b55798dbc711c9c290701e2ad8a6e97e` | 641 |
| `fontes/inde-P03c-schema37-original.tar.gz` | `2e97a15ddcda56b8f5892d033c9a7427d6102bcf650454678491c50ff5459ef2` | 757 |
| `fontes/inde-integrado-schema37.tar.gz` | `554623abec5d7c3c175dab2aa6bbabc8a95abd6e3d98a27191cea0260a4965c4` | 758 |

Os hashes esperados vêm de `metadata/BASE-INDE.json` e `metadata/INTEGRACAO-INDE.json`. Os três arquivos foram extraídos em diretório temporário novo, com recusa de links, caminhos absolutos, travessia e membros duplicados. Todos os bytes extraídos foram novamente comparados aos membros dos arquivos. O inventário integrado também coincide exatamente com `arquivos_integrados` do manifesto. São checksums fixados no repositório, não atestação criptográfica externa da origem histórica.

A comparação schema 28 → P03c original encontrou **116 arquivos acrescentados, 84 alterados, 557 idênticos e zero removidos**. P03c original → integrado encontrou **um acrescentado, 17 alterados, 740 idênticos e zero removidos**. A lista completa está em [fontes.json](../validacao/evidencias-obj01/fontes.json). Os 17 ajustes do integrado estão concentrados na UI, incluindo visibilidade, clipboard, leitura, revisão e especializações. O novo `include/inde/ui/text_coordinates.hpp` recusa offsets GTK não representáveis e coordenadas não finitas antes de convertê-los. Portanto, a base integrada não deve ser substituída pela original por aparente semelhança de nomes ou schema.

Também foram consultados os contratos e testes existentes nas duas bases, as demos, os overlays, os patches e os serviços atuais do repositório. A busca por outros `.zip`, `.tar.gz`, `.tgz` e `.tar.xz` nos diretórios locais acessíveis de Codex e Programa INDE não encontrou outro pacote de fontes. O antigo caminho `Documents/Study/dev/Web/HTML/lit/UniLit/v3` não existe nesta máquina. Nenhum arquivo ausente foi tratado como auditado; arquivos autorais não foram abertos nem extraídos.

Reprodução independente, a partir da raiz do repositório:

```sh
python3 tests/audit_obj01_sources.py --extract-to /tmp/obj01-fontes-novas
```

O destino precisa ser novo. O script apenas confere fontes; não abre projetos nem executa migradores. Para preparar a compilação existente, foram usados os verificadores já presentes: `python3 verificar_base.py --extrair` e `python3 verificar_base.py --integrado --extrair`.

## Inventário das autoridades reais

Os caminhos `include/…`, `src/…` e `tests/…` nesta seção são membros de `inde-integrado-schema37.tar.gz`, extraíveis por meio do script acima. Caminhos iniciados por `cpp/`, `motor/` ou nomes Python na raiz pertencem ao repositório externo.

| Área / implementação lida | Aprendizado confirmado | Consequência para OBJ-01 |
| --- | --- | --- |
| `include/inde/project/writing.hpp`, `src/project/writing.cpp` | `Document` tem corpo, formatação, âncoras, referências, semântica, parágrafos, resmas e perfis. Colocação editorial é vínculo opcional. | `ObjectKind::Paragraph` não contém esse documento e não o substitui. Proibido criar segunda autoridade de texto por simples espelhamento. |
| `document_paragraphs.hpp`, `src/project/document_paragraphs.cpp`, `src/persistence/paragraph_codec.cpp` | Parágrafos têm UUID, conteúdo imutável compartilhado, ordem e coordenadas Unicode derivadas. IDs aposentados são preservados. Codec v1 usa offsets; v2 separa blocos e ordem. Validação exige igualdade entre renderização e `Document.content`. | Mover ocorrência visual não chama reordenação textual. Integração futura precisa mapa explícito de IDs/revisões e sincronização transacional comprovada. |
| `src/persistence/schema_migrator.cpp`, `apply_version_37` | A migração 37 consolida conteúdo dos parágrafos, sem trocar suas identidades, sob transação externa. | O schema 37 já contém a autoridade estrutural textual; não é justificativa para nova migração nesta fase. |
| `document_alternatives.hpp`, `src/project/document_alternatives.cpp`, `src/application/document_alternatives.cpp` | A folha ativa está no intervalo de `Document.content`; folhas cobertas guardam conteúdo e formatação. `safe_range` impede troca que transfira marcações por posição; P04 continua sendo gate específico. | Resma não equivale a contêiner gráfico nem a lista de ocorrências. Não mover ou divulgar folhas cobertas por aproximação. |
| `src/application/document_reading_order.cpp` | `reading_blocks` une parágrafos cobertos por resmas. O movimento recompõe o corpo, mantém os payloads/IDs, reparte estilos e só aceita âncoras/resmas como intervalos íntegros na mesma ordem. Separador perdido ou intervalo invertido causa recusa. | Movimento de objetos é outra operação. A futura ponte textual deve conservar todas essas recusas; não pode apenas permutar IDs OBJ-01. |
| `src/ui/writing_workspace_reading_order.cpp`, `writing_workspace_specializations.cpp`, `reading_block_gutter.cpp` | Já existe especialização estrutural, vizinho identificado e guarda de composição/IME/modal. A interface usa vizinhos visíveis, mantendo ordem relativa dos ocultos. | Contrato por âncora de ocorrência, nunca índice filtrado. Não é necessário decidir nova UI ou criar outro workspace nesta fase. |
| `src/application/document_visibility.cpp`, `src/project/document_visibility.cpp` | Perfis são projeções com intervalos fonte/vista, limites e proteção de LF. Referências ausentes não são adivinhadas. | Selecionar/mover pela vista precisa identidade e revisão da fonte. Visibilidade do editor não significa exclusão editorial ou permissão de publicar. |
| `include/inde/project/composition.hpp`, `src/experimental/text_blocks.cpp`, `composition_codec.cpp`, `composition_store.cpp` | A Composição existente tem blocos, revisões, origens congeladas, rascunhos, perfil e ocorrências live/pinned/forked/reference-only. Live acompanha a revisão do bloco, não o Documento original. Revisões e origens aplicadas são imutáveis; save confere revisão/hash. | A nova `objects::Composition` é grafo isolado, não substituto ou formato de serialização dessa Composição. Mantêm-se distintos os IDs e as autoridades. |
| `src/application/document_edit_session.cpp`, `document_text_transaction.cpp`, `src/ui/writing_workspace_search.cpp` | Undo inclui seleção e camadas, verifica o estado esperado e controla memória; timestamps pertencem à persistência. | Editor OBJ-01 tem estado privado, revisão monotônica, lote atômico e histórico limitado. Não reaproveita nem interfere no histórico textual. |
| `document_recovery.hpp`, `src/persistence/document_recovery_store.cpp`, `src/application/document_recovery_service.cpp` | Recovery tem sessão, revisão atual/durável, baseline, checksum, conflito e exportação. Durabilidade exige bytes efetivamente persistidos. | Undo em memória não é recovery. Este corte não promete restauração de OBJ-01 após reinício. |
| `text_semantics.hpp`, `src/application/semantic_anchor_engine.cpp`, `semantic_codec.cpp` | Offsets são Unicode, fingerprints incluem contexto; evidência divergente permanece ambígua. Relocate verifica texto idêntico e incrementa revisão sem inventar interpretação. | IDs não bastam para mover significado. O núcleo de objetos não cria personagens, POV, classificações, vínculos ou equivalências sem declaração autoral. |
| `src/persistence/sqlite_database.cpp`, `project_database_repository.cpp`, `sqlite_writing_repository.cpp`, `project_operation_store.cpp` | Chaves estrangeiras são ligadas e verificadas; transações fazem rollback por RAII; documentos persistem camadas relacionadas. Save As e operações de projeto têm regras próprias. | Uma transferência lógica não pode ser persistida como apagar/inserir fora de transação. Nenhum banco foi integrado a OBJ-01. |
| `cpp/comic_model.hpp`, `comic_model.cpp`, `comic_ui.cpp` | Quadros têm sequência, elementos têm geometria percentual limitada ao quadro. Arraste usa transformação tela/quadro; resize tem mínimo e limites; snap usa centro e tolerância de 1,5 unidades percentuais. Hit-test percorre pintura de trás para frente. | Aproveitar a separação entre sequência e geometria, não copiar percentuais, mínimos ou tolerâncias para unidades abstratas. Snap é política explícita da especialidade, não regra universal. |
| `cpp/comic_ui.cpp`, `comic_workspace_state.cpp` | Reordenação de quadros troca posições; checkpoints agrupa propriedades/nudges em 700 ms; undo retém até 40 estados leves e recupera mídia separadamente; revisão persistida não retrocede. | Alterar geometria ou pintura não altera leitura. Revisão do editor OBJ-01 também não retrocede em undo/redo; não se transportam bytes Base64 para cada comando. |
| `cpp/comic_service.cpp`, `publication_service.cpp` | Comic e publicação têm bancos separados, revisão esperada e retratos preparados; mudar fonte invalida preparo. | Revisão OBJ-01 é token local, não substitui revisões desses serviços, nem implementa transação distribuída entre bancos. |
| `cpp/snapshot_model.cpp`, `public_identity.cpp`, `ponte_inde.py`, `publicacao.py`, `motor/contrato_semantico.py` | Publicação seleciona documentos, conserva numeração/URLs e emite avisos: usa corpo corrente integral; alternativas cobertas ficam privadas e perfis não filtram a Edição. Recursos públicos têm inventário/hash. | IDs públicos não se derivam de posição visual. Exportação de novos objetos ainda exige tradutor, seleção e relatório de perdas; não há fallback que publique dados de autoria. |

## Fonte arquivada e aplicativo efetivamente compilado

A receita atual não compila diretamente o tar: `cpp/otimizar_base.py` cria uma cópia em `build-nativo/inde-source`, aplicando substituições exatas de `cpp/base_patches.json`; `cpp/integrar_gtk.py` e `integrar_workspace.py` geram os overlays usados pelo aplicativo. Esses mecanismos também foram examinados. O perfil ensaiado mantém `INDE_CODE_OPTIMIZATIONS=ON`, avisos como erros e desliga LTO para conter o custo desta recompilação local.

Há **23 arquivos diferentes** entre a base integrada e sua cópia de build: 20 sob correções de diagnósticos/conversões/testes e três sob otimizações de busca, revisão ortográfica e produção. Isso é transformação preexistente da receita, não alteração dos originais pela OBJ-01. [base-compilada.json](../validacao/evidencias-obj01/base-compilada.json) registra os hashes antes/depois e dos overlays. Os tipos `Document`, parágrafos, alternativas, a operação de leitura e a sessão de edição examinados permanecem os do pacote integrado; codecs de semântica/recovery têm conversões numéricas explícitas na cópia. Os testes de núcleo usam asserções habilitadas (`-UNDEBUG`) mesmo em Release.

A cópia de Documento para Comic em `comic::copy_document` leva somente texto corrente e procedência (`source_document`, hash do corpo). Não é referência viva nem transporte de formatação, resmas e semântica. Essa fronteira exige relatório de conservação no futuro adaptador; não se presume que copiar um Documento criou um objeto universal sem perdas.

## Riscos, invariantes e verificação

| Risco | Invariante / decisão neste corte | Evidência ou gate |
| --- | --- | --- |
| Deslocar todos os usos de um objeto compartilhado | Comandos operam em IDs de ocorrências; objetos/recursos não são reescritos | Testes `reorder_arrangements`, `transfer_and_identity`, oráculos gerados |
| Confundir leitura com sobreposição | `children` é leitura; `paint_order` é pintura independente para layouts espaciais | `frames_and_layers`; reordenação congela ordem implícita antes da mudança |
| Perder/duplicar ocorrência durante transferência | Estado candidato privado; seleção inteira deve ter um só pai; validação antes do commit | `cycles_and_bad_destinations`, `randomized_transfers` |
| Ciclo ao mover contêiner | Árvore única enraizada, alcançável, sem compartilhamento de layout | `nested_subtrees_and_deep_operations`, 250 árvores adversariais |
| Inventar posição ou perder retângulo | Preserve/Clear/Replace explícitos; Freeform exige frame; fluxo rejeita frame | `geometry_policies`, `change_arrangements`, `frame_assignment_errors` |
| Degradação de identidade ao desfazer | Snapshots completos do grafo; igualdade estrutural exata; revisão monotônica inclusive undo/redo | `transactions_and_conflicts`, oráculos e testes de profundidade |
| Sessão velha sobrescrever estado novo | Revisão esperada obrigatória; conflito não modifica estado, histórico ou redo | Testes de conflito depois de undo e redo |
| Recusa parcial de lote | Toda operação trabalha em cópia; falha em qualquer comando descarta lote inteiro | Lote com reorder válido seguido de frame inválido |
| No-op poluir histórico | Estado idêntico não avança revisão, não cria undo nem limpa redo | `noops_and_budget` |
| Memória/grafo desmedidos | Preflight de entidades/referências/bytes antes dos índices; histórico limitado em bytes e quantidade | `limits_and_invalid_initial`, `command_payload_limit`; limites não são estimativa de RSS |
| Overflow/NaN na geometria | Posição, dimensões e arestas resultantes finitas; tamanho positivo | Casos não finitos e overflow de arestas |
| Perda de texto, folhas ou semântica na futura integração | Não existe ponte de escrita neste corte; estruturas atuais permanecem autoritativas | Hashes dos fontes + regressões existentes; gate futuro de equivalência integral |
| Tratar visibilidade como autorização editorial | Fundação não publica nem filtra texto | Teste existente de publicação integral/alternativa privada; gate futuro de exportação OBJ-01 |
| Copiar projeto aberto como se fosse backup transacional | `ProjectRepository::save_as` usa cópia recursiva; `ProjectService` reatribui identidade do banco autoral; overlays adotam publicação/Comic depois, em transações separadas. Isso não prova snapshot simultâneo sob escrita concorrente ou WAL. A atualização da extensão de publicação, diferentemente, usa `VACUUM INTO` e verifica integridade. | Risco de compatibilidade observado em código; concorrência/falha entre adoções não ensaiada neste corte. Gate obrigatório antes de arquivo único; sem alterar Save As agora. |
| Tratar restore de recovery como transação entre dois bancos | Serviço pode persistir Documento e depois falhar ao arquivar journal; informa a persistência parcial e conserva evidência | `document_recovery_service.cpp`; não prometer atomicidade multibanco a partir da atomicidade local dos comandos OBJ-01 |
| Usar `source_base` público como prova suficiente do executável | `cpp/snapshot_model.cpp` mantém identificação histórica schema 28, junto a `source_schema=37`; identidade de build depende também de pacote/hash/patches/commit | Registrar proveniência completa; não alterar protocolo público silenciosamente |
| Confundir undo com durabilidade | Histórico somente em memória, sem SQLite/recovery OBJ-01 | Limite declarado, sem promessa de recuperação após fechar processo |
| Alterar arquivo autoral por teste | Somente fixtures novas em diretórios temporários; checkout original preservado | Logs da suíte e comparação de Git; nenhum projeto real como entrada |

## Compatibilidade e lacunas que continuam explícitas

A fundação continua experimental e não modifica os pacotes de fontes, migradores, codecs, arquivos SQLite ou protocolos de publicação. O campo novo `Layout.paint_order` é somente em memória; vazio mantém a interpretação anterior de pintura por `children`. Os comandos congelam esse default antes de reordenar a leitura. Não existe arquivo OBJ-01 anterior a migrar.

O contrato de movimentação cobre Stack/Row por inserção antes de vizinho ou ao fim; Grid por sequência compacta em linhas, sem células reservadas/spans/troca implícita; Overlay/Freeform por leitura, retângulo e pintura distintos. Transferência espacial acrescenta ocorrências no topo da pintura, na ordem relativa da seleção na origem. Coordenadas de destino pertencem ao destino; preservar valores é uma escolha explícita, não conversão automática de unidades.

Ainda exigem desenho e teste antes de integração: vínculo rico com parágrafos e revisões; aliases/instâncias de contêiner reutilizável; semântica entre folhas P04; agrupamento/desagrupamento e clipboard estrutural; inclusão/remoção de objetos; grade com spans/células fixas; alinhamento, spacing, constraints, overflow e responsividade; seleção visual e acessibilidade; drag-preview/cancelamento de gesto; permissões de edição; formatação e texto em objetos; recursos binários/deduplicação real; limites específicos de ingestão; serialização; recovery durável; exportação; migração física e SQLite entre serviços. Não se decidiu que uma nova especialidade visual é necessária. Nenhum desses itens é declarado entregue por existir um enum ou estrutura de dados.

Não avançamos para adaptadores, GTK, unificação de bancos ou arquivo único `.inde`. Os testes dos migradores existentes, quando executados, usam exclusivamente fixtures sintéticas; não representam migração de obras reais.

## Demonstrações e testes reaproveitáveis

| Demonstração | Aprendizado a preservar | Testes existentes correspondentes |
| --- | --- | --- |
| Integrada P03c | Parágrafos, resmas, visibilidade e texto corrente publicado sem folhas privadas | `tests/test_integracao_nativa.py`, testes C++ de parágrafos/alternativas/visibilidade/leitura/editor unificado |
| Comic | Seleção por ID, sequência de quadros, geometria/snap, undo e mídia | `cpp/comic_tests.cpp`, `tests/test_comic_nativo.py`, `tests/verificar_workspace_comic.py` |
| Catálogo | Obras/documentos e numeração pública independente de posição visual | `cpp/catalog_tests.cpp`, `tests/test_catalogo_nativo.py` |
| Importação e obra importada | Comparação explícita, hashes de fontes, IDs públicos permanentes | `tests/test_ambiente_importacao.py`, `tests/test_importacao_obra.py` |
| Publicação e sessão | Retratos preparados, integridade, geração local e fronteira autoria/público | `tests/test_publicacao.py`, `tests/test_sessao_nativa.py` |

Consolidar esses percursos significa conservar suas autoridades e recusas, não trocar seus tipos por `Object` sem protocolo de compatibilidade.

## Lacuna encontrada na própria evidência histórica

O driver GTK P03b `ui_document_visibility_smoke.cpp` ainda procura um segundo `TextView` chamado `visibility-lens-editor`. O P03c usa o editor contínuo único com tags; esse driver histórico falhou nos dois temas antes de exercer suas operações. Os percursos atuais de `ui_editor_specializations_smoke.cpp` passaram incluindo a visibilidade no mesmo editor, clipboard, semântica e ordem filtrada. O inventário diferencia essa incompatibilidade do teste de uma falha de conteúdo comprovada no aplicativo; os dois logs e a cobertura atual constam no relatório. Não se deve usar o sucesso histórico de P03b como evidência direta do P03c nem recriar o segundo editor para satisfazer o driver antigo.

## Complemento: save/readback e recovery de laboratório

O [corte de protocolo](SALVAMENTO-RECOVERY-OBJ-01.md) confirmou três pontos na implementação real: o CAS precisa permanecer dentro das transações proprietárias; o readback documental pode ordenar spans por posição sem mudar seu estado autoral; o journal documental não representa apresentação ou Composição OBJ-01. O reconhecimento usa igualdade proprietária e adota o readback, preservando o histórico com timestamp reservado. Dois processos ensaiam commit antes de ack e interrupção entre autoridades independentes. Não se promove esse ensaio a store de produção, lease, atomicidade conjunta ou migração. [Evidências](../validacao/VALIDACAO-OBJ-01-SALVAMENTO.md).

## Complemento: durabilidade e lease

O [store de laboratório](ARMAZENAMENTO-RECOVERY-OBJ-01.md) distingue arquivo temporário, geração publicada visível e publicação sincronizada. O lease por descritor impede acesso cooperante simultâneo ao mesmo store e é liberado na morte do processo; não substitui CAS autoral. Retenção remove somente versões antigas explicitamente selecionadas, com resultado parcial/indeterminado declarado em falha. Seis fronteiras de interrupção foram ensaiadas em processos novos, preservando a geração anterior. [Validação](../validacao/VALIDACAO-OBJ-01-ARMAZENAMENTO.md).

## Reconciliação de prioridade — 2026-11-21

A [matriz atual M01–M22](RECONCILIACAO-OBJ-01.md) separa capacidades implementadas de contratos e entregas pendentes. O inventário histórico permanece válido no escopo observado, mas não prova conteúdo multimídia, integração visual ou recursos compartilhados. O [contrato mínimo](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md) e o [piloto](PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md) passam a orientar os próximos cortes. Infraestrutura adicional não é requisito geral sem vínculo demonstrado a um caso de aceitação.

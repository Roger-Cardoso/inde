# OBJ-01 — Contrato mínimo de conteúdo e autoria composta

Data: 2026-11-21. **Contrato lógico 1.0 para revisão**, vinculado ao fechamento C01–C06 da [reconciliação](RECONCILIACAO-OBJ-01.md). Este documento não descreve uma API existente, não define migração do schema 37 e não altera o ABI do núcleo. Os nomes de campos/operações abaixo são conceituais; não são classes, tabelas ou comandos já implementados. O [piloto](PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md) será sua primeira prova executável.

## 1. Perfil e fronteiras

O primeiro perfil autoral admite parágrafo vinculado ao motor textual, figura PNG, tabela de texto simples e contêineres de apresentação. A equação recebe contrato de conteúdo desde já, com implementação/renderização posterior ao piloto inicial. Não se presume que `ObjectKind::Image`, `Table` ou `Equation` atuais possuam esses payloads.

O piloto usa uma captura privada de um Documento sintético schema 37. Sua origem externa é congelada e identificada; o trabalho composto é uma autoridade experimental separada. Não há live update do Documento original, substituição de `project::Document` ou uso de `project::Composition` textual como se já fosse multimídia. Nenhum conteúdo é classificado narrativamente por inferência.

## 2. Identidades, relações e revisões

- **Objeto:** identidade lógica estável e tipo. Possui conteúdo/revisão ou binding explícito para uma fonte proprietária. Não recebe identidade por posição, nome ou prosa semelhante.
- **Ocorrência:** uso identificado de objeto no documento. Mover esse uso não move os outros. Pai/layout, legenda contextual e apresentação pertencem a esse uso ou ao perfil correspondente.
- **Layout/contêiner:** estrutura de apresentação identificada. Conserva árvore única, alcançável e sem compartilhamento indevido, conforme o núcleo atual. Instâncias reutilizáveis ficam adiadas.
- **Recurso lógico:** UUID e metadados declarados, separado do hash SHA-256 dos bytes originais. Nome/caminho de origem é proveniência opcional; não é a autoridade de acesso ao recurso incorporado.
- **Relação:** aresta tipada por IDs e política de revisão explícita. O perfil mínimo precisa apenas de pertencimento estrutural, uso de recurso e binding de fonte. Relações semânticas existentes são preservadas em seu proprietário; não se cria uma ontologia paralela ou vínculos adivinhados.

Conteúdo aplicado e bytes identificados por hash são imutáveis. Edição gera revisão; undo/redo restaura conteúdo/links anteriores sem retroceder o token monotônico da sessão. Revisão local, revisão do payload, revisão persistida e revisão da origem são conceitos distintos. Captura histórica não pode ser rotulada como revisão corrente sem conferência.

A leitura de cada ocorrência declara se incorpora conteúdo de uma revisão ou apenas referencia um alvo. O primeiro perfil usa conteúdo próprio e snapshots explícitos; não acrescenta novos modos live. Os modos live/pinned/forked/reference-only da Composição existente mantêm seus significados e não são traduzidos automaticamente.

Editar conteúdo compartilhado exige escopo explícito: **alterar o objeto e seus usos vivos** ou **criar uma cópia independente para o uso selecionado**. Ausência dessa escolha quando houver vários usos é recusa. Uma mudança de posição/tamanho ou legenda de ocorrência não modifica bytes nem conteúdo dos outros usos.

## 3. Conteúdo mínimo por tipo

| Tipo | Conteúdo autoritativo mínimo | Projeções e restrições |
| --- | --- | --- |
| Parágrafo | Binding de projeto/Documento/parágrafo e snapshot de trabalho validado pelo proprietário, com texto, formatação e camadas | Coordenadas Unicode derivadas. Não guardar outra string concorrente no objeto genérico. Um parágrafo da fonte tem um único uso incorporador no perfil inicial; reutilização adicional exige referência explícita futura |
| Figura | Objeto aponta para recurso identificado e revisão esperada; ocorrência guarda legenda/alt-text textuais e decisão explícita de figura decorativa | Original PNG preservado. Dimensões de exibição pertencem à apresentação, não reescrevem a imagem. Texto alternativo e significado não são inferidos |
| Tabela | Linhas e colunas com IDs estáveis, células com IDs próprios e texto UTF-8; uma célula por par linha/coluna; cabeçalhos declarados | Ordem de linhas/colunas é autoral. Sem fórmulas, mesclagens, objetos aninhados ou rich text no perfil inicial. Mover a tabela não transforma células em tabulações no Documento |
| Equação | Fonte UTF-8 editável, `source_format=latex-math`, modo inline/block e identidade/revisão; rótulo/numeração apenas se declarados | Representação gráfica é derivada. Ausência de renderizador não autoriza perder a fonte ou publicar só uma imagem. Interpretador/renderizador não é escolhido neste corte |
| Contêiner | Referência a layout interno exclusivo | Não recebe conteúdo textual duplicado. Agrupamento/instância universal ficam adiados; o piloto pode construir o Row inicial por uma operação experimental controlada |

Legenda e alt-text de figura são campos subordinados à ocorrência, não novos parágrafos ocultos no corpo textual. O texto de células e equações tem coordenadas próprias: não participa dos offsets de `Document.content`. Uma projeção de leitura composta pode gerar segmentos por objeto/célula, sem fingir que são intervalos do Documento legado.

### Recursos mínimos do piloto

Importação aceita `image/png` estático. Confere bytes, tamanho, SHA-256 real, decodificação e dimensões antes de publicar a referência. MIME/extensão declarados não substituem essa verificação. Arquivo inválido, PNG animado, formato não suportado ou limite excedido é recusa sem ocorrência parcial; não há conversão silenciosa.

Os bytes ficam disponíveis no armazenamento autoral experimental; apagar/mover o arquivo original depois da importação não quebra a reabertura. Toda referência incorporadora deve resolver recurso e blob íntegros. O mesmo conteúdo binário pode ser endereçado por hash, sem confundir isso com igualdade de recursos lógicos ou autorização de alterar todos os seus usos.

Perfil inicial proposto para implementação e teste: PNG até 8 MiB, lados até 8.192 pixels e até 16.777.216 pixels decodificados; total de blobs referidos pelo documento até 32 MiB. Tabela até 64 linhas × 32 colunas, texto até 4.096 escalares Unicode por célula e strings agregadas do documento até 8 MiB. Não são capacidades medidas nem limites do INDE atual. O código futuro deve validar limites individuais e agregados antes de admitir o candidato, sem truncamento; ajustes exigem justificativa no perfil.

Todo campo textual exige UTF-8 válido; uma entrada malformada é recusada, sem substituição silenciosa de caracteres. A ponte textual conserva seus limites atuais e pode recusar uma fonte além deles. O limite proprietário de 100.000 identidades de parágrafo não é promessa de admissão pelo piloto/ponte, atualmente limitada a 10.000 parágrafos.

## 4. Autoridades e ordenação — decisão C02

O agregado experimental possui **uma sequência autoral explícita de ocorrências incorporadoras**, incluindo parágrafos, figuras e tabelas. Contêineres de apresentação não são itens textuais artificiais dessa sequência. Toda ocorrência incorporadora ativa aparece exatamente uma vez; objetos/recursos de biblioteca sem uso não ganham posições fictícias.

O grafo OBJ-01 continua representando árvore, ordem local de seus filhos, frames e pintura. Seu `Layout.children` não passa a ser uma segunda fonte independente da sequência autoral do novo documento. A ordem local da apresentação e suas projeções precisam de binding explícito com a sequência autoral. Não se deriva autoria por coordenadas, topo/esquerda, z-index ou ordem de armazenamento de vetores.

Invariantes do agregado composto:

1. Filtrar a sequência autoral pelos bindings incorporadores de parágrafos deve produzir exatamente a ordem de IDs em `working_text.paragraphs.rows`.
2. O corpo textual continua sendo a renderização exata desses parágrafos pelo motor proprietário; figuras, legendas, células e equações não são injetadas nessa string como marcadores sem autoridade.
3. Uma reordenação autoral que afete parágrafos atualiza sequência composta e snapshot textual na mesma unidade privada, por meio do motor existente, ou é integralmente recusada.
4. Um grupo de leitura/resma não pode ser dividido, nem receber figura/tabela no seu interior sem um contrato adicional. O piloto só insere entre grupos completos.
5. Conteúdo/IDs das folhas cobertas, alternativas, âncoras, semântica, visibilidade e aposentadoria de parágrafos são conservados pelo proprietário. Recusa de intervalo não é contornada por aproximação de offsets.
6. Alteração exclusivamente visual conserva a sequência autoral e todos os payloads. O leitor/acessibilidade consulta a sequência autoral explicitamente, não uma ordenação espacial implícita.
7. Como nas sessões atuais, intenção autoral exige apresentação previamente alinhada à sequência autoral. No perfil inicial, alinhamento significa que percorrer os filhos da árvore em profundidade, na ordem local declarada, omitindo contêineres e incluindo apenas ocorrências incorporadoras, produz exatamente a sequência autoral; pintura e coordenadas não entram nesse cálculo. Se uma reordenação apenas visual divergiu, restaurar/desfazer esse alinhamento é ação explícita anterior, sem promoção automática.

Essa redundância validada entre sequência composta e representação textual proprietária não concede duas autoridades editáveis: apenas o comando transacional do agregado pode modificá-las, respeitando os invariantes. O Documento original da fixture permanece congelado; não há writeback simultâneo a dois bancos legados.

## 5. Apresentação e leitura

A prova inicial usa Stack e um Row com parágrafo e figura lado a lado. Grid simples pode ser exercitado como regra existente; Overlay/Freeform permanecem disponíveis para regressão geométrica, sem serem requisitos de uma interface completa.

Larguras proporcionais pertencem a um **perfil de apresentação do piloto**, indexado por IDs de ocorrência, separado de `Frame`. No Row inicial as frações positivas finitas somam 1 (tolerância numérica de 1e-9); gap é não negativo e finito em unidades lógicas do renderizador. Não se normalizam silenciosamente entradas inválidas. O renderizador converte esse perfil em geometria disponível; `Frame` atual continua em unidades abstratas locais, sem ser reinterpretado como pixels/percentuais.

O piloto usa largura de viewport controlada para a prova lado a lado. Responsividade completa, spans, colisão, snap universal, coordenadas globais e retenção de preferências de arranjos inativos ficam adiados. Um comando que exigiria converter unidades sem regra definida é recusado.

Imagem ausente/corrompida deve produzir diagnóstico visível e impedir confirmação de um save incorreto. Não se substitui silenciosamente mídia por espaço vazio. O leitor experimental apresenta legenda/alt-text/cabeçalhos declarados, sem numeração automática ou classificação inventada.

## 6. Operações — decisão C03

Nomes abaixo são contratos de operações futuras, não acréscimos ao `Command` atual.

| Operação lógica | Pré-condição e efeito mínimo | Reuso / falta atual |
| --- | --- | --- |
| Criar conteúdo + inserir ocorrência | IDs novos, payload válido, destino entre grupos; recurso verificado antes da referência; publicação da unidade inteira ou recusa | Pendente no agregado; não simular por edição direta de vetores expostos |
| Editar parágrafo | Seleção/identidade/revisão válidas; delegar ao motor textual, remapear/conservar camadas e sincronizar novos/aposentados bindings | Motor existente reutilizável; adaptador composto pendente |
| Editar figura/legenda | Escopo objeto/ocorrência explícito; substituir recurso cria vínculo/revisão nova, conserva original/histórico | Payload e comando pendentes |
| Editar célula | Identificar tabela/célula por ID e revisão; alterar somente conteúdo escolhido e revisão correspondente | Pendente |
| Inserir/remover linha ou coluna | Identidades sobreviventes preservadas; IDs novos para elementos novos; nunca deixar tabela sem linha/coluna; remoção reversível | Contrato definido; implementação pode seguir a primeira edição de célula sem exigir merges/fórmulas |
| Reordenar/transferir | Intenção explícita, IDs/vizinhos e revisão; preservar ordem relativa e identidade; sincronizar texto se autoral | Núcleo geométrico existe; tradução multimídia pendente |
| Alterar arranjo/frame/pintura | Conservar sequência autoral/payload; aplicar política geométrica explícita | Reusar comandos atuais, sem misturar medidas do perfil com `Frame` |
| Remover ocorrência | Retirar um uso, preservar objeto/bytes/revisões necessários a undo e outros usos; se parágrafo, respeitar motor/camadas | Pendente; não chamar exclusão global de Documento/recurso |
| Desfazer/refazer | Restaurar agregado, referências, seleção e apresentação coerentes; revisão monotônica | Adaptar histórico ao novo agregado completo; sessões tipadas atuais não cobrem payload multimídia |

No perfil inicial não há coleta automática de blobs/objetos sem uso. Exclusão definitiva de recursos, clipboard universal e instâncias de contêiner não são requisitos do piloto. Edição nova após undo invalida redo conforme o contrato existente.

## 7. Contexto de edição e especialidades

Contexto compartilhado mínimo: projeto/documento composto, revisão, ocorrência selecionada e seleção tipada. A seleção é uma destas: intervalo textual, ocorrência(s), célula ou alvo geométrico. Mudança de instrumento não cria cópia de conteúdo.

- **Texto:** cursor, digitação e formatação; seleção/IME continuam sob as guardas do editor existente.
- **Estrutura:** seleção explícita de ocorrências e comandos de sequência/transferência/remoção.
- **Composição:** arranjo, proporção, frame e pintura.

Arraste textual e arraste de objeto não compartilham gesto ambíguo sem seleção/alça/modo explícito. Toda operação mínima tem alternativa por teclado. A transição preserva o alvo por ID e recusa contexto/revisão inválidos. Zoom, rolagem, painéis e filtros são estado de interface por especialidade; não definem conteúdo ou autorização para publicar.

A prova só implementa os instrumentos necessários ao percurso escolhido. Um framework completo de workspaces ou alternância automática não é dependência.

## 8. Persistência experimental — decisão C04

**Registro de implementação P2-A:** a [arquitetura do piloto](P2A-DOCUMENTO-COMPOSTO-OBJ-01.md) documenta a exceção ao reuso dos wrappers canônicos: `SqliteTransaction` depende também de `OperationCapture`/histórico/remoção/migração. O alvo isolado usa adaptador SQLite local com prepared statements e rollback por RAII, conservando as regras transacionais abaixo, sem alterar wrappers originais ou ligar serviços canônicos por stubs. P2-A persiste apenas estado salvo; o registro de trabalho recuperável permanece pendente para P2-C.

Decisão para o piloto: **SQLite auxiliar exclusivo de fixture**, criado novo, fora do `.inde` de origem e sem abertura automática pelo aplicativo. Nome de trabalho: `obj01-piloto.sqlite3`. Tem seu próprio protocolo lógico versão 1; não altera `project.sqlite3`, codecs/migradores do schema 37 nem o formato físico `.inde`.

O modelo lógico persistido possui documento composto, payloads/bindings, sequência autoral, apresentação/seleção, revisão/hash e registro de recursos/blobs. O snapshot da origem schema 37 é congelado e identificado; a cópia de trabalho só muda pelo agregado experimental. JSON versionado legível pode transportar o snapshot privado, e bytes PNG ficam em BLOBs verificados. A definição física das tabelas é tarefa do corte executável, respeitando este contrato e usando os wrappers transacionais existentes.

O estado autoral salvo e o estado de trabalho recuperável são registros distintos nesse mesmo armazenamento auxiliar. Um checkpoint de trabalho referencia somente blobs já confirmados nele; importação de bytes e sua primeira referência recuperável pertencem à mesma transação. Save compara revisão/hash esperados e confirma o estado salvo; readback revalida conteúdo e vínculos. A origem congelada não é atualizada por esse processo.

Não há sincronização distribuída com os bancos legados. Um conflito mantém o candidato/trabalho recuperável e recusa a gravação pretendida, sem rebase automático. Versão desconhecida, hash falso, referência pendente ou conteúdo incompleto não é carregado parcialmente nem sobrescrito para fazê-lo passar.

O repositório mínimo de PNG é implementação do contrato de recurso para a prova, não serviço universal da Fase 3. Não inclui integração Comic, migração de capas, importação em massa ou consolidação de mídia existente.

## 9. Histórico, recuperação e conservação — decisão C05

Uma unidade de edição abrange todos os dados afetados: sequência, snapshot textual, payloads, relações, seleção e apresentação. O mecanismo pode reutilizar candidatos privados e comandos existentes, mas não aplicar apenas o grafo e esquecer a tabela/imagem. Bytes imutáveis podem ser retidos por referência; remover um uso não invalida blob necessário a histórico ou recuperação.

A prova precisa de undo/redo de inserção, edição de célula, movimentação e remoção. Save reconhecido na sessão viva mantém o histórico. Reinício recupera trabalho completo, seleção e apresentação, abrindo nova sessão sem tickets nem undo/redo antigos; esse limite deve ser apresentado claramente. Histórico persistente entre reinícios é posterior, não resultado implícito dos testes existentes.

Os codecs `OBJ01-RECOVERY-LAB-1` e o store atual foram feitos para Documento/Composição textuais e apresentação capturada. **Não representam células ou bytes de mídia do novo agregado.** Não se pode emitir aquele checkpoint e declará-lo recuperação completa do piloto. O registro de trabalho recuperável do piloto deve cobrir seus conteúdos e referenciar blobs verificados; adaptar um envelope exige versão explícita e testes de conservação.

O store Linux já entregue fica preservado como laboratório/testes de falha. Seu coordenador automático e a manutenção de temporários não são dependências gerais do piloto. Só serão propostos se um caso P01–P14 demonstrar bloqueio que o armazenamento auxiliar transacional não resolva.

Alternativas/folhas privadas, rascunhos e origens permanecem privados. Visibilidade editorial, visualização e seleção pública continuam decisões distintas. Exportador antigo que não sabe representar os tipos novos deve recusar o documento composto; omitir figura/tabela/equação silenciosamente é proibido. Publicação completa fica em corte próprio.

## 10. Critério de uso deste contrato

O contrato fica pronto para revisão quando todas as decisões C01–C06 possuem regra e seus casos de aceitação estão definidos. Ele não prova implementação. O primeiro código do piloto deve ligar cada operação e limitação a [P01–P14](PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md), registrar testes e parar no escopo autorizado. Mudanças de autoridade, formato ou escopo precisam ser explicadas e registradas, respeitando as fontes protegidas e os limites do trabalho. O usuário dispensou a confirmação ritual por corte após a discussão do planejamento.

## Registro P2-B

[Implementação de tabela e histórico vivo](P2B-DOCUMENTO-COMPOSTO-OBJ-01.md): perfil experimental 2 separado de P2-A; IDs de células e revisão de conteúdo; remoção de usos com biblioteca conservada; histórico limitado em memória. O perfil 1 não é migrado. P13 e histórico entre reinícios continuam pendentes.

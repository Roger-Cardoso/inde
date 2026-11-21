# OBJ-01 — P2-A implementado: Documento sintético e PNG

Data: 2026-11-21. Corte autorizado expressamente após apresentação de entrega, áreas, testes, riscos e parada: “prossiga, pode iniciar agora”. Base: `42a623311db44017634b1504a2b34c0e8bf3da25`. Implementação: `f3edd18`. Branch `feature/inde-obj-01-foundation`.

P2-A é o primeiro corte executável da Fase 2 proposta. Não altera retroativamente o escopo da reconciliação documental nem conclui a Fase 2 inteira. Ver [piloto e critérios](PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md), [contrato](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md), [validação](../validacao/VALIDACAO-OBJ-01-P2A.md) e [como abrir](../../tools/obj01-pilot/README.md).

## Entrega e autoridades

O alvo isolado `inde_object_composite_pilot` captura um Documento proprietário sintético e conserva seu snapshot integral. A origem inclui três parágrafos Unicode, identidades, formatação, âncora, resma de dois parágrafos, folha alternativa coberta e perfil de visibilidade. O piloto cria essa fixture; não abre bancos de obras nem um projeto SQLite schema 37 completo.

`Session` possui a cópia de trabalho privada, os usos de figura, recursos binários e as sequências autoral/visual. A origem textual é congelada: não existe edição textual ou writeback ao Documento original neste corte. A sequência autoral filtra para exatamente a ordem de parágrafos da ponte; grupos de leitura precisam permanecer contíguos nessa sequência. Figuras/legendas não viram caracteres no corpo nem offsets artificiais.

Figura possui objeto, recurso e ocorrência com identidades distintas. Outro uso compartilha objeto/recurso e recebe ID de ocorrência próprio, conservando sua legenda/descrição contextual. O PNG é imutável; importar o mesmo conteúdo outra vez pode criar outra identidade lógica, mantendo um blob por hash. Este corte não edita posteriormente conteúdo compartilhado.

A apresentação tem sequência visual e perfis Row de dois filhos adjacentes: uma figura e um parágrafo. Esses dados produzem uma árvore `Composition` com contêineres/Layouts Row identificados e validada pelo núcleo existente; o grafo não vira autoridade textual. Perfis são persistidos com os mesmos IDs. Movimento usa `CompositionEditor`/`ReorderOccurrences` em candidato privado, e o agregado é validado antes de substituir a sessão.

## Operações implementadas

| Operação | Efeito / guarda |
| --- | --- |
| Capturar/abrir | Fonte íntegra e profunda pela ponte existente; snapshot original conferido na reabertura |
| Importar PNG | Base64 estrito, assinatura, chunks/CRC, IHDR, limites, recusa de APNG, decodificação e dimensões; recurso/objeto/uso novos em unidade privada |
| Criar outro uso | Compartilha objeto e recurso, cria ocorrência/legenda contextual; não duplica bytes por posição |
| Mover figura | Intenção obrigatória: visual mantém sequência autoral; autoral exige alinhamento e conserva ordem dos parágrafos/grupos |
| Mostrar lado a lado | Perfil Row finito, frações positivas somando 1, filhos adjacentes; não muda autoria |
| Coluna única | Desfaz o Row do uso escolhido; mover para a coluna principal também desfaz esse Row explicitamente |
| Restaurar apresentação | Reconstrói visual pela sequência autoral e retira Rows; ação explícita, sem promover ordem visual a autoria |
| Selecionar | Alvo por ocorrência; clique ou Enter/Espaço, foco conservado; seleção é parte do estado salvo |
| Salvar | CAS de revisão/hash, blobs + snapshot na mesma transação, validação/readback antes do commit, reconhecimento da revisão salva |
| Reabrir salvo | Valida versão, origem, hash, conteúdo e blobs; trabalho pendente exige sinal explícito de descarte; recusa não substitui a sessão |

As operações públicas usam revisão esperada da sessão; revisões de sessão e persistência são distintas. Recusas deixam candidato, ordem e seleção anteriores intactos. O perfil é deliberadamente menor que o contrato completo: sem transferência arbitrária entre todos os arranjos, editor de parágrafos, remoção, undo/redo ou histórico persistente.

## Persistência física experimental

Uma pasta dedicada, pertencente ao usuário e sem acesso de grupo/outros, contém:

- `origem-documento.json`: snapshot proprietário original, criado exclusivamente uma vez e nunca regravado pelo piloto.
- `obj01-piloto.sqlite3`: banco auxiliar exclusivo, sem migrador ou tabelas do projeto canônico.
- `.pilot.lock`: lease de uma sessão por pasta. A exclusividade não é autenticação contra outro processo do mesmo usuário.

O banco usa `application_id=0x4f424a31`, `user_version=1`, journal DELETE e conexão com `synchronous=FULL`. A tabela `saved` tem uma linha com revisão, SHA-256 e snapshot; `blobs` guarda os bytes por SHA-256. O snapshot lógico tem formato `INDE-OBJ01-COMPOSITE-P2A-1`, origem completa, bindings derivados por IDs, sequências, figuras, perfis e seleção; não inclui cópia concorrente de texto de parágrafo.

Uma transação `BEGIN IMMEDIATE` lê/confere o estado salvo esperado, insere blobs ausentes, atualiza por revisão/hash e reabre/valida o candidato ainda na mesma transação antes de confirmar. Rollback impede referência ou blob novo parcial nos erros exercitados. CAS conflitante conserva o trabalho em memória; não existe rebase implícito. Não se alega prova de todas as falhas físicas de commit ou de energia.

Ao reabrir, tamanho/versão e SHA do snapshot são conferidos, a origem externa deve coincidir byte a byte com a captura congelada, e todo blob deve resolver, ter hash/dimensões corretos e decodificar. Limites precedem cópia de valores grandes do banco. Banco não reconhecido e versão desconhecida não são migrados nem sobrescritos.

**Exceção fundamentada ao reuso de wrappers da seção 8 do contrato:** `SqliteTransaction` canônico referencia `OperationCapture` também no destrutor/commit, trazendo histórico de operações e dependências de remoção/migração. Para manter este alvo isolado, P2-A usa um adaptador SQLite local com prepared statements e rollback por RAII. Não há stubs desses serviços nem mudança dos wrappers originais; não se propõe outro serviço universal. O contrato lógico/transacional continua o mesmo.

O serializador JSON proprietário usa precisão padrão de stream e termina com LF. O piloto reutiliza parser e escape de escalares, mas serializa seus números com `max_digits10` e emite uma única linha por resposta. Teste de proporções/gap com muitas casas decimais cobre a conservação por save/reabertura. Os codecs originais não foram modificados.

## Interface e demonstração

Python fornece HTML/CSS/JS e encaminha pedidos JSON ao processo C++ vivo. Não calcula ordem, IDs, hash, validação documental ou save. O navegador renderiza respostas do núcleo e imagens vindas do registro de recursos; nenhuma funcionalidade é simulada por estado local sem backend.

O servidor escuta apenas em loopback, exige token de sessão nos endpoints e confere Host/Origin; não oferece caminhos livres para arquivos. CSP e `textContent` impedem interpretar legenda/descrição como HTML. O teste visual confirmou legenda `<img ...>` como texto literal, sem criar elemento de mídia correspondente. Esses controles não equivalem a auditoria de segurança universal.

A interface distingue apresentação e leitura, oferece alternativa por teclado e identifica que o texto é somente leitura. A folha coberta é conservada no snapshot, sem ser enviada à interface; a vista ativa é autoral integral e não aplica filtros do perfil de visibilidade. Não existe publicador conectado.

## Perfil e fronteira de parada

PNG até 8 MiB, lados até 8.192 e até 16.777.216 pixels; até 32 MiB/64 blobs distintos, 256 usos e 128 Rows; legendas/descrições até 4.096 bytes UTF-8 e strings agregadas até 8 MiB. Row: frações finitas positivas somando 1 em tolerância 1e-9, gap 0–256 unidades lógicas do renderizador; `Frame` continua sem reinterpretação de unidades.

Somente estado salvo é durável neste corte. **Não há checkpoint de trabalho multimídia nem recuperação de alterações não salvas.** Encerrar o processo C++ perde o trabalho em memória. O codec textual anterior não é usado para alegar P13.

P01–P05 têm prova técnica no perfil descrito, com limites visuais registrados; P09 está comprovado para texto/PNG/perfis, ainda sem células. P06–P08, P10 e P13 continuam pendentes; P11/P12 têm provas parciais relativas a este perfil; P14 não é promovido por existência de duas abas de visualização. A conclusão de P2-A não autoriza P2-B, GTK, migração, publicação, integração Comic ou arquivo único `.inde`.

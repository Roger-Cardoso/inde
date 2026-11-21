# OBJ-01 — P2-C: trabalho recuperável e grupos textuais

Data: 2026-11-21. Base: `b1ae38f` (fechamento P2-B). Implementação: `b637d0b`. Branch: `feature/inde-obj-01-foundation`. Ver [roteiro utilizável](../../tools/obj01-pilot/P2C.md), [validação](../validacao/VALIDACAO-OBJ-01-P2C.md), [contrato](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md) e [inventário das bases](INVENTARIO-OBJ-01.md).

Este corte dá ao agregado experimental registro durável de trabalho ainda não salvo, reordenação de grupos textuais pelo motor existente e instrumentos mínimos Texto/Estrutura/Composição. Reutiliza P2-A/B e preserva as fontes canônicas, o núcleo 0.8 e os laboratórios anteriores. Não conclui automaticamente a Fase 2 nem inicia a Fase 3: P14 ainda tem lacuna de integração com o publicador principal e o aceite humano permanece separado.

## Aprendizados aplicados

| Evidência da auditoria e dos cortes anteriores | Decisão em P2-C | Consequência verificável |
| --- | --- | --- |
| O Documento contém parágrafos, grupos de leitura, alternativas, folhas privadas, intervalos e camadas que não cabem numa lista de texto | Conservar snapshot integral congelado e nova cópia textual de trabalho; aceitar somente a transformação integral produzida pelo motor proprietário | Snapshot alterado fora da reordenação suportada é recusado; origem continua idêntica |
| Mover um grupo pode afetar offsets e intervalos semânticos | Chamar `application::move_reading_block` com revisão da sessão e grupo completo; comparar snapshot resultante com reprodução a partir da origem | P10 reordena os dois parágrafos da resma; P11 recusa grupo parcial e intervalo incompatível atomicamente |
| Ordem autoral e apresentação são autoridades distintas; Row não é parte do texto | Movimento textual exige ordens alinhadas e desfaz somente Rows que contêm parágrafos movidos | Outro Row permanece íntegro; conteúdo e bytes não recebem marcadores de layout |
| Recovery textual antigo não carrega tabela ou PNG | Perfil auxiliar 3 com snapshot completo do agregado e blobs verificados na mesma transação | Reinício recupera figura, Row, células, ordens, cópia textual, seleção e instrumento |
| Salvar não deve significar apenas que uma tentativa ocorreu | Readback, hashes, revisão/base/geração e confirmação após commit; candidato retido e bloqueio de escrita quando o resultado é incerto | Falha antes do commit faz rollback; confirmação perdida exige reabrir e conferir |
| Undo textual isolado não restaura mídia e células | Histórico P2-B do agregado, com checkpoint do resultado antes de publicar undo/redo | Estado desfeito também sobrevive a SIGKILL; histórico antigo não é anunciado após reinício |
| Remover uso não autoriza excluir recurso global | Biblioteca conservada; blobs não referidos após descarte explícito continuam verificados e sujeitos aos limites | Reabrir salvo não apaga bytes, e novo uso do mesmo PNG funciona |
| Ferramentas distintas precisam manter o alvo | Contexto explícito, seleção comum por IDs e guardas no núcleo | Troca Texto/Estrutura/Composição conserva seleção/célula e recusa operação fora do instrumento |
| Publicação antiga pode omitir tipos que não conhece | A fronteira `export_legacy` do piloto recusa antes de produzir saída | Nenhum exportador principal é chamado; integração canônica continua pendente |

A conferência das três fontes compactadas foi repetida neste corte, com extração separada e comparação de bytes. O [inventário original](INVENTARIO-OBJ-01.md) continua sendo a referência para Document, Composition, alternativas/visibilidade, Escrita/undo/recovery, Comic/drag/resize/snap, referências, SQLite e publicação. Este corte aplica os aprendizados pertinentes; não reimplementa Comic nem transforma a regressão nativa em prova de integração gráfica.

## Formato e compatibilidade

P2-A conserva `user_version=1`; P2-B conserva `user_version=2`. P2-C usa `user_version=3` e `INDE-OBJ01-COMPOSITE-P2C-1` em banco auxiliar identificado. Não altera o schema 37 canônico. Cada perfil recusa abertura de versão incompatível, sem upgrade implícito.

A cópia P2-B → P2-C lê somente `saved` por conexão READONLY e transação consistente. Exige destino novo; não importa trabalho apenas em memória da sessão B. Identidades, recursos e origem são conservados. As demos anteriores permanecem independentes e seus hashes foram conferidos antes/depois.

Além de `saved` e `blobs`, o perfil 3 contém uma linha `working` com geração monotônica, revisão/hash da base salva, hash e snapshot completo. `source` continua a origem congelada, também conservada em `origem-documento.json`; `working_text` é a cópia proprietária reordenável. O snapshot inclui contexto e seleção; mudar instrumento pode portanto marcar diferença em relação ao salvo.

## Transação e recuperação

1. Construir candidato privado, validar o agregado e preparar o histórico dentro dos limites existentes.
2. Abrir transação SQLite; conferir `saved` e `working`, suas revisões/hashes, versões, origem e recursos. Conferir a base do trabalho e a geração esperada.
3. Inserir blobs imutáveis e substituir o registro de trabalho; conferir por nova leitura antes de commit. O banco usa `synchronous=FULL` e journal `DELETE`.
4. Só após commit conhecido atualizar a memória, histórico e revisão da sessão. O resultado positivo enviado à interface é a fronteira de proteção.
5. Salvar promove o trabalho a `saved` e atualiza a base de `working` na mesma transação; preserva histórico da sessão viva.
6. Reabrir trabalho verifica o registro atual. Reabrir salvo faz descarte explícito por novo checkpoint do salvo. Ambos abrem histórico vazio. Registro corrompido não provoca fallback silencioso para um salvo mais antigo.

Antes de tentar commit, uma exceção conserva estado publicado, histórico e banco por rollback e expõe erro. Se a tentativa de commit ocorreu e não se pode confirmar seu resultado, o candidato fica retido em memória, o status vira incerto e novas alterações são recusadas até reabertura. Não se promete tornar esse candidato volátil recuperável se uma falha ambígua não tiver gravado nada e o próprio processo acabar.

O executável normal não aceita injeção de falhas. Um driver separado, compilado somente para testes, interrompe de fato o processo com `_exit(86)` em cinco pontos de checkpoint e cinco de Salvar. Outros testes usam SIGKILL, erro antes do commit e exceção após commit. Eles não equivalem a ensaios de falta de energia, controladora de disco ou todos os erros físicos possíveis.

## Riscos, invariantes e cobertura

| Risco | Invariante / proteção | Prova |
| --- | --- | --- |
| Mover só a vista e deixar texto/semântica antigos | Grupo completo + motor proprietário + validação integral reproduzível | P10, HTTP, undo/redo e reinício |
| Partir resma ou intervalo protegido | Operação candidata atômica, recusa proprietária antes da gravação | Teste negativo com intervalo sintético atravessando grupos |
| Checkpoint referir PNG ainda ausente | Blobs e snapshot na mesma transação; hash/PNG/MIME/dimensões na admissão/leitura | Dez janelas de interrupção, integridade e corrupção |
| Outra escrita tornar a base obsoleta | Lease da pasta e CAS de revisão/hash/geração | Conflito de geração e reabertura; lease herdado dos testes P2-A |
| Gravação falhar mas a tela anunciar proteção | Status de erro/incerto e confirmação só após commit conhecido | Falha antes e depois do commit; Save com confirmação perdida |
| Descarte apagar biblioteca necessária | Retenção sem GC; verificar blobs não referidos e contar limites globais | Descarte + reinício + reimportação |
| Reinício anunciar undo inexistente | Histórico exclusivamente vivo; estado durável independente | SIGKILL depois de edição e depois de undo; botões vazios no navegador |
| Troca de instrumento perder alvo | Seleção/célula por identidade no agregado | Núcleo, HTTP e observação da interface |
| Omitir tipos na publicação | Recusa explícita no piloto | Núcleo/HTTP/interface; publicador principal não integrado |
| Converter perfis ou afetar obras | Novo diretório, versão explícita, seed READONLY e recusa de `.inde` | Hashes das demos A/B, regressões e conferência de main/fontes |

Limites herdados: 256 usos de figura, 64 tabelas, 128 Rows, 64 blobs/32 MiB, 8 MiB de strings agregadas, snapshot até 24 MiB; cada célula até 4.096 escalares Unicode. O histórico conserva até 32 entradas com orçamento de 32 MiB de metadados incluindo corrente e 32 MiB de blobs distintos. A reprodução da ordem textual admite até 256 grupos quando a origem muda. Esses orçamentos não equivalem a limite de RSS ou promessa de latência.

## Fronteira de entrega

P10 e P13 têm implementação e prova técnica; P11/P12 recebem casos adicionais. P14 tem contexto/privacidade e recusa no piloto, com lacuna explícita de integração ao publicador canônico. P01–P09 conservam as provas dos perfis anteriores e sua regressão, sem promover o seletor nativo de PNG a passe visual.

Não foram implementados edição livre dos parágrafos, linhas/colunas adicionais, fórmulas, equações, mesclagens, tabela compartilhada, drag/resize/snap genérico, migração Comic, publicação multimídia, GTK ou `.inde` único. O próximo trabalho de integração precisa tratar a lacuna concreta do publicador e passar por revisão do experimento; não é iniciado por este relatório.

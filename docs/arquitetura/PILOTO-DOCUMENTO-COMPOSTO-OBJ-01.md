# OBJ-01 — Primeiro experimento autoral e seus critérios de aceitação

Especificação inicial: 2026-11-21, preparada no fechamento documental da Fase 1. Referências normativas: [reconciliação](RECONCILIACAO-OBJ-01.md) e [contrato mínimo 1.0](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md). **Atualização posterior: P2-A foi autorizado expressamente e implementado**, com [arquitetura própria](P2A-DOCUMENTO-COMPOSTO-OBJ-01.md), [demo utilizável](../../tools/obj01-pilot/README.md) e [validação](../validacao/VALIDACAO-OBJ-01-P2A.md). P2-B foi implementado depois, com [tabela e histórico vivo](P2B-DOCUMENTO-COMPOSTO-OBJ-01.md) e [validação própria](../validacao/VALIDACAO-OBJ-01-P2B.md). P2-C acrescentou [trabalho recuperável e reordenação textual](P2C-DOCUMENTO-COMPOSTO-OBJ-01.md), com [validação própria](../validacao/VALIDACAO-OBJ-01-P2C.md); a integração canônica de publicação de P14 e o aceite humano ainda não estão concluídos. O usuário dispensou a confirmação ritual por corte, mantendo os limites de preservação.

## Resultado observável

O autor abre uma fixture, vê parágrafos, importa um PNG, coloca a figura entre grupos textuais ou ao lado de um parágrafo, acrescenta uma tabela simples e edita suas células. Pode mover/remover ocorrências, desfazer/refazer, salvar, fechar e reabrir o mesmo documento experimental sem perder conteúdo, bytes, identidades ou apresentação.

Uma janela experimental dedicada pode demonstrar esse percurso quando o corte de interface for autorizado. Não precisa substituir a Escrita principal, reunir todos os workspaces, oferecer todas as opções de layout ou inaugurar o `.inde` único. Uma representação estática, enum de tipos ou teste só de grafo não satisfaz essa entrega.

## Fixture única e fontes controladas

- Projeto schema 37 **novo e sintético**, com três parágrafos incluindo Unicode; uma marcação semântica/âncora declarada, formatação e uma resma de mais de um parágrafo para os casos de recusa/conservação.
- PNG sintético local conhecido e licenciado para os testes, com bytes/hash/dimensões fixados. Não usar ilustração autoral real ou dependência remota.
- Tabela de 2 × 3 com texto explícito, incluindo cabeçalhos e pelo menos uma célula Unicode. IDs de linhas, colunas e células são verificáveis.
- SQLite auxiliar exclusivo, novo, separado da pasta `.inde` de origem; todos os dados do experimento ficam nessa área temporária/dedicada. Recursos continuam disponíveis depois de remover o arquivo PNG usado na importação.
- Conteúdo privado da resma e perfil de visibilidade permanecem na autoria; não são exportados implicitamente.

A fixture schema 37 é origem congelada. O experimento não instala migrador, altera obras, muda `main`, sobrescreve demos ou materializa o composto no Documento antigo. Não faz publicação real.

## Cortes propostos e condições de parada

| Corte | Entrega concreta | Dependências mínimas | Parada |
| --- | --- | --- | --- |
| P2-A — Texto e figura | Parágrafos + PNG no experimento, inserção entre grupos e lado a lado, recurso armazenado, save/reabertura | Adaptador do agregado, importação PNG mínima, persistência auxiliar e visualização experimental estritamente necessárias | Demonstração verificável de P01–P05 e partes de P09; sem tabelas/serviço universal nem expansão genérica de recovery |
| P2-B — Tabela real | Inserir tabela 2 × 3, editar células/navegar por teclado, mover e reabrir | Payload de tabela e operações focadas; mesmo documento/armazenamento | P06–P08, com regressão do percurso da figura |
| P2-C — Documento composto completo | Unir o percurso, undo/redo, falhas e reinício, contexto mínimo dos instrumentos | Reaproveitar validações/planos/histórico pertinentes; completar o registro recuperável do agregado | P01–P14 comprovados e demonstração humana; registrar limitações, não começar Fase 3 automaticamente |

Cada corte registra arquivos/áreas, teste observável, riscos, limites e condição de parada. A orientação atual do usuário autoriza continuidade pelo planejamento discutido sem confirmação ritual por corte. P2-A deve produzir uma prova autoral visível; um novo framework de infraestrutura sozinho não encerra esse corte.

## Casos de aceitação P01–P14

Os casos abaixo definem o piloto completo. P01–P05 têm prova técnica no perfil P2-A; P06–P08 têm prova de tabela e histórico vivo no perfil P2-B. P09 inclui agora texto, PNG, células, ordens, perfis e seleção por ID em processo novo. P11/P12 possuem provas parciais dos perfis; P10/P13 têm implementação e prova técnica em P2-C; P14 tem contexto e recusa na fronteira do piloto, com integração ao publicador canônico ainda pendente. Os relatórios identificam os limites, incluindo o seletor de PNG não confirmado pela automação visual. Aprovação técnica e aceite humano continuam distintos.

| ID | Ação / condição | Resultado exigido | Evidência futura |
| --- | --- | --- | --- |
| P01 | Abrir fixture e capturar texto/camadas | Bindings explícitos, fonte original intacta, IDs e folhas privadas conservados; nenhuma mídia/tabela inventada | Teste de snapshot/origem e demonstração |
| P02 | Importar PNG válido e inserir figura | Bytes, hash real, MIME e dimensões conferidos; IDs de recurso/objeto/ocorrência distintos; imagem visível, legenda/alt-text declarados | Teste do importador + visualização |
| P03 | Colocar figura entre dois grupos textuais | Sequência composta representa o uso; ordem dos parágrafos e offsets proprietários não recebem marcadores aproximados | Teste do agregado e leitura |
| P04 | Mostrar parágrafo e figura em Row | Conteúdo/IDs iguais; frações/gap válidos no perfil; apresentação lado a lado em viewport controlada | Teste de perfil e captura visual revisada |
| P05 | Mover uma ocorrência de figura | Só o uso escolhido muda; outros usos e bytes originais permanecem; operação autoral/visual explicitamente distinta | Teste com dois usos + demonstração |
| P06 | Criar tabela 2 × 3 e editar célula | Linhas/colunas/células são conteúdo estruturado com IDs; edição atinge apenas alvo/escopo escolhido | Teste de payload e interface |
| P07 | Navegar/selecionar tabela por teclado | Tab/Shift+Tab e seleção por IDs não alteram células por índice de vista ou acionam movimento geométrico ambíguo | Percurso automatizado/manual identificado |
| P08 | Mover/remover e desfazer tabela/figura | Payloads, IDs, recursos, seleção e apresentação retornam coerentemente; nenhuma exclusão global de recurso por remover um uso | Teste de agregado + undo/redo |
| P09 | Salvar, fechar e reabrir | Fonte de trabalho, sequência, imagem, células, seleção definida pelo protocolo e perfil reabrem completos; independe do PNG externo | Teste de reabertura por processo novo + demonstração |
| P10 | Reordenar grupo textual completo | Sequência composta e projeção textual proprietária mudam juntas, preservando todas as camadas; revisão esperada obrigatória | Teste diferencial com motor existente |
| P11 | Tentar dividir resma ou deslocar intervalo incompatível | Recusa antes do commit, fonte/grafo/payloads/histórico intactos, diagnóstico identificável | Teste negativo de conservação |
| P12 | Entrada inválida ou conflito | PNG inválido/excedido, blob ausente/hash falso, versão desconhecida e revisão stale são recusados sem save parcial nem omissão de tipos | Testes de admissão, transação e readback |
| P13 | Encerrar com trabalho composto não salvo | Registro de trabalho recuperável + blobs permitem reconstruir conteúdo, seleção e apresentação; nova sessão explicita histórico reiniciado | Teste com processo novo e falha localizada |
| P14 | Conferir privacidade e contexto | Trocar Texto/Estrutura/Composição mantém o alvo por ID; folhas privadas não vazam; publicador antigo recusa tipos sem tradução | Teste de contexto/conservação e demonstração revisada |

P13 exige cobertura do agregado multimídia. O codec antigo de checkpoint textual não satisfaz o caso. O teste deve distinguir trabalho salvo, trabalho recuperável e resultado incerto; um registro incompleto não pode ser reconhecido como save completo. Não se promete cobrir todos os pontos de falha física.

Inserção/remoção de linha/coluna é contrato definido para o perfil; sua execução pode ser um complemento de P2-B após a primeira edição de célula. Não bloqueia provar a tabela 2 × 3, mas precisa permanecer registrada como pendência se não for incluída no corte autorizado. Equações têm contrato de fonte/revisão e podem seguir em P2-E próprio; não entram na demonstração mínima acima.

## Verificações e aceite

1. Testes de conteúdo/identidade/ordem/revisão e transação do agregado.
2. Regressões pertinentes da fundação e dos motores reutilizados, sem promover número de testes a prova de interface.
3. Testes reais de persistência, mídia e reinício em fixture; falhas descritas individualmente.
4. Capturas/percurso visual para figura lado a lado e tabela editável. Registrar o que foi automatizado, observado e aceito pelo usuário.
5. Relatório com casos aprovados, recusas esperadas, pendências, riscos e commit exato.

A aprovação técnica habilita revisão da entrega, não merge, publicação, migração, alteração de obra ou avanço de fase. O aceite humano continua explícito. Sem prova visível de texto/imagem/tabela, a Fase 2 não é declarada concluída.

## Dependências que não podem reaparecer sem justificativa

Não exigir previamente coordenador genérico do store Linux, limpeza automática de temporários, repositório universal com todos os módulos, migração do Comic, plugin system, renderizador matemático completo, catálogo de símbolos, spans de grade, responsividade total, arquivo único `.inde` ou publicador multimídia completo.

Se uma dessas frentes parecer necessária, registrar qual caso P01–P14 bloqueia, qual é a menor solução e a alternativa de recusa/limitação. Explicar e registrar o ajuste de escopo, respeitando as restrições de preservação. Não substituir a demonstração autoral por uma nova sequência indefinida de infraestrutura.

## Extensão posterior aprovada: P2-D

O usuário propôs e aprovou Fórmula como objeto calculável no documento e nas células, com variáveis, referências e alterações explícitas de estado. O [contrato de Fórmula](FORMULAS-OBJ-01.md) e a [validação P2-D](../validacao/VALIDACAO-OBJ-01-P2D.md) registram esse acréscimo sem alterar o que os cortes P2-A/B/C provaram. Fórmula e Equação têm contratos distintos. Integração de publicação de P14 e aceite humano permanecem separados.

## Refinamento posterior aprovado: P2-D2

O [contrato de texto misto e comando consumível](FORMULAS-TEXTO-OBJ-01.md) e a [validação P2-D2](../validacao/VALIDACAO-OBJ-01-P2D2.md) registram o pedido posterior do usuário: atual na fonte, frase editável, resultados textuais condicionais e alterar solitário retirado após agir. As provas anteriores continuam vinculadas aos próprios perfis; aceite humano e publicação canônica permanecem separados.

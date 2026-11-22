# OBJ-01 — Reconciliação, situação real e fechamento contratual da Fase 1

Data: 2026-11-21. Base verificada: `09eb932efb880736e72c52ca31ed7715f87e14ec`, branch `feature/inde-obj-01-foundation`. Este corte é **documental**: não acrescenta funcionalidades, não inicia a Fase 2 e não autoriza integração GTK, migração ou escrita em obras reais.

## Origem da direção e autorização deste corte

A [conversa de referência — Análise do INDE e LaTeX](https://chatgpt.com/share/6b018d1b-bab4-83e9-9099-6429b82d4c2d) registra o objetivo autoral: documentos por objetos estruturados, arranjos, integração das demos, especialidades sobre os mesmos dados e, posteriormente, Projeto autossuficiente em arquivo `.inde`. A sequência de fases foi proposta naquela conversa; as instruções expressas do usuário delimitaram auditoria, fortalecimento da fundação e preservação da base antes de avançar.

Nesta conversa de execução, o usuário interrompeu o trabalho para reconciliar o percurso com essa direção. O plano conciliado foi apresentado, e a execução foi autorizada expressamente. O primeiro corte anunciado e executado é: **matriz de situação, contratos mínimos e definição do experimento de texto/imagem/tabela**. Aprovação de código em CI, conclusão documental e aceite humano são registros diferentes.

## Escala de situação

- **I — implementado e testado:** existe código específico e evidência identificada, respeitando seu escopo.
- **C — contrato definido:** a regra está fixada neste corte para revisão; não significa implementação.
- **P — necessário ao piloto:** trabalho executável ainda pendente na Fase 2 proposta.
- **A — adiado explicitamente:** não é pré-requisito do primeiro documento composto.

Uma linha pode registrar situação atual I parcial e dependência P: o componente existente é reutilizável, mas não entrega sozinho o recurso final.

## Matriz de situação e reaproveitamento

| ID | Capacidade | Situação verificada | Reaproveitamento / falta concreta | Fonte verificável |
| --- | --- | --- | --- | --- |
| M01 | Auditoria das fontes e demos | I, com limites documentados | Conservar inventário e casos de regressão; não presumir integração das demos | [inventário](INVENTARIO-OBJ-01.md), [fontes conferidas](../validacao/evidencias-obj01-armazenamento/fontes.json) |
| M02 | Objeto, ocorrência, layout e recurso lógico | I, grafo sem payload autoral dos tipos novos | Reutilizar identidades/validador; acrescentar conteúdos pelo contrato C01 | [model.hpp](../../cpp/object_model.hpp), [model.cpp](../../cpp/object_model.cpp) |
| M03 | Stack/Row/Grid/Overlay/Freeform e árvore | I estrutural; renderização não implementada | Reutilizar invariantes; larguras proporcionais do piloto ficam em perfil de apresentação, sem mudar `Frame` | [contrato atual](INDE-OBJ-01.md) |
| M04 | Reordenar, transferir, frame, pintura e arranjo | I no grafo | Preservar comandos; não chamar isso de edição completa de conteúdo | [API](../../cpp/object_commands.hpp), [testes](../../cpp/object_commands_tests.cpp) |
| M05 | Inserir/criar/remover ocorrência ou editar payload | C/P | Não estão no `Command` atual; definir operações e implementá-las apenas no agregado do piloto | [contrato mínimo](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md) |
| M06 | Parágrafos e camadas existentes | I por captura e motor proprietário | Reutilizar bindings, grupos de leitura e recusas; não duplicar o texto em novo objeto genérico | [ponte documental](ADAPTADOR-DOCUMENTO-OBJ-01.md) |
| M07 | Composição textual existente e seus modos | I por captura, plano e sessão limitados | Conservar live/pinned/forked/reference-only e origens; não equiparar a documento multimídia | [ponte da Composição](ADAPTADOR-COMPOSICAO-OBJ-01.md) |
| M08 | Imagem como conteúdo documental | Tipo/ligação a recurso I; bytes/importação/exibição P | `Resource.sha256` atual é declarado; não prova bytes de mídia. Implementar PNG mínimo e uso por ocorrência | [Resource/Object](../../cpp/object_model.hpp), C01 |
| M09 | Tabela com células e identidades | Tipo Table I; conteúdo/edição P | Criar linhas/colunas/células reais, não tabulação no corpo textual | C01, [piloto](PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md) |
| M10 | Equação editável | Tipo Equation I; conteúdo C; renderizador A para piloto inicial | Preservar fonte matemática e modo no contrato; não alegar renderização existente | C01 |
| M11 | Símbolos | Texto Unicode existente; catálogo próprio A | Inserção textual simples pode acompanhar o piloto; catálogo/favoritos não o bloqueiam | C01 |
| M12 | Relações e referências universais | Arestas estruturais I; contrato geral C | Reusar identidades e referências existentes; links de recurso/fonte primeiro, sem ontologia narrativa inferida | C01 |
| M13 | Revisões, planos e intenção explícita | I, restritos às fontes atuais | Reutilizar revisão esperada e separação visual/autoral; novos payloads exigem adaptador de agregado | [planos](PLANO-ALTERACOES-OBJ-01.md), [API](../../cpp/object_change_plan.hpp) |
| M14 | Sessão e undo/redo | I para Documento/Composição atuais | Reutilizar garantias; sessões tipadas atuais não editam tabelas/imagens ou árvores compostas novas | [sessões](SESSOES-OBJ-01.md), [API](../../cpp/object_change_session.hpp) |
| M15 | Save/readback/CAS | I em protocolo e fixtures | Reaproveitar precondições e reconhecimento; não há escritor multimídia instalado | [protocolo](SALVAMENTO-RECOVERY-OBJ-01.md) |
| M16 | Checkpoint e armazenamento Linux | I em laboratório | Preservar como experimento/testes de falha; não são banco autoral do piloto nem pré-requisito de novo coordenador | [store](ARMAZENAMENTO-RECOVERY-OBJ-01.md) |
| M17 | Documento composto salvo e reaberto | P | É a entrega central da Fase 2; não foi demonstrada pelos 61 testes históricos | [aceitação do piloto](PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md) |
| M18 | Texto/Estrutura/Composição e contexto compartilhado | C/P | Definir seleção por IDs; demonstrar só os instrumentos mínimos do piloto antes de montar todos os workspaces | C01 |
| M19 | Serviço de recursos usado por Escrita e Comic | P na Fase 3 | O registro mínimo de PNG do piloto não equivale à integração dos dois módulos | Roteiro abaixo |
| M20 | Consolidação funcional das demos | P na Fase 4 | Usar as demos como exemplos e regressões dos mesmos serviços; não como programas apenas reunidos visualmente | [inventário das demos](INVENTARIO-OBJ-01.md) |
| M21 | `.inde` único com recursos incorporados | A para Fase 5 | Não converter projetos nem alterar formato físico antes da prova lógica e de recursos | Roteiro abaixo |
| M22 | Publicação de mídia/tabela/equação e distribuição | P/A conforme fase | Não é implementada pelo grafo; exportador antigo não pode omitir objetos silenciosamente | C01 e Fase 6 |

## Riscos, invariantes e compatibilidade

| Risco | Invariante / recusa exigida | Compatibilidade preservada | Prova futura |
| --- | --- | --- | --- |
| R01 — Duas ordens autorais divergirem | Uma sequência composta; projeção de parágrafos igual ao proprietário; alteração autoral transacional ou recusa integral | Corpo, grupos e camadas continuam sob o motor textual | P03, P10, P11 |
| R02 — Mover um uso ou editar conteúdo atingir outros usos por engano | Posição/legenda são da ocorrência; edição compartilhada exige escopo explícito; remover uso conserva recursos/histórico | Identidades do núcleo e semântica de referências não são confundidas com posição | P05, P08 |
| R03 — Perder alternativas, privacidade ou coordenadas textuais | Captura completa, bindings explícitos, sem marcadores de mídia inseridos no corpo por aproximação | IDs, folhas, âncoras, semântica, visibilidade e offsets do proprietário | P01, P10, P11, P14 |
| R04 — Hash declarado ou caminho externo ser tratado como mídia incorporada íntegra | Conferir bytes/decoder/hash antes de referenciar; reabrir pelos blobs verificados; diagnóstico em vez de omissão | `Resource` atual segue metadado lógico; importar bytes é capacidade nova | P02, P09, P12 |
| R05 — Reconhecer save/recovery parcial como documento completo | Unidade cobre texto, sequência, payloads, recursos e apresentação; codec/versionamento adequados; histórico retém referências | Protocolos antigos continuam válidos para seus tipos; não recebem cobertura multimídia fictícia | P08, P09, P12, P13 |
| R06 — Experimento alterar schema, origem ou obra | Banco auxiliar novo e isolado; origem congelada; revisão/hash esperados e recusa de conflito | Schema 37, fontes arquivadas, `main` e obras reais preservados | P01, P09, P12 e comparação da origem |
| R07 — Publicar incompleto ou vazar folhas privadas | Visibilidade/autoria/publicação distintas; exportador sem tradução recusa o composto | Publicação legada continua com seu escopo; nenhum fallback silencioso | P14 |
| R08 — Percentual, pixel e frame abstrato se confundirem | Perfil Row separado, medidas válidas e conversão definida; sem normalização silenciosa | `Frame` e regras geométricas existentes não são reinterpretados | P04, P12 |
| R09 — Entrada excedida, inválida ou versão desconhecida corromper candidato | Limites individuais/agregados e UTF-8 válidos antes do commit; recusa sem truncamento | Quotas do proprietário/ponte continuam explícitas, sem promessa de capacidade maior | P11, P12 |
| R10 — CI e infraestrutura serem promovidas a aceite de produto | Evidências ligadas ao escopo; todos os casos novos pendentes até prova; avanço depende de revisão/autorização | Histórico técnico conservado sem apagar limites ou decisões do usuário | P01–P14 e registro de aceite humano |

Esses casos não foram executados neste corte documental. São obrigações do piloto, além das regressões que já existem. Uma limitação descoberta deve indicar a linha de risco e o caso que bloqueia antes de ampliar o escopo.

## Decisões de prioridade

1. **Conservar** commits, contratos, validadores, comandos, pontes e testes. Não apagar experimentos úteis por causa da reconciliação.
2. **Congelar a expansão de infraestrutura por iniciativa própria.** Coordenador de store/escritor/ack, manutenção de temporários e outras melhorias do laboratório só entram se um caso de aceitação do piloto demonstrar bloqueio, com novo escopo explícito.
3. **Não promover componentes pelo volume de testes.** O resultado histórico de 61/61 testes nativos e 21 jobs comprova seus cenários, não imagem/tabela editáveis, interface ou serviço universal.
4. **Manter bancos/formatos separados por responsabilidade.** Checkpoint é recuperação de trabalho; o novo conteúdo autoral exige contrato próprio. Store de recovery não vira documento por conveniência.
5. **Voltar ao percurso autoral:** texto + imagem; tabela com células; documento completo salvo/reaberto e recuperado em fixture.

## Roteiro reconciliado e marcos

| Fase | Entrega | Gate finito |
| --- | --- | --- |
| 0 — Auditoria | Inventário, hashes, autoridades e demos | Usar evidência já entregue; nova varredura somente por lacuna concreta |
| 1 — Contrato universal | Identidades, conteúdos, relações, revisões, operações e compatibilidade | C01–C06 abaixo definidos; matriz de faltas ligada ao piloto; revisão/aceite separados |
| 2 — Composição documental | Experimento com texto, PNG e tabela, arranjo, save/reabertura e recuperação | Casos P01–P14 do piloto, mais regressões pertinentes |
| 3 — Recursos compartilhados | Escrita e Comic usam o mesmo serviço lógico | Reutilização/bytes/proveniência/acesso sob demanda demonstrados nos dois módulos |
| 4 — Demos consolidadas | Percursos sobre os serviços comuns | Integração funcional e regressões, não simples abertura pela mesma janela |
| 5 — `.inde` único | Conversor e transporte autossuficiente | Cópias verificadas, recursos incorporados, reabertura e comparação; original preservado |
| 6 — Auditoria/distribuição | Diagnóstico, exportação e publicação seletiva integrados | Seleção pública, identidades e dependências verificadas; autoria privada protegida |

Testes, conservação e auditabilidade acompanham todas as fases; a Fase 6 os consolida como capacidades de produto. Equações, numeração, catálogo de símbolos e melhorias de apresentação entram em cortes próprios sem bloquear a prova básica de texto/imagem/tabela.

## Fechamento contratual da Fase 1

| Decisão | Resultado deste corte | Implementação seguinte |
| --- | --- | --- |
| C01 — Conteúdos e identidades | [Contrato mínimo](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md), seções 1–3 | Payloads, recursos mínimos e vínculos do piloto |
| C02 — Autoridades e ordenação | Seções 4–5: sequência autoral única e projeção textual validada | Agregado/adaptador transacional composto |
| C03 — Operações e estados de edição | Seções 6–7: pré-condições, efeitos e seleção | Comandos mínimos e interface experimental |
| C04 — Persistência e compatibilidade | Seção 8: SQLite auxiliar de fixture, formato próprio, schema 37 preservado | Codec/store experimental do documento composto |
| C05 — Histórico, save e recuperação | Seções 8–9: estados separados e cobertura completa do conteúdo | Reaproveitar mecanismos mediante adaptador, sem prometer cobertura dos codecs antigos |
| C06 — Aceitação e limites | [Piloto P01–P14](PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md) | Prova executável antes de qualquer promoção |

**Estado:** fechamento contratual preparado para revisão humana. Nenhum gate funcional do piloto foi aprovado neste corte; os contratos novos ainda não têm implementação. A Fase 2 não foi iniciada nem autorizada por este registro.

Não são condições de encerramento contratual: novo coordenador durável genérico, sistema de plugins, instâncias reutilizáveis de contêiner, spans de grade, layout responsivo completo, clipboard universal, renderização matemática completa ou unificação física do `.inde`.

## Controle de escopo daqui em diante

Cada corte deve identificar entrega observável, arquivos/áreas, testes, limites, condição de parada e relação com um caso do piloto. Uma descoberta deve ser classificada como bloqueio demonstrado desse caso ou pendência posterior. A pendência não vira automaticamente o próximo corte.

O corte documental original terminou na documentação, verificação de consistência e envio à branch. Posteriormente, o usuário esclareceu que a confirmação precavida servia à discussão do planejamento e autorizou a continuidade sem repetir esse procedimento por corte. A execução segue o planejamento discutido, registrando escopo, evidência e limites. Preservação de obras, main, schemas reais e demais restrições continua vigente; aceite humano e aprovação técnica permanecem distintos.

Ver [validação desta reconciliação](../validacao/VALIDACAO-OBJ-01-RECONCILIACAO.md). Relatórios históricos permanecem evidência dos respectivos cortes; esta reconciliação passa a governar a prioridade futura.

## Atualização após autorização de P2-A

O usuário autorizou posteriormente o corte específico de texto/PNG e interface local sem GTK. Sua implementação e limites estão em [P2A-DOCUMENTO-COMPOSTO-OBJ-01](P2A-DOCUMENTO-COMPOSTO-OBJ-01.md), com [validação própria](../validacao/VALIDACAO-OBJ-01-P2A.md). A matriz acima continua sendo o retrato da base identificada no início deste documento; o novo relatório registra a evolução sem apagar as faltas históricas. A autorização não se estende a P2-B ou à Fase 2 inteira.

## Atualização P2-B

P2-B acrescenta tabela estruturada, seleção por célula, edição e histórico vivo, com perfil próprio e cópia somente de leitura de P2-A. Ver [implementação](P2B-DOCUMENTO-COMPOSTO-OBJ-01.md) e [validação](../validacao/VALIDACAO-OBJ-01-P2B.md). P2-C continua como marco técnico seguinte; não há confirmação ritual por corte.

## Atualização P2-C

[P2-C](P2C-DOCUMENTO-COMPOSTO-OBJ-01.md) registra trabalho composto ainda não salvo, move grupos textuais pelo motor proprietário e acrescenta instrumentos mínimos. A [validação](../validacao/VALIDACAO-OBJ-01-P2C.md) distingue recuperação técnica, demonstração e aceite humano. A recusa de publicação é local ao piloto; a integração com o publicador principal continua pendente. A Fase 2 inteira e a Fase 3 não são declaradas concluídas/iniciadas por essa atualização.

## Extensão aprovada P2-D — Fórmula

Após a proposta do usuário e sua aprovação expressa, o [objeto Fórmula](FORMULAS-OBJ-01.md) foi acrescentado ao piloto: cálculo comum no documento/células, variáveis/referências, atribuições explícitas, histórico e recuperação. [Validação própria](../validacao/VALIDACAO-OBJ-01-P2D.md). Não transforma a Fase 3 ou a publicação canônica em trabalho iniciado/concluído.

## Refinamento aprovado P2-D2

[Texto misto e comando consumível](FORMULAS-TEXTO-OBJ-01.md), com [validação própria](../validacao/VALIDACAO-OBJ-01-P2D2.md): valor atual real na fonte, palavras editáveis ao redor de resultados, lógica textual e consumo atômico do alterar solitário. Perfil novo preserva o numérico anterior. Origem somente leitura e publicação/GTK permanecem fora da integração entregue.

## Atualização — cartões preenchíveis, 2026-11-22

Após escolha e autorização do usuário, o [editor por cartões](CARTOES-FORMULAS-OBJ-01.md) foi integrado ao piloto P2-D2, com fonte única, prévia no motor e regra/frase atômicas. [Validação própria](../validacao/VALIDACAO-OBJ-01-CARTOES.md). O vínculo com entidades reais, a integração GTK e a publicação canônica permanecem pendentes.

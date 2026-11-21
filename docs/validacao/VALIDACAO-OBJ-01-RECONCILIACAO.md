# OBJ-01 — Validação da reconciliação e do fechamento contratual

Data: 2026-11-21. Base inspecionada: `09eb932efb880736e72c52ca31ed7715f87e14ec`. Branch `feature/inde-obj-01-foundation`. Escopo autorizado e executado: documentação de reconciliação, matriz de situação, contratos mínimos e critérios do primeiro experimento autoral. **Nenhuma funcionalidade nova foi implementada neste corte.**

## Fontes usadas e conferência

A direção foi reconciliada com a [conversa compartilhada](https://chatgpt.com/share/6b018d1b-bab4-83e9-9099-6429b82d4c2d), lida na discussão anterior deste mesmo chat, e com o plano conciliado expressamente autorizado depois dela. Não se atribui ao usuário cada proposta técnica antiga do assistente como decisão já homologada. As novas escolhas contratuais ficam identificadas para revisão antes de execução do piloto.

Foram conferidos na branch atual os headers/modelos/contratos de objetos, comandos, pontes, planos, sessões e store, além dos relatórios já registrados. A verificação confirmou:

- `Object` possui IDs/tipo e vínculos opcionais; não possui células, fonte de equação ou bytes de imagem.
- `Resource` guarda metadados e SHA declarado; sua validação sintática não prova integridade de mídia.
- `Command` oferece cinco famílias: reorder, transfer, frame, paint e layout; não oferece create/insert/edit-payload/remove.
- Pontes capturam fontes textuais reais e seus bindings, sem transformá-las em documento multimídia.
- Sessões e codecs tipados atuais não representam o novo agregado com mídia e tabela.
- O store durável é laboratório Linux, sem integração automática com o aplicativo ou uso em obras reais.

A evidência histórica mais recente registra **61/61 testes nativos e 21/21 jobs** no corte de armazenamento. Ela foi usada com seu escopo original, sem apresentá-la como teste de imagem, tabela, renderização, fonte matemática ou publicação que não existem. Ver [resumo histórico](evidencias-obj01-armazenamento/resultados.json).

## Entregas deste corte

| Entrega | Resultado |
| --- | --- |
| [Reconciliação e matriz M01–M22](../arquitetura/RECONCILIACAO-OBJ-01.md) | Implementação, contrato, necessidade do piloto e adiamento explicitamente separados; dez riscos R01–R10 ligados a invariantes, compatibilidade e provas futuras; infraestrutura preservada sem dirigir automaticamente a fila |
| [Contrato mínimo 1.0, C01–C06](../arquitetura/CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md) | Conteúdos, identidades/revisões, ordenação, operações, contexto, persistência auxiliar e conservação definidos para revisão |
| [Experimento e P01–P14](../arquitetura/PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md) | Texto/PNG/tabela, cortes propostos, condição de parada e provas automatizadas/visuais descritas; todos os casos funcionais ainda pendentes |
| Contrato principal e índice | Passam a apontar para a reconciliação como prioridade futura; implementação experimental anterior permanece descrita separadamente |

O contrato fixa uma sequência autoral única e projeção textual validada pelo motor existente, sem editar duas autoridades livremente. Define recurso mínimo PNG com bytes reais e tabela de células identificadas, preserva fonte de equação e separa seleção por tipo. A persistência proposta é SQLite auxiliar novo em fixture, sem alterar o schema 37; estado salvo e trabalho recuperável são distintos. O codec de recovery antigo não é declarado cobertura multimídia.

As quotas/perfil de conteúdo são requisitos futuros do piloto, não medições da implementação atual. DDL, código, renderização e testes novos desses conteúdos não foram escritos. A autorização deste corte não foi usada para iniciar Fase 2, GTK, migração ou outro coordenador de infraestrutura.

## Validação executada e limites

Este é um corte documental. As verificações locais são consistência de IDs C/M/P/R e referências, resolução de 87 links locais, hashes dos headers e da evidência histórica consultada e `git diff --check`. Os resultados e a lista de arquivos ficam no [resumo estruturado](evidencias-obj01-reconciliacao/resultados.json). Não foram criados testes que apenas repetem o texto do contrato. Os arquivos compactados não foram reextraídos localmente neste corte; a auditoria histórica continua identificada separadamente, e a CI existente os verificou novamente.

Não foi reexecutada localmente a suíte CTest nativa de 61 programas, pois código/configuração de build não foram alterados. A CI existente executou suas matrizes mínimas e a verificação de fontes por gatilho documental; isso é regressão do código inalterado, **não aprovação funcional do contrato novo**.

| Verificação deste corte | Resultado / evidência |
| --- | --- |
| IDs e referências locais | M01–M22, C01–C06, P01–P14 e R01–R10 consistentes; 87 links resolvidos |
| Evidência histórica consultada | Nove artefatos conferidos contra seu manifesto SHA-256 |
| Preservação de implementação e fontes | Diff vazio em `cpp`, `tests`, `.github`, `fontes` e `metadata` contra `09eb932`; somente documentação mudou |
| CI do commit documental `4bbb4ef22c75448a81f4e105c3c3d250fce3450b` | **21/21 jobs aprovados**; [execução 44908064629](https://github.com/Roger-Cardoso/inde/actions/runs/44908064629), [resultado bruto](evidencias-obj01-reconciliacao/ci.json) |
| Código novo / aceite do piloto | Não há código novo; P01–P14 continuam pendentes; aceite humano do contrato separado |

O manifesto [SHA256SUMS](evidencias-obj01-reconciliacao/SHA256SUMS) usa caminhos relativos à raiz da worktree. Conferência reproduzível: `sha256sum -c docs/validacao/evidencias-obj01-reconciliacao/SHA256SUMS`. O resultado bruto da CI identifica o commit efetivamente testado; não se atribui a ele um commit documental posterior. O commit final de consolidação recebe sua própria CI pelo mesmo workflow.

Não houve edição de fontes C++, testes executáveis, workflows, arquivos compactados, bancos ou projetos autorais. A `main` local permaneceu em `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, preservando sua alteração preexistente de permissão em `Preparar-ponte.sh`. Nenhuma migração, implantação ou merge foi realizado.

## Estado de aceite e parada

C01–C06 têm decisões contratuais documentadas. Isso conclui a preparação documental anunciada e permite revisão humana do fechamento da Fase 1. O aceite do contrato permanece separado; não se declara que o piloto existe ou que P01–P14 foram aprovados.

O próximo candidato é P2-A (texto e PNG, com visualização e reabertura), a apresentar com arquivos/áreas, teste observável, limites e ponto de parada para autorização expressa. Não foi iniciado. A expansão genérica do laboratório fica congelada, salvo bloqueio demonstrado de um caso do piloto e novo escopo confirmado.

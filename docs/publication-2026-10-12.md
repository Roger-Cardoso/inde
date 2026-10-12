# Publicação da reforma do INDE em 12 de outubro de 2026

O usuário confirmou o aceite humano do candidato `contextual_keyboard`, schema
28, e autorizou a publicação do código e documentação técnica no repositório
Roger-Cardoso/inde. Conversas integrais, acervo privado e projetos autorais
permanecem locais. Este registro sucede os estados de aceite pendente dos
documentos históricos, sem reescrever seus registros de emissão.

## Identidade aprovada

- Fonte completa original: 645 arquivos, incluindo arquivos antes não rastreados.
- SHA-256 do arquivo-fonte: `a870cb05aa2d356a7b757d7835972be35e77f36be89c6d344cbbdab718c1f4f3`.
- SHA-256 do executável principal: `2e398cac23c77213b3a03e766d57dce5f78988f0a37d2277471e9440d0b6152f`.
- SHA-256 do laboratório: `5805330370537084fb1ec30d66f41a24eef2e37fa7d92341dd02243997864cfa`.
- Schema de projeto: 28. Identificador CMake histórico: 0.1.0; esse número
  isoladamente não identifica todos os cortes de desenvolvimento.

O snapshot público preserva byte a byte o código aprovado. Sete arquivos de
projetos `.inde`, incluindo bancos SQLite e seus recursos, foram excluídos da
publicação e continuam no arquivo-fonte integral local. O histórico privado que
continha esses bancos não é enviado: o commit público sucede diretamente o
último commit público observado, sem force-push. Dados sintéticos para testes
podem ser produzidos pelos geradores do código, não por copiar projetos autorais.

README, este registro, regras de exclusão e configuração de CI foram atualizados
para a distribuição do snapshot. O contrato funcional aprovado não foi alterado.
As notas técnicas anteriores continuam disponíveis e são históricas: seus
schemas, resultados e pendências correspondem ao momento de emissão.

## Evidência técnica do candidato

- 35 de 35 testes CTest; 3.334 casos de política de teclado.
- 16 cenários GTK: principal Release, Debug focal, ASan/UBSan focal e laboratório,
  em projeto vazio/Atlas e temas claro/escuro.
- Regressões selecionadas de editor e montagem, dois startups limitados e seis
  capturas inspecionadas. Inicialização não equivale a uma sessão de escrita.
- 208 comandos explícitos no principal e 37 no laboratório.

Debug e sanitizadores cobrem 11 unidades focais e a sonda, com outras unidades e
dependências Release. LSan desativado. Esses resultados não certificam toda a
aplicação instrumentada, vazamentos, teclado físico ABNT2, IME real, Wayland,
leitor de tela, HiDPI nem sessões longas. O aceite humano declarado não transforma
limites técnicos em testes executados. CI remoto é evidência separada.

Esta publicação de código não declara a reforma inteira concluída nem entrega
um novo AppImage certificado. Não há mudança de licença, visibilidade do
repositório, migração de projetos autorais ou aprovação automática de catálogos.

## Histórico da reforma

| Data | Incrementos documentados |
|---|---|
| 02–04/10 | Recuperação e modularização do editor, escala, semântica, interação e laboratório |
| 07/10 | Shell progressivo, temas, organização, continuidade, workspaces e revisão editorial |
| 08/10 | Produção, histórico/reversão de operações e composição persistente |
| 10/10 | Aliases, sistemas temporais, calendários, vínculos ao texto, seletores gráficos e exclusão assistida |
| 11/10 | Construtor narrativo, alternativas, acesso inicial, tema, realização textual e continuidade |
| 12/10 | Vocabulário autoral, teclado contextual, aceite humano e publicação do snapshot |

Essa tabela é um índice técnico, não uma transcrição de chats. Consulte os
contratos datados em `docs/` para limites, dependências e decisões de cada corte.

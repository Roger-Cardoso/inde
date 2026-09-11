# Atlas de Véspera — Projeto Integral

Fixture humana e técnica primária do INDE, gerada por
`tools/demo_project_generator.cpp`.

**Estado em 8 de setembro de 2026:** formato `.inde` v2, banco SQLite no esquema
v12, `integrity_check=ok` e zero violações de chave estrangeira.

## Conteúdo verificável

| Camada | Quantidade |
|---|---:|
| propriedades intelectuais / Obras | 2 / 4 |
| nós editoriais ativos / totais | 157 / 160 |
| tipos de entidade / entidades | 8 / 114 |
| tipos de relação / relações | 11 / 104 |
| escopos de Obra / referências editoriais | 210 / 134 |
| eixos / pontos / ocorrências | 3 / 41 / 32 |
| participações / presenças | 96 / 68 |
| grupos documentais / Documentos | 3 / 12 |
| intervalos / âncoras / referências textuais | 25 / 21 / 62 |
| modelos / itens / vínculos entre itens | 7 / 99 / 15 |
| estruturas editoriais / narrativas | 5 / 5 |
| linhas / unidades / pertencimentos | 6 / 12 / 18 |
| vínculos unidade-entidade / narrativos | 12 / 12 |
| papéis / atribuições contextuais | 5 / 3 |

Os 157 nós editoriais usados pelas projeções pertencem às estruturas ativas; os
três restantes exercitam uma alternativa editorial. As 12 unidades narrativas
incluem uma estrutura ativa e uma derivação, com múltiplas linhas, vínculos e
referências à realidade ficcional.

## Uso seguro

Para testes manuais destrutivos, copie a pasta inteira e abra a cópia. Os
benchmarks do repositório também trabalham sobre cópias temporárias. Não edite o
banco versionado manualmente: a interface e os serviços são as fronteiras do
produto.

O roteiro de validação da Etapa 11 está em `docs/stage-11-plan.md`, na raiz do
repositório.


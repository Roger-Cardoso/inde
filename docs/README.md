# Documentação do INDE

- [Guias](guias/) para instalar, abrir e diagnosticar o aplicativo.
- [Roteiros](roteiros/) para experimentar GTK, Comic, catálogo, importação e publicação.
- [Arquitetura](arquitetura/) com contratos, decisões de integração e otimização.
- [Validação](validacao/) com os resultados registrados e o escopo de cada entrega.

## Núcleo experimental de objetos

- [Objetos e equações no INDE nativo](../tools/obj01-pilot/NATIVO.md), [contrato](arquitetura/MATEMATICA-NATIVA-OBJ-01.md) e [validação](validacao/VALIDACAO-OBJ-01-MATEMATICA-NATIVA.md): entrada pelo documento real, motor compartilhado e limites de integração.

- [Linhas e colunas recuperáveis](../tools/obj01-pilot/ESTRUTURA-TABELAS.md), [contrato](arquitetura/ESTRUTURA-TABELAS-OBJ-01.md) e [validação](validacao/VALIDACAO-OBJ-01-ESTRUTURA-TABELAS.md).
- [Tabelas visuais](../tools/obj01-pilot/TABELAS.md), [contrato](arquitetura/TABELAS-VISUAIS-OBJ-01.md) e [validação](validacao/VALIDACAO-OBJ-01-TABELAS-VISUAIS.md).
- [Compor e remover objetos](../tools/obj01-pilot/COMPOSICAO.md), [contrato](arquitetura/COMPOSICAO-REMOCAO-OBJ-01.md) e [validação](validacao/VALIDACAO-OBJ-01-COMPOSICAO-EDICAO.md).
- [Hub de fórmulas — cinco caminhos](../tools/obj01-pilot/HUB.md), [contrato](arquitetura/HUB-FORMULAS-OBJ-01.md) e [validação](validacao/VALIDACAO-OBJ-01-HUB.md).

- [Texto por cartões preenchíveis](../tools/obj01-pilot/CARTOES.md), [contrato](arquitetura/CARTOES-FORMULAS-OBJ-01.md) e [validação](validacao/VALIDACAO-OBJ-01-CARTOES.md).

A prioridade é governada pela reconciliação abaixo. Após autorização expressa, **P2-A foi implementado como piloto isolado de texto/PNG**, com demonstração local e persistência própria. P2-B acrescentou tabela e histórico vivo em perfil separado. P2-C acrescenta recuperação do trabalho composto ainda não salvo, reordenação de grupos pelo motor textual e instrumentos mínimos. Após aprovação da proposta de Fórmula, P2-D acrescentou cálculo numérico comum ao documento e às células, variáveis e alterações explícitas em cadeia. P2-D2 acrescentou texto editável ao redor dos resultados, condições textuais e comandos de uso único com valor atual na fonte. A edição livre dos parágrafos de origem e a integração com o publicador principal seguem pendentes; a Fase 2 inteira não está concluída.

- [Texto misto e comando consumível — P2-D2](../tools/obj01-pilot/P2D2.md).
- [Contrato do refinamento](arquitetura/FORMULAS-TEXTO-OBJ-01.md) e [validação P2-D2](validacao/VALIDACAO-OBJ-01-P2D2.md).
- [Fórmulas no documento e nas células — P2-D](../tools/obj01-pilot/P2D.md).
- [Contrato de Fórmula](arquitetura/FORMULAS-OBJ-01.md) e [validação P2-D](validacao/VALIDACAO-OBJ-01-P2D.md).
- [Abrir e experimentar P2-C — recuperação e ordem textual](../tools/obj01-pilot/P2C.md).
- [Implementação P2-C](arquitetura/P2C-DOCUMENTO-COMPOSTO-OBJ-01.md) e [validação](validacao/VALIDACAO-OBJ-01-P2C.md).
- [Abrir e experimentar P2-B — tabela e desfazer/refazer](../tools/obj01-pilot/P2B.md).
- [Validação de P2-B](validacao/VALIDACAO-OBJ-01-P2B.md).
- [Abrir e experimentar P2-A](../tools/obj01-pilot/README.md).
- [Implementação e limites de P2-A](arquitetura/P2A-DOCUMENTO-COMPOSTO-OBJ-01.md).
- [Validação e demonstração de P2-A](validacao/VALIDACAO-OBJ-01-P2A.md).

- [Reconciliação, matriz de situação e fechamento contratual da Fase 1](arquitetura/RECONCILIACAO-OBJ-01.md).
- [Contrato mínimo de conteúdo e autoria composta](arquitetura/CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md).
- [Primeiro experimento autoral e aceitação P01–P14](arquitetura/PILOTO-DOCUMENTO-COMPOSTO-OBJ-01.md).
- [Validação da reconciliação](validacao/VALIDACAO-OBJ-01-RECONCILIACAO.md).

- [Fundação OBJ-01 e comandos](arquitetura/INDE-OBJ-01.md).
- [Auditoria aprofundada das bases](arquitetura/INVENTARIO-OBJ-01.md).
- [Ponte somente de leitura do Documento](arquitetura/ADAPTADOR-DOCUMENTO-OBJ-01.md).
- [Validação da ponte de leitura](validacao/VALIDACAO-OBJ-01-LEITURA.md).

- [Ponte somente de leitura da Composição persistida](arquitetura/ADAPTADOR-COMPOSICAO-OBJ-01.md).
- [Validação da ponte da Composição](validacao/VALIDACAO-OBJ-01-COMPOSICAO.md).

- [Planos imutáveis de alterações e intenção explícita](arquitetura/PLANO-ALTERACOES-OBJ-01.md).
- [Validação dos planos](validacao/VALIDACAO-OBJ-01-PLANOS.md).

- [Sessões de aplicação e histórico em memória](arquitetura/SESSOES-OBJ-01.md).
- [Validação das sessões](validacao/VALIDACAO-OBJ-01-SESSOES.md).

- [Protocolo de salvamento e recovery em laboratório](arquitetura/SALVAMENTO-RECOVERY-OBJ-01.md).
- [Validação de save/recovery](validacao/VALIDACAO-OBJ-01-SALVAMENTO.md).

- [Armazenamento durável de checkpoints em laboratório](arquitetura/ARMAZENAMENTO-RECOVERY-OBJ-01.md).
- [Validação do armazenamento](validacao/VALIDACAO-OBJ-01-ARMAZENAMENTO.md).

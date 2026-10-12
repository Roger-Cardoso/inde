# Incremento semântico 2 — perspectiva situada e relações entre Documentos

Ordem direta de 2026-10-03: remover o botão redundante Busca semântica e,
depois, implementar profundamente o próximo incremento semântico. O restante
do candidato de consolidação recebeu aceite humano geral, sem certificação
individual inferida de IBus ou sessão humana de 45–60 minutos.

Base independente: candidato editor-consolidacao-2026-10-03, HEAD f155bd8,
branch codex/editor-semantic-2-2026-10-03. Não editar originais ou artefatos
congelados; nenhum commit, push ou release. Revisões anteriores preservadas.

## Contrato do corte

- Editor tem Camadas semânticas, Lentes semânticas e Marcar seleção. A consulta
  do Projeto permanece em Camadas, sem outro atalho redundante.
- Nova declaração autoral versionada de perspectiva sobre intervalo:
  pessoa narrativa, focalização, focalizador canônico opcional e nota. Não
  converter ou substituir a perspectiva textual livre do Documento. Não
  afirmar perspectiva global nem verdade ficcional a partir dessa anotação.
- Relações explícitas: selecionar alvo local ou persistido de outro Documento
  do mesmo Projeto, revisar endpoints antes de confirmar, navegar entradas e
  saídas, desfazer, remover vínculo e explicar proteção de alvos usados.
- Consultas paginadas, parametrizadas e escopo de Projeto; UI sem SQL. Texto
  local não salvo não se torna resultado persistido por busca automática.
- Identidades, âncoras, campos desconhecidos, formatação, undo/redo,
  checkpoints e comparação otimista preservados. Nenhum match, acepção,
  binding lexical, inferência de personagem ou eixo temporal implícito.
- Manter schema principal 16, journal 1 e payload 2; tipos anteriores v1
  permanecem imutáveis. Nova capacidade não encerra toda a Etapa 4.

## Verificação e portão

Testes de domínio/aplicação/SQLite, regressão GTK em Projeto vazio e Atlas,
Unicode/IME e sessão curta com metadados; sanitização focal proporcional.
Aceite deste novo incremento pendente. Evidência de sessão longa anterior
continua histórica, não prova este novo executável. Configuração recomendada:
GPT-6.1 Sol/high, custo relativo alto, mesma tarefa.

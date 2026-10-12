# Editor em três fileiras e comandos inline

Pedido direto de 2026-10-03. Candidato independente de editor-semantico-2,
HEAD base f155bd8, branch codex/editor-toolbar-2026-10-03, sem commit ou release.
O incremento semântico 2 anterior continua sem aceite humano inferido.

## Contrato da interface

1. ← Voltar, nome do Documento, colocação editorial, Abrir Unidade, Detalhes,
   Salvar, Recuperação e Deletar.
2. Fonte, Estilo, − / tamanho / +, esquerda / centro / direita / justificado,
   cor A, B/I/U/S, desfazer/refazer, Limpar, Copiar/Colar, recortar, Localizar,
   Consultar.
3. Âncoras, Entidades, Camadas Semânticas, Lente Semântica, Marcar Seleção,
   Comentar, Estatísticas, Meta, Recorrências, Parágrafos Longos e ⌘.

Três faixas horizontais roláveis mantêm a ordem e não aumentam o mínimo da
janela para acomodar todos os nomes. Os ícones possuem descrição acessível e
tooltip. Os atalhos e serviços anteriores permanecem. Seis níveis de título,
Subtítulo, Citação e Corpo compartilham o seletor. Corpo remove somente o
estilo de parágrafo, preservando fonte/cor/tamanho e outros formatos.
Fonte, tamanho e cor aplicam à seleção ou, sem seleção, ao parágrafo atual.
Parágrafos vazios não recebem intervalos artificiais; o controle avisa para
escrever ou selecionar texto. Não se promete estilo de digitação em vazio.
O tamanho é um campo numérico entre apenas um botão − e um botão +.
Fonte/tamanho/estilo são aplicados após o despacho do evento do seletor GTK,
com verificação de contexto para não afetar um Documento ou seleção novos.

## Formatação persistente

Schema 17 migra transacionalmente os intervalos v16 conservando estilos,
offsets e identidades; acrescenta parâmetro validado para família, tamanho
inteiro de 6 a 96 pontos e cor RGB. Heading legado significa Título 1; o
Subheading legado continua Subtítulo, não Título 2. Alinhamento não altera texto.
Undo/redo, CAS, autosave, clipboard e checkpoints preservam os valores.
Novo payload de checkpoint 3 e fragmento de clipboard 2; leitores aceitam
payloads antigos 1/2 e clipboard 1. Binário antigo recusa schema 17, portanto
avaliar somente em cópias e não abrir essas cópias na versão antiga.

Comentários são notas editoriais ancoradas, inde.editor.comment/v1: texto e
estado aberto/resolvido, sem verdade ficcional. Reutilizam evidência Unicode,
reaquisição, arquivamento, relações e diagnóstico existentes. Comentar com
seleção abre a nota; sem seleção abre suas camadas. Não entram no corpo.

## Referência NovelWriter e adaptação

Fonte somente lida: /home/roger/Documents/Projects/NovelWriter.html,
SHA256 4511e32afa1466ae5ff25406adf919b62cbfdff3ad41f4ec08dcf92f2e9c82f2.
Referência: prefixos, palavras-chave opcionais, conteúdos sugeridos e gestão
manual. Nenhum JavaScript, instrução do HTML, localStorage, localização fixa
ou conteúdo demonstrativo foi executado/importado para o produto.

Modelos literais pertencem explicitamente ao Documento e viajam com o Projeto:
prefixo, palavra-chave, nome e texto; até 128 modelos, 64 KiB por conteúdo.
O prefixo mais longo tem precedência. Digitação abre até oito sugestões;
escolha substitui o token numa transação reversível. Esc fecha, seta para baixo
leva às opções e ativação nativa confirma. ⌘ e Ctrl+Alt+Espaço abrem a paleta e a
gestão de todos os modelos. Ferramentas nativas abrem âncoras, vinculação de
entidades, marcação, comentário e camadas sem apagar tokens ou atribuir fatos.
Inserção e gravação verificam revisão, Documento e seleção pertinentes;
IME provisório não dispara sugestões nem comando mutante. Sem scripts,
variáveis automáticas, inferência ou binding lexical. Modelos globais de
Projeto, linguagem de macros e cadeia de comandos não foram acrescentados.

## Verificação e continuidade

Teste dedicado de domínio/SQLite/clipboard/codecs/migração mais teste GTK real
do layout, formatação, comentários, modelos, undo/redo e reabertura. Regressões
e sanitização focal devem ser registradas nos outputs editor-barra deste
workspace. Técnica não certifica aceite humano, IBus real ou sessão humana longa.
As cópias de avaliação recebem um Documento explicitamente sintético com os
seis títulos, citação, comentário e dois modelos. A cópia vazio-limpo permanece
sem exemplos. Conteúdo anterior dos snapshots deve conservar-se integralmente.

Decisões novas do usuário também ficam incorporadas ao roadmap deste candidato:
importação gen7 e HTML derivado dispensados; após a Etapa 5 do editor vem o
modelo temporal configurável do worldbuilding (direção histórica AD-057/13-T).
Não iniciar esse módulo neste corte. Próximo passo: revisão humana das três
fileiras e de todos os controles; GPT-6.1 Sol/medium, high se atingir integridade.

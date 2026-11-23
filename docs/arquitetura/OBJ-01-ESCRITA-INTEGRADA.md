# OBJ-01 — especialidades do mesmo editor de Escrita

Direção de 23/11/2026, em `feature/inde-obj-01-foundation`. Este documento sucede, quanto à interface e à ligação com a Escrita, o corte [GTK nativo](OBJ-01-GTK-NATIVO.md). A validação daquele corte permanece histórica.

## Autoridade e organização

O Documento da Escrita é a autoridade para título, corpo, identidades de parágrafo, formatação, âncoras, referências, alternativas, folhas e visibilidade. O editor original, seu `TextView`, suas transações, salvamento e recuperação continuam responsáveis pela edição textual. A composição é uma superfície de `WritingWorkspace`, dentro da mesma página de editor. Não é uma janela, outro workspace ou uma cópia editável concorrente do corpo.

A barra de especialidades dá acesso a Texto, Semântica, Linguística, Estrutura e Vínculos já existentes, acrescidos de Composição, Tabelas, Fórmulas e Matemática. As novas especialidades compartilham a superfície de composição e filtram os instrumentos pela tarefa. Os painéis começam recolhidos; clicar na especialidade abre seus instrumentos, e o botão Instrumentos permite recolhê-los. O documento permanece no centro; a lateral fica à direita.

Os objetos não apresentam cabeçalho de seleção nem uma fileira permanente de ações. Clique direito ou Menu/Shift+F10 abre as ações do objeto, incluindo posição/alinhamento, retirada e edição específica. Uma célula de tabela continua clicável como conteúdo. A edição de um parágrafo a partir da composição volta ao editor textual original naquele parágrafo.

## Ligação viva e proveniência

A entrada nativa usa `DocumentBinding::Live`. O modo congelado anterior permanece disponível para validar compatibilidade e os laboratórios históricos. Não é usado pela Escrita integrada.

O arquivo `origem-documento.json` permanece a captura original, byte a byte. O snapshot de trabalho incorpora o Documento completo mais recente em `working_text`. O formato privado novo `INDE-OBJ01-COMPOSITE-P2D2-6` declara essa semântica; formatos antigos continuam reconhecidos. O SQLite auxiliar continua na versão 5 e o banco canônico no schema 37. Não há migração em lote de projetos ou alteração das fontes arquivadas.

Ao adotar uma edição da Escrita:

- o adaptador valida o Documento completo e exige a mesma identidade;
- os objetos, recursos e valores de fórmulas são mantidos;
- parágrafos existentes conservam identidade; novos parágrafos entram pela ordem canônica;
- a sequência autoral respeita os grupos de leitura e mantém os outros objetos ancorados ao próximo grupo sobrevivente;
- uma apresentação independente conserva a ordem relativa dos itens sobreviventes;
- referências a parágrafos efetivamente apagados são retiradas da apresentação; pares que perderam adjacência são desfeitos;
- histórico de objetos é atualizado para a fonte textual vigente, impedindo que Desfazer um objeto ressuscite texto antigo;
- a atualização é validada e confirmada no journal auxiliar antes de substituir o estado em memória.

Adoção não escreve no proprietário. Um movimento autoral de texto é devolvido à Escrita pelo caminho de alteração editorial do editor original, que conserva suas transações e marcações. Reabrir o salvo da composição não autoriza restaurar uma versão antiga do corpo canônico. As lentes textuais são respeitadas na apresentação sem apagar conteúdo do Documento.

## Salvamento e interação

Salvar o Documento/projeto, trocar de Documento e abrir outra especialidade consultam a mesma barreira de rascunho/tarefa/seletor. Os controles externos de edição ficam temporariamente indisponíveis durante um rascunho ou operação de objetos. Os formulários aplicam ou cancelam explicitamente. Campos não aplicados não recebem promessa de recuperação após queda.

O histórico de digitação continua no editor original; o histórico das operações de objetos continua no motor de composição. São contextuais, não uma linha cronológica global única. A adoção da fonte atualiza os estados retidos do histórico de objetos para evitar reversão textual indevida. Essa fronteira deve continuar visível na revisão humana e nos testes.

## Limites e próximos trabalhos

A integração usa layouts de especialidades: a escrita contínua permanece na especialidade Texto e a distribuição de objetos na Composição. Não há inserção arbitrária de widgets no buffer textual nem edição simultânea de vários parágrafos em caixas concorrentes. Não se afirma que toda tipografia avançada do editor já esteja desenhada com fidelidade na superfície de composição.

A publicação/exportação dos novos objetos fica adiada por decisão do usuário, com documento HTML como destino prioritário futuro. A recusa do publicador evita omissão silenciosa dos objetos enquanto essa frente não estiver implementada. Não foram adicionados exportadores novos neste corte.

Variáveis ligadas a entidades/pseudônimos, equivalência completa entre objetos da Escrita e Comic, matemática tipada em células e distribuição em arquivo único `.inde` permanecem frentes próprias. A integração desta interface não representa conclusão dessas capacidades, release ou aceite humano.

## Fronteiras complementares de revisão

As lentes do editor existente atualizam imediatamente a superfície de objetos. Atualizar somente a visibilidade não salva o proprietário nem o banco auxiliar; a apresentação é recalculada no fluxo de visibilidade. Uma seleção que fica oculta deixa de ser alvo dos controles de movimento, e os destinos apresentados respeitam a lente.

O comando Salvar da composição passa pela barreira de salvamento da Escrita, incluindo metadados textuais pendentes. A gravação do proprietário e a do banco auxiliar continuam sequenciais; não se afirma uma transação SQLite única entre ambos. Uma interrupção entre gravações exige respeitar a autoridade do Documento e revalidar a composição ao retomar. Este corte não substitui essa fronteira por uma migração de projetos.

Busca, estatísticas e semântica do corpo continuam com o alcance do texto canônico. Não se afirma que já agreguem automaticamente palavras, intervalos ou anotações dos resultados de cartões, células e fontes matemáticas. A integração visual e a ligação viva não encerram essa adaptação transversal.

A checagem automática abre e aciona menus em passos distintos e captura a interface após uma pintura do GTK. Testa uma janela compacta; a demonstração registrada usa também a tela maior do ambiente local. Avisos críticos GTK/GDK continuam sendo motivo de falha, sem supressão.

Os formulários usam a área central com largura de leitura útil e escondem a lateral e os comandos inativos enquanto um objeto está sendo editado. A apresentação não fica presa a um divisor de painel que reserve a maior parte da janela para instrumentos desativados. A fila curta entre escolher uma ação do menu e abrir seu formulário também participa das barreiras de edição/fechamento.

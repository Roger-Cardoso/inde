# Plano de maturidade do editor do INDE

Data: 24/11/2026. Análise da branch `feature/inde-obj-01-foundation`, documentação em `ad9189c6ca7a256175adf8d87d2984eecf21d8c6`, código validado anterior em `bc636c18aa288972b8badfde38973ff6abc8c045`.

**Estado: diagnóstico e plano para execução futura. Nenhuma correção é implementada por este documento.** O usuário pediu análise extensa e planejamento porque não dispõe de franquia para mandar executar as correções agora. A execução futura não começa automaticamente com a entrega deste plano.

**Retomada em 26/11/2026:** o usuário autorizou iniciar o trabalho. O primeiro corte de implementação atua em E1a, reaproveitando vistas de parágrafos, atualizando a lista de alvos de forma incremental e evitando decodificações repetidas do mesmo Documento na adoção do histórico, com os mecanismos de checkpoint existentes conservados. O avanço, as medições e as lacunas ficam no [relatório de fluidez](../validacao/VALIDACAO-OBJ-01-FLUIDEZ-E1A.md). Essa retomada não declara E1, E1a ou a maturidade completa concluídos; especialmente o objetivo de resposta de até 100 ms e os perfis maiores exigem prova adicional.

**Corte adicional de 26/11/2026, solicitado pelo usuário:** otimização da reordenação e arrasto dos círculos para formar linhas lado a lado. O [relatório de reordenação](../validacao/VALIDACAO-OBJ-01-REORDENACAO-LATERAL.md) registra a conservação das vistas, os indicadores nas quatro bordas e o movimento da linha inteira mesmo com ordens divergentes. É avanço em M08/M09/M11 e E1a/E2; a inserção depois de arranjos divergentes (A02/M10), os perfis maiores e a meta de latência continuam separados e abertos.

**Exclusão visual e continuidade de E2 em 26/11/2026:** o usuário pediu a retirada de elementos/composições em Planejar e a execução direta do próximo corte remanescente. A implementação acrescenta a exclusão confirmada com preservação dos acontecimentos compartilhados, verificação de vínculos e histórico. Em E2, resolve a inserção nativa após ordens divergentes com destinos separados, explicita as confirmações de posição e encaminha o alinhamento ao parágrafo editável. O [relatório desta entrega](../validacao/VALIDACAO-OBJ-01-EXCLUSAO-E2.md) reúne a prova e seus limites. Os achados abaixo conservam o diagnóstico original; A02/M10, A04 e A05 devem ser lidos junto com essa correção. E3 segue como próximo corte, sem promover os ensaios sintéticos a maturidade global.

O editor tem uma base nativa aproveitável e recursos reais, mas ainda não oferece confiança suficiente para o trabalho diário com conteúdo misto. O próximo investimento deve completar os percursos de uso e suas combinações, preservando o Documento, as especialidades e o trabalho já realizado. A quantidade de controles, testes ou trabalhos de CI não substitui essa comprovação.

**Fase 5 antecipada em 26/11/2026:** por escolha e autorização expressa do usuário, o trabalho de arquivo único foi iniciado antes dos cortes restantes da série E. O [contrato P5-A](ARQUIVO-UNICO-P5-A.md) entrega estados imutáveis e transporte reversível, com limites e demonstração próprios. Isso não encerra E3–E7; a próxima continuidade do editor deve considerar a integração de abertura/salvamento/recuperação do arquivo único, mantendo as prioridades e pendências aqui registradas.

**Continuidade de armazenamento em 26/11/2026:** [P5-B](ARQUIVO-UNICO-P5-B.md) integrou abertura, confirmação e recuperação ao aplicativo nativo. [P5-C](ARQUIVO-UNICO-P5-C.md) atua na granularidade dos recursos e nas medições de confirmação, mantendo os arquivos anteriores no perfil original. Esses cortes não encerram E3–E7 nem tornam assíncrona a confirmação do arquivo.

**Aceite e continuidade:** após o usuário informar aprovação do teste C, [P5-D1](ARQUIVO-UNICO-P5-D1.md) separa a confirmação de Salvar/fechar/encerrar do processamento da interface, com pausa de edição, cancelamento antes da confirmação final e saída pendente. Abertura, criação, cópias e histórico ainda têm trechos síncronos. As pendências de fluidez e E3–E7 permanecem.

**Aceite D1 e lote D2:** o usuário aprovou D1 e autorizou o próximo da fila como grande lote combinado. [P5-D2](ARQUIVO-UNICO-P5-D2.md) reúne abertura/troca, criação, duas cópias, consulta paginada e restauração do histórico no trabalhador exclusivo. A drenagem e a adoção nativa seguem na interface. A fase 5 e as pendências editoriais continuam abertas.

**Aceite D2 e lote D3:** após aprovação do usuário, [P5-D3](ARQUIVO-UNICO-P5-D3.md) trabalha a preparação nativa: acompanha recuperação/salvamento sem esperar os coordenadores no thread GTK e confirma os objetos por trabalhador exclusivo antes do arquivo. A adoção da folha permanece na interface, com etapa de passagem explícita. Fase 5 e E3–E7 continuam abertas.

## 1. O resultado a alcançar

Um escritor deve conseguir abrir seu projeto, escrever, inserir uma imagem, criar duas tabelas, colocá-las lado a lado, continuar escrevendo, reorganizar o conjunto, consultar suas marcações, desfazer um engano, salvar, encerrar e retomar sem desmontar o documento nem precisar compreender as divisões internas do programa.

Maturidade tem três dimensões que precisam ser avaliadas separadamente:

| Dimensão | Resultado esperado |
|---|---|
| Estabilidade | Operações aceitas conservam conteúdo e identidades; falhas têm saída; salvar, recuperar e encerrar funcionam |
| Utilidade | Os percursos frequentes terminam pela interface, com poucos passos compreensíveis e efeito visível |
| Completude | O conjunto aprovado de recursos está integrado, incluindo dependências entre ferramentas e uma saída HTML quando essa frente for retomada |

Uma versão pode tornar-se útil para trabalho local antes de completar a visão inteira. Deve declarar seu alcance. Não deve ser chamada de editor composto completo enquanto a exportação omitir ou recusar seus objetos, ou enquanto partes aprovadas existirem apenas em demonstrações históricas.

### Direção preservada

- Um mesmo Documento e uma mesma folha de conteúdo em Texto, Semântica, Linguística, Estrutura, Vínculos, Composição, Tabelas, Fórmulas e Matemática.
- Cada especialidade oferece instrumentos e disposição adequados ao seu trabalho; todas conservam a presença do conteúdo misto.
- O Documento existente continua sendo autoridade do texto, identidades, alternativas, marcações e vínculos. Não criar outra cópia editável concorrente da prosa.
- Interface nativa C++/GTK. O laboratório HTML serve de referência histórica e de comparação de comportamento.
- Ferramentas recolhíveis e ações contextuais, com acesso por teclado e identificação clara do alvo.
- Reordenação é capacidade central de Estrutura. Apresentação visual não define automaticamente leitura, significado ou cronologia.
- Fórmulas de cálculo e objetos de notação matemática são capacidades distintas.
- Compatibilidade de saída será trabalhada depois da estabilização local, priorizando documento HTML.
- Recursos comuns com Comic, arquivo único `.inde` e conversões de projetos ficam em frentes posteriores. Os cortes abaixo não renumeram as fases históricas de OBJ-01.

## 2. Alcance e natureza das evidências

A análise examina o editor de Escrita e suas integrações imediatas: projeção textual, especialidades, objetos, composição, ferramentas, histórico, salvamento, recuperação e proteção de exportação. Não certifica o restante do INDE, como todos os percursos de cartografia, planejamento, Comic ou distribuição.

Foram lidos os caminhos atuais de implementação e os contratos associados. A reprodução no motor de composição descrita na seção 4 ocorreu na revisão imediatamente anterior, em workspace temporário sintético, com o executável de domínio existente e sem abrir projetos autorais. Ela comprova aqueles resultados no motor; não reproduz a tentativa exata de imagem do usuário nem substitui interação gráfica.

Nesta etapa de planejamento não foram recompilados o aplicativo nem executadas novamente as suítes completas. O resultado anterior de 98 testes locais e 26 trabalhos de CI permanece histórico e vinculado ao seu commit. Os 26 trabalhos incluem variantes e camadas técnicas; não representam 26 percursos independentes de escritor aprovados.

Classificação usada neste plano:

- **Relato:** dificuldade informada pelo usuário; a causa pode continuar aberta.
- **Reprodução:** comportamento observado no ensaio sintético especificado.
- **Código:** regra ou caminho localizado na implementação; o efeito gráfico pode exigir prova própria.
- **Lacuna:** comportamento sem cobertura suficiente ou recurso não integrado no caminho examinado.
- **Proposta:** comportamento recomendado para a execução futura; não equivale a alteração já feita.

Referências de partida: [folha comum](OBJ-01-FOLHA-COMUM.md), [integração na Escrita](OBJ-01-ESCRITA-INTEGRADA.md), [resgate das especialidades](../validacao/VALIDACAO-OBJ-01-ESPECIALIDADES-RESTAURADAS.md), [contrato mínimo](CONTRATO-DOCUMENTO-COMPOSTO-OBJ-01.md), [reconciliação](RECONCILIACAO-OBJ-01.md) e [inventário das bases](INVENTARIO-OBJ-01.md). Os registros históricos que descrevem superfícies separadas não governam a direção atual da folha comum.

## 3. O que deve ser aproveitado

| Base existente | Valor a conservar | Cuidado na continuidade |
|---|---|---|
| Motor de Documento e transações textuais | Identidades, formatação, referências, alternativas e conservação de camadas | Acrescentar objetos não pode contornar suas regras de integridade |
| Projeções `ObjectTextProjection` | Escrita nativa diretamente na folha, vinculada ao buffer existente | Completar gestos, navegação, apresentação e escala; não confundir projeção com cópia autoral |
| Comandos de composição | Revisão esperada, candidatos validados, operações recusadas sem aplicação parcial no motor | A recusa precisa ser traduzida em orientação útil na interface |
| Modelo de objetos e ocorrências | Separação de conteúdo, uso, recurso e posição | Mover um uso não deve modificar outros usos sem escolha explícita |
| Recursos incorporados e hashes | Reabertura independente do arquivo original, integridade de bytes | Uma nova interface de importação deve conservar essa proteção |
| Tabelas com IDs de eixos e células | Conteúdo, fórmulas e estilos sobrevivem a mudanças estruturais | A seleção e os comandos precisam atuar pelo ID correto na tela |
| Fórmulas e cartões | Tipos, dependências, condições, comandos consumíveis e avaliação limitada | Completar a interação e a ligação com entidades sem criar outro motor de valores |
| Matemática nativa | Fonte autoral conservada e apresentação derivada | Renderização incompleta deve manter fonte e diagnóstico |
| Ferramentas anteriores de especialidades | Comandos e controles ligados aos serviços existentes | Recuperar o widget não comprova paridade de todos os seus gestos e efeitos |
| Testes de domínio, persistência e entrada real | Detectam classes diferentes de regressão | Ampliar os percursos ausentes sem abandonar a cobertura útil |
| Arquivos fixados e overlays determinísticos | Proveniência e comparação com versões anteriores | Corrigir no código ativo e no gerador correspondente, sem editar a fonte congelada |

Não há evidência que justifique descartar essa base e reescrever o editor inteiro. A necessidade é fechar contratos entre as peças e comprovar seus resultados no aplicativo.

## 4. Achados prioritários

### A01. Inserção falha depois de escolher a imagem

**Relato:** o usuário escolhe o arquivo, mas não consegue concluir. A causa específica dessa tentativa permanece aberta.

**Código:** `pick_image()` em `cpp/object_native_ui.cpp` lê PNG, abre campos de legenda e descrição e depois envia `import`. A operação pode ser recusada por descrição ausente sem marca de figura decorativa, formato/dimensões/tamanho ou por regras de ordenação. O caminho atual não é um importador geral de imagens: aceita PNG estático, até 8 MiB e limites de dimensões.

**Consequência:** é preciso observar a tentativa até o resultado, conservando motivo e campos. O teste de um PNG sintético decorativo não certifica todos os arquivos ou estados de documento. Primeiro fazer o perfil atual funcionar e explicar suas condições; ampliar formatos deve ser uma decisão posterior explícita.

### A02. Organizar visualmente pode impedir novas inserções

**Reprodução e código:** `arrange_objects` pode alterar `visual_order` sem alterar `author_order`. `import`, criação de tabela, criação de equação e inserção de uso de fórmula no documento exigem igualdade entre essas ordens em seus caminhos atuais.

| Sequência reproduzida | Resultado |
|---|---|
| Criar duas tabelas e pareá-las invertendo sua ordem visual | Uma linha criada; `aligned=false` |
| Importar PNG em seguida | Recusa `authorial_requires_alignment` |
| Criar terceira tabela em seguida | Mesma recusa |
| Voltar o selecionado à coluna única | Pareamento retirado; `aligned` continua falso |
| Restaurar apresentação pela leitura | Alinhamento restaurado; os pares são retirados |

**Consequência:** uma proteção deliberada do perfil experimental tornou-se um obstáculo ao uso contínuo do editor integrado. A solução não deve promover a ordem visual a autoral silenciosamente. O comando precisa resolver separadamente o destino autoral e a colocação visual, ou pedir uma escolha localizada quando não houver destino inequívoco. Esta mudança exige revisar o contrato antigo, cuja restrição era explícita.

### A03. Mover um membro desmonta a linha

**Reprodução e código:** duas tabelas foram pareadas com ordens ainda alinhadas. Mover uma tabela pela operação `move` retirou o pareamento. O caminho chama `unpair(candidate, id)`. A interface estrutural considera uma linha como destino inteiro em alguns casos, mas isso não equivale a movê-la como origem inteira.

**Consequência:** distinguir mover objeto, mover linha e retirar objeto da linha. Preservar proporções, espaçamento, membros e identidades ao mover a linha. Grupos textuais indivisíveis e linhas gráficas são conceitos diferentes e precisam de regras compatíveis.

### A04. O formulário oferece operações com confirmações diferentes

**Código:** `layout_editor()` reúne mover, compor lado a lado, voltar à coluna única e tamanho/alinhamento. O rodapé Aplicar executa `set_presentation`; formar a linha depende de Compor lado a lado. O formulário pode, portanto, conservar campos que a pessoa acredita ter aplicado sem executar aquela operação.

**Consequência:** cada intenção precisa de confirmação inequívoca e efeito visível. Separar as tarefas em grupos ou etapas curtas, reaproveitando os comandos. Não exigir que o escritor conheça nomes internos das operações.

### A05. Alinhamento de parágrafo pode ser armazenado sem chegar ao campo visível

**Código; confirmação gráfica pendente:** o parágrafo editável é colocado em `Gtk::ScrolledWindow`. A rotina de `text_align` percorre os filhos diretos da caixa e procura `Gtk::TextView`; esse filho agora é o contêiner. Não foi localizado outro encaminhamento da propriedade para `ObjectTextProjection` nos arquivos examinados.

**Consequência:** acrescentar prova da propriedade efetiva do campo e da aparência na tela, antes e depois de reabrir. Conferir o JSON salvo não basta para certificar alinhamento.

### A06. O histórico ainda depende da espécie de conteúdo e do foco

**Código e contrato atual:** texto usa o histórico editorial; objetos usam o histórico da composição. Atalhos e botões encaminham a um ou ao outro. A sincronização atualiza estados de objetos para evitar que desfazer um objeto ressuscite texto antigo.

**Risco:** a sequência percebida pelo autor não coincide necessariamente com os passos disponíveis. Alterar texto pode retirar pares, e desfazer o texto não promete restaurá-los integralmente.

**Consequência:** definir a unidade de desfazer de uma ação composta antes de ampliar operações. O objetivo é uma sequência coerente de ações do usuário. Isso pode ser coordenado sobre os mecanismos existentes; não exige reescrever os motores de texto e objetos. Enquanto não houver coordenação completa, mostrar o escopo e impedir reversão inesperada de conteúdo relacionado.

### A07. Salvar a folha envolve fronteiras de persistência

**Código:** `flush_changes()` confirma o Documento e depois chama `flush_document_objects()`. O SQLite auxiliar tem checkpoint, revisões, hashes, transação e readback próprios. Não existe uma transação única que abranja automaticamente todos os passos da folha.

**Risco, sem perda autoral demonstrada nesta revisão:** uma interrupção entre confirmações exige reconciliação correta. Mensagem de salvo deve representar o conjunto confirmado, e um resultado incerto precisa conservar meios de retomada.

**Consequência:** ensaiar explicitamente as interrupções entre proprietário e auxiliar. Não remover os bloqueios de integridade apenas para liberar a interface; oferecer recuperação compreensível. Um coordenador mínimo de operação/recuperação deve ser avaliado antes de considerar migração para outro armazenamento.

### A08. Uma folha visualmente comum ainda tem alcances funcionais diferentes

**Código e documentação:** busca, estatísticas e análises do corpo consultam o texto canônico. Células, legendas, frases de fórmulas e fontes matemáticas têm coordenadas e autoridades próprias.

**Consequência:** Localizar no Documento não pode sugerir que examinou todo o conteúdo se só pesquisou parágrafos. Definir escopos visíveis. A busca composta deve devolver alvos tipados por objeto/célula e revisão, sem inventar offsets no corpo. Substituir resultado calculado ou fonte matemática requer regras próprias.

### A09. Seleção contínua e navegação ainda não têm paridade completa

**Lacuna documentada:** há vistas de parágrafos separadas sobre a mesma origem. Seleção canônica e substituição entre parágrafos têm cobertura, mas isso não comprova todos os gestos de arrasto, navegação por linha visual, seleção atravessando objetos, teclado e acessibilidade.

**Consequência:** uma pessoa não pode perder a habilidade cotidiana de selecionar e revisar prosa para ganhar objetos. Preservar os testes existentes e ampliar os gestos efetivos, inclusive nos limites de parágrafos e arranjos.

### A10. A visão aprovada e o conjunto nativo entregue não são idênticos

**Lacunas:** referências das fórmulas consultadas apontam para fórmulas e variáveis. A ligação completa com entidades, pseudônimos e troca de personagem não foi encontrada nesse caminho e continua pendente na documentação. Texto ao redor de resultados é editado por instrumento próprio; isso não equivale à edição contínua de qualquer trecho do corpo. Equação compacta não significa inserção entre caracteres. Não há promessa de compatibilidade integral Excel ou LaTeX.

**Consequência:** manter esses requisitos em uma lista explícita e distinguir ausências de defeitos. Não apresentar uma simulação visual ou uma capacidade do laboratório como integração nativa concluída.

### A11. Os limites atuais podem interromper um uso prolongado

**Código e contratos:** há quotas individuais e agregadas, incluindo blobs, tabelas, equações, fórmulas conservadas e histórico. Fórmulas consumidas e conteúdo retirado continuam retidos. O histórico da composição tem limite de 32 estados e 32 MiB; o núcleo geométrico isolado tem política própria. Não extrapolar a quota de um subsistema para outro.

**Consequência:** exibir limites relevantes e tratar esgotamento sem mensagens genéricas. Comandos de alteração que desaparecem visualmente ainda podem consumir a quota de fórmulas. Uma política de retenção/liberação precisa preservar dependências e recuperação; não acrescentar descarte automático nesta estabilização.

### A12. Desempenho e manutenção ainda precisam de prova do conjunto

**Código e inferência:** há um widget de texto por parágrafo, sinais de atualização ligados à origem, reconstruções estruturais, snapshots e validações do agregado. São mecanismos compreensíveis, mas seu custo conjunto pode crescer com manuscrito, tabelas e histórico. O relato posterior A13 confirma lentidão percebida em uma tarefa específica; a distribuição do custo entre esses mecanismos ainda não foi medida.

**Manutenção:** `object_native_ui.cpp` tem 3.220 linhas e `object_composite_pilot.cpp`, 3.653 no corte analisado. Tamanho não comprova defeito; indica custo de alterar várias responsabilidades juntas. Os overlays acrescentam a necessidade de manter gerador e saída coerentes.

**Consequência:** medir antes de otimizar, dividir apenas as responsabilidades tocadas e evitar uma refatoração total como condição prévia para corrigir o uso.

### A13. Apagar parágrafos vazios causa pausas longas na edição cotidiana

**Relato acrescentado em 24/11/2026:** o usuário colou texto separado por parágrafos vazios. Ao apagar cada vazio, percebeu processamento com demora muito grande. Não foram fornecidos tamanho do documento, quantidade de parágrafos, duração das pausas ou tecla usada; não se atribui uma causa exclusiva sem medição. A dificuldade é de edição básica, não apenas de capacidade para manuscritos grandes.

**Caminho localizado no código:** Backspace no início ou Delete no fim de uma vista pode apagar a quebra no buffer de origem. A edição atualiza a estrutura textual e pode agendar `schedule_common_sheet()`. Essa rotina chama a sincronização diretamente no fluxo da interface. `Session::synchronize_document()` serializa o Documento e, quando há mudança efetiva, reconcilia o candidato e os estados retidos de undo/redo, valida e confirma checkpoint. A rotina externa compara as identidades exibidas e a ordem; quando a estrutura muda, `render()` limpa a folha e recria suas vistas. Além disso, cada projeção está inscrita em sinais da origem e pode agendar atualização de texto, tags e seleção.

Esse caminho contém trabalho potencialmente desproporcional a retirar uma linha vazia. Ele sustenta a investigação, mas não comprova qual parcela causou a pausa do usuário. Os intervalos de agendamento de análise, autosave ou agrupamento de undo não devem ser apresentados como se fossem a duração medida do atraso.

**Consequência de prioridade:** antecipar o diagnóstico e a correção da fluidez de edição para E1a, logo após as garantias de conservação de E1 e antes da conclusão do bloco inicial E1/E2. E6 continua responsável por capacidade e uso prolongado; não deve adiar uma falha que já interrompe a revisão cotidiana.

**Resultado esperado:** apagar uma sequência de parágrafos vazios acompanha o ritmo das teclas, conserva o cursor no ponto lógico, não salta a rolagem, não perde nem duplica entradas e não apaga texto adicional por repetição atrasada. Desfazer, salvar e recuperar conservam o resultado. Não remover vazios automaticamente ao colar: eles podem ser uma escolha autoral, e essa normalização não corrige o custo da edição.

## 5. Inventário de capacidades a fechar

O estado abaixo é uma classificação de cobertura, não um novo passe funcional. Cada conjunto deve se decompor em ações identificadas no corte que o executar.

| Conjunto | Ações que precisam de prova completa | Situação de partida |
|---|---|---|
| Projeto e Documento | Criar, abrir, renomear, trocar, salvar, copiar, fechar e retomar | Percursos básicos existentes; conteúdo misto e falhas precisam de cruzamento |
| Digitação | Inserir, Enter, Backspace/Delete nos limites, acentos, emoji, composição IME, colar trecho longo | Base existente; paridade gestual incompleta e atraso relatado ao retirar vazios (A13) |
| Seleção e foco | Palavra, trecho, vários parágrafos, Ctrl+A, teclado, seleção de objeto/célula/linha e retorno de diálogos | Vários caminhos coexistem; alvo e foco precisam de contrato comum |
| Formatação | Negrito, itálico, sublinhado, cores, estilos existentes, limpar e conservar após movimentação | Compartilha tags; apresentação dos parágrafos exige revisão |
| Clipboard | Copiar/recortar/colar texto e estilos; PRIMARY quando aplicável | Caminho conservador existente; clipboard universal de objetos não entregue |
| Busca | Localizar, navegar, substituir uma/todas, limites por lente e conteúdo composto | Corpo coberto; alcance composto pendente |
| Texto | Estatísticas, metas, apresentação e retorno ao foco de escrita | Controles restaurados; escopo precisa ser explícito |
| Linguística | Recorrências, parágrafos longos, consulta ao dicionário e construções | Alguns comandos exercitados; cobertura integral pendente |
| Semântica | Comentar, tipar, realçar, inspecionar, editar/remover marca, tempo/datas e perspectiva | Não presumir que todas as interações do widget antigo foram transportadas |
| Estrutura | Identidades, grupos de leitura, mover por teclado/arrasto, destino escolhido e limites | Texto básico coberto; linha composta e ordens divergentes são prioridades |
| Alternativas | Criar, selecionar, ativar, comparar quando disponível e conservar folhas cobertas | Motor existente; interação com objetos e arranjos requer prova específica |
| Visibilidade | Criar/editar lente, ocultar/mostrar, navegar e editar respeitando proteção | Projeção existente; verificar seleção e destinos ocultos em operações mistas |
| Vínculos | Marcar, renomear, retirar, navegar, referenciar entidade/trecho e consultar usos | Âncora simples exercitada; demais percursos exigem matriz |
| Imagem | Escolher, validar, inserir, descrever, duplicar, editar metadados quando disponível, dimensionar e retirar | Relato de bloqueio após escolha; PNG é o perfil atual |
| Tabela | Criar dimensões, editar células, inserir/retirar/restaurar eixos, conservar cabeçalho e identidades | Motor com cobertura própria; paridade nativa por operação pendente |
| Estilo de tabela | Modelos, cores, bordas disponíveis, proporções, altura, título e exceções de célula | Verificar efeito real e persistência de cada propriedade exposta |
| Composição | Parear tipos e mesmo tipo, inverter lados, proporções, espaçamento, alinhar, desagrupar e mover linha | Dois membros por linha no perfil atual; conflitos de ordenação confirmados |
| Fórmulas | Biblioteca, guardar valor, calcular, texto por condição, adicionar/reordenar/retirar condição, escrita livre | Motor e cartões existentes; testes gráficos não cobrem todo o ciclo |
| Dependências de fórmulas | Referir variável/resultado, alterar à distância, consumir, desfazer, refazer e diagnosticar ciclo | Regra de consumo existente; repetir em uso nativo e após reabertura |
| Frases com resultados | Editar palavras, incluir vários resultados e conservar após recálculo | Trechos próprios existentes; edição contínua no corpo não equivale ao formulário |
| Fórmulas em tabelas | Criar, anexar, editar, retirar uso e conservar ao ocultar/restaurar eixos | Comprovar células e usos por ID, sem alteração de outra célula |
| Entidades nas fórmulas | Vincular papel a personagem, trocar alvo, escolher nome/pseudônimo e atualizar usos | Requisito aprovado ainda não integrado no caminho atual |
| Matemática | Fonte, modelos, prévia, edição, modo compacto/destaque, diagnóstico e conservação | Renderização nativa existente; não é TeX completo nem cálculo de planilha |
| Retirada e restauração | Objeto, grupo textual, linha/coluna, fórmula e uso; dependências e posição | Retenção reversível existente; quota e arranjo precisam ficar compreensíveis |
| Histórico | Sequências intercaladas, seleção, aparência, dependências e edição após desfazer | Dois históricos; coordenação é requisito de maturidade |
| Persistência e recuperação | Save, readback, retomada, conflitos, cópia completa e falhas entre bancos | Há proteções úteis; falta prova unificada de todos os cenários relevantes |
| Interface e acesso | Ferramentas, menus, atalhos, foco, rolagem, temas, escala de tela, teclado e leitor de tela | Percurso X11 controlado; ambiente cotidiano não certificado |
| Saída | Exportar HTML composto, recursos locais, texto selecionável e relato de limitações | Objetos novos ainda recusados; frente posterior preservada |

## 6. Contratos que devem orientar as correções

### Uma ação tem alvo e efeito identificáveis

Todo comando deve conhecer Documento, revisão, alvo tipado, intenção e destino. Seleção de texto, ocorrência, célula e linha composta não podem ser intercambiadas pelo foco acidental de outro widget. Painel, menu contextual e atalho da mesma ação precisam usar a mesma avaliação de disponibilidade e o mesmo caminho de aplicação.

Um controle sem alvo válido deve mostrar como selecionar o alvo ou explicar a indisponibilidade. Evitar callbacks que simplesmente retornam sem efeito perceptível. O inventário de ações deve registrar rótulo, alvo, condição, efeito, teste e limitação.

### Apresentação e leitura são independentes, mas precisam cooperar

Inserir depois de organizar visualmente deve ser possível sem apagar pares. Escolher destino autoral por identidade e posição visual por identidade/contêiner. Quando a interface não tiver informação suficiente, oferecer a escolha correspondente ao problema local.

Mover linha conserva membros e apresentação; mover um membro para fora é uma operação reconhecível. Editar um parágrafo não deve apagar o arranjo como efeito colateral invisível. Se um arranjo ficar estruturalmente impossível, conservar a intenção recuperável e explicar a resolução; desfazer deve recompor o estado completo quando a ação original for desfeita.

Não transformar uma linha gráfica em resma, nem quebrar grupo textual protegido para acomodar uma tabela. Não ampliar agora o perfil de dois membros para contêineres arbitrários sem necessidade comprovada.

### Aplicar, cancelar, salvar e recuperar têm significados diferentes

Aplicar confirma a edição proposta; Cancelar conserva a versão anterior; Salvar confirma o conjunto durável; Recuperar retoma um estado identificado. Prévia nunca executa comando consumível nem altera o Documento.

Centralizar as regras de transição entre entrada provisória, formulário, seletor, tarefa, confirmação e gravação incerta. O fechamento deve consultar essas regras e oferecer saída adequada. Uma tarefa lenta não deve parecer um bloqueio sem motivo. Rascunhos não aplicados precisam ter seu comportamento declarado; persistência desses rascunhos pode ser um refinamento posterior.

### Desfazer acompanha a ação do escritor

Separar motores internamente não deve obrigar a pessoa a adivinhar qual histórico contém sua última ação. Definir rótulos e unidades de reversão. A meta é coordenação cronológica das ações mistas, com restauração de conteúdo, arranjo, seleção e dependências pertinentes.

Uma mudança em cadeia de variável é um passo lógico; prévia é zero passos; mudar o painel é estado de vista. Reabrir não promete histórico persistente se essa capacidade não existir. Não simular um histórico único apenas juntando botões ou disparando dois undos sem transação e referência ao estado esperado.

### A fonte de cada conteúdo continua explícita

Texto de célula, texto alternativo, frase autoral ao redor de resultado e fonte matemática não são offsets fictícios em `Document.content`. Busca composta e ferramentas transversais precisam de segmentos tipados e navegação ao proprietário correto.

Uma variável de papel, como Vilão, deve apontar para identidade de entidade, com escolha de nome/apelido como apresentação. Trocar o alvo não deve renomear a entidade globalmente; editar uma entidade não deve ser disfarçado de cálculo. Remover alvo ou pseudônimo exige diagnóstico e decisão autoral, sem inferir substitutos pelo nome.

## 7. Plano de execução em cortes verificáveis

As ordens abaixo são prioridades de estabilização, não novas fases do projeto. Cada corte termina em um resultado utilizável, relatório curto, teste de regressão pertinente e limites conhecidos. O corte seguinte usa o mesmo editor e fixtures evoluídas de forma controlada.

### Corte E0. Registrar o contrato de uso e os casos que falham

**Entrega desta análise:** este plano, a distinção de evidências e o inventário acima.

**Primeiro trabalho futuro:** converter as ações expostas em uma matriz rastreável e fixar os três percursos bloqueados como regressões: inserir imagem após escolha, formar duas tabelas lado a lado e mover a linha inteira. Identificar versão executada para evitar testar um binário e usar outro. Conservar cópias sintéticas representativas.

**Concluído quando:** cada falha tem passos, resultado esperado, resultado observado, versão e teste de destino. Não repetir a auditoria dos compactados inteira sem divergência ou nova necessidade.

### Corte E1. Proteger a sessão e permitir sair de qualquer fluxo

**Prioridade:** máxima. **Custo relativo:** médio, com ampliação se a reconciliação entre bancos revelar inconsistência.

Revisar transições de formulário/seletor/tarefa/erro, mensagens, Aplicar/Cancelar, troca de Documento, fechamento e confirmação de salvo. Exercitar a fronteira entre gravação do Documento e do auxiliar, inclusive resultado incerto e reabertura. Reutilizar checkpoint e recovery existentes.

**Concluído quando:** cada estado tem saída normal ou recuperação explícita; erro não apaga campos; nenhum teste deixa o editor permanentemente bloqueado; interrupções ensaiadas retomam estado coerente e identificado. O sucesso de uma gravação parcial não é apresentado como confirmação da folha inteira.

### Corte E1a. Restabelecer fluidez na edição de parágrafos

**Dependência:** garantias de conservação de E1; não depende de fechar todas as especialidades. **Prioridade:** bloqueio de uso cotidiano. **Custo relativo:** a medir após localizar a etapa dominante; não presumir correção pequena nem reescrita total.

Fixar o percurso colar texto com vazios → apagar vários vazios → continuar escrevendo → desfazer/refazer → salvar/reabrir. Medir do evento de tecla ao resultado visível e o tempo de ocupação do fluxo da interface, distinguindo edição textual, captura/serialização, reconciliação do histórico, checkpoint, atualização das vistas e layout/pintura.

Comparar documentos só de texto e mistos, histórico recente e mais preenchido, exclusões isoladas e consecutivas. Usar quantidades crescentes de parágrafos, por exemplo 20, 200 e 2.000, com e sem vazios, dentro dos limites existentes. Comparar também textos com tamanho parecido em bytes e quantidades diferentes de parágrafos. São perfis de ensaio propostos, não medições realizadas.

Investigar primeiro: atualização restrita às identidades afetadas, reaproveitamento de vistas, redução de atualizações redundantes e retirada de trabalho pesado do caminho imediato de entrada. Agrupar trabalho ou executá-lo fora desse caminho exige revisão esperada, ordem das edições, tratamento de resultado antigo e barreira de salvamento correta. Não enfraquecer checkpoints nem apenas deslocar uma pausa para mais tarde sem conservar o trabalho e medir o resultado.

**Concluído quando:** M41–M45 aprovam o comportamento, as medidas identificam a redução da causa dominante e a edição consecutiva mantém resposta previsível no perfil declarado. Usar o objetivo inicial de até 100 ms de resposta de edição no perfil médio como proposta a calibrar, incluindo percentis e pausas máximas; a média sozinha pode esconder travamentos. Uma ação que termina corretamente depois de longa pausa não passa como fluida.

### Corte E2. Completar imagem, posição e linhas lado a lado

**Dependência:** regras de sessão de E1, fluidez básica de E1a e definição da unidade de histórico para as operações deste corte. **Custo relativo:** médio a alto.

Resolver inserção em ordens divergentes com destinos explícitos. Revisar o formulário de posição, a confirmação de cada intenção e o alinhamento efetivo de parágrafos. Implementar movimento de linha preservando o pareamento, deixando a retirada individual separada. Manter o perfil atual antes de acrescentar mais tipos de arranjo.

**Concluído quando:** o escritor insere PNG, cria duas tabelas, coloca-as lado a lado, troca lados/proporções, move o conjunto, acrescenta outra imagem e continua escrevendo. Desfazer/refazer e salvar/reabrir conservam os resultados. Nenhum passo exige restaurar toda a apresentação para conseguir continuar.

### Corte E3. Consolidar escrita contínua, seleção e histórico misto

**Dependência:** E1; usa os percursos de E2. **Custo relativo:** alto; pode ser dividido em seleção e coordenação de histórico sem esconder a parte pendente.

Fechar navegação, seleção entre parágrafos, fronteiras com objetos, formatação, clipboard, busca de parágrafos e foco após diálogos. Coordenar ações intercaladas de texto e objetos, preservando identidades, camadas e arranjos. Comprovar que Enter, remoção e desfazer não desmontam silenciosamente apresentação válida.

**Concluído quando:** uma sessão de revisão de prosa com objetos conserva as habilidades cotidianas do editor anterior; a sequência de Desfazer é previsível e não ressuscita texto antigo, perde vínculos nem deixa um arranjo impossível.

### Corte E4. Fechar a paridade das especialidades e das tabelas

**Dependência:** E2 e E3 para validar alvos e reversão. **Custo relativo:** médio a alto, proporcional ao inventário de comandos.

Conferir cada ferramenta de Texto, Linguística, Semântica, Estrutura e Vínculos pelo efeito. Incluir alternativas, lentes, tempos, referências e retorno ao trecho. Fechar todas as propriedades expostas de tabela: conteúdo, eixos, cabeçalho, estilo e medidas. Preservar layouts próprios e menus contextuais sobre a mesma folha.

Declarar o alcance das análises imediatamente. Acrescentar busca composta por segmentos quando o contrato estiver definido, sem transformar resultado calculado em texto livre. Selecionar uma célula deve permitir editar a célula e seu estilo sem atingir outra.

**Concluído quando:** todos os controles expostos têm efeito comprovado ou indisponibilidade explicada, e as especialidades oferecem seu trabalho completo no perfil declarado. A avaliação inclui consulta, alteração, retirada e reabertura, não apenas abrir painéis.

### Corte E5. Completar cartões, dependências e entidades aprovadas

**Dependência:** histórico, alvos e persistência estabilizados. **Custo relativo:** alto para entidades; médio para percursos de cartões já existentes.

Exercitar cartões numéricos/textuais, frases preenchíveis, condições ordenadas, referências entre fórmulas, células, mudança em cadeia e comando que desaparece após produzir efeito fixo. Dar diagnóstico de dependência/ciclo/erro de tipo sem perder o trabalho. Tratar explicitamente a quota de comandos consumidos.

Integrar papel de entidade, alvo por ID, nomes alternativos/pseudônimos e troca de alvo, usando o catálogo existente. Distinguir alteração de valor de alteração da entidade. Conservar a escrita livre como alternativa; escritores não devem precisar aprender identificadores técnicos para usar os cartões.

Revisar a matemática pelo mesmo ciclo de objeto: fonte, apresentação, edição, posição, histórico e retomada. Não acrescentar um ambiente TeX geral, planilha Excel completa ou execução de scripts como condição para estabilizar o perfil aprovado.

**Concluído quando:** o exemplo HP 100 → 80 funciona de outra fórmula e de uma célula, com consumo e reversão corretos; condições textuais mudam a frase preservando palavras autorais; Vilão pode trocar de personagem/nome apresentado sem substituir prosa por busca textual e sem alterar entidades erradas.

### Corte E6. Demonstrar capacidade de uso prolongado e acessibilidade

**Dependência:** percursos funcionais fechados; aproveita as medições e correções de fluidez cotidiana de E1a. **Custo relativo:** médio, variável conforme gargalos encontrados.

Medir abertura, digitação, seleção, movimentação, recálculo, salvar e memória em perfis representativos. Usar como pontos iniciais de ensaio documentos de 10 mil, 50 mil e 150 mil palavras, com quantidades controladas de parágrafos/objetos dentro dos limites vigentes. São tamanhos propostos para teste, não capacidades já demonstradas.

Avaliar janela menor, escala 100%/150%/200%, temas claro/escuro, teclado, foco e leitor de tela. Repetir entrada em ambiente gráfico local usado pelo autor e por pelo menos um método externo de entrada suportado. A sessão X11 isolada continua útil; não certifica automaticamente outros ambientes.

Objetivos iniciais propostos: resposta de digitação percebida em até 100 ms e ações comuns em até 200 ms no perfil médio, avaliadas por percentis e no equipamento registrado. Operações mais longas devem mostrar andamento e preservar responsividade. Ajustar esses objetivos com medidas; não prometer valores universais nem virtualizar toda a folha antes de localizar o custo.

**Concluído quando:** há um envelope publicado de uso suportado, sem falha progressiva nas sessões prolongadas ensaiadas, sem controles essenciais inacessíveis e com medições repetíveis. Um limite conhecido é informado antes de destruir o trabalho.

### Corte E7. Entregar saída HTML e preparar uma versão utilizável

**Dependência:** estabilização local demonstrada. **Custo relativo:** alto; frente posterior já prevista pelo usuário.

Exportar documento composto para HTML com texto selecionável, recursos locais incorporados ao pacote, tabelas, resultado de fórmulas, matemática e arranjos compatíveis. Declarar perdas. Excluir fontes privadas, alternativas cobertas e dados não selecionados para saída conforme o contrato editorial. HTML é derivado; não substitui o projeto de autoria.

Conferir abrir em outro local sem o arquivo de imagem original, sem caminhos da máquina e sem depender do repositório. Preparar identificação de versão, instruções de abertura, compatibilidade, backup/cópia e retorno a uma cópia anterior. Não misturar isso com arquivo único `.inde` ou serviço comum com Comic.

**Concluído quando:** o escritor consegue produzir uma saída fiel ao perfil declarado, transportá-la e conferi-la fora do editor; a instalação candidata executa os percursos aprovados e não omite objetos silenciosamente. Promoção a main/distribuição é decisão separada.

## 8. Matriz mínima de aceitação

Cada caso deve registrar versão, ambiente, alvo, passos e resultado. Persistência é conferida depois de fechar/reabrir quando aplicável. Testes negativos verificam também que o estado anterior foi conservado. A lista não exige um teste de UI para cada combinação de valores: regras combinatórias ficam no domínio; percursos essenciais e falhas de interação ficam no aplicativo.

| ID | Percurso | Prova decisiva |
|---|---|---|
| M01 | Criar projeto e Documento vazio, escrever e reabrir | Texto, título e identidades corretos |
| M02 | Acentos, emoji, Enter, Backspace e Delete nos limites | Texto exato, sem perda de foco ou entrada provisória |
| M03 | Selecionar e substituir entre parágrafos | Intervalo correto e camadas preservadas |
| M04 | Copiar/colar com e sem estilos, recortar e desfazer | Conteúdo esperado, sem copiar vínculos indevidamente |
| M05 | Inserir PNG com descrição; inserir decorativo | Figura visível e bytes incorporados após reabrir |
| M06 | Arquivo recusado, descrição ausente, cancelar/repetir | Motivo útil, campos conservados e editor liberado |
| M07 | Retirar o arquivo original após importação na fixture | Projeto reabre a figura pelo recurso interno |
| M08 | Formar texto–texto, texto–imagem e imagem–tabela | Geometria lado a lado e leitura declarada conservadas |
| M09 | Formar tabela–tabela e inverter lados | Ambas editáveis, sem sobreposição ou largura inutilizável |
| M10 | Compor com objetos não adjacentes e depois inserir | Nenhuma restauração global obrigatória |
| M11 | Mover linha composta por arrasto e teclado | Membros, proporções e intervalo conservados |
| M12 | Retirar só um membro da linha | Efeito explícito e reversível, demais objetos conservados |
| M13 | Ajustar largura e alinhamento de parágrafo/tabela | Propriedade efetiva, aparência e reabertura coerentes |
| M14 | Escrever/Enter/apagar perto de uma linha e desfazer | Texto e arranjo restaurados conforme unidade da ação |
| M15 | Criar tabela, editar células, inserir/retirar/restaurar eixos | IDs, textos e fórmulas certos por célula |
| M16 | Alterar cada família de estilo/medida da tabela | Mudança visível, cancelamento e undo corretos |
| M17 | Valor, cálculo, condição numérica e condição textual | Resultados corretos e cartões compreensíveis |
| M18 | Adicionar, editar, ordenar e retirar condições | Prioridade efetiva corresponde à ordem mostrada |
| M19 | Alterar variável remota e consumir comando | Efeito uma vez, dependências atualizadas e reversão completa |
| M20 | Usar/criar fórmula na célula e retirar seu uso | Célula correta e recuperação do conteúdo anterior |
| M21 | Editar palavras ao redor de vários resultados | Recálculo conserva prosa e referências |
| M22 | Ciclo, referência ausente, divisão por zero e tipo inválido | Diagnóstico localizado e nenhuma mudança parcial |
| M23 | Vincular Vilão, escolher pseudônimo e trocar alvo | Todos os usos pertinentes atualizados por identidade |
| M24 | Equação válida e fonte incompleta/não suportada | Fonte preservada e apresentação/diagnóstico corretos |
| M25 | Marcar, comentar, tipar, editar, retirar e navegar | Trecho correto antes/depois de reordenar |
| M26 | Dicionário, recorrências, meta e parágrafo longo | Conteúdo/escopo corretos e navegação ao alvo |
| M27 | Âncoras, referências de entidades e entre trechos | Identidades preservadas ao mover e reabrir |
| M28 | Alternativas e lentes com folha mista | Ocultação não vira exclusão nem publicação indevida |
| M29 | Buscar por escopo e substituir conteúdo editável | Resultados tipados; geração protegida não é sobrescrita |
| M30 | Trocar as nove especialidades após operações mistas | Mesma folha, seleção e instrumentos adequados |
| M31 | Intercalar texto, imagem, estilo e fórmula; desfazer/refazer | Sequência compreensível, sem reverter fonte errada |
| M32 | Erro/cancelamento em cada espécie de formulário e seletor | Nenhum bloqueio permanente; saída normal disponível |
| M33 | Salvar, trocar Documento, Salvar como e reabrir cópia | Todos os conteúdos e recursos presentes na cópia |
| M34 | Interrupção antes/entre/depois das confirmações de gravação | Salvo/trabalho identificados e reconciliação segura |
| M35 | Versão incompatível, conflito externo e blob alterado | Recusa explicada, sem regravar ou simplificar a fonte |
| M36 | Atingir limite de retenção/capacidade em fixture | Mensagem específica e trabalho anterior conservado |
| M37 | Sessões prolongadas nos perfis de escala | Medidas registradas e ausência de degradação bloqueante |
| M38 | Teclado, foco, temas, escala e tecnologias de acesso | Ações essenciais alcançáveis e alvos identificáveis |
| M39 | Exportar HTML e abrir longe do projeto original | Conteúdo misto correto, recursos locais e privacidade |
| M40 | Percurso completo de escritor sem observador de teste | Resultado confirmado na aplicação normal |
| M41 | Colar prosa com vazios e apagar um vazio por Backspace e por Delete | Exclusão correta, resposta medida, cursor e rolagem estáveis |
| M42 | Apagar vários vazios em sequência e continuar digitando | Sem fila longa de entrada, perda/duplicação de teclas ou exclusão de prosa por atraso |
| M43 | Repetir exclusões com tamanhos e contagens de parágrafos diferentes | Crescimento do custo identificado por etapa, percentis e pausas máximas registrados |
| M44 | Repetir perto de tabela/figura/linha composta, com histórico preenchido | Fluidez e conservação de arranjos, seleção e dependências |
| M45 | Desfazer/refazer a retirada dos vazios, salvar e retomar | Conteúdo, identidades e apresentação coerentes; nenhuma otimização enfraquece recuperação |

Para M08–M16, incluir medições de posição/tamanho ou comparação visual pertinente. `rows.size()==1` não demonstra que os widgets ficaram lado a lado. Para M25–M29, a existência de uma janela não demonstra que ela recebeu o trecho certo. Para M34, integridade SQLite sozinha não demonstra coerência entre estados de conteúdo.

## 9. Como organizar a validação e poupar trabalho

### Camadas complementares

1. **Domínio:** regras, dependências, grupos, identidades, revisões e recusas. Muitos casos rápidos, sem interface.
2. **Persistência:** salvar, retomar, cópia, leitura compatível e falhas injetadas. Candidatos e hashes comparados, além de integridade SQLite.
3. **Integração nativa:** foco, seletores, widgets, contexto e transições reais. Asserções diretas de widgets continuam úteis quando identificadas como tal.
4. **Entrada real:** mouse e teclado para percursos completos no editor; observador não preenche campos nem executa a ação em lugar do usuário.
5. **Sessão normal:** percurso independente do modo de teste, com demonstração sintética utilizável, e avaliação humana de entendimento, esforço e continuidade.

Dividir o atual percurso longo em cenários menores, com preparação comum, para que uma falha no seletor não esconda todas as verificações de tabela e fórmula posteriores. Não transformar todo teste em um percurso lento completo. Fixar dados determinísticos e usar sementes diferentes em testes combinados de domínio quando isso explorar interações relevantes.

### Evidência proporcional por corte

Registrar o problema, a causa, o comportamento final, o teste que falhava antes e passa depois, o escopo e as lacunas. Incluir captura apenas quando ela comprovar algo visual. Evitar recompilar tudo ou repetir todas as auditorias após mudanças exclusivamente documentais.

Durante a implementação: rodar os testes afetados; depois, uma regressão integrada pertinente. Para o candidato de um corte funcional: compilação limpa representativa, suíte obrigatória, entrada real e gates de sanitização aplicáveis. Não promover como concluído um trabalho de CI ainda em andamento. Evidências fixam o commit testado, não se reescrevem para aparentar validar código posterior.

Manter a verificação dos geradores/overlays e das fontes fixadas. Distinguir skip, indisponibilidade do ambiente e aprovação. Uma prova por sinal GTK não passa a ser prova de gesto físico por estar no mesmo relatório.

### Uso da franquia futura

- Primeiro obter o bloco E1 + E1a + E2 utilizável, incluindo apagar vazios sem pausas longas; não abrir frentes de novas funções enquanto ele estiver quebrado.
- Aproveitar as reproduções A02/A03 e a estrutura de teste existente. Não começar outra auditoria geral do acervo.
- Reservar E3 como trabalho de integração substancial, em vez de escondê-lo em pequenas mudanças de botões.
- Executar E4 por especialidade com a mesma matriz e infraestrutura de teste.
- Em E5, separar a estabilização dos cartões existentes da nova integração de entidades. Assim cada entrega conserva valor mesmo se a próxima aguardar franquia.
- Medir custos e resultados por corte; não estimar tokens, dias ou porcentagem global sem executar um corte representativo.
- Interromper a expansão de escopo quando aparecer um bloqueio fora do corte; registrar a dependência e sua prova, mantendo a correção relacionada rastreável.

## 10. Critérios para chamar a versão de estável e útil

### Editor local estável no perfil declarado

- Sem falhas bloqueantes conhecidas nos percursos básicos de escrever, inserir, arranjar, mover, desfazer, salvar, recuperar e sair.
- Matriz aplicável aprovada, com faltas de visão completa listadas como ausências reais, não como funções entregues.
- Nenhuma perda ou alteração indevida nos testes de conteúdo, vínculos, recursos e recuperação.
- Estados de erro deixam saída e preservam trabalho; não há sucesso aparente para uma ação sem efeito.
- Capacidade de uso e ambientes suportados declarados com medidas e testes.
- Duas sessões propostas de 45–60 minutos em projeto sintético representativo, incluindo revisão e retomada em outro momento, sem auxílio para superar bloqueios do aplicativo. São gates futuros, não testes realizados.
- Passe humano separado dos testes automáticos; não transferir ao usuário a tarefa de descobrir sistematicamente os defeitos que a equipe deveria cobrir.

### Visão madura do editor composto

Além do perfil local estável: recursos aprovados integrados, alvos/alcances coerentes, histórico de ações mistas compreensível, ligação de fórmulas com entidades concluída quando incluída no perfil, acessibilidade essencial e saída HTML fiel. Uma versão não precisa ser Excel ou um ambiente TeX para cumprir esse objetivo.

## 11. Fronteiras e trabalhos adiados

Este plano não autoriza nem executa correções de código, migração de schema, conversão de projetos reais, merge em main ou publicação. O trabalho desta etapa é documentação local. Os projetos autorais continuam fora dos ensaios.

Na futura estabilização, usar fixtures e cópias verificadas para validação; conservação de compatibilidade acompanha cada mudança. Se uma alteração de formato se revelar necessária, apresentar a necessidade concreta e o procedimento de cópia, comparação e reversão antes de promover conversão de trabalho autoral.

Permanecem separados: serviço universal com Comic, formatos de imagem adicionais, grade geral com spans, arranjos arbitrários aninhados, resize/snap universal, planilha completa, TeX completo, histórico persistente ilimitado, coleta definitiva automática, colaboração e arquivo único `.inde`. A existência de primitivas desses temas na fundação não os torna requisitos de reparo imediato nem capacidades prontas da interface.

## 12. Pontos de código para a retomada

| Assunto | Referência no corte analisado |
|---|---|
| Importação e mensagens | `cpp/object_native_ui.cpp`: `pick_image`, `error`, mapa `messages` |
| Formulário de posição | Mesmo arquivo: `layout_editor`, botões `native-move`, `native-pair`, rodapé `set_presentation` |
| Renderização e alinhamento | Mesmo arquivo: `render`, `item`, `FractionRow`, `table_view` |
| Reordenação nativa | Mesmo arquivo: `reading_unit`, `move_relative`, `move_step`, controladores dos círculos |
| Regras do agregado | `cpp/object_composite_pilot.cpp`: `request`, `move`, `arrange_objects`, `stack`, `restore`, `validate` |
| Confirmação e recuperação | Mesmo arquivo: `checkpoint`, `save_composite`, `publish`, `replay`, `Session::synchronize_document` |
| Ligação com o Documento | `cpp/writing_object_integration.cpp`: `show_object_specialization`, `flush_document_objects`, `make_paragraph_view` |
| Projeção de entrada | `cpp/object_text_projection.cpp` e `.hpp` |
| Salvamento editorial | `cpp/overlay/src/ui/writing_workspace_document.cpp`: `flush_changes` |
| Histórico textual | `cpp/overlay/src/ui/writing_workspace_editor.cpp`: `undo`, `redo`, grupos de edição |
| Ferramentas recuperadas | `cpp/writing_specialty_restore.cpp`, overlay de clipboard/controles/especializações |
| Busca e estatísticas | Overlays `writing_workspace_search.cpp` e `writing_workspace_tools.cpp` |
| Cartões, fórmulas e tabela | `cpp/object_formula*.cpp`, `cpp/object_table_design.cpp`, formulários em `object_native_ui.cpp` |
| Geração da integração | `cpp/integrar_objetos.py`, `cpp/integrar_gtk.py`, `cpp/CMakeLists.txt` |
| Testes existentes | `tests/object_composite_pilot`, `cpp/object_native_tests.cpp`, `cpp/object_native_smoke.cpp`, `tests/test_obj01_real_input.py` |
| Observação e sessão privada | `cpp/object_usage_probe.cpp`, `tests/run_obj01_real_input.py` |
| Limite de saída atual | `cpp/overlay/src/ui/writing_workspace_export.cpp`, `Session::plain_text_compatible` e serviço de publicação |

Retomada sugerida: confirmar branch e versão, ler este plano e os achados A01–A07 e A13, fixar as regressões de E0 e executar E1/E1a/E2 no editor existente. Conservar a direção de um Documento e a separação entre diagnóstico, implementação, teste, aceite humano e promoção.

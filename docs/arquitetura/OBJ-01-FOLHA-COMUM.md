# OBJ-01 — uma folha comum nas especialidades

Direção de 23/11/2026, na branch `feature/inde-obj-01-foundation`. Sucede a divisão de superfícies descrita em [Escrita integrada](OBJ-01-ESCRITA-INTEGRADA.md). O corte anterior sincronizava duas superfícies; essa solução não cumpria a exigência de ver e operar a mesma folha em todas as especialidades.

## Comportamento

Texto, Semântica, Linguística, Estrutura, Vínculos, Composição, Tabelas, Fórmulas e Matemática apresentam a mesma folha mista. Trocar instrumentos conserva parágrafos editáveis, figuras, tabelas, equações, resultados de fórmulas, ordem de apresentação e arranjos lado a lado. A troca não cria outra instância de documento, salva automaticamente uma edição diferente nem reconstrói os campos GTK quando a estrutura permanece igual.

Os parágrafos podem ser editados diretamente em qualquer especialidade. A ação contextual do parágrafo leva ao campo presente na folha, sem trocar para um editor que ocultaria os objetos. Os instrumentos continuam recolhíveis; ações dos objetos seguem no clique direito ou Menu/Shift+F10.

## Autoridade e edição

`WritingWorkspace` conserva sua autoridade: buffer canônico, identidades de parágrafo, formatação, referências, semântica, alternativas, transações, autosave e recuperação. O `TextView` linear original permanece como adaptador interno e caminho de compatibilidade para documentos sem parágrafos canônicos.

`ObjectTextProjection` é uma vista editável de um intervalo desse buffer. Cada vista usa marcas de início/fim em coordenadas Unicode; sua representação local não constitui outro Documento nem possui histórico independente. Inserções e remoções atravessam primeiro os guardas existentes do buffer canônico. Uma entrada recusada não é aplicada localmente. Uma vista desatualizada recusa a edição e se atualiza, em vez de sobrescrever texto novo.

As vistas compartilham a tabela efetiva de tags da Escrita. Formatação, realces semânticos, busca e invisibilidade são projetados conservando as coordenadas da origem, inclusive quando parte de um parágrafo está oculta. Seleções locais atualizam a seleção canônica usada pelas ferramentas existentes. Atalhos de cópia, recorte e colagem usam o caminho editorial já existente; Ctrl+A pode selecionar o corpo canônico. Busca, referências e inspetores devolvem o foco à folha comum.

As vistas usam um contêiner GTK que propaga a altura natural do conteúdo, incluindo quebra de linhas, e uma altura mínima explícita de 32 pixels antes do primeiro cálculo de layout. O cursor acompanha a área visível da folha durante digitação e navegação. A digitação comum atualiza vistas sem destruir a entrada GTK. Mudanças estruturais, como Enter, reconstroem os intervalos necessários à apresentação; as identidades vêm do mecanismo de parágrafos existente. Composição provisória de IME bloqueia troca de especialidade, reconstrução e comandos de objetos. O texto aceito volta ao mesmo fluxo de salvamento e recuperação; não se implementou um motor próprio de entrada.

A máscara de visibilidade é calculada a partir do mesmo retrato de Documento adotado pelo motor, antes da atualização da folha. A comparação usa as identidades dos parágrafos exibidos, incluindo um parágrafo vazio quando o endpoint é editável pela lente canônica. A reconciliação da entrada e a atualização da folha são agendadas depois da entrega do evento, com prioridade anterior aos ciclos de layout e pintura GTK. Isso evita depender de uma pausa prolongada na pintura para completar uma entrada aceita. Atualizar a apresentação não grava bancos.

A rolagem pertence à folha externa. Uma notificação de tamanho da vista focada agenda a conferência do cursor para um próximo ciclo do loop; não cancela animações de ajuste dentro da fase de layout. Focar um parágrafo não inicia outra animação de rolagem interna do `TextView`, cuja altura já acomoda o conteúdo completo. Os percursos gráficos usam `G_DEBUG=fatal-criticals` para transformar erros críticos de ciclo GTK em falhas do teste.

A composição continua no motor privado existente. O Documento canônico segue a autoridade sobre texto e metadados. Permanecem o schema canônico 37, SQLite auxiliar versão 5 e formato privado versão 6. Não se migram projetos reais ou os arquivos compactados.

## Compatibilidade

Documentos anteriores sem identidades canônicas continuam no editor linear. O programa informa que é necessário identificar os parágrafos em Estrutura para usar a folha com objetos. Essa é uma ação editorial existente e explícita: abrir ou trocar especialidade não converte projetos em lote nem inventa identidades persistentes.

Documentos novos identificados, inclusive vazios, usam a folha comum. Os formatos anteriores de composição permanecem reconhecidos. Publicação dos objetos, arquivo único `.inde` e revisão dos estados narrativos de variáveis não integram este corte.

Abrir a folha pode criar o banco auxiliar mesmo em um Documento que só contém parágrafos. A exportação HTML/TXT e a publicação textual anterior continuam permitidas quando uma conferência somente de leitura comprova: mesmo projeto e Documento, snapshots salvos e de trabalho íntegros, sequência de parágrafos idêntica e ausência de objetos, arquivos, arranjos ou apresentação adicional. Composição desconhecida, inválida ou incompatível recusa a saída com explicação, antes de gerar um arquivo incompleto. Essa exceção não implementa exportação dos objetos novos.

## Limites de interação a verificar

- O histórico de texto e o de objetos ainda são contextuais; não existe uma linha cronológica global de Desfazer. A digitação nas vistas usa exclusivamente o histórico textual existente.
- Seleção por arrasto contínuo entre várias vistas e equivalência de navegação linha a linha com o antigo editor precisam de trabalho adicional. Seleção canônica, ferramentas e clipboard não devem ser confundidos com equivalência completa dos gestos.
- Realces de tags aparecem na folha; tooltips e gestos especializados ligados exclusivamente ao antigo widget precisam de adaptação própria. Não se declara paridade integral de acessibilidade ou dos controladores de mouse do editor anterior.
- O motor ainda pode remover pares de apresentação quando uma edição retira identidades ou quebra adjacências; desfazer o texto não promete restaurar todo arranjo retirado.
- Busca, estatísticas e semântica continuam no corpo canônico. Resultados de cartões, células e fontes matemáticas ainda não se tornam automaticamente parte desse alcance.
- A gravação do proprietário e do auxiliar segue sequencial. Não se declara atomicidade entre bancos nem recuperação de formulários não aplicados.
- O passe automático de composição IME usa sinais GTK; digitação física por métodos externos, sessão longa, desempenho em manuscritos grandes e leitores de tela permanecem verificações específicas.

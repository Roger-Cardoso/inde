# OBJ-01 — validação da correção C++/GTK4

Data: 2026-11-22. Branch `feature/inde-obj-01-foundation`. Base `47328b9fc90a03beff3a640d9ec7b90ccaf62016`; implementação principal `764c648a224a5977e2169e247305d2e03377d09c`; compatibilidade GCC 13 `7d6edea6e6aa55c7c1bd449e40826c49acc5092c`; revisão final de código `40488abf05c13333e4f1fba87523834c366fade4`.

[Contrato nativo](../arquitetura/OBJ-01-GTK-NATIVO.md), [roteiro utilizável](../../tools/obj01-pilot/NATIVO.md), [resultados verificáveis](evidencias-obj01-gtk/resultados.json), [manifesto](evidencias-obj01-gtk/SHA256SUMS).

## Requisito corrigido e escopo

A entrega anterior embutia HTML/JavaScript por WebKit em uma janela GTK. O usuário esclareceu que isso não atendia à implementação na linguagem nativa do INDE. Este corte substitui a superfície por C++/GTK4 e a matemática por desenho Cairo/Pango com MicroTeX. Não se usa a aprovação do piloto web como prova dos controles novos.

O motor C++ existente foi aproveitado, incluindo cálculo, cartões, vínculos entre fórmulas, texto misto, tabelas, ordem/composição, retirada/restauração, histórico e persistência. Seus arquivos de domínio e codecs não foram reimplementados nem alterados por essa correção. A janela usa `ProjectService` e o Documento real selecionado na Escrita.

A biblioteca matemática importada tem revisão fixa, fontes e licenças incluídas. A fronteira da prévia limita comandos/ambientes, UTF-8, tamanho, profundidade e células; fontes recusadas na prévia continuam preserváveis no objeto. Os recursos são derivados, sem TeX externo, shell ou navegador. A ponte Cairo portada, o wrapper e a interface própria conservam os avisos estritos do INDE. Somente o núcleo importado usa seu perfil de avisos de terceiros.

## Matriz executada

| Verificação | Resultado | Evidência |
| --- | --- | --- |
| Aplicativo Release, regressão completa na revisão final | **63/63 testes** em 172,78 s | [CTest](evidencias-obj01-gtk/nativo-63.txt) |
| Dependências do executável, vínculo de projeto e percurso GTK | **3/3 testes** em 13,657 s | [integração](evidencias-obj01-gtk/nativo-python.txt) |
| Fronteira de entrada matemática | **1 programa**, com 7 expressões aceitas e 12 recusadas | [limites](evidencias-obj01-gtk/matematica.txt) |
| Proveniência de dependências nativas | **245 arquivos conferidos**, com um ajuste explícito do núcleo | [proveniência](evidencias-obj01-gtk/dependencias.txt) |
| Publicação anterior e recusa de composição ainda não traduzida | **4/4 testes** em 1,629 s | [publicação](evidencias-obj01-gtk/publicacao.txt) |
| Auditor de arquivos compactados | **5/5 testes** | [auditor](evidencias-obj01-gtk/auditor.txt) |
| Compilação direta GCC 13 das quatro unidades novas | **4/4**, sem diagnósticos | [compilação](evidencias-obj01-gtk/gcc13-unidades.json) |
| CI final, incluindo matriz GCC/Clang e sanitizers do motor | **26/26 jobs** | [metadados](evidencias-obj01-gtk/ci.json) |

A CI final é a execução **[45084601502](https://github.com/Roger-Cardoso/inde/actions/runs/45084601502)**, revisão `40488abf05c13333e4f1fba87523834c366fade4`. Inclui compilação limpa GCC 13 Release, fontes nativas verificadas, teste matemático, os três testes nativos sob Xvfb e a publicação anterior. A declaração redundante que falhou na primeira execução foi retirada; não se desabilitaram diagnósticos do código próprio. [Log do job nativo](evidencias-obj01-gtk/ci-nativo.txt).

A conferência direta das quatro unidades GCC 13 foi uma pré-conferência de portabilidade; a CI final recompila o aplicativo completo com a revisão final. Os 63/63 testes locais também foram repetidos nessa revisão. As aprovações anteriores de 62 testes e das interfaces em navegador permanecem históricas.

## Percurso real dos controles GTK

O teste `--objetos-smoke` cria um projeto novo, abre `MainWindow`, seleciona o documento na Escrita e aciona `document-objects`. Opera botões, seletores e buffers de texto GTK; nenhuma mutação é enviada por JavaScript. A observação de estado é somente de leitura, e a seleção do arquivo de imagem usa o seletor nativo.

1. Abrir o editor de equações, tentar fechar Objetos e a janela principal e conferir que o rascunho impede ambos os fechamentos.
2. Selecionar os seis modelos matemáticos e conferir que cada um produz um desenho nativo; criar segundo grau, somatório e matriz.
3. Escolher um PNG pelo seletor GTK, preencher descrição/legenda e importar seus bytes pelo motor.
4. Criar tabela e uma fórmula Vida na célula, com entrada 100 e frase ao redor do resultado.
5. Criar outro cartão que altera Vida.HP para 80; executar e conferir desaparecimento do comando. Desfazer retorna 100; refazer aplica 80 novamente.
6. Aplicar o modelo Caderno à tabela; inserir, retirar e restaurar linha, conservando a fórmula.
7. Criar um cartão numérico referenciado em Vida.HP, reordenar suas condições e conferir “em combate”. Criar cartão textual com entrada “hostil”, retirar uma condição e conferir “ameaçador”.
8. Retirar/restaurar equação e compor a matriz ao lado do texto, conservando ordem de leitura separada da apresentação.
9. Salvar, aplicar nova edição ainda não salva, fechar Objetos e usar Salvar projeto na janela principal. Reabrir em outra `Session` e conferir salvo, conteúdo, resultado 80 e desenhos nativos.
10. Comparar integralmente o Documento proprietário com a captura inicial: **idêntico**.

A árvore de widgets é percorrida pelo teste, que recusa tipos WebKit e exige `MathPreview` com desenho produzido. Outro teste examina as dependências reais do executável por `ldd` e recusa WebKit/JavaScriptCore. [Dependências do binário](evidencias-obj01-gtk/ldd.txt), [registro da demonstração](evidencias-obj01-gtk/gtk-final.json), [log](evidencias-obj01-gtk/demonstracao.txt).

A amostra final está em `dados/obj01-gtk-nativo-final/Projeto.inde`, com ambiente separado. O iniciador a preserva se já existir. [Editor de fonte e prévia](evidencias-obj01-gtk/gtk-editor-equacao.png), [documento](evidencias-obj01-gtk/gtk-final.png), [reabertura](evidencias-obj01-gtk/gtk-final-reaberto.png). As capturas foram inspecionadas e mostram controles GTK reais; isso não constitui aceite humano de UX.

## Falhas encontradas e tratamento

- O primeiro teste tentou aceitar o arquivo antes de o seletor GTK concluir a seleção. A prova agora aguarda o `GFile` selecionado antes de emitir a resposta do seletor. [Registro](evidencias-obj01-gtk/seletor-teste-antes.txt).
- O teste encontrou códigos incorretos de posição (`left/top`) onde o domínio exige `start/center/end`, além de avisos de filhos ainda parentados na destruição das linhas. Os códigos foram corrigidos; o widget proporcional foi registrado e libera seus filhos em `dispose`, de forma idempotente. O teste final recusa avisos de filhos restantes e críticas GTK. [Falha anterior](evidencias-obj01-gtk/gtk-antes.txt).
- A prova de reabertura tratava a janela ocultada como ausência de editor antes de executar a etapa de Salvar projeto/reabrir. A sequência foi corrigida, sem alterar dados para contornar a conferência. [Registro anterior](evidencias-obj01-gtk/reabertura-teste-antes.txt).
- A primeira CI deste corte, [45083318525](https://github.com/Roger-Cardoso/inde/actions/runs/45083318525), passou 25 jobs e recusou um ponto e vírgula extra na ponte matemática em GCC 13 com `-Wpedantic -Werror`. A declaração foi corrigida. [Metadados](evidencias-obj01-gtk/ci-primeira.json), [diagnóstico](evidencias-obj01-gtk/ci-primeira-falha.txt).
- A execução intermediária [45083950119](https://github.com/Roger-Cardoso/inde/actions/runs/45083950119) aprovou os 26 jobs após essa correção. A execução final repete a validação com os ajustes finais dos campos acessíveis, ambiente isolado e captura do editor. [Registro intermediário](evidencias-obj01-gtk/ci-intermediaria.json).

No display local foram registrados avisos GTK de mapeamento de teclas `Can't handle >16bit keyvals`. Os marcadores do teste os situam na construção da janela principal, **antes de abrir Objetos**. Uma tentativa de tratá-los como falha geral foi separada da validação do novo componente; os avisos não foram filtrados nem convertidos em aprovação de compatibilidade de teclado. O ambiente final de teste usa o mesmo preparador de ambiente do INDE, com configuração, dados e cache temporários. [Classificação verificável](evidencias-obj01-gtk/avisos.json). GTK com todos os métodos de entrada/teclados não foi aprovado por estes resultados.

## Integridade, proveniência e preservação

Os bancos canônico e auxiliar da amostra final passaram `integrity_check` e `foreign_key_check`. O canônico mantém schema 37 em `schema_migrations` e `user_version=0`; o auxiliar mantém `user_version=5`, snapshot privado V5 e estado salvo igual ao trabalho. [Integridade](evidencias-obj01-gtk/integridade-projeto.json).

Foram conferidos os hashes de **26 arquivos de demonstrações anteriores**, incluindo as amostras matemática e WebKit, sem modificá-las. Os três arquivos compactados originais mantêm os hashes já registrados. Os dez manifestos anteriores e o manifesto do corte WebKit foram verificados nas respectivas revisões: **549 entradas históricas**, sem sobrescrever evidências. [Preservação](evidencias-obj01-gtk/preservados.json), [histórico](evidencias-obj01-gtk/historico.json).

Os bytes importados de MicroTeX/TinyXML2 são preservados, inclusive espaços e terminações de linha originais. `.gitattributes` limita a exceção de whitespace a `cpp/vendor/**`; o código próprio continua formatado e conferido. A auditoria por SHA-256 e o ajuste explícito em `PATCHES.json` permitem verificar as dependências sem reformatar o material original.

Main permanece `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com a alteração preexistente de modo em `Preparar-ponte.sh` preservada. Não se abriram/migraram obras autorais nem se promoveu main. O registro posterior ao código aprovado contém somente documentação/evidências; os bytes executáveis e a configuração de CI permanecem iguais aos da execução final aprovada.

## Limites materiais

A correção de tecnologia não conclui os marcos futuros: o corpo original ainda é editado na Escrita, e alterações posteriores exigem reconciliação com a composição; publicação dos objetos, entidades/nomes alternativos/pseudônimos, serviço comum com Comic, inserção livre inline, equações tipadas em células e numeração/referências automáticas continuam pendentes. Não há arquivo único `.inde`.

A publicação desta branch recusa documentos com composição ainda não traduzida. Aplicativos antigos podem mostrar/publicar somente o corpo canônico e não recebem essa proteção retroativamente. Campos GTK ainda não aplicados não têm promessa de recuperação após queda do processo; o fechamento normal protege esses campos e as operações confirmadas continuam recuperáveis.

Não foram comprovados instalação limpa/AppImage, todos os gráficos/teclados/IME, acessibilidade assistiva completa, desempenho/RSS, concorrência ampla, toda falha física de armazenamento ou sanitizers/LeakSanitizer da interface GTK e do renderizador importado. A matriz de sanitizers remota aprova o motor/fundação no seu escopo, e não substitui esses testes gráficos nem o aceite humano.

Para conferir o pacote, execute `sha256sum -c docs/validacao/evidencias-obj01-gtk/SHA256SUMS` na raiz do worktree.

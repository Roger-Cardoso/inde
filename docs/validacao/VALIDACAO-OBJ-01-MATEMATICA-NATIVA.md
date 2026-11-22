# OBJ-01 — validação de matemática e integração nativa

Data: 2026-11-22. Base `703ba822acd5e7acb256751481d505f7e09c9cf2`; funcionalidade `9336b3d4986360b0e15e4b0299d98101e1130915`; revisão do aplicativo `943e4d8de5a03654ca12a5db0839d66de43364bd`; configuração final de CI `04e391438a604ef3097b7deea9b3200e3d9bac3b`; branch `feature/inde-obj-01-foundation`. [Contrato de integração](../arquitetura/MATEMATICA-NATIVA-OBJ-01.md), [roteiro utilizável](../../tools/obj01-pilot/NATIVO.md), [resultados](evidencias-obj01-matematica-nativa/resultados.json), [manifesto](evidencias-obj01-matematica-nativa/SHA256SUMS).

## Entrega observada

Objeto Equação com fonte LaTeX editável, prévia local, modelos matemáticos, apresentação visual/MathML e participação nas operações comuns do documento. A ação **Objetos** do INDE GTK abre o documento selecionado na Escrita, usa o mesmo motor C++ em processo e guarda o composto dentro do projeto. Não inicia servidor Python/HTTP nem navegador externo. A demonstração está em `dados/obj01-objetos-nativos/Projeto.inde`, separada das anteriores e de obras autorais.

A integração inclui figura, tabela, fórmulas/cartões/hub, composição, tamanho/alinhamento, retirada/restauração, histórico, salvamento e recuperação já implementados. A edição livre do corpo proprietário e sua reconciliação com a composição ainda não estão integradas. Aprovação técnica não significa aceite humano nem release.

## Matriz local

| Verificação | Resultado | Tempo / evidência |
| --- | --- | --- |
| Motor GCC Debug | 22/22 programas | 124,56 s — [log](evidencias-obj01-matematica-nativa/motor-gcc.txt) |
| Motor Clang Debug | 22/22 programas | 146,41 s — [log](evidencias-obj01-matematica-nativa/motor-clang.txt) |
| Motor GCC ASan + UBSan | 22/22 programas | 435,11 s — [log](evidencias-obj01-matematica-nativa/motor-asan.txt) |
| Aplicativo nativo Release / regressão | 62/62 testes CTest | 157,53 s — [log](evidencias-obj01-matematica-nativa/nativo-62.txt) |
| Vínculo nativo + fluxo GTK | 2/2 testes Python | 13,56 s — [log](evidencias-obj01-matematica-nativa/nativo-python.txt) |
| Publicação anterior + nova recusa | 4/4 testes Python | 1,945 s — [log](evidencias-obj01-matematica-nativa/publicacao.txt) |
| Auditor das fontes | 5/5 testes | [log](evidencias-obj01-matematica-nativa/auditor-fontes.txt) |

O programa novo de matemática contém **seis casos de núcleo**, além dos 21 programas anteriores do motor. Há 12 interrupções reais de processo nesse programa: criação/edição/retirada de equação × `after_blobs`, `after_snapshot`, `before_commit`, `after_commit`. A configuração de memória local usa `detect_leaks=0`; não é aprovação de LeakSanitizer na interface GTK. A matriz remota do motor mantém `detect_leaks=1`.

Os 62 testes nativos foram repetidos neste corte, incluindo o novo teste de vínculo. Não são apenas os 61 resultados históricos. A compilação Release do aplicativo foi efetivamente refeita, com avisos tratados como erros. JS/Python/iniciador e diff foram conferidos.

Na primeira execução local dos programas HTTP, o ambiente recusou abrir o socket de loopback; a bateria inteira foi repetida num contexto com esse acesso e passou. Essa tentativa não foi classificada como falha funcional do servidor nem usada como resultado aprovado.

## Matemática no motor e no navegador

O núcleo confere fonte, modo, revisão, identidade e versão explícita; recusa campos extras, tipos/medidas inválidos e revisão obsoleta sem alterar o estado. Preserva fontes incompletas, comandos não suportados e aparência de markup como dados. Move, compõe, estiliza, retira/restaura e desfaz/refaz equações; salva e recupera após SIGKILL, distinguindo o salvo do trabalho mais recente. Os testes de crash conferem atomicidade nos pontos registrados, sem prometer toda pane física.

Na conferência pelo navegador do piloto foram criadas **Equação do segundo grau** e **Transformação**. A fração/raiz e a matriz foram renderizadas com KaTeX e MathML, salvas e reabertas. Uma fonte incompleta exibiu diagnóstico editável; cancelar conservou a expressão anterior. O comando de link externo não gerou elemento de link ou imagem. Sem avisos/erros no console da conferência final.

- [Prévia de matriz](evidencias-obj01-matematica-nativa/matriz-previa.jpg).
- [Diagnóstico de fonte incompleta](evidencias-obj01-matematica-nativa/erro-previa.txt).
- [Documento matemático](evidencias-obj01-matematica-nativa/matematica-final.jpg) e [estado acessível](evidencias-obj01-matematica-nativa/matematica-final.txt).

Esse percurso é do piloto web de desenvolvimento. A prova de integração no aplicativo real é a sessão GTK descrita a seguir; uma captura do navegador não foi apresentada como janela nativa.

## Percurso no INDE real

O teste `--objetos-smoke` cria um projeto schema 37 novo, abre `MainWindow`, seleciona o documento pela Escrita e ativa a ação real `document-objects`. A janela encontrada pertence ao aplicativo GTK; o conteúdo usa `inde://ui/` e o canal nativo, não HTTP.

Percurso final:

1. Abrir edição de equação e tentar fechar tanto Objetos quanto a janela principal. Ambas permanecem abertas com o rascunho; cancelar permite continuar.
2. Importar o PNG sintético, recuperar seus bytes pelo canal nativo e esperar a imagem efetivamente carregar.
3. Criar três equações: segundo grau, somatório e matriz. Retirar/restaurar e desfazer/refazer uma delas, mantendo identidades/fonte.
4. Criar tabela e preencher células. Inserir nela uma fórmula Vida, inicialmente `100 pontos`.
5. Criar e executar um comando separado que altera Vida.HP para 80. Desfazer retorna a 100; refazer fixa 80 e consome o comando. A ocorrência do comando desaparece, com metadados privados conservados.
6. Inserir, retirar e restaurar linha. Aplicar Caderno à tabela; ajustar a equação e colocá-la ao lado do texto.
7. Salvar/reabrir dentro do editor. Conferir três representações KaTeX/MathML, figura, tabela e resultado 80.
8. Fazer nova edição aplicada de equação, ainda não salva; fechar Objetos e usar **Salvar projeto** da janela principal. Reabrir Objetos em outra sessão de `Session`; conferir que o estado está salvo e os conteúdos permanecem.
9. Comparar integralmente o snapshot do Documento proprietário com a captura anterior: **idêntico**.

[Registro do percurso](evidencias-obj01-matematica-nativa/nativo-smoke.json), [log da demonstração final](evidencias-obj01-matematica-nativa/nativo-demonstracao.txt), [janela final](evidencias-obj01-matematica-nativa/nativo-final.png) e [janela reaberta](evidencias-obj01-matematica-nativa/nativo-reaberto.png).

A primeira implementação mostrou uma corrida real: fechar imediatamente após abrir o rascunho podia preceder a chegada da notificação assíncrona. [Falha reproduzida](evidencias-obj01-matematica-nativa/fechamento-antes.txt). O fechamento agora consulta o editor vivo e só esconde a janela após conferir rascunho/operações/estado incerto. O teste final aguarda a resposta de fechamento antes de conferir a proteção e também cobre o fechamento da janela principal.

O teste de vínculo sem interface usa `ProjectService` e `project::Document` reais em um projeto temporário: captura completa, composição dentro do projeto, recuperação, “Salvar cópia”, origem inalterada, versão canônica 37 e recusa quando a origem muda. O construtor antigo do piloto continua recusando caminhos `.inde`. [Log do vínculo](evidencias-obj01-matematica-nativa/vinculo.txt).

No hardware local, o início registrou desativação de aceleração gráfica porque GL estava desabilitado pelo modo de compatibilidade. O fluxo gráfico completou e as capturas foram inspecionadas. Uma tentativa local adicional de tela virtual não iniciou porque `xvfb-run` não está instalado nesse computador; os percursos locais aprovados usaram o display real, e a tela virtual é verificada separadamente na CI. Isso não comprova todos os drivers, dispositivos, foco/IME ou acessibilidade assistiva.

## Formato, publicação e integridade

KaTeX 0.19.1 incluído com integridade do pacote e licença; sem dependência remota para editar. A matemática usa snapshot privado **P2D2-5**, com campos de equações, e o banco auxiliar continua `user_version=5`, sem DDL novo. O banco canônico continua schema **37**, registrado em `schema_migrations`; seu `user_version=0` é esperado. Não houve migração de obras autorais.

O motor V4 foi preservado de um processo anterior e executado contra cópia V5: recusou com `unsupported_snapshot_version`, sem alterar os bytes de origem/banco. [Compatibilidade](evidencias-obj01-matematica-nativa/compatibilidade.json). Os formatos anteriores continuam cobertos pela regressão do motor.

A demonstração nativa final possui três equações, uma figura, uma tabela e a fórmula com resultado 80. Salvo e trabalho conferidos como iguais depois do Salvar projeto. Os bancos canônico/auxiliar passaram `integrity_check` e `foreign_key_check`. [Integridade](evidencias-obj01-matematica-nativa/integridade-projeto.json).

O publicador anterior recusa os documentos selecionados que têm armazenamento de composição por objetos. O teste insere um marcador sintético para demonstrar essa recusa antes da geração, retira somente esse marcador de teste e repete a publicação anterior. Os quatro testes de publicação passaram. A exportação dos objetos novos ainda precisa ser implementada; não foi substituída por exportação silenciosa só do texto.

## Preservação e CI

Os 20 arquivos de origem/banco das dez demonstrações anteriores foram comparados por SHA-256 nesta execução. Os 18 arquivos das nove mais antigas também foram conferidos contra o registro anterior. A última demonstração de estrutura foi comparada ainda com seu estado integral já registrado. [Início](evidencias-obj01-matematica-nativa/preservados-inicio.json), [fim](evidencias-obj01-matematica-nativa/preservados-final.json), [comparação do estado anterior](evidencias-obj01-matematica-nativa/demo-anterior.json).

Os dez manifestos históricos foram conferidos nas revisões correspondentes: **442 entradas**. As evidências antigas não foram sobrescritas. Os três arquivos compactados foram novamente conferidos por hashes, inventários e bytes extraídos: [fontes](evidencias-obj01-matematica-nativa/fontes.json). Main permanece `d41a5c9b9809ca7d40beb6ff10f22f1a5b451e0b`, com a alteração preexistente apenas de modo em `Preparar-ponte.sh` preservada.

A primeira CI, [45073832109](https://github.com/Roger-Cardoso/inde/actions/runs/45073832109), aprovou 25 jobs e recusou a compilação nativa GCC 13 em modo otimizado. O diagnóstico foi reproduzido localmente e tratado por construção direta dos valores JSON no clipboard; a compilação limpa encontrou também a inserção de proporções de coluna. Os avisos continuam habilitados. [Correções de portabilidade](evidencias-obj01-matematica-nativa/portabilidade.json). Os testes dirigidos de copiar/colar e os quatro programas de aparência/estrutura passaram depois desses ajustes. As bases compactadas não foram modificadas; a transformação de clipboard ocorre na cópia de build.

A compilação local limpa com **GCC 13 Release/LTO** concluiu os 269 passos dos alvos solicitados (aplicativo e helper). O vínculo de projeto e o percurso GTK completo também passaram nesse binário. [Build GCC 13](evidencias-obj01-matematica-nativa/gcc13-clean-build.txt), [fluxo gráfico GCC 13](evidencias-obj01-matematica-nativa/gcc13-native-smoke.txt) e [captura](evidencias-obj01-matematica-nativa/nativo-gcc13-reaberto.png). Uma chamada adicional ao CTest dessa pasta encontrou apenas o helper compilado: os outros 61 executáveis não haviam sido construídos ali e ficaram **não executados**, sem constituir aprovação ou falha funcional deles. [Registro da preparação incompleta](evidencias-obj01-matematica-nativa/gcc13-ctest-nao-preparado.txt). A aprovação integral de 62/62 reportada acima é da pasta Release com GCC 15.

A execução [45074910600](https://github.com/Roger-Cardoso/inde/actions/runs/45074910600) foi cancelada quando a compilação local revelou o segundo ponto de portabilidade; não foi contabilizada como aprovação.

A execução intermediária [45076948174](https://github.com/Roger-Cardoso/inde/actions/runs/45076948174) passou a compilação nativa limpa e o teste de vínculo, mas o fluxo GTK foi impedido pelo AppArmor do runner: `bwrap: loopback: Failed RTM_NEWADDR: Operation not permitted`. [Falha gráfica registrada](evidencias-obj01-matematica-nativa/ci-grafico-antes.txt), [metadados](evidencias-obj01-matematica-nativa/ci-intermediaria.json). A configuração de CI passou a instalar/carregar o perfil restrito de Bubblewrap fornecido pelo pacote Ubuntu, com uma prova mínima de namespace/rede antes do build. A política global e o isolamento interno do WebKit continuam ativos; não foi usada a variável que desabilita sua proteção. A alteração se aplica somente aos runners descartáveis, sem mudar a política do computador do usuário. A necessidade de perfil explícito é documentada nas [notas oficiais do Ubuntu 24.04](https://documentation.ubuntu.com/release-notes/24.04/#unprivileged-user-namespace-restrictions).

A execução final **[45077874910](https://github.com/Roger-Cardoso/inde/actions/runs/45077874910)** aprovou **26/26 jobs**, na revisão `04e391438a604ef3097b7deea9b3200e3d9bac3b`, incluindo compilação limpa GCC 13, vínculo real, fluxo GTK e publicação anterior/recusa dos objetos. [Metadados completos](evidencias-obj01-matematica-nativa/ci.json), [log do job nativo](evidencias-obj01-matematica-nativa/ci-nativo.txt). O perfil restrito de Bubblewrap foi carregado e seu teste mínimo passou; o isolamento do WebKit permaneceu ativo. A matriz do motor passou com GCC/Clang e sanitizers, com LeakSanitizer remoto habilitado; isso não aprova LeakSanitizer GTK.

O registro posterior a essa revisão contém somente documentação e evidências, sem alterar código, recursos executáveis ou CI. O manifesto identifica os bytes efetivamente entregues; a aprovação remota acima se refere expressamente à revisão de implementação, sem atribuir uma reexecução aos documentos.

## Limites materiais

- A escrita livre continua no Documento proprietário. Se essa origem mudar depois da composição, a abertura recusa a divergência e preserva os dois estados; ainda falta reconciliação. Esse modo não substitui toda a Escrita.
- A tipografia inline é compacta dentro do objeto. Não há equação tipada entre caracteres do corpo original nem dentro de células, numeração/referências automáticas ou LaTeX completo.
- Vínculos de variáveis com entidades/pseudônimos, serviço comum com Comic e publicação dos objetos continuam pendentes. Não há `.inde` único.
- Executáveis anteriores não apresentam o armazenamento novo de composição e podem mostrar/publicar somente o corpo canônico. A recusa adicionada ao publicador protege esta branch; não retroage aos programas antigos. O motor privado V4, testado separadamente, recusa V5 explicitamente.
- Não foi produzida release nem promovida main. A integração foi implementada e testada nesta branch, usando somente projetos novos de teste/demonstração.
- Não se comprovaram instalação limpa/AppImage, todos os sistemas gráficos/entradas, concorrência ampla, latência/RSS, todas as panes físicas ou aceite humano. GTK com sanitizers/LeakSanitizer não foi aprovado por estes resultados.

Para conferir o pacote: `sha256sum -c docs/validacao/evidencias-obj01-matematica-nativa/SHA256SUMS`, na raiz do worktree. O iniciador de demonstração está no [roteiro nativo](../../tools/obj01-pilot/NATIVO.md).

# Validação atual — PUB-03B / laboratório 0.6

Data: 31/10/2026. Aceite humano pendente. Os testes utilizaram Projetos fictícios novos em `/tmp`; o acervo autoral e os Projetos das demos anteriormente usados pelo usuário não foram convertidos ou operados pelos testes.

## Resultados aprovados

- Compilação Release do aplicativo completo, ponte, testes de Site/Edição, catálogo, importação simples e de Obra e smoke GTK. Logs `evidencias/build-pub03b*.log`.
- **76 testes Python aprovados, sem skips**, incluindo as suítes C++ reais. Registro `evidencias/testes-pub03b.log`.
- Escolha explícita por capítulo; duas fontes revalidadas, inclusive a não escolhida; recusa de fonte/manifesto/plano adulterados, número repetido, contador regressivo, ID incompatível, destino existente e definições conflitantes de um Personagem. Falhas conservam o destino e não deixam uma Obra parcialmente promovida.
- Criação de uma Obra com dois Documentos na ordem declarada; personagem canônico compartilhado; personagens homônimos com IDs distintos permanecem distintos. A Edição guarda mapa público e reservas em transação, com revisão/histórico.
- IDs públicos de Obra/capítulos/personagens/cenas/marcações, pasta anterior, rota atual, alias e números 7/11 preservados na geração. Um capítulo novo criado pelos serviços do INDE recebe 18. Salvar formulário, retirar/restaurar e reabrir conservam mapa e reservas. Operações de publicação não alteram o banco autoral.
- Progresso: execução do script real de leitura sobre estado com IDs anteriores reconhece o capítulo lido. Discussão: execução da função real conserva o identificador e aceita caminho base. Nenhum serviço remoto foi ativado ou contatado.
- Projeção de negrito/itálico/sublinhado e âncoras das cenas; estilos cruzando intervalos e parágrafos conservam texto Unicode. Âncora no separador entre parágrafos permanece navegável. Camadas ainda não suportadas são recusadas.
- Extensão schema 3: fixtures reais de schemas 1 e 2 consultadas sem migração; primeira gravação cria backup íntegro da versão anterior e normaliza os registros em transação. Reservas e banco autoral conservados. Schema futuro e formas incompletas são recusados. O schema autoral continua 28.
- Produções com mapa público usam protocolo versão 2. Rebaixar seu protocolo é recusado; versões antigas do gerador não aceitam essa versão. Produções nativas sem mapa continuam na versão 1.
- GTK real: escolha de fontes por capítulo, criação, abertura do Projeto importado, preparação do Site com IDs/rotas corretos e retorno ao mesmo Documento/buffer/texto/seleção, com hash autoral preservado. O fluxo simples 0.5 permanece funcional. Registro `evidencias/gtk-pub03b.log`.
- Regressão GTK: CRUD, catálogo, rascunhos, salvamento, geração, retorno, seleção e desfazer passaram. Dois cliques na prévia conservaram processo/porta/recibo, com ambiente de desktop restaurado ao abridor simulado. Registro `evidencias/regressao-gtk-pub03b.log`.
- Chrome: biblioteca → Obra → capítulo 7; negrito e locutor exibidos; ponto de vista Mara durante a rolagem; concluir capítulo levou o índice a 50%; endereço anterior redirecionou ao capítulo 11, com a fonte HTML escolhida. A aba e o servidor de teste foram encerrados. Registro `evidencias/navegador-pub03b.txt`; captura `evidencias/leitor-importado-pub03b.png`.
- Os 641 arquivos da fonte INDE e os 24 originais Literature foram conferidos. O checkout INDE aceito permaneceu limpo. A alteração de comentários está somente na cópia incluída e foi registrada na proveniência.
- Pacote fonte 0.6 com manifesto exato; extração, configuração CMake, comparação, plano, importação/reabertura nativas e geração por uma pasta independente conferidos no relatório `evidencias/empacotamento-0.6.json`. Os ZIPs 0.3/0.4/0.5 permanecem preservados.

## Limites

A entrada de Obra usa manifesto explícito do importador e fontes v5 no subconjunto declarado. Não há varredura nem migração integral automática do acervo. Hierarquias, recursos sem tradutor e integração entre Projetos continuam pendentes. Os materiais autorais reais ainda exigem ampliação dos tradutores; nenhum deles foi convertido nesta entrega.

Identificadores estáveis não transferem armazenamento de navegador entre origens, contas ou comentários remotos. Configuração do destino é separada do envio; GitHub e serviços remotos não foram ativados. O pacote é fonte, com dependências do host e runtime não embutido. A pasta independente foi validada no mesmo computador; outro computador físico/SO, sessão prolongada, acessibilidade e todos os pontos de interrupção não foram ensaiados.

A política Cairo original continua ativa; avisos externos de driver/fontes/Chromium não foram declarados resolvidos por estes testes. O diagnóstico do corte anterior permanece aplicável às suas evidências, com a separação de ambiente e reutilização da prévia mantidas.

---

# Histórico — PUB-03A / laboratório 0.5

Data: 31/10/2026. O aceite humano permanece pendente. A verificação usa dados fictícios em `/tmp`; os Projetos e perfis da demo já usados pelo usuário foram preservados.

## Resultados aprovados

- Release: aplicativo completo `inde-especial`, ponte, testes de Site/Edição, catálogo e importação, smoke GTK. Registros `evidencias/build-pub03a*.log`.
- **55 testes Python aprovados, sem skips**, acionando também os serviços C++ e o bloqueio de publicação da Obra importada. Registro `evidencias/testes-pub03a.log`.
- Comparação de HTML e `.narrproj` v5; escolha explícita; texto Unicode; identificação de divergências; recusa de recursos sem tradutor, scripts, múltiplos capítulos, IDs repetidos e alteração das fontes/conferência. O C++ revalida novamente as fontes antes de promover o Projeto.
- Criação e reabertura nativas: texto, três estilos, cenas/âncoras, fala, pensamento, perspectiva e mapa de identidades. Dois personagens com o mesmo nome continuam distintos. Destino existente é recusado, sem alterar seu banco. Fontes brutas e hashes acompanham o novo Projeto.
- Site/Edição podem ser configurados para a nova Obra, mas preparar sua publicação é recusado por identidades públicas pendentes. O teste confere ausência de preparação e preservação do banco autoral.
- Prévia HTTP: mesma URL/processo/recibo ao reabrir, ambiente do desktop restaurado, diagnóstico do abridor preservado, controle removido no encerramento. O navegador foi simulado para comparar exatamente o ambiente recebido.
- Janela GTK real: catálogo, reordenação/salvamento/geração e dois cliques na prévia conservaram PID/porta/recibo. Retorno ao editor conserva Documento, buffer, texto, seleção e desfazer. A revisão GTK criou um Projeto pelos serviços e foi renderizada/inspecionada; a tela usa nomes de recursos em português. Registro `evidencias/gtk-pub03a.log` e captura `evidencias/importacao-pub03a.png`.
- O lançador atualizado, com o override indisponível de IBus removido somente do ambiente nativo, percorreu as três janelas sem o aviso de módulo ausente. A política Cairo original permaneceu ativa.
- Fonte INDE: 641 arquivos conferidos contra o arquivo aceito; checkout original limpo. Literature: os 24 originais e suas cópias correspondem à proveniência. Nenhum desses originais foi editado.
- Um export autoral foi apenas diagnosticado: 82 apontamentos impedem a conversão pelo subconjunto atual. Não houve conversão de texto autoral, escolha automática de backup ou alteração do catálogo original.
- O pacote 0.5 contém fontes e leitor, com manifesto exato; extração, fonte aceita, conferência/plano e criação/reabertura nativa em pasta independente conferidos no relatório externo `evidencias/empacotamento-0.5.json`. Os ZIPs anteriores foram preservados.

## Limites

A importação atual é de um capítulo para uma Obra/Projeto novos. Migração integral, metadados adicionais e continuidade de IDs públicos/URLs/progresso/discussões são PUB-03B. A publicação da Obra importada é bloqueada até a reconciliação.

O navegador real do log do usuário não foi reconfigurado nem seus processos encerrados. Os testes provam a separação de ambiente e reutilização da prévia; não provam desaparecimento de todos os erros de vídeo, Fontconfig, zygote ou mojo. Diagnóstico com fontes e graus de certeza em `DIAGNOSTICO-ABERTURA.md`.

O ZIP não é instalador universal, não embute Python/bibliotecas, e não foi instalado em outro computador físico. Não houve envio GitHub, implantação, teste de todos os pontos de interrupção ou avaliação prolongada/acessibilidade. Capturas e testes não substituem o aceite humano.

---

# Histórico — PUB-02B / laboratório 0.4

Data: 31/10/2026. A avaliação humana desta entrega está pendente; este registro separa a prova técnica dos limites de distribuição.

## Verificações aprovadas

- Compilação Release do aplicativo completo `inde-especial`, ponte, serviço de publicação, suítes nativas e smoke GTK. Nenhum arquivo da fonte aceita foi editado.
- **39 testes Python passaram, sem skips**, incluindo as suítes C++ de Site/Edição e catálogo. Registro: `evidencias/testes-pub02b.log`.
- Seleção explícita e ordenada de Edições do Site; uma Edição por Obra; recusa de vínculo estrangeiro, repetição, revisão obsoleta e retirada em uso. Reordenação e restauração conservam identidades e reservas; restaurar não inclui a Edição automaticamente.
- Preparação conjunta, reordenação, retirada de uma Obra e catálogo vazio. A falha de uma Obra posterior impede criar a nova saída e conserva a saída anterior. O texto de uma Obra excluída e seus arquivos privados não aparecem no ZIP público.
- Personagem canônico compartilhado entre duas Obras sem remapear IDs; definições incompatíveis são recusadas. O gerador recusa combinar fontes de Projetos distintos. Os endereços e as estruturas públicas conservam suas identidades ao mudar a ordem.
- Abrir uma cópia sintética de extensão schema 1 não alterou seus bytes nem criou backup. A primeira gravação criou uma cópia consistente e atualizou para schema 2. Foram conferidos integridade, configuração anterior no backup, preparações e números conservados, e ausência de alteração no banco autoral. Schema futuro e Site incompleto foram recusados sem mudanças adicionais.
- O banco autoral foi comparado por SHA-256 antes/depois dos comandos de catálogo, sem mudanças. Alterar fonte selecionada invalida a preparação; mudanças de texto/cadastro fora da seleção não causam inclusão nem invalidam indevidamente o conteúdo público. Salvar cópia mantém a seleção e invalida a preparação anterior.
- A base congelada continua com **641 arquivos conferidos** e o checkout original permanece limpo. As **24 fontes originais** do Literature continuam iguais à proveniência. O ZIP 0.3 conserva seu hash anterior.

## GTK e navegador

Foram usados apenas Projetos fictícios novos em `/tmp`, com perfis isolados.

O percurso individual PUB-02A foi repetido e passou, incluindo bloqueio sem salvamento implícito, CRUD, geração, retorno, seleção e desfazer. No percurso de catálogo, o teste acionou Subir, constatou que a ordem ainda não tinha sido gravada, tentou fechar, reabriu o mesmo formulário, salvou e gerou duas Obras. O retorno manteve Documento, buffer, texto, seleção e banco autoral. Registros: `evidencias/smoke-individual-pub02b.log` e `evidencias/smoke-catalogo-pub02b.log`.

A primeira asserção de reabertura verificava o estado cedo demais; o driver foi corrigido para esperar a reanexação da aba. O percurso corrigido passou e a conferência final usa uma nova fixture. Não se adotou uma espera fixa como evidência de sucesso.

O Chrome abriu o Site realmente gerado pela sessão GTK. A biblioteca mostrou Observatório e Estação na ordem escolhida, sem Arquivo de bordo. Foram abertos os índices e os capítulos das duas Obras, com fala/pensamento de Mara. O capítulo sem POV não recebeu uma perspectiva inventada. O leitor usa os IDs de Obra/Documento para suas chaves de progresso, preservados pela reordenação.

Capturas: `evidencias/catalogo-gtk-pub02b.png`, `evidencias/biblioteca-multipla-pub02b.png` e `evidencias/leitor-observatorio-pub02b.png`. São componentes reais, não simulação visual. Avaliação prolongada, acessibilidade integral, uso simultâneo em vários processos e outros computadores físicos não foram atestados.

## Limites preservados

O catálogo combina até 100 Obras de um único Projeto; cada Edição aponta para uma Obra e uma estrutura. O subconjunto semântico e estrutural do projetor continua documentado no roteiro. Avaliações de conteúdo e relevância não são inventadas a partir de âncoras. Tradutores adicionais, importação comparada do acervo, envio ao GitHub e instalador com runtime continuam pendentes.

O ZIP 0.4 contém fontes, base congelada e leitor incluído; não contém dados/perfis privados nem é um instalador universal. O relatório externo de empacotamento registra a extração, a verificação e o uso em pasta independente, sem afirmar instalação em outra máquina física. Os logs e capturas deste workspace ficam fora do pacote de fontes.

---

# Histórico — PUB-02A / laboratório 0.3

Data: 31/10/2026. Este registro descreve o corte nativo atual. Os registros PUB-01 e PUB-00 foram preservados abaixo como histórico.

## Prova técnica aprovada

- Compilação Release do aplicativo completo `inde-especial`, da ponte, do serviço de publicação e dos dois percursos GTK. O aplicativo conserva os workspaces reais da base e acrescenta o painel na mesma janela.
- **31 testes Python passaram, sem skips**, incluindo a suíte C++ de persistência/CRUD acionada em fixture sintética. Log: `evidencias/testes-pub02.log`.
- CRUD nativo: criação, consulta, edição com revisão esperada, retirada e restauração de Site/Edição. A revisão antiga é recusada; Site em uso não é retirado; endereço de Edição retirada permanece reservado; restauração conserva identidade, escolhas e números.
- O banco autoral foi comparado por SHA-256 antes/depois das operações de publicação, sem alterações. Reabrir o Projeto conserva os cadastros; consultar antes do primeiro cadastro não cria o banco da extensão.
- Preparações: hashes de entrada/fontes, dupla conferência, armazenamento local e criação exclusiva do arquivo. Adulteração da entrada ou do hash fornecido pelo chamador é recusada. Alterar fonte ou escolhas invalida a preparação anterior.
- Configuração com fonte ausente continua recuperável, mas preparar é recusado até corrigir as referências. Nada é truncado ou convertido silenciosamente.
- `Salvar cópia`: a nova identidade exige adoção da extensão; o destino da cópia volta ao modo Local, com origem/repositório limpos. A configuração da origem permanece independente.
- O mesmo projetor e o leitor Literature incluído produzem o site e o ZIP. Recibo SHA-256 confere todos os arquivos; Documento privado, título privado, bancos, retratos e destino GitHub ficam fora do pacote público.
- `verificar_base.py`: os **641 arquivos** continuam iguais ao arquivo fonte aceito. Os **24 arquivos originais** do Literature registrados na proveniência continuam iguais aos hashes iniciais. O checkout original INDE continua limpo no commit aceito.
- Sintaxe dos scripts shell, módulos Python e JavaScript conferida. Não houve instalação de dependências, push, autenticação ou implantação.

## Percursos GTK e conferência visual

Foram usados Projetos novos em `/tmp`, com perfis isolados e conteúdo fictício. Nenhum teste operou os dados autorais nem os projetos existentes em `dados/`.

1. Painel: preenchimento, salvamento e seleção explícita; botões de retirada/conferência/restauração de Site e Edição; bloqueio de Site em uso; geração assíncrona e recarga dos cadastros.
2. Janela completa: uma alteração no editor permaneceu pendente quando a tentativa de abrir Publicação foi recusada; a comparação do banco provou ausência de salvamento implícito. O teste solicitou o salvamento explicitamente em seguida.
3. Fechar o aplicativo com formulário pendente foi bloqueado. Fechar o painel com esse formulário o reabriu no mesmo estado. A geração ativa também bloqueou o fechamento.
4. A geração terminou na própria sessão. O retorno conservou Documento, objeto do buffer, texto, seleção e estado da vista. O comando de desfazer restaurou o texto anterior. O painel reabriu e o aplicativo encerrou com o formulário limpo.
5. O navegador Chrome exibiu biblioteca → Obra → capítulo, com o caminho `/biblioteca`. Auxílios mostraram “Mara” e “Pensamento · Mara”, a partir das marcações do INDE. A prévia HTTP respondeu 200 nas rotas públicas e 404 para arquivo fictício fora do recibo, tentativa de acesso ao banco e rota fora do caminho base.

Capturas locais: `evidencias/sessao-viva-pub02.png`, `evidencias/editor-retorno-pub02.png`, `evidencias/painel-gtk-pub02.png` e `evidencias/leitor-sessao-pub02.png`. São renderizações dos componentes reais, não simulação visual. As ações GTK foram exercidas por drivers C++ da própria aplicação; o percurso web foi conferido pela interface do navegador. Esses resultados não substituem avaliação humana de uso.

## Limites atuais

Cada geração contém uma Edição de uma Obra. Vários cadastros são persistentes, mas o catálogo combinado de várias Obras permanece PUB-02B. O projetor continua limitado ao subconjunto documentado no roteiro; não equivale a sincronização bidirecional nem a todo o contrato semântico do Literature/INDE. Não se afirma cobertura de acessibilidade, sessão prolongada, uso simultâneo em vários processos ou recuperação após interrupção em todos os pontos.

O ZIP 0.3 contém as fontes, a base congelada compactada e o leitor incluído. Não é instalador universal e não embute Python nem bibliotecas do sistema. A abertura neste workspace usa binário Linux já compilado; outro computador compatível precisa executar o preparo. Empacotamento com runtime, outro SO/hardware, importação do acervo e publicação remota continuam pendentes. O aceite humano da versão especial e uma release pública não foram realizados.

---

# Histórico — PUB-01 / laboratório 0.2

Data: 31/10/2026. O histórico PUB-00 permanece abaixo; seus limites descrevem a entrega anterior.

## Verificações aprovadas no corte atual

- Compilação Release de `inde_web_bridge`, ligada a `inde_core` e `inde_production` da fonte aceita, sem modificar os arquivos dessa fonte.
- `verificar_base.py`: 641 arquivos conferidos byte a byte por hash contra o arquivo Git archive do commit `89b47f27e83d76e1ef61a46cb6c29b8c75c305c4`.
- `python3 -m unittest discover -s laboratorio/inde-publicacao/tests -v`: **27 testes passaram, sem skips no workspace**. São 15 contratos anteriores, seis de gestão/recuperação e seis de integração nativa.
- `node --check interface/interface.js` e compilação sintática dos módulos Python.
- A criação nativa usa `ProjectService`, `WritingService`, `TextSemanticService` e `prepare_work_production`; a atualização reabre a cópia pelos serviços. O adaptador Python não lê nem grava SQLite.
- O Projeto real contém `data/project.sqlite3`; os textos/IDs/revisões/âncoras selecionados correspondem ao retrato. O Documento privado e seu título não aparecem na edição pública.
- A atualização foi comparada por hashes de todos os arquivos da origem antes/depois, sem alterações na origem.
- Recusas verificadas: edição autoral na projeção, atualização direta do original sem marcador de cópia, texto divergente do hash, âncora órfã, tipo semântico sem correspondência, exclusão de recurso em uso, gravação/remoção com revisão obsoleta e restauração adulterada.
- CRUD verificado: Obra e capítulo criados/editados/removidos/restaurados; número removido não reutilizado; recursos criados/editados/removidos/restaurados; Site inteiro removido/restaurado com identidade e conteúdo preservados.

## Conferência visual atual

Usou-se uma pasta de dados fictícios em `/tmp`, separada dos projetos já existentes da demo. No Chrome, foram conferidos:

1. Abertura da amostra, conferência de remoção e bloqueio de excluir Mara, com lista de referências de POV/fala/pensamento.
2. Remoção do capítulo “A porta”, aparecimento da recuperação e restauração com o mesmo número, texto e estado rascunho.
3. Criação de Nara, edição do nome/descrição, salvamento, remoção e restauração da mesma ficha.
4. Criação do Projeto `.inde` pelo botão da ponte; indicação da origem e apresentação do conteúdo autoral como consulta.
5. Atualização do retrato pela cópia estável e geração da edição web.
6. Navegação biblioteca → Obra → capítulo e ativação de fala/pensamento/POV nos Auxílios. O texto exibiu “Mara” e “Pensamento · Mara” associados às marcações do C++.

Evidências: `evidencias/gestao-recuperacao-pub01.jpg`, `evidencias/ponte-nativa-pub01.jpg` e `evidencias/leitor-nativo-pub01.jpg`.

A tentativa inicial no navegador interno ficou presa em uma confirmação JavaScript. A aplicação passou a usar diálogo próprio para conferência/cancelamento, e os percursos acima foram retestados no Chrome. As preferências e abas pessoais do navegador não foram alteradas.

## Limites atuais

A prova nativa cobre os serviços e a persistência de um Projeto sintético. Não constitui integração de controles na sessão GTK viva, importação de projetos autorais ou ensaio de uma sessão longa do editor. Casos sem correspondência são recusados; os limites de tipos e estruturas estão no README e no contrato.

A gestão local JSON tem CRUD inicial; ainda não substitui transações, undo e histórico do INDE. Múltiplas Obras, hierarquias/cenas/notas/marcações e serviços externos continuam pendentes. Não houve envio, autenticação GitHub, publicação externa ou alteração de infraestrutura.

O pacote fonte 0.2 é testado extraído em outra pasta para gerar a edição web. Inclui a fonte C++ aceita compactada e os scripts; o binário local e o build não são enviados como instalação universal. Instalação física em outro computador, runtime incorporado, outros sistemas operacionais e aceite humano do usuário seguem pendentes.

# Histórico PUB-00 — 31/10/2026

## Núcleo funcional

Comando: `python3 -m unittest discover -s laboratorio/inde-publicacao/tests -v`, a partir da raiz Literature. Resultado final: **15 testes passaram**.

Cobertura: site vazio sem conta/domínio; semântica e cena no leitor; preservação do texto Unicode; recusa de marcação obsoleta; rascunhos/dados privados/destino fora do ZIP; links e recursos na raiz e em `/biblioteca`; domínio próprio com CNAME; github.io sem CNAME; salvamento concorrente; identidade de conteúdos; número público permanente; escape de nomes em HTML/JSON; configurações inválidas; hashes públicos; execução de uma cópia do motor/recursos fora do workspace original, com Python em modo isolado.

`node --check interface/interface.js` e compilação sintática dos módulos Python também passaram. Os scripts inline dos modelos foram conferidos pela execução no navegador; o processo de empacotamento confere adicionalmente a integridade dos arquivos da entrega.

## Navegador

Conferência funcional pela interface local em `127.0.0.1`, sem autenticação ou serviços externos:

- Criação de um Site com obra fictícia e alteração de nome/autoria/caminho base.
- Salvamento e geração com links de prévia e pacote público.
- Navegação da biblioteca personalizada para a Obra e o capítulo em `/biblioteca/`.
- Títulos e marca visível com a identidade escolhida.
- Abertura dos Auxílios e ativação da identificação de falante/pensador, com “Mara” e “Pensamento · Mara” no texto.
- Opção de POV disponível e ativável. A faixa de POV depende da posição do trecho na área de leitura; não foi usada como prova de um resolvedor contextual do INDE.
- Na versão final, criação de Site vazio e geração concluída pela interface.
- Na amostra final “Biblioteca Horizonte”, a palavra “estação” abriu o diálogo “Estação das marés”, com definição e link ao universo da Obra.

Capturas: `evidencias/criador-de-site.jpg`, `evidencias/leitor-semantico.jpg` e `evidencias/consulta-verbete.jpg`. São evidências da demo local, sem manuscritos reais. O leitor testado é o componente Literature, alimentado pela nova projeção; não é integração com uma sessão C++.

Os servidores de conferência e suas abas foram encerrados após o teste. O script `scripts/demos/Abrir-demo.sh` inicia uma nova sessão com os projetos conservados.

## Proveniência e preservação

24 arquivos de código/modelos/recursos foram selecionados explicitamente do Literature. Os hashes de origem foram registrados antes das adaptações e reconferidos depois: originais preservados. `PROVENIENCIA.json` distingue hash de origem e cópia adaptada.

O checkout de referência do INDE foi reconferido no commit `89b47f27e83d76e1ef61a46cb6c29b8c75c305c4`, branch `publicacao-2026-10-12`, sem entradas em `git status --short`. Uma cópia Git archive da fonte aceita foi preservada em `fontes/inde-schema28-89b47f27.tar.gz`, com hash em `BASE-INDE.json`. Não houve compilação, migração de banco ou mudança no checkout aceito.

O ZIP transportável seleciona somente código, modelos, testes e documentação deste laboratório. Exclui projetos locais, gerações de uso, credenciais, lixeira, backups e a fonte C++ arquivada. A prova de transporte usa uma cópia em pasta temporária e não atesta outra distribuição Linux, máquina física ou sistema operacional.

## Limites de aceite

Não houve envio ao GitHub, criação de infraestrutura, alteração de domínio ou publicação externa. Não foram testados runtime empacotado, instalador, integração GTK, importação de projetos reais, recuperação de autoria, atualização de versões, sessão prolongada, leitor de tela ou dispositivo móvel.

Os testes aprovam contratos deste corte de laboratório. O aceite humano e a distribuição pública do produto permanecem pendentes.

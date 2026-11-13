# Contrato inicial — INDE com autoria e publicação web

Data: 31/10/2026. Estado atual: laboratório 0.6 / PUB-03B. CRUD Site/Edição, catálogo, sessão GTK e importação comparada de vários capítulos implementados no subconjunto declarado, com identidades públicas separadas dos IDs internos. Aceite humano, migração integral, publicação remota e instalador completo continuam pendentes. As linhas do contrato inicial abaixo são complementadas pelos registros de cada corte.

## Resultado pretendido

Uma pessoa instala o INDE em um computador compatível, cria um Projeto e uma Obra, escreve no Documento, prepara uma edição web com identidade própria, confere o leitor e publica no destino escolhido. O produto fornece os modelos e o leitor; uma instalação prévia do Literature não é requisito.

O Projeto/Documento do INDE será a autoridade de autoria. A produção web é derivada e rastreável. A edição distribuível de referência é INDE 0.1.0/schema 28, commit `89b47f27e83d76e1ef61a46cb6c29b8c75c305c4`. Candidatos posteriores não entram implicitamente na base.

## Autoridade e configuração

1. Projeto: Obras, Documentos, estruturas, entidades, anotações e histórico, sob os serviços do INDE.
2. Site de publicação: identidade, apresentação, Obras incluídas, edições e opções do leitor. Pode ser configurado sem possuir hospedagem.
3. Edição: estrutura escolhida, seleção explícita de Documentos/versões, recursos públicos e política de revelação.
4. Destino: provedor, repositório, branch, origem pública e caminho base. Nenhum destino pessoal predefinido.
5. Computador/perfil: diretórios, dependências e preferências locais.
6. Credenciais: armazenamento privado por conta/serviço; não seguem no Projeto ou no pacote público. Reconexão em outra máquina.

Uma edição gerada não comprova envio; envio não comprova implantação; implantação não comprova sincronização de interações. Data ficcional, estado de desenvolvimento, revisão autoral e disponibilidade pública são independentes.

## Matriz de correspondência e cobertura

| ID | Função Literature | Autoridade prevista no INDE | Estado neste corte / lacuna |
| --- | --- | --- | --- |
| INT-01 | Biblioteca de Obras | Catálogo/Work | Demo com uma Obra por Site; integração e múltiplas Obras pendentes. |
| INT-02 | Ficha da Obra | Work e apresentação pública | Título/sinopse/autoria demonstrados; campos completos e capa própria pendentes. |
| INT-03 | Sagas/volumes/arcos/capítulos | EditorialStructure/StructuralNode | Contrato identificado; demo usa capítulos simples; importação da hierarquia pendente. |
| INT-04 | Texto HTML e autoria .narrproj | Documento único do INDE | Retrato C++ produzido por ProjectService/WritingService/TextSemanticService/WorkProduction e consumido pelo leitor. Sessão GTK e comparação HTML/.narrproj pendentes. |
| INT-05 | Falas/pensamentos | TextAnnotation e campos de entidade | Fala/pensamento simples vindos dos serviços C++ preservam entidade, âncora, ID e intervalo Unicode. Campos adicionais e sobreposições são recusados neste corte. |
| INT-06 | POV | Anotação de perspectiva/âncora | POV de Documento nativo com correspondência explícita de pessoa/focalização/focalizador. Perspectiva por trecho e casos não unívocos continuam pendentes. |
| INT-07 | Personagens | NarrativeEntity/EntityWorkScope | CRUD local de personagens/verbetes e bloqueio por referências implementados. Recursos nativos são consultados na projeção; apelidos e conciliação geral pendentes. |
| INT-08 | Participações editoriais | EditorialEntityReference | Associação sintética da amostra; UI e vínculos por cena pendentes. Não inferir presença física. |
| INT-09 | Guia de cenas | DocumentAnchor + apresentação editorial | Guia funcional da amostra; cena cobre o Documento inteiro. Classificação completa e vínculos pendentes. |
| INT-10 | Notas e verbetes | Documento de referência/entidade, conforme o caso | Verbete da amostra com consulta interativa no texto; notas e vínculos completos pendentes. Separar do dicionário linguístico. |
| INT-11 | Revelações/nomes contextuais | Apresentação pública de entidades e trechos | Modelo ainda pendente. Não exportar segredos como dados apenas escondidos pela interface. |
| INT-12 | Identidades, URLs e ordem | UUID interno + identidade pública persistente | IDs nativos preservados na projeção; CRUD local conserva IDs e reserva números após remoção. Mapa de migração e redirects completos pendentes. |
| INT-13 | Prévia | Retrato consistente de Produção | Leitor real consome o retrato C++; atualização observa cópia estável da origem. Não há controle da sessão GTK viva. |
| INT-14 | Site autossuficiente | Recursos instalados com o INDE | Modelos/motor locais incluídos na demo; runtime/instalador final pendentes. |
| INT-15 | Personalização | Configuração do Site/modelo versionado | Nome, autoria, descrição e três acentos implementados; logo/layout/fontes pendentes. |
| INT-16 | Uso sem domínio | Origem opcional + caminho base | Implementado: local, github.io e subdiretório; verificação em provedor real pendente. |
| INT-17 | Pacote público | Produção derivada com manifesto | ZIP, filtros por seleção pública e hashes implementados. Não é backup. |
| INT-18 | Configuração GitHub | Destino independente do Site | Validação e persistência locais implementadas; conexão/envio pendentes. |
| INT-19 | Primeiro repositório/Pages | Serviço explícito de preparação | Pendente: autenticar, escolher/criar, conferir alterações e instalar workflow público mínimo. |
| INT-20 | Publicar/acompanhar | Revisão imutável/recibo por edição | Gerações locais por revisão; integração com envio/implantação pendente. |
| INT-21 | Comentários/reações | Serviço por Site/identidades públicas | Desativados na demo; múltiplos serviços, moderação e implantação pendentes. |
| INT-22 | Recuperação/concorrência | Transações, revisão, histórico e recuperação do INDE | Revisão obsoleta, gravação atômica, versões anteriores e remoção/restauração locais. Integração com transações, undo e recuperação nativos pendente. |
| INT-23 | Outra máquina | Projeto portátil + reconexão de contas | Teste de cópia independente passou; instalação em outro SO/hardware não verificada. |
| INT-24 | Atualização | Versões de schema, motor e modelos separadas | Versão inicial identificada; migradores e atualização preservando overrides pendentes. |

## Fronteira de integração com C++

O exportador HTML e a produção de Obra atuais do schema 28 omitem semântica/âncoras. A nova produção web precisa de um caminho específico. O JSON da demo não substitui persistência do INDE nem autoriza ler/escrever diretamente no SQLite.

O primeiro adaptador real deverá receber um retrato consistente de Produção obtido pelos serviços do INDE: Projeto/Obra/estrutura; nós e sua ordem; seleção explícita de Documentos e revisões; corpo UTF-8, intervalos Unicode, âncoras e seu estado; anotações tipadas e referências; recursos públicos selecionados. Sua versão de protocolo deverá ser independente do schema SQLite.

A projeção deve informar todo campo/tipo sem equivalência, âncora ambígua/órfã, anotação sobreposta e conteúdo omitido. Não remover marcações silenciosamente nem converter geometria em tempo ficcional. Nomes de campos do Literature precisam de mapa explícito aos campos C++; igualdade de nomes não basta.

Alterações de autoria serão comandos do INDE com revisão esperada. A observação/exportação não salva nem promove rascunhos automaticamente. Gerar congela a seleção; revisar conserva seus hashes; enviar recusa fontes/destino/remoto obsoletos. Falhas mantêm a última edição válida.

Na importação, comparar HTML aplicado e .narrproj salvo, preservar procedência e permitir seleção humana quando divergirem. IDs/nome iguais não fundem entidades. Preservar IDs públicos, números URL, progresso e identidade de discussões, além de registrar remapeamentos internos.

## Sequência de entregas

| Corte | Resultado verificável | Estado |
| --- | --- | --- |
| PUB-00 | Contrato, base registrada, criador do zero, leitor real, ZIP e prova de cópia independente | Implementado neste laboratório, sujeito à avaliação humana. |
| PUB-01 | Retrato de uma Obra sintética produzido pelo C++ schema 28 e consumido pela produção web | Implementado com Projeto sintético salvo e reabertura em cópia; GTK ainda pendente. |
| PUB-02A | Site/Edição persistidos no Projeto e gestão na sessão GTK real | Implementado no laboratório 0.3; aceite humano pendente. |
| PUB-02B | Catálogo com várias Obras, ordem e entidades canônicas compartilhadas | Implementado no laboratório 0.4; avaliação humana pendente. |
| PUB-03A | Comparação de duas fontes e migração de um capítulo para Projeto novo | Implementado no laboratório 0.5; subconjunto explícito e publicação da Obra bloqueada por identidades públicas pendentes. |
| PUB-03B | Vários capítulos planos, mapa público na Edição e projeção de estilos/cenas | Implementado no laboratório 0.6; avaliação humana pendente. |
| PUB-03C | Seleção assistida e tradutores dos recursos autorais ainda não suportados | Pendente. |
| PUB-04 | Primeiro uso do GitHub, preparo de destino, revisão/envio/recibo/implantação | Pendente. |
| PUB-05 | Serviços opcionais por Site, comentários/reações e moderação | Pendente. |
| PUB-06 | Instalador completo, runtime, licenças, migração/recuperação e atualização em máquina limpa | Pendente. |

PUB-01 demonstrou **criação/salvamento pelos serviços do INDE → retrato de Produção → mesmo texto e marcações no leitor**, com Projeto sintético. A atualização reabre uma cópia estável pelos serviços nativos sem gravar a origem. O laboratório 0.3 acrescenta o comando de produção na sessão GTK viva, com a mesma autoridade do editor e sem leitura SQLite pelo Python. O acervo/distribuível e as obras reais continuam preservados.


## CRUD como critério de entrega

O usuário apontou a ausência de CRUD em PUB-00 e autorizou prosseguir mantendo isso em mente. Cada novo cadastro deve ter criação, consulta, edição, remoção recuperável e uma política explícita de vínculos. Não considerar um cadastro completo apenas por permitir criar e exibir.

O laboratório 0.2 implementa esse percurso para Sites, uma Obra por Site, capítulos, personagens e verbetes. Remoções usam conferência interna da aplicação. Recursos em uso são bloqueados com indicação das referências; capítulos removidos conservam seus números reservados; restaurações de capítulo/Obra retornam como rascunho. A remoção do Site guarda sua pasta inteira e a restauração verifica a revisão guardada. Salvamentos guardam a versão anterior.

A gestão local JSON é um laboratório de UX e contratos. Obras projetadas do C++ são consultadas na web; comandos autorais devem usar os serviços do INDE. Múltiplas Obras, cenas, notas, marcações e agrupamentos ainda precisam receber seu CRUD completo na integração GTK. Retirar uma projeção da edição preserva a origem nativa.

## Protocolo PUB-01 e limites efetivos

`cpp/snapshot.cpp` liga `inde_core` e `inde_production` da fonte aceita, sem modificar seus 641 arquivos. `--demo` cria apenas uma pasta nova, com perfil local isolado. Salva um Projeto real, Documentos/anotações e a seleção de Produção; um Documento privado fica fora. O retrato carrega protocolo/versão, base/schema, Projeto/Obra/estrutura, unidades, Documentos, revisões, hashes, âncoras, semântica tipada, entidades selecionadas e omissões locais.

`--snapshot-copia` exige a marca de uma cópia descartável. A aplicação confere a origem antes/depois, recusa arquivos de banco com sessão pendente e vínculos simbólicos e só então aplica uma projeção compatível. As verificações por hash recusam uma origem que mude durante a leitura; não equivalem a captura transacional de uma sessão viva. Fechar o Projeto é pré-condição desta demo.

`ponte_inde.py` não acessa o banco. O mapeamento inicial aceita um Documento por capítulo de nível superior, texto sem formatação, personagens, falas/pensamentos com apenas a referência principal e perspectiva de Documento unívoca. Outros campos/estruturas e estados de âncora sem resolução são recusados; notas privadas de perspectiva ficam no retrato local com diagnóstico. Sobreposições continuam recusadas pelo renderizador. O próximo corte deve ampliar esse contrato sem permitir perdas silenciosas.

A referência distribuível original permanece intacta. O binário da ponte é um artefato local de Release e não uma nova release binária portátil do INDE. O pacote fonte 0.2 inclui a fonte aceita compactada e os scripts de preparação; não inclui projetos usados em testes, perfis locais ou o build do computador.


## Persistência nativa PUB-02A

Na entrega 0.3, a extensão `data/publicacao.sqlite3` foi introduzida na pasta e na identidade do Projeto, com schema próprio 1. Não altera o schema 28 do banco autoral. Site contém identidade pública, tema, origem, caminho base e destino; Edição contém Obra/estrutura, escolhas explícitas, personagens e reservas de números. Ambos têm criação, consulta, edição com revisão esperada, retirada e restauração. A retirada de Site é bloqueada enquanto existirem Edições ativas. Restaurar configuração não publica nem recupera automaticamente uma fonte removida; preparar exige referências válidas.

O painel GTK usa o mesmo `ProjectService` de `MainWindow`. Não há outro editor: a escrita, estruturas, personagens e suas anotações continuam sob os serviços do INDE. O painel abre somente com o Documento sem trabalho pendente de salvamento/IME. Formulários sujos e geração ativa impedem saída e troca de seleção. A geração consome um retrato preparado, não o banco SQLite, e roda fora da thread GTK. Antes de apresentar a saída como atual, verifica revisões, fontes e integridade da entrada. O arquivo preparado é criado de forma exclusiva e conferido por hash.

`Salvar cópia` adota a extensão para a nova identidade e redefine destino para Local, limpando origem e repositório da cópia. A origem permanece independente. Nenhuma conta ou credencial está armazenada no modelo. Preparações anteriores continuam como histórico, sem serem apresentadas como atuais. A prévia serve somente os arquivos declarados no recibo público, em loopback, sem expor a pasta do Projeto.

A base de 641 arquivos fica intocada. A janela especial usa cinco arquivos de sobreposição gerados de forma determinística e código novo da publicação; o programa conserva os demais workspaces da base. Essa técnica preserva a proveniência do laboratório e ainda precisa ser reconciliada com uma futura branch oficial do INDE.


## Catálogo PUB-02B

Site possui uma lista ordenada de IDs de Edição. O autor pode acrescentar, retirar, substituir a versão escolhida ou mudar a ordem; a gravação exige a revisão esperada do Site. Uma Obra pode ter várias Edições cadastradas, mas só uma entra em cada catálogo. Referências a outro Site, Edições repetidas/retiradas e Obras duplicadas são recusadas. Edição em uso não pode ser retirada nem mudar de Obra sem antes sair do catálogo. Restaurar não cria novamente esse vínculo. Um catálogo vazio é um resultado válido e explícito.

O protocolo `inde.web.catalog` v1 guarda Site, seleção ordenada, Edições e retratos nativos. A preparação confere todo o conjunto novamente antes de gravar; ao terminar a geração e ao abrir a prévia, o serviço confere revisões, fontes selecionadas e integridade da entrada. Avisos de omissão ficam registrados no retrato do instante; a ordem desses diagnósticos sobre material não selecionado não compõe a revisão do conteúdo público. A preparação individual e seu protocolo anterior são preservados.

O gerador usa pastas derivadas da identidade da Obra para seus recursos internos. As rotas públicas seguem os endereços das Edições e seus números reservados, independentemente da ordem do catálogo. A saída combinada não reaproveita a pasta genérica interna `obra/` da produção individual; os endereços públicos canônicos são conservados. Personagens compartilhados conservam o ID nativo. O motor aceita repetição entre Obras somente quando tipo e definição pública são coerentes, recusando colisões com identidades estruturais ou definições divergentes. Nenhuma fusão é inferida pelo nome.

A extensão schema 1 é apenas lida ao abrir. A primeira gravação cria uma cópia consistente SQLite, confere integridade e atualiza a extensão em transação para schema 2. A migração acrescenta o catálogo vazio às configurações e uma tabela própria para preparações combinadas; conserva os cadastros, revisões anteriores e retratos individuais. Formatos futuros ou registros incompatíveis são recusados. O banco autoral e a fonte aceita permanecem inalterados.

O catálogo amplia a composição da biblioteca, mantendo o subconjunto semântico atual. Importação comparada, tradutores adicionais de marcações/hierarquia, publicação GitHub e instalador com runtime continuam como frentes distintas.

## Importação comparada PUB-03A

A escolha explícita de HTML aplicado ou autoria salva só é executada após revalidação das duas fontes. A conversão cria um Projeto novo em área reservada, usa os serviços nativos e promove a pasta com criação exclusiva. Não mescla nomes nem sobrescreve destinos existentes. Preserva cópias, hashes, escolha e remapeamentos; recursos sem correspondência impedem a conversão. O escopo e a demo estão em `ROTEIRO-IMPORTACAO.md`.

IDs públicos anteriores ainda não foram reconciliados com os novos IDs internos. O serviço nativo impede preparar publicação da Obra importada enquanto essa pendência existir. Não se afirma migração integral, progresso preservado ou continuidade de discussões nesta etapa.

Abertura da prévia: processo reutilizado por geração, verificação local de identidade/recibo e ambiente do navegador restaurado a partir do desktop. Logs de navegador ficam fora dos arquivos publicados. Diagnóstico, evidência e limites em `DIAGNOSTICO-ABERTURA.md`.

## Obra com identidades públicas PUB-03B

O manifesto explícito declara Obra, pasta anterior, rotas/aliases, números/reservas e pares de fontes por capítulo. Cada escolha é conferida novamente no Python e no C++; nenhuma versão é inferida de timestamps. Conflitos de definição para o mesmo ID impedem o conjunto inteiro. IDs distintos com nomes iguais permanecem distintos. O resultado é um Projeto novo, com Site/Edição configurados para destino Local.

A Edição schema 3 guarda o mapa validado em transação, com histórico e revisão. O formulário conserva esse mapa, e as reservas se estendem a capítulos novos. Retirada/restauração mantém identidades; a autoria continua sob os serviços do INDE. Produções vinculadas usam protocolo 2 e geradores antigos devem recusá-las. A migração da extensão 1/2 cria backup antes de gravar e não altera o schema autoral.

A continuidade dos identificadores não equivale a migração de dados entre origens do navegador ou de serviços remotos. Comentários/reações continuam desativados. A matriz inicial e as seções históricas acima precisam ser lidas com os complementos de cada corte. Escopo, formato e demo atuais em `ROTEIRO-OBRA-IMPORTADA.md`.

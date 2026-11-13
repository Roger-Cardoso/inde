# INDE · Criação e publicação · 0.10

INT-01 integra a base autoral schema 37 do candidato P03c com Publicação, catálogo,
importação e o espaço Comic do especial. O perfil de otimização OPT-CXX-01 da 0.9
é aplicado à compilação ampliada. Compilação estrita, 47 testes CTest, 87 testes
Python, migração sintética e percursos GTK passaram. O fechamento e seus limites
estão em [VALIDACAO-INTEGRACAO01.md](VALIDACAO-INTEGRACAO01.md).

```sh
bash Abrir-demo-integrada.sh
```

A amostra contém alternativas no mesmo Documento, perfil de trabalho, vínculo
textual e reuso de um acontecimento em duas composições. O
[roteiro integrado](ROTEIRO-INTEGRACAO.md) cobre planejamento visual, editor e
publicação. As demos anteriores continuam disponíveis em seus lançadores.

A autoria usa schema 37; Publicação continua schema 3 e Comic, schema 1. A base 28
original e a 0.9 permanecem preservadas. Abrir uma cópia de Projeto antigo permite
sua migração; o Projeto migrado exige o aplicativo novo. Conserve a cópia anterior
para continuar usando a 0.9. O pacote público não substitui o Projeto completo.

A publicação seleciona a redação corrente integral. Perfis de visibilidade não
retiram conteúdo da Edição; alternativas encobertas continuam privadas. O painel
da Edição explica essa regra. O mesmo editor conserva seleção, histórico,
parágrafos e marcações ao trocar instrumentos ou perfis. Em Estrutura, criar e
gerir alternativas têm acesso direto, além dos caminhos contextuais.

Também foi incorporada a correção da fila de revisão: filtrar antes de completar
as páginas de resultados, consultando lotes limitados e permitindo cancelamento.
Pendências ficam antes de itens já decididos, sem apagar seu histórico.

O [contrato de integração](INTEGRACAO-01.md) delimita o aproveitamento. Layouts
próprios completos, nomenclatura final das alternativas e integração compartilhada
P04 continuam pendentes; o P03c não é apresentado como resolução desses trabalhos.
As direções de estados globais, economia, plantas e cartografia ampliada continuam
estudos próprios. Não há envio remoto, instalador autossuficiente ou aceite humano
presumido. Os relatos 0.9 e anteriores abaixo são históricos.

---

# Histórico preservado · 0.9

OPT-CXX-01: o INDE usa compilação estrita C++20 com `-Wall -Wextra -Wpedantic -Wconversion -Werror`, `-O3` e LTO em Release. Inclui reduções de alocações e cópias na busca, corretor, produção e Comic. O [guia de otimização](OTIMIZACAO-CXX.md) explica as alterações, a compilação e o fluxo opcional de PGO. A [validação](VALIDACAO-OPT-CXX-01.md) distingue os testes e as medições de serviços isolados do desempenho da interface.

COMIC-02, 02/11/2026: a produção de comics passa a ocupar um **espaço de trabalho nativo**, acessível por **Criar → Comic**. A barra principal fica **Criar, Planejar, Revisar, Manuscrito, Cronologia, Mapas e Obras**. Criar oferece Escrita e Comic.

```sh
bash Abrir-demo-comic.sh
```

A demonstração abre diretamente em Comic. O espaço tem biblioteca e sequência à esquerda, composição central com zoom e redimensionamento, e inspetor de Elemento/Quadro/Obra. Desfazer/Refazer pertence à composição. Alternar de espaço conserva o rascunho; o fechamento/troca do Projeto protege alterações pendentes.

A configuração e a geração de publicação existentes são aproveitadas. O editor foi retirado do painel geral de Publicação. Os modelos persistentes continuam compatíveis com os comics da versão 0.7.

O [roteiro](ROTEIRO-COMIC.md) explica o uso. O [contrato COMIC-02](CONTRATO-COMIC-02.md) define a arquitetura atual, o que foi integrado e os recursos restantes para equivalência integral com o estúdio 0.4. A [validação](VALIDACAO-COMIC-02.md) registra os testes e limites.

Modelo, persistência, edição e geração do HTML são C++20/GTK4. O leitor usa HTML/CSS/JavaScript. Materialização, ZIP e prévia reutilizam o mecanismo Python já existente. O banco autoral permanece schema 28, a publicação schema 3 e comics schema 1. O estúdio Python 0.4 não é executado pelo INDE.

## Base de publicação preservada · PUB-03B / 0.6

Entrega PUB-03B, 31/10/2026: importação de vários capítulos comparados para uma nova Obra/Projeto, com Site/Edição e correspondência entre IDs nativos e públicos. O leitor mantém pasta anterior, rotas/aliases, IDs de progresso/discussão e reservas de URL. O schema autoral continua 28, com 641 arquivos preservados; a extensão de publicação usa schema 3, com leitura dos schemas 1/2 e backup antes da primeira gravação.

## Experimentar a Obra importada

```sh
bash Abrir-demo-obra-importada.sh
```

Compare e escolha a fonte de cada capítulo, informe destino novo e use **Criar Projeto e Edição → Abrir no INDE**. No aplicativo completo, use **Publicação → Catálogo do Site → Preparar Site → Abrir prévia**. A demo contém dois capítulos fictícios, com URLs 7 e 11 e próxima reserva em 18; a mesma Personagem nativa é compartilhada entre eles. A geração continua derivada dos Documentos do INDE.

O [roteiro da Obra importada](ROTEIRO-OBRA-IMPORTADA.md) explica a demo, o manifesto explícito de entrada e o comando `Revisar-obra.sh`. O mapa pertence à Edição e não troca os IDs internos do INDE. Progresso/posição armazenados anteriormente no navegador também dependem da mesma origem; mudar domínio/porta não transfere esses dados automaticamente. Identificadores de discussão são preservados; serviços remotos permanecem desativados.

Este corte aceita capítulos planos e o subconjunto semântico declarado. O acervo autoral existente não foi convertido; seleção assistida, hierarquias e tradutores adicionais continuam pendentes. Produções vinculadas usam protocolo versão 2 para impedir que um gerador anterior ignore seus IDs públicos.

## Experimentar a importação comparada

```sh
bash Abrir-demo-importacao.sh
```

A janela GTK mostra duas fontes fictícias divergentes e seus recursos. Escolha explicitamente a origem, informe a nova Obra/destino e use **Criar Projeto importado → Abrir no INDE**. O [roteiro da importação](ROTEIRO-IMPORTACAO.md) documenta o subconjunto aceito e o comando para conferir um par de arquivos escolhido. Fontes com recursos sem tradutor são bloqueadas; nada foi importado automaticamente do acervo autoral.

O fluxo simples do PUB-03A cria uma Obra de um capítulo e registra os remapeamentos, sem contrato público. Para múltiplos capítulos e identidades públicas, use a demo do PUB-03B acima. Preparar publicação da Obra importada é bloqueado até essa reconciliação. Essa limitação não afeta as Obras nativas já usadas nas demos.

## Correções na abertura da prévia

A mesma geração reutiliza servidor e porta; uma nova geração pode usar outro endereço. O navegador recebe o ambiente original do desktop, separado do perfil isolado do INDE. O GTK escolhe seu método de entrada quando o override IBus não encontra módulo externo. Os diagnósticos do navegador são preservados em arquivo privado ao lado da geração.

Feche a janela antiga e reabra pelo lançador para carregar estas correções. A mensagem de compatibilidade Cairo é esperada nesta GPU. Os avisos de vídeo, Fontconfig e processos Chromium foram classificados com seus limites no [diagnóstico da abertura](DIAGNOSTICO-ABERTURA.md); não se afirma que todos tenham desaparecido.

## Experimentar o catálogo múltiplo

```sh
bash Abrir-demo-catalogo.sh
```

A amostra própria contém três Obras fictícias e publica somente as duas escolhidas. Abra **Publicação → Catálogo do Site**, use **Subir/Descer**, **Salvar catálogo**, **Preparar Site** e **Abrir prévia**. O [roteiro do catálogo](ROTEIRO-CATALOGO.md) também cobre exclusão da seleção, biblioteca vazia, recuperação e abertura de projetos da versão anterior.

Uma Edição pode ser retirada depois de removida do catálogo. Restaurá-la conserva sua identidade e reservas, sem reincluí-la automaticamente. Reordenar Obras conserva os endereços públicos e o progresso associado às identidades. O mesmo personagem nativo pode aparecer em várias Obras com a mesma identidade; definições incompatíveis para esse ID são recusadas. Fontes de Projetos diferentes não são fundidas pelo gerador.

O preparo guarda o conjunto escolhido e suas revisões; a saída inteira só é promovida depois de todas as Obras passarem. Falha em uma delas conserva as pastas anteriores. Gerar não autentica nem envia ao GitHub. O pacote de fontes atual leva fontes e leitor; o binário Linux deste workspace está pronto, enquanto outro computador compatível exige o preparo e suas dependências.

## Abrir o INDE especial

```sh
bash Abrir-INDE-especial.sh
```

O workspace já contém o binário Linux compilado. A primeira abertura cria somente a amostra sintética em `demo-nativa/`, com perfil isolado. Use **Publicação** no cabeçalho para criar Site e Edição. O [roteiro GTK](ROTEIRO-GTK.md) explica cadastro, seleção, geração, prévia, retirada e restauração. Para começar com um Projeto vazio, use `bash Abrir-INDE-especial.sh --vazio`.

Em outra instalação compatível, `bash Preparar-INDE-especial.sh` verifica a fonte congelada e compila o aplicativo especial, a ponte e os testes. O script não instala dependências nem usa a rede. Python e bibliotecas nativas ainda precisam estar presentes. A aplicação web de leitura está incluída; não é necessário possuir outro checkout do Literature.

Cada Edição aponta para uma Obra, uma estrutura e uma seleção explícita de Documentos e personagens. A aba Catálogo do Site escolhe uma Edição de cada Obra e sua ordem, para produzir uma biblioteca combinada. Revisões impedem gravar por cima de alterações externas. Retiradas são recuperáveis e não removem texto autoral. Preparações guardam as fontes usadas; a geração confere novamente origem e entrada ao terminar e antes de abrir a prévia. **Salvar cópia** redefine o destino da cópia para Local.

O painel não abre sobre texto pendente e não pede ao editor que salve implicitamente. Voltar ao INDE mantém o editor existente. Os dados de publicação ficam em `data/publicacao.sqlite3`, dentro do `.inde`; os Documentos permanecem sob os serviços autorais do INDE. A observação da configuração não cria o banco da extensão.

## Interface web do laboratório anterior

```sh
bash Abrir-demo.sh
```

O comando abre o navegador no servidor local. Pode ser chamado pelo caminho completo de qualquer pasta. Requer Python 3.12 ou superior. Os projetos existentes da demo continuam em `dados/`; a abertura não os reescreve. O botão **Criar demo pelo núcleo INDE** está disponível quando a ponte foi compilada.

Para escolher porta e diretório de dados:

```sh
bash Abrir-demo.sh --sem-navegador --porta 8770 --dados /caminho/para/dados
```

Somente loopback `127.0.0.1` é atendido. Cada prévia serve exclusivamente sua edição pública. Encerre com Ctrl+C. Use um único servidor por diretório de dados.

## Roteiro: gestão e recuperação

1. Crie um Site vazio ou use **Experimentar obra fictícia**. A amostra Python permite editar o conteúdo da demo.
2. Edite identidade, Obra, capítulos ou fichas e use **Salvar projeto** antes de outra operação.
3. Em **Personagens e verbetes**, crie um recurso, altere nome/descrição e salve. Sua identidade permanece a mesma.
4. **Remover recurso com recuperação** mostra uma conferência. Recursos referenciados em falas, pensamentos, POV ou glossário têm a remoção bloqueada com indicação dos usos.
5. Remova um capítulo sem apagar o texto definitivamente. A opção **Restaurar capitulo** reaparece abaixo da Obra. O capítulo restaurado mantém ID, texto e número, voltando como rascunho.
6. Novos capítulos recebem o próximo número público reservado. A remoção não libera o número antigo para reutilização.
7. **Retirar Obra com recuperação** conserva sua estrutura e seus recursos. A restauração exige o espaço da Obra livre e volta como rascunho.
8. **Remover Site com recuperação** guarda a pasta completa. **Restaurar Site**, na lateral, conserva sua identidade e retorna com a Obra fora da publicação para conferência.
9. Gere novamente para atualizar a edição local. Essas operações não alteram hospedagens remotas nem cópias já baixadas do site.

O CRUD entregue cobre Sites, uma Obra por Site, capítulos, personagens e verbetes. Cada salvamento com mudanças também guarda a versão anterior em `historico/`. Essa cópia não é um sistema completo de backup, undo ou restauração seletiva do histórico. Recursos presentes somente em capítulos removidos podem ser retirados; restaure primeiro os recursos necessários antes de restaurar o capítulo.

## Roteiro: Documento INDE até o leitor

1. Use **Criar demo pelo núcleo INDE**. O executável C++ cria um Projeto real `.inde`, uma Obra, dois capítulos, Documentos e uma entidade, usando os serviços da base aceita. Um Documento privado é deixado fora da seleção de produção.
2. A tela identifica a origem e exibe o caminho de `Publicacao.inde`. Texto e recursos dessa edição ficam para consulta: sua autoridade é o INDE. A identidade do Site continua configurável.
3. **Gerar e conferir site** produz biblioteca, Obra e leitor com o conteúdo selecionado pelo C++. Abra o primeiro capítulo e ative fala/pensamento/POV nos **Auxílios**.
4. Se desejar editar a origem, abra o Projeto indicado no INDE instalado. Use uma cópia apenas sintética. Feche o Projeto antes de **Atualizar retrato da origem**.
5. A atualização confere hashes da origem, cria uma cópia temporária, abre a cópia pelos serviços C++, prepara a seleção de Produção e projeta o novo retrato. A origem é conferida novamente e não é gravada pela observação.
6. A seleção de capítulos/Documentos deste corte está em `origem-inde/selecao.json`. A demo não oferece edição arbitrária dessa seleção. IDs e números existentes da edição são preservados na atualização.

O protocolo `inde.web.snapshot` v1 é independente do schema SQLite. O adaptador Python não acessa o SQLite. Ele recebe o retrato tipado e verifica hashes de texto, referências, estados de âncora e correspondências semânticas. Esse fluxo web anterior continua disponível; o fluxo GTK atual usa a sessão real, conforme o roteiro nativo.

O corte nativo suporta capítulos de nível superior com um Documento cada, texto sem formatação, personagens, fala e pensamento simples, e uma perspectiva de Documento com focalizador inequívoco. Mantém a âncora de cena que cobre o Documento inteiro. Formatação, campos semânticos adicionais, relações, âncoras ambíguas/órfãs, sobreposições e outras estruturas sem correspondência são recusadas ou diagnosticadas conforme o contrato; não são convertidas silenciosamente.

O exportador C++ continua separado do editor distribuível: não adicionamos ainda um botão ao GTK nem alteramos o schema do Projeto. A prova deste corte é a cadeia pelos serviços nativos e pelo leitor real, conferida também no navegador.

## Site e destino

Nome, autoria, apresentação e acento são próprios de cada Site. São oferecidos três acentos para biblioteca/Obra; as preferências de conforto ficam com o leitor. Logotipo customizado, outros layouts e fontes ainda estão pendentes.

A origem pública é opcional no modo local. `https://autora.github.io` com caminho `/biblioteca` gera links de subdiretório sem CNAME; um domínio próprio gera CNAME. **Preparar GitHub Pages** conserva e valida destino e branch; não autentica, cria repositório ou envia nesta entrega. Comentários e reações ficam desativados.

O ZIP público contém apenas a edição gerada. Projetos `.inde`, retratos, fontes privadas, lixeira, histórico e destino GitHub ficam fora. Conteúdo já publicado ou baixado não é revogado por uma remoção local. A data de capítulo mostrada nesta demo é a data da preparação da edição, sem atestar implantação remota.

## Transportar a demo e preparar a ponte

A edição web do laboratório funciona com Python padrão. O pacote de fontes 0.8 leva também o código da ponte e a fonte aceita compactada; o binário compilado neste computador não é incluído como instalador universal.

Para habilitar a etapa C++ em outra instalação compatível:

```sh
bash Preparar-ponte.sh
```

A compilação exige as dependências da base INDE: CMake 3.22+, Ninja, compilador C++20, pkg-config, GTKmm 4.8+, GLib, UUID, SQLite com Session/Preupdate, Pango/Cairo e ICU 74+. O script não instala dependências nem usa a rede. Ele extrai a fonte aceita se necessário, verifica seus hashes e compila apenas a ponte e bibliotecas necessárias.

Neste workspace a ponte já foi compilada em Release. Ainda não foram validados outro SO, outra máquina física, instalador com runtime embutido ou uma sessão GTK prolongada. A base atual é Linux x86_64.

```sh
python3 -m unittest discover -s tests -v
python3 empacotar.py /pasta/nova/INDE-Criacao-0.9-fontes.zip
```

Na conferência COMIC-01 deste workspace passaram 86 testes, incluindo 10 novos casos de comics que executam 39 verificações do núcleo C++. O percurso nativo anterior de persistência/CRUD e a projeção com reservas públicas continuam cobertos. Também há smoke GTK dos formulários e da geração; o relatório separa sua execução da avaliação humana. Quando a ponte não estiver compilada, os testes nativos são explicitamente pulados; isso não equivale à validação nativa. Não copie `dados/` para um repositório público. Para mudar de diretório/computador, feche o processo e copie a pasta de dados completa; não há credenciais nela nesta entrega.

## Escopo restante

Ainda faltam CRUD completo da hierarquia/cenas/notas/marcações, ampliação da gestão GTK, importação comparada do Literature/.narrproj, seleção geral de versões e estruturas, serviços externos por Site, autenticação/envio GitHub e instalador final.

A amostra Python mantém texto simples e intervalos Unicode não sobrepostos. Se o trecho marcado mudar, salvar/gerar é recusado até remover/refazer suas marcações. A UI permite remover as marcações; sua criação e reancoragem completas serão integradas aos serviços do INDE. A ficha de personagem ou verbete não cria automaticamente uma presença, um fato ou uma marcação no texto.

Os contratos iniciais de configuração e a matriz estão em `CONTRATO-INTEGRACAO.md`. `VALIDACAO.md` separa prova automatizada, conferência visual e limites. `PROVENIENCIA.json` registra as 24 fontes/recursos selecionados do Literature. `BASE-INDE.json` e `verificar_base.py` identificam e conferem a fonte C++ aceita. Os ZIPs 0.1, 0.2 e 0.3 anteriores continuam preservados em `entregas/` como histórico.

Esta é uma entrega de laboratório; revisão humana, revisão das licenças/componentes e distribuição pública do produto final permanecem separadas.

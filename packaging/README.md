# Distribuição AppImage

Esta pasta contém a receita oficial do INDE para produzir um AppImage Linux
`x86_64`. A saída é um artefato portátil; ela não substitui o código-fonte nem
altera o formato dos projetos `.inde`.

## O que entra no pacote

O processo parte do `cmake --install` e monta um AppDir com:

- binário Release do INDE;
- bibliotecas GTK 4/gtkmm e recursos descobertos pelo plugin GTK;
- SQLite e demais bibliotecas não consideradas parte da base do sistema;
- uma compilação relocável e verificada do Enchant 2, o provedor Hunspell,
  Hunspell e o dicionário `pt_BR`;
- arquivo desktop, ícone 256×256, definição MIME e metadados AppStream;
- licença do INDE, avisos de terceiros e textos de licença encontrados pela
  ferramenta de empacotamento.

`glibc`, `libstdc++` e algumas bibliotecas fundamentais do sistema não são
incorporadas, conforme a política do linuxdeploy. Por isso, a distribuição de
referência é construída dentro de Debian 12, que oferece gtkmm 4.8 e uma linha
de base glibc 2.36: compilar numa distribuição mais nova pode gerar um artefato
que não inicia em sistemas mais antigos.

## Dependências de construção

Em Debian 12 ou derivado:

```bash
sudo apt-get update
sudo apt-get install --yes --no-install-recommends \
  appstream \
  build-essential \
  ca-certificates \
  cmake \
  curl \
  desktop-file-utils \
  file \
  gobject-introspection \
  hunspell-pt-br \
  imagemagick \
  libenchant-2-2 \
  libglib2.0-dev \
  libgirepository1.0-dev \
  libgtkmm-4.0-dev \
  libhunspell-dev \
  librsvg2-dev \
  libsqlite3-dev \
  ninja-build \
  pkg-config \
  uuid-dev
```

## Construção local

Na raiz do repositório:

```bash
packaging/build-appimage.sh
```

A receita:

1. valida o arquivo desktop e os metadados AppStream;
2. baixa linuxdeploy, o plugin GTK, o runtime AppImage e o código-fonte do
   Enchant fixados em `appimage-tools.env`, sempre verificando SHA-256;
3. configura e compila Release com testes habilitados;
4. executa `ctest`;
5. recompila Enchant em modo relocável, monta o AppDir e verifica o corretor
   empacotado com o benchmark de revisão, inclusive contra uso acidental do
   provedor instalado no host;
6. gera o AppImage e valida seu runtime;
7. grava em `dist/` o AppImage e um checksum portátil, baseado apenas no nome
   do arquivo.

Diretórios padrão:

```text
build-appimage/   árvore de build, ferramentas verificadas e AppDir
dist/             AppImage e SHA-256 finais
```

Eles são ignorados pelo Git. Os caminhos e o paralelismo podem ser alterados
sem modificar a receita:

```bash
INDE_APPIMAGE_BUILD_DIR=/tmp/inde-build \
INDE_APPIMAGE_OUTPUT_DIR=/tmp/inde-dist \
INDE_APPIMAGE_TOOLS_DIR=/tmp/inde-tools \
INDE_APPIMAGE_JOBS=2 \
packaging/build-appimage.sh
```

Não troque um checksum apenas para fazer um download novo passar. Primeiro
confirme a versão, a origem, o changelog e o digest publicado pelo projeto da
ferramenta; depois atualize URL e SHA em conjunto.

## Automação no GitHub

O workflow `.github/workflows/appimage.yml` roda numa máquina Ubuntu 24.04,
mas executa o build dentro do contêiner `debian:12-slim`. Isso fornece gtkmm 4.8
e mantém a linha de base glibc 2.36, sem depender de um runner Bookworm nativo.

Há duas formas de acioná-lo:

- manualmente em **Actions → AppImage → Run workflow**, produzindo somente um
  artefato de CI;
- por uma tag `v*`, por exemplo `v0.1.0`, produzindo o artefato e anexando-o à
  Release de mesma tag. Se a Release ainda não existir, o workflow a cria com
  notas geradas pelo GitHub.

O push da tag é, portanto, a ação explícita de publicação. Antes dele, atualize
a versão em `CMakeLists.txt` e a entrada correspondente em
`io.github.inde.appdata.xml`.

## Validação antes de publicar

A automação comprova:

- build Release e suíte central;
- validade dos metadados desktop/AppStream;
- presença operacional do Enchant/Hunspell e do dicionário pt-BR dentro do
  AppDir;
- estrutura e checksum do AppImage;
- abertura GTK do Atlas Integral por tempo limitado em Xvfb.

Esses itens são evidência técnica, não aceite humano. Antes de tratar uma versão
como release aprovada, copie o AppImage para pelo menos uma instalação limpa de
Linux compatível e revise:

- abertura sem dependências de desenvolvimento instaladas;
- Projeto vazio e uma cópia do Atlas Integral;
- digitação e colagem com acentos, foco, Tab e atalhos;
- rolagem, legibilidade, abertura/salvamento, autosave e retomada;
- revisão ortográfica de palavras corretas e incorretas em pt-BR;
- abertura por duplo clique/associação MIME quando houver integração de desktop;
- execução com `APPIMAGE_EXTRACT_AND_RUN=1` em sistema sem FUSE.

Registre separadamente sistema, versão, arquitetura, resultado e ressalvas. Uma
janela permanecer aberta até o `timeout` demonstra inicialização; não demonstra
que a experiência de escrita foi aceita.

## Limites atuais

- A receita produz somente `x86_64`; ARM64 exige ferramenta, dependências,
  workflow e validação próprios.
- AppImage não fornece atualização automática nesta primeira versão.
- A integração com menus e MIME depende do ambiente desktop ou de uma ferramenta
  de integração instalada pelo usuário; o arquivo continua executável sozinho.
- O artefato local reflete a base ABI da máquina que o construiu. Para candidato
  de distribuição, use o artefato produzido pelo workflow de referência.

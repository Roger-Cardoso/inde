# INDE — Integrated Narrative Development Environment

O **INDE** é um aplicativo desktop nativo para Linux voltado ao desenvolvimento, planejamento, organização e escrita de projetos narrativos.

O programa reúne em um mesmo ambiente informações editoriais, elementos narrativos, documentos, relações e referências temporais, mantendo os dados do projeto armazenados localmente.

O INDE está atualmente em desenvolvimento.

## Principais recursos

A versão atual inclui:

* criação, abertura, salvamento e cópia de projetos `.inde`;
* catálogo de Propriedades Intelectuais e Obras;
* estrutura editorial hierárquica;
* gerenciamento de entidades narrativas;
* tipos de entidades personalizados;
* relações entre entidades;
* planejamento temporal ficcional;
* acontecimentos e participantes;
* presença e localização de entidades;
* vínculos entre entidades narrativas e estruturas editoriais;
* estruturas narrativas e modelos associados a Obras;
* Biblioteca de Documentos;
* editor de texto formatado;
* âncoras e referências entre documentos e entidades;
* pesquisa e filtros contextuais;
* timeline ficcional consultiva;
* Cartografia: planetas procedurais locais, prévia/aceite, câmera e camadas;
* edição de relevo com prévia, aplicação, undo/redo e bloqueios autorais;
* posições de Locais compartilhados, medição esférica e exportação SVG da vista;
* revisão ortográfica pt-BR;
* dicionários pessoais;
* persistência local utilizando SQLite.

Alguns recursos permanecem em desenvolvimento e podem sofrer alterações incompatíveis entre versões.

### Cartografia (C-01 + C-02)

Na aba **Cartografia**, crie um planeta, ajuste seed/água/relevo e gere uma
prévia. Aceite para salvar ou descarte sem alterar o Projeto. Arraste o mapa,
use a roda ou os botões de zoom e escolha uma camada. **Visual suave** usa o
melhor LOD que cabe no orçamento e interpolação bilinear; **Visual raster**
preserva a aparência das células quando ela for desejada. **Contorno de costa**
realça somente a borda terra–água e pode ser desligado. Selecione um Local do
Planejamento para definir coordenadas ou usar o modo de posicionamento por
clique. O modo de medição calcula distância sobre a esfera, não uma rota.
**Exportar SVG** salva a vista atual. A geração funciona offline e sem IA.

Para editar o relevo, escolha **Elevar terreno**, **Rebaixar terreno** ou
**Suavizar terreno**, ajuste raio/intensidade e clique no mapa. O clique cria
uma prévia: use **Aplicar relevo** para persistir ou **Descartar prévia** para
preservar o Projeto. Undo/redo de terreno cobre as últimas dez operações da
sessão. **Bloquear área do pincel** protege uma região nomeada e persistente;
não representa território nem permissão de acesso.

O terreno tem quatro níveis, até 1024×512 amostras; zoom só revela dados
existentes e não refina o planeta. Rios, biomas, importação de heightmap e
territórios/rotas temporais ainda não estão presentes. O esquema atual é
**v15**: faça cópia de seus Projetos antes de testar, e não abra cópias migradas
com executáveis antigos. Um AppImage anteriormente gerado precisa ser
reconstruído para incluir estas alterações. A validação humana de C-02
permanece pendente.

## Plataforma

O INDE é desenvolvido atualmente para ambientes Linux.

Dependências principais:

* compilador com suporte a C++20;
* CMake 3.22 ou superior;
* GTK 4.8 ou superior;
* gtkmm 4.8 ou superior;
* SQLite 3 com módulo R-tree (`ENABLE_RTREE`);
* libuuid;
* Enchant 2;
* dicionário Hunspell pt-BR.

### Ubuntu / Linux Lite

```bash
sudo apt update

sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  libgtkmm-4.0-dev \
  uuid-dev \
  libsqlite3-dev \
  libenchant-2-2 \
  hunspell-pt-br
```

## Compilação

Clone o repositório:

```bash
git clone https://github.com/Roger-Cardoso/inde.git
cd inde
```

Configure o projeto:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

Compile:

```bash
cmake --build build
```

Execute os testes:

```bash
ctest --test-dir build --output-on-failure
```

Execute o programa:

```bash
./build/inde
```

Também é possível abrir diretamente um projeto:

```bash
./build/inde "/caminho/Projeto.inde"
```

## AppImage para Linux x86_64

As versões distribuídas em AppImage reúnem o executável, o runtime GTK 4 e a
revisão ortográfica pt-BR num único arquivo. Depois de baixar o AppImage e o
arquivo `.sha256` correspondente em
[Releases](https://github.com/Roger-Cardoso/inde/releases):

```bash
sha256sum --check INDE-0.1.0-x86_64.AppImage.sha256
chmod +x INDE-0.1.0-x86_64.AppImage
./INDE-0.1.0-x86_64.AppImage
```

Um projeto também pode ser aberto diretamente:

```bash
./INDE-0.1.0-x86_64.AppImage "/caminho/Projeto.inde"
```

Se a distribuição não oferecer FUSE, use o modo de extração temporária:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./INDE-0.1.0-x86_64.AppImage
```

A receita local, as dependências, o fluxo de publicação e a matriz de validação
estão documentados em [`packaging/README.md`](packaging/README.md).

## Formato de projeto `.inde`

Um projeto INDE é armazenado como um diretório-contêiner com extensão `.inde`.

Estrutura básica:

```text
Projeto.inde/
├── manifest.json
├── assets/
├── data/
│   └── project.sqlite3
└── documents/
```

O `manifest.json` contém informações fundamentais do projeto.

O banco `project.sqlite3` é a principal fonte operacional dos dados estruturados nas versões atuais do formato.

A estrutura em diretório permite que o projeto permaneça autocontido e possa ser copiado ou armazenado como uma unidade.

## Arquitetura

O código é organizado principalmente em quatro áreas:

```text
include/inde/ + src/
├── project/       domínio e estruturas fundamentais
├── application/   serviços e casos de uso
├── persistence/   armazenamento e SQLite
└── ui/            interface gráfica GTK
```

A lógica central do programa é mantida separada da interface gráfica sempre que possível, permitindo que operações de domínio e persistência sejam testadas sem inicializar GTK.

## Persistência

A persistência principal utiliza SQLite.

O projeto implementa, entre outros mecanismos:

* chaves estrangeiras;
* migração versionada de esquema;
* transações;
* rollback de operações;
* statements preparados;
* validações de integridade;
* identificadores persistentes;
* operações de cópia de projeto com nova identidade.

O formato do banco pode evoluir enquanto o programa estiver em desenvolvimento.

## Testes e benchmarks

O projeto possui uma suíte de testes para o núcleo e programas auxiliares de benchmark.

Após a compilação:

```bash
ctest --test-dir build --output-on-failure
```

Alguns executáveis adicionais podem ser gerados quando `BUILD_TESTING` está habilitado, incluindo benchmarks relacionados ao planejamento narrativo, timeline, escrita, workspace e revisão textual.

## Projetos de demonstração

O diretório `examples/` contém projetos sintéticos criados especificamente para demonstração e testes do INDE.

Eles não contêm obras literárias de terceiros.

Um exemplo pode ser aberto diretamente:

```bash
./build/inde "examples/Atlas de Véspera — Projeto Integral.inde"
```

## Dados do usuário

O INDE funciona localmente e os projetos permanecem armazenados no computador do usuário.

O programa não depende de uma conta online para utilizar o formato de projeto e seus recursos locais atuais.

Recomenda-se manter cópias de segurança regulares de projetos importantes.

## Licença

O código original do INDE é distribuído sob a **INDE Non-Commercial Educational License 1.0**.

Consulte:

* `LICENSE`
* `THIRD_PARTY_NOTICES.md`

A licença permite determinados usos pessoais, estudantis, educacionais e não comerciais do código, conforme seus termos.

Obras, textos, documentos e demais conteúdos produzidos pelos usuários utilizando uma versão autorizada do INDE permanecem propriedade de seus respectivos autores. A licença do programa não reivindica propriedade ou royalties sobre esses conteúdos.

Componentes de terceiros utilizados pelo projeto permanecem sujeitos às suas próprias licenças.

## Autor

**Roger Cardoso**

INDE — Integrated Narrative Development Environment

https://github.com/Roger-Cardoso/inde
